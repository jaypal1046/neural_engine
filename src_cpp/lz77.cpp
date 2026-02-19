#include "lz77.h"
#include <unordered_map>
#include <vector>
#include <cstring>
#include <stdexcept>
#ifdef __SSE2__
#  include <immintrin.h>
#endif

// -----------------------------------------------------------------------------
// Hash table key: pack 3 bytes into a uint32
// -----------------------------------------------------------------------------
static inline uint32_t hash3(const uint8_t* p) {
    return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}

// -----------------------------------------------------------------------------
// match_length: count matching bytes between a and b, up to max_len.
// Uses SSE2 16-bytes-at-a-time when available.
// -----------------------------------------------------------------------------
static inline int match_length(const uint8_t* a, const uint8_t* b, int max_len) {
    int len = 0;
#ifdef __SSE2__
    while (len + 16 <= max_len) {
        __m128i va   = _mm_loadu_si128((const __m128i*)(a + len));
        __m128i vb   = _mm_loadu_si128((const __m128i*)(b + len));
        uint32_t diff = (~(uint32_t)_mm_movemask_epi8(_mm_cmpeq_epi8(va, vb))) & 0xFFFFu;
        if (diff) return len + __builtin_ctz(diff);
        len += 16;
    }
#endif
    while (len < max_len && a[len] == b[len]) ++len;
    return len;
}

// -----------------------------------------------------------------------------
// lz77_encode
// -----------------------------------------------------------------------------
std::vector<LZToken> lz77_encode(
    const uint8_t* data, size_t n,
    std::function<void(size_t, size_t)> progress_cb,
    bool use_lazy)
{
    std::vector<LZToken> tokens;
    tokens.reserve(n / 2);

    // hash_table: 3-byte hash -> sorted list of positions in data
    // We store up to 8 candidates per key to keep memory sane on large files
    static constexpr int MAX_CHAIN = 8;
    std::unordered_map<uint32_t, std::vector<uint32_t>> ht;
    ht.reserve(1 << 16);

    auto add_pos = [&](size_t pos) {
        if (pos + 3 > n) return;
        uint32_t key = hash3(data + pos);
        auto& chain  = ht[key];
        if (chain.size() < MAX_CHAIN)
            chain.push_back((uint32_t)pos);
        else {
            // Replace oldest (index 0) — sliding window eviction
            chain.erase(chain.begin());
            chain.push_back((uint32_t)pos);
        }
    };

    // -------------------------------------------------------------------------
    // find_best_match: search hash table at position pos, return best
    // (length, distance) pair. Returns length=0 if no match >= LZ_MIN_MATCH.
    // -------------------------------------------------------------------------
    auto find_best_match = [&](size_t pos, int& out_len, int& out_dist) {
        out_len  = 0;
        out_dist = 0;
        if (pos + LZ_MIN_MATCH > n) return;

        uint32_t key       = hash3(data + pos);
        auto     it        = ht.find(key);
        if (it == ht.end()) return;

        size_t win_start = (pos >= (size_t)LZ_WINDOW) ? pos - LZ_WINDOW : 0;
        const auto& chain = it->second;

        for (int c = (int)chain.size() - 1; c >= 0; --c) {
            size_t cand = chain[c];
            if (cand < win_start) break;
            if (cand >= pos) continue;

            int max_len = (int)std::min((size_t)LZ_MAX_MATCH, n - pos);
            int length  = match_length(data + cand, data + pos, max_len);

            if (length > out_len) {
                out_len  = length;
                out_dist = (int)(pos - cand);
                if (out_len == LZ_MAX_MATCH) break;
            }
        }
    };

    // -------------------------------------------------------------------------
    // Lazy matching main loop (zlib level 4+ strategy):
    //   At position i, find best match (prev_len, prev_dist).
    //   Peek at i+1. If that match is strictly longer, emit literal at i
    //   and take the better match at i+1 instead.
    //   This gives 1-3% better compression vs pure greedy at ~25% speed cost.
    // -------------------------------------------------------------------------
    size_t i        = 0;
    size_t next_rpt = 65536;

    int prev_len  = 0;
    int prev_dist = 0;

    // Prime: find match at position 0
    if (n > 0) find_best_match(0, prev_len, prev_dist);

    while (i < n) {
        // Progress reporting
        if (progress_cb && i >= next_rpt) {
            progress_cb(i, n);
            next_rpt = i + 65536;
        }

        // Near end of input — no room for lazy lookahead, emit literal
        if (i + LZ_MIN_MATCH > n) {
            LZToken t; t.is_match = false; t.byte = data[i];
            t.dist = 0; t.len = 0;
            tokens.push_back(t);
            add_pos(i);
            ++i;
            prev_len = 0;
            continue;
        }

        // Lazy check thresholds (zlib-inspired):
        //   NICE_LENGTH: skip lazy if prev_len already this long (not worth it)
        //   LAZY_MIN:    only do lazy if prev_len <= this (minimal matches only)
        // These together mean: only try lazy when the current match is short
        // (3-8 bytes) and not already at the "nice" threshold.
        static constexpr int NICE_LENGTH = 32;
        static constexpr int LAZY_MIN    = 8;

        if (prev_len < LZ_MIN_MATCH) {
            // No usable match at i — emit literal, find match at i+1
            LZToken t; t.is_match = false; t.byte = data[i];
            t.dist = 0; t.len = 0;
            tokens.push_back(t);
            add_pos(i);
            ++i;
            find_best_match(i, prev_len, prev_dist);
        } else {
            // We have a match at i. Check i+1 for a better one (lazy step),
            // but only within the useful range [LZ_MIN_MATCH, LAZY_MIN].
            int next_len = 0, next_dist = 0;
            if (use_lazy && prev_len <= LAZY_MIN && prev_len < NICE_LENGTH && i + 1 < n)
                find_best_match(i + 1, next_len, next_dist);

            if (next_len > prev_len) {
                // Better match one byte ahead — emit literal at i, defer
                LZToken t; t.is_match = false; t.byte = data[i];
                t.dist = 0; t.len = 0;
                tokens.push_back(t);
                add_pos(i);
                ++i;
                prev_len  = next_len;
                prev_dist = next_dist;
            } else {
                // Take the match at i
                LZToken t;
                t.is_match = true;
                t.byte     = 0;
                t.dist     = (uint16_t)prev_dist;
                t.len      = (uint8_t)(prev_len - LZ_MIN_MATCH);
                tokens.push_back(t);
                for (int j = 0; j < prev_len; ++j)
                    add_pos(i + j);
                i += prev_len;
                // Find match at new position
                find_best_match(i, prev_len, prev_dist);
            }
        }
    }

    if (progress_cb) progress_cb(n, n);
    return tokens;
}

