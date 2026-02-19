#include "ppm.h"
#include <unordered_map>
#include <cstring>
#include <stdexcept>
#include <algorithm>

// =============================================================================
// Range Coder (arithmetic coding, 32-bit state)
// Based on the classic Schindler/Subbotin range coder.
// Encoder emits bytes to a vector; decoder reads from a byte pointer.
// =============================================================================

struct RangeEncoder {
    uint32_t low  = 0;
    uint32_t range = 0xFFFFFFFFu;
    std::vector<uint8_t> buf;

    void encode(uint32_t cum, uint32_t freq, uint32_t total) {
        range /= total;
        low   += cum * range;
        range *= freq;
        // Renormalize: while top byte is equal, emit it
        while ((low ^ (low + range)) < (1u << 24)) {
            buf.push_back((uint8_t)(low >> 24));
            low   <<= 8;
            range <<= 8;
        }
        // Handle range underflow
        while (range < (1u << 16)) {
            buf.push_back((uint8_t)(low >> 24));
            range = (~low) << 8;
            low  <<= 8;
        }
    }

    std::vector<uint8_t> finish() {
        // Flush: emit 4 bytes of low
        for (int i = 3; i >= 0; --i)
            buf.push_back((uint8_t)(low >> (i * 8)));
        return std::move(buf);
    }
};

struct RangeDecoder {
    uint32_t       low   = 0;
    uint32_t       range = 0xFFFFFFFFu;
    uint32_t       code  = 0;
    const uint8_t* src   = nullptr;
    size_t         pos   = 0;
    size_t         len   = 0;

    void init(const uint8_t* data, size_t size) {
        src = data; len = size;
        // Read 4 bytes to prime the code register
        for (int i = 0; i < 4; ++i)
            code = (code << 8) | (pos < len ? src[pos++] : 0);
    }

    // Returns cumulative up to (but not including) the symbol for the given total
    uint32_t get_cum(uint32_t total) {
        range /= total;
        return (code - low) / range;
    }

    void decode(uint32_t cum, uint32_t freq) {
        low   += cum * range;
        range *= freq;
        while ((low ^ (low + range)) < (1u << 24)) {
            code  = (code  << 8) | (pos < len ? src[pos++] : 0);
            low  <<= 8;
            range <<= 8;
        }
        while (range < (1u << 16)) {
            code  = (code  << 8) | (pos < len ? src[pos++] : 0);
            range = (~low) << 8;
            low  <<= 8;
        }
    }
};

// =============================================================================
// PPM Context Table
// =============================================================================

struct ContextEntry {
    uint32_t total = 0;
    uint16_t freq[256];

    ContextEntry() {
        memset(freq, 0, sizeof(freq));
    }
};

// Pack N bytes (N=1..4) into a uint64 context key.
// High byte encodes order (N) to avoid collisions between orders.
static inline uint64_t make_key(const uint8_t* ctx, int order) {
    uint64_t key = (uint64_t)(order) << 32;
    for (int i = 0; i < order; ++i)
        key |= (uint64_t)ctx[i] << (i * 8);
    return key;
}

// =============================================================================
// PPM Encode
// =============================================================================

