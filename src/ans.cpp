#include "ans.h"
#include <algorithm>
#include <stdexcept>
#include <cstring>

// -----------------------------------------------------------------------------
// Frequency normalization: raw counts -> freq[] summing exactly to ANS_M.
// -----------------------------------------------------------------------------
bool ans_build_table(const uint32_t raw_freq[256], AnsTable& table) {
    memset(&table, 0, sizeof(table));

    uint64_t total = 0;
    int active = 0;
    for (int s = 0; s < 256; ++s) {
        if (raw_freq[s] > 0) { total += raw_freq[s]; ++active; }
    }
    if (active == 0) return false;

    // Scale to ANS_M: each active symbol gets at least 1 slot.
    uint32_t scaled[256] = {};
    uint32_t used = 0;
    for (int s = 0; s < 256; ++s) {
        if (raw_freq[s] == 0) continue;
        scaled[s] = (uint32_t)std::max((uint64_t)1,
                        (uint64_t)raw_freq[s] * ANS_M / total);
        used += scaled[s];
    }

    // Fix rounding error by distributing slack to symbols with largest remainder.
    // Repeat until the sum is exactly ANS_M (handles large over/under-allocation).
    while (used != ANS_M) {
        struct SR { int sym; int64_t rem; };
        SR rem[256]; int nr = 0;
        for (int s = 0; s < 256; ++s) {
            if (raw_freq[s] == 0) continue;
            int64_t r = (int64_t)raw_freq[s] * ANS_M - (int64_t)scaled[s] * total;
            rem[nr++] = {s, r};
        }
        std::sort(rem, rem + nr, [](const SR& a, const SR& b){ return a.rem > b.rem; });

        int delta = (int)(ANS_M - used);
        if (delta > 0) {
            // Under-allocated: add slots to symbols with highest positive remainder
            for (int d = 0; d < delta && d < nr; ++d)
                ++scaled[rem[d].sym];
            used += delta;
        } else {
            // Over-allocated: remove slots from symbols with largest scaled (most to give)
            // Sort by scaled value descending to find symbols that can afford to lose a slot
            std::sort(rem, rem + nr, [&](const SR& a, const SR& b){
                return scaled[a.sym] > scaled[b.sym];
            });
            int removed = 0;
            for (int d = 0; d < nr && removed < -delta; ++d) {
                if (scaled[rem[d].sym] > 1) {
                    --scaled[rem[d].sym];
                    ++removed;
                }
            }
            used -= removed;
        }
    }

    for (int s = 0; s < 256; ++s)
        table.freq[s] = (uint16_t)scaled[s];

    // Build cumulative
    table.cumul[0] = 0;
    for (int s = 0; s < 256; ++s)
        table.cumul[s+1] = table.cumul[s] + table.freq[s];

    // Build decode table
    for (int s = 0; s < 256; ++s) {
        for (uint32_t x = table.cumul[s]; x < (uint32_t)table.cumul[s+1]; ++x) {
            table.sym[x]  = (uint8_t)s;
            table.bias[x] = (uint16_t)(x - table.cumul[s]);
        }
    }
    return true;
}

void ans_write_freqs(const AnsTable& table, std::vector<uint8_t>& out) {
    for (int s = 0; s < 256; ++s) {
        out.push_back( table.freq[s]       & 0xFF);
        out.push_back((table.freq[s] >> 8) & 0xFF);
    }
}

AnsTable ans_read_freqs(const uint8_t freqs[512]) {
    AnsTable table;
    memset(&table, 0, sizeof(table));
    for (int s = 0; s < 256; ++s)
        table.freq[s] = (uint16_t)freqs[s*2] | ((uint16_t)freqs[s*2+1] << 8);
    table.cumul[0] = 0;
    for (int s = 0; s < 256; ++s)
        table.cumul[s+1] = table.cumul[s] + table.freq[s];
    for (int s = 0; s < 256; ++s) {
        for (uint32_t x = table.cumul[s]; x < (uint32_t)table.cumul[s+1]; ++x) {
            table.sym[x]  = (uint8_t)s;
            table.bias[x] = (uint16_t)(x - table.cumul[s]);
        }
    }
    return table;
}