// -----------------------------------------------------------------------------
// lz77_decode
// -----------------------------------------------------------------------------
std::vector<uint8_t> lz77_decode(const LZToken* tokens, size_t count) {
    std::vector<uint8_t> out;
    out.reserve(count * 4);   // rough estimate

    for (size_t i = 0; i < count; ++i) {
        const LZToken& t = tokens[i];
        if (!t.is_match) {
            out.push_back(t.byte);
        } else {
            int actual_len  = t.len + LZ_MIN_MATCH;
            size_t start    = out.size();
            if (t.dist > start)
                throw std::runtime_error("lz77_decode: invalid distance");
            size_t src = start - t.dist;
            // Copy byte by byte — handles overlapping matches correctly
            for (int j = 0; j < actual_len; ++j)
                out.push_back(out[src + j]);
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// lz77_serialize
// Format: 0x00 <byte>              for literal   (2 bytes)
//         0x01 <dH> <dL> <len>     for match     (4 bytes)
// -----------------------------------------------------------------------------
std::vector<uint8_t> lz77_serialize(const LZToken* tokens, size_t count) {
    std::vector<uint8_t> out;
    out.reserve(count * 3);

    for (size_t i = 0; i < count; ++i) {
        const LZToken& t = tokens[i];
        if (!t.is_match) {
            out.push_back(0x00);
            out.push_back(t.byte);
        } else {
            out.push_back(0x01);
            out.push_back((t.dist >> 8) & 0xFF);
            out.push_back(t.dist & 0xFF);
            out.push_back(t.len);
        }
    }
    return out;
}

// -----------------------------------------------------------------------------
// lz77_deserialize
// -----------------------------------------------------------------------------
std::vector<LZToken> lz77_deserialize(const uint8_t* data, size_t len) {
    std::vector<LZToken> tokens;
    size_t i = 0;
    while (i < len) {
        uint8_t flag = data[i++];
        if (flag == 0x00) {
            if (i >= len)
                throw std::runtime_error("lz77_deserialize: truncated literal");
            LZToken t;
            t.is_match = false;
            t.byte     = data[i++];
            t.dist     = 0;
            t.len      = 0;
            tokens.push_back(t);
        } else if (flag == 0x01) {
            if (i + 3 > len)
                throw std::runtime_error("lz77_deserialize: truncated match");
            LZToken t;
            t.is_match = true;
            t.byte     = 0;
            t.dist     = ((uint16_t)data[i] << 8) | data[i+1];
            t.len      = data[i+2];
            i += 3;
            tokens.push_back(t);
        } else {
            throw std::runtime_error("lz77_deserialize: unknown flag byte");
        }
    }
    return tokens;
}