std::vector<uint8_t> ppm_encode(const uint8_t* data, size_t len) {
    // Context table (shared across all orders, keyed by (order, ctx_bytes))
    // Bounded by PPM_MAX_ENTRIES — LRU eviction not used; simply stop adding
    // new entries when table is full (model freezes but remains correct).
    std::unordered_map<uint64_t, ContextEntry> table;
    table.reserve(PPM_MAX_ENTRIES);

    // Order-0 model: always present, initialized flat (1 per symbol)
    ContextEntry order0;
    for (int s = 0; s < 256; ++s) {
        order0.freq[s] = 1;
        order0.total  += 1;
    }

    RangeEncoder enc;
    uint8_t ctx[PPM_ORDER] = {};  // sliding context window

    for (size_t i = 0; i < len; ++i) {
        uint8_t sym = data[i];
        bool encoded = false;

        // Try from highest order down to order-1
        for (int ord = PPM_ORDER; ord >= 1 && !encoded; --ord) {
            // Build context key from the last `ord` bytes
            // ctx[0] = most recent byte, ctx[ord-1] = oldest
            uint64_t key = make_key(ctx, ord);
            auto it = table.find(key);
            if (it == table.end() || it->second.freq[sym] == 0) {
                // Context not seen or symbol not in context: emit escape
                // Escape is coded as a uniform probability 1/(unique_syms+1)
                // (simplified: use 1/(total+1) for escape slot)
                if (it != table.end()) {
                    auto& e = it->second;
                    uint32_t escape_cum = e.total;  // cumulative = sum of all freqs
                    uint32_t escape_freq = 1;
                    uint32_t escape_total = e.total + 1;
                    enc.encode(escape_cum, escape_freq, escape_total);
                }
                // fall through to lower order
            } else {
                // Encode symbol in this context
                auto& e = it->second;
                // Compute cumulative frequency for sym
                uint32_t cum = 0;
                for (int s = 0; s < sym; ++s) cum += e.freq[s];
                uint32_t total = e.total + 1;  // +1 for escape slot
                enc.encode(cum, (uint32_t)e.freq[sym], total);
                encoded = true;
            }
        }

        if (!encoded) {
            // Encode with order-0 (always succeeds — all symbols present with freq≥1)
            uint32_t cum = 0;
            for (int s = 0; s < sym; ++s) cum += order0.freq[s];
            enc.encode(cum, order0.freq[sym], order0.total);
        }

        // Update all context models with sym (order 1..PPM_ORDER)
        // Update order-0 always
        if (order0.freq[sym] < 65535) {
            order0.freq[sym]++;
            order0.total++;
        }

        // Update order 1..PPM_ORDER
        for (int ord = 1; ord <= PPM_ORDER; ++ord) {
            if ((int)i < ord - 1) break;  // not enough history yet
            uint64_t key = make_key(ctx, ord);
            if (table.size() < (size_t)PPM_MAX_ENTRIES || table.count(key)) {
                auto& e = table[key];
                if (e.freq[sym] < 65535) {
                    e.freq[sym]++;
                    e.total++;
                }
            }
        }

        // Advance context window (shift right, put newest at index 0)
        for (int j = PPM_ORDER - 1; j > 0; --j)
            ctx[j] = ctx[j-1];
        ctx[0] = sym;
    }

    return enc.finish();
}

// =============================================================================
// PPM Decode
// =============================================================================

std::vector<uint8_t> ppm_decode(const uint8_t* coded, size_t coded_len, size_t sym_count) {
    std::unordered_map<uint64_t, ContextEntry> table;
    table.reserve(PPM_MAX_ENTRIES);

    ContextEntry order0;
    for (int s = 0; s < 256; ++s) {
        order0.freq[s] = 1;
        order0.total  += 1;
    }

    RangeDecoder dec;
    dec.init(coded, coded_len);

    std::vector<uint8_t> out;
    out.reserve(sym_count);

    uint8_t ctx[PPM_ORDER] = {};

    for (size_t i = 0; i < sym_count; ++i) {
        uint8_t sym = 0;
        bool decoded = false;

        for (int ord = PPM_ORDER; ord >= 1 && !decoded; --ord) {
            uint64_t key = make_key(ctx, ord);
            auto it = table.find(key);
            if (it == table.end()) continue;  // context not seen → skip (no escape emitted)

            auto& e = it->second;
            uint32_t total = e.total + 1;  // +1 for escape slot
            uint32_t target = dec.get_cum(total);

            if (target >= e.total) {
                // Escape: consume the escape coding
                dec.decode(e.total, 1);
                // Try lower order
            } else {
                // Find symbol
                uint32_t cum = 0;
                for (int s = 0; s < 256; ++s) {
                    if (e.freq[s] == 0) continue;
                    if (cum + e.freq[s] > target) {
                        sym = (uint8_t)s;
                        dec.decode(cum, e.freq[s]);
                        decoded = true;
                        break;
                    }
                    cum += e.freq[s];
                }
            }
        }

        if (!decoded) {
            // Decode from order-0
            uint32_t target = dec.get_cum(order0.total);
            uint32_t cum = 0;
            for (int s = 0; s < 256; ++s) {
                if (cum + order0.freq[s] > target) {
                    sym = (uint8_t)s;
                    dec.decode(cum, order0.freq[s]);
                    break;
                }
                cum += order0.freq[s];
            }
        }

        out.push_back(sym);

        // Update models (same as encoder)
        if (order0.freq[sym] < 65535) {
            order0.freq[sym]++;
            order0.total++;
        }
        for (int ord = 1; ord <= PPM_ORDER; ++ord) {
            if ((int)i < ord - 1) break;
            uint64_t key = make_key(ctx, ord);
            if (table.size() < (size_t)PPM_MAX_ENTRIES || table.count(key)) {
                auto& e = table[key];
                if (e.freq[sym] < 65535) {
                    e.freq[sym]++;
                    e.total++;
                }
            }
        }

        for (int j = PPM_ORDER - 1; j > 0; --j)
            ctx[j] = ctx[j-1];
        ctx[0] = sym;
    }

    return out;
}