// -----------------------------------------------------------------------------
// AnsEncoder
// State invariant: state in [ANS_M, ANS_M*256) between calls.
// Encoding happens in REVERSE symbol order.
// -----------------------------------------------------------------------------
void AnsEncoder::put(uint8_t sym, const AnsTable& table) {
    uint32_t fs = table.freq[sym];
    if (fs == 0)
        throw std::runtime_error("ans_encode: symbol not in table");

    uint32_t x = state;

    // Renormalize: bring x into [fs, fs*256) by emitting low bytes.
    // After encode: x' = (x/fs)*ANS_M + cumul[s] + (x%fs) is in [ANS_M, ANS_M*256).
    uint32_t upper = fs << 8;
    while (x >= upper) {
        buf.push_back((uint8_t)(x & 0xFF));
        x >>= 8;
    }

    state = (x / fs) * ANS_M + table.cumul[sym] + (x % fs);
}

std::vector<uint8_t> AnsEncoder::finish() {
    // Output: [state_LE_4bytes] + [emitted bytes reversed for forward decode]
    std::vector<uint8_t> out;
    out.reserve(4 + buf.size());
    out.push_back((uint8_t)( state        & 0xFF));
    out.push_back((uint8_t)((state >>  8) & 0xFF));
    out.push_back((uint8_t)((state >> 16) & 0xFF));
    out.push_back((uint8_t)((state >> 24) & 0xFF));
    out.insert(out.end(), buf.rbegin(), buf.rend());
    return out;
}

// -----------------------------------------------------------------------------
// AnsDecoder
// -----------------------------------------------------------------------------
void AnsDecoder::init(const uint8_t* data, size_t size) {
    if (size < 4)
        throw std::runtime_error("ans_decode: stream too short");
    src   = data;
    len   = size;
    state = (uint32_t)src[0] | ((uint32_t)src[1]<<8)
          | ((uint32_t)src[2]<<16) | ((uint32_t)src[3]<<24);
    pos   = 4;
}

uint8_t AnsDecoder::get(const AnsTable& table) {
    uint32_t x    = state;
    uint32_t slot = x & (ANS_M - 1);
    uint8_t  sym  = table.sym[slot];
    uint32_t fs   = table.freq[sym];
    uint32_t bias = table.bias[slot];

    x = fs * (x >> ANS_LOG_M) + bias;

    // Renormalize: bring x up to [ANS_M, ANS_M*256)
    while (x < ANS_M && pos < len)
        x = (x << 8) | src[pos++];

    state = x;
    return sym;
}

// -----------------------------------------------------------------------------
// Order-1 context model implementation
// -----------------------------------------------------------------------------

void ans_ctx1_build(const uint32_t pair_freq[256][256], AnsCtx1& ctx) {
    for (int c = 0; c < 256; ++c) {
        // Check if this context has any data
        uint64_t total = 0;
        for (int s = 0; s < 256; ++s) total += pair_freq[c][s];
        if (total == 0) {
            // Unused context: build a flat table so decoder doesn't crash
            uint32_t flat[256];
            for (int s = 0; s < 256; ++s) flat[s] = 1;
            ans_build_table(flat, ctx.tables[c]);
        } else {
            ans_build_table(pair_freq[c], ctx.tables[c]);
        }
    }
}

void ans_ctx1_write(const AnsCtx1& ctx, std::vector<uint8_t>& out) {
    for (int c = 0; c < 256; ++c)
        ans_write_freqs(ctx.tables[c], out);
    // Total: 256 * 512 = 131072 bytes
}

std::unique_ptr<AnsCtx1> ans_ctx1_read(const uint8_t* data) {
    auto ctx = std::make_unique<AnsCtx1>();
    for (int c = 0; c < 256; ++c)
        ctx->tables[c] = ans_read_freqs(data + c * 512);
    return ctx;
}

std::vector<uint8_t> ans_ctx1_encode(const uint8_t* data, size_t len,
                                      const AnsCtx1& ctx) {
    if (len == 0) {
        // Return 4-byte initial state only
        AnsEncoder enc;
        return enc.finish();
    }

    // Encode backwards: data[len-1], data[len-2], ..., data[0]
    // Context for data[i] is data[i-1] (or 0 for i==0)
    AnsEncoder enc;
    for (int i = (int)len - 1; i >= 0; --i) {
        uint8_t ctx_byte = (i > 0) ? data[i-1] : 0;
        enc.put(data[i], ctx.tables[ctx_byte]);
    }
    return enc.finish();
}

std::vector<uint8_t> ans_ctx1_decode(const uint8_t* encoded, size_t enc_len,
                                      size_t sym_count, const AnsCtx1& ctx) {
    std::vector<uint8_t> out(sym_count);
    if (sym_count == 0) return out;

    AnsDecoder dec;
    dec.init(encoded, enc_len);

    uint8_t prev = 0;  // initial context
    for (size_t i = 0; i < sym_count; ++i) {
        out[i] = dec.get(ctx.tables[prev]);
        prev   = out[i];
    }
    return out;
}

// -----------------------------------------------------------------------------
// Order-2 context model
// Context key = (prev2 * 5 + prev1) & 0xFF
// Reuses AnsCtx1 (same 256-table structure, same 131KB wire format).
// The * 5 multiplier spreads the 256x256 context space into 256 buckets
// with low collision rate for typical byte streams.
// -----------------------------------------------------------------------------

static inline uint8_t ctx2_key(uint8_t prev2, uint8_t prev1) {
    return (uint8_t)((prev2 * 5u + prev1) & 0xFFu);
}

void ans_ctx2_build(const uint8_t* data, size_t len, AnsCtx1& ctx) {
    // Accumulate pair frequencies per folded context key
    // pair_freq[key][symbol]
    auto pf = std::make_unique<uint32_t[]>(256 * 256);
    memset(pf.get(), 0, 256 * 256 * sizeof(uint32_t));

    uint8_t prev2 = 0, prev1 = 0;
    for (size_t i = 0; i < len; ++i) {
        uint8_t key = ctx2_key(prev2, prev1);
        pf[key * 256 + data[i]]++;
        prev2 = prev1;
        prev1 = data[i];
    }

    // Build 256 tables from accumulated frequencies
    const uint32_t (*pf2d)[256] = reinterpret_cast<const uint32_t(*)[256]>(pf.get());
    ans_ctx1_build(pf2d, ctx);
}

std::vector<uint8_t> ans_ctx2_encode(const uint8_t* data, size_t len,
                                      const AnsCtx1& ctx) {
    if (len == 0) {
        AnsEncoder enc;
        return enc.finish();
    }
    // Encode backwards: data[len-1], ..., data[0]
    // Context for data[i]: prev1 = data[i-1], prev2 = data[i-2]
    AnsEncoder enc;
    for (int i = (int)len - 1; i >= 0; --i) {
        uint8_t prev1 = (i > 0) ? data[i-1] : 0;
        uint8_t prev2 = (i > 1) ? data[i-2] : 0;
        uint8_t key   = ctx2_key(prev2, prev1);
        enc.put(data[i], ctx.tables[key]);
    }
    return enc.finish();
}

std::vector<uint8_t> ans_ctx2_decode(const uint8_t* encoded, size_t enc_len,
                                      size_t sym_count, const AnsCtx1& ctx) {
    std::vector<uint8_t> out(sym_count);
    if (sym_count == 0) return out;

    AnsDecoder dec;
    dec.init(encoded, enc_len);

    uint8_t prev2 = 0, prev1 = 0;
    for (size_t i = 0; i < sym_count; ++i) {
        uint8_t key = ctx2_key(prev2, prev1);
        out[i] = dec.get(ctx.tables[key]);
        prev2  = prev1;
        prev1  = out[i];
    }
    return out;
}
