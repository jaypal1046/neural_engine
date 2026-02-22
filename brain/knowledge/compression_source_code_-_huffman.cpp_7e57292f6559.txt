#include "huffman.h"
#include <algorithm>
#include <queue>
#include <stdexcept>
#include <cstring>

// -----------------------------------------------------------------------------
// Internal: build tree and assign code lengths
// -----------------------------------------------------------------------------

struct HNode {
    uint32_t freq;
    int      sym;    // -1 for internal nodes
    int      left;   // index into node pool, -1 if leaf
    int      right;
};

// Assign code lengths by walking the tree (BFS/DFS from root)
// No cap — raw depths from tree. Length limiting handled separately.
static void assign_lengths(const std::vector<HNode>& nodes, int idx,
                            int depth, uint8_t lengths[HUFF_SYMBOLS]) {
    if (idx < 0) return;
    const HNode& n = nodes[idx];
    if (n.sym >= 0) {
        // leaf — store depth (may exceed 15 for unbalanced trees)
        lengths[n.sym] = (uint8_t)std::min(depth, 255);
        return;
    }
    assign_lengths(nodes, n.left,  depth + 1, lengths);
    assign_lengths(nodes, n.right, depth + 1, lengths);
}

// Limit Huffman code lengths to max_bits using the standard approach:
// 1. Clamp lengths > max_bits down to max_bits.
// 2. Fix the Kraft inequality by increasing lengths of short-code symbols
//    until sum(2^(max_bits - len)) == 2^max_bits.
// This preserves correctness: the resulting lengths define a valid prefix code.
static void limit_lengths(uint8_t lengths[HUFF_SYMBOLS], int max_bits) {
    bool any_over = false;
    for (int s = 0; s < HUFF_SYMBOLS; ++s) {
        if (lengths[s] > (uint8_t)max_bits) {
            lengths[s] = (uint8_t)max_bits;
            any_over = true;
        }
    }
    if (!any_over) return;

    // Fix Kraft inequality: excess = kraft - 2^max_bits.
    // When we clamp L symbols from depth d > max_bits to max_bits,
    // we add too many "slots". We fix by repeatedly finding the
    // symbol with the current shortest length (most slots) and
    // incrementing it by 1 (halves its slot count).
    for (int iter = 0; iter < 10000; ++iter) {
        // Compute current Kraft sum
        long long kraft = 0;
        for (int s = 0; s < HUFF_SYMBOLS; ++s)
            if (lengths[s] > 0)
                kraft += (1LL << (max_bits - lengths[s]));

        long long target = (1LL << max_bits);
        if (kraft <= target) break;  // done (under or equal is fine — unused slots OK)

        // Too many slots: find shortest-length active symbol and increase it
        int min_len = max_bits + 1;
        for (int s = 0; s < HUFF_SYMBOLS; ++s)
            if (lengths[s] > 0 && lengths[s] < min_len)
                min_len = lengths[s];

        if (min_len > max_bits) break;  // can't increase further

        // Increase the LAST symbol with min_len (least likely to affect
        // high-frequency symbols)
        for (int s = HUFF_SYMBOLS - 1; s >= 0; --s) {
            if (lengths[s] == (uint8_t)min_len) {
                lengths[s]++;
                break;
            }
        }
    }
}

// Build canonical codes from lengths:
// Sort symbols by (length, symbol), then assign codes sequentially.
// This matches the canonical Huffman standard used by DEFLATE/gzip.
static void build_canonical_codes(HuffTable& table) {
    // Count how many symbols have each bit length
    int bl_count[16] = {};
    for (int s = 0; s < HUFF_SYMBOLS; ++s)
        if (table.code_len[s] > 0)
            bl_count[table.code_len[s]]++;

    // Find starting code for each bit length
    uint32_t next_code[16] = {};
    uint32_t code = 0;
    for (int bits = 1; bits <= 15; ++bits) {
        code = (code + bl_count[bits - 1]) << 1;
        next_code[bits] = code;
    }

    // Assign codes
    for (int s = 0; s < HUFF_SYMBOLS; ++s) {
        int len = table.code_len[s];
        if (len > 0) {
            table.code[s] = next_code[len]++;
        }
    }
}

// -----------------------------------------------------------------------------
// Public: huffman_build
// -----------------------------------------------------------------------------

HuffTable huffman_build(const uint32_t freq[HUFF_SYMBOLS]) {
    HuffTable table;
    memset(table.code_len, 0, sizeof(table.code_len));
    memset(table.code,     0, sizeof(table.code));

    // Count active symbols
    int active = 0;
    for (int s = 0; s < HUFF_SYMBOLS; ++s)
        if (freq[s] > 0) ++active;

    if (active == 0) return table;

    // Edge case: only one unique symbol — give it length 1
    if (active == 1) {
        for (int s = 0; s < HUFF_SYMBOLS; ++s) {
            if (freq[s] > 0) {
                table.code_len[s] = 1;
                table.code[s]     = 0;
                return table;
            }
        }
    }

    // Build node pool
    std::vector<HNode> nodes;
    nodes.reserve(HUFF_SYMBOLS * 2);
    for (int s = 0; s < HUFF_SYMBOLS; ++s) {
        if (freq[s] > 0)
            nodes.push_back({freq[s], s, -1, -1});
    }

    // Min-heap: (freq, node_index)
    auto cmp = [](std::pair<uint32_t,int> a, std::pair<uint32_t,int> b) {
        return a.first > b.first;  // min-heap
    };
    std::priority_queue<std::pair<uint32_t,int>,
                        std::vector<std::pair<uint32_t,int>>,
                        decltype(cmp)> pq(cmp);

    for (int i = 0; i < (int)nodes.size(); ++i)
        pq.push({nodes[i].freq, i});

    // Merge until one node remains (the root)
    while (pq.size() > 1) {
        auto [f1, i1] = pq.top(); pq.pop();
        auto [f2, i2] = pq.top(); pq.pop();
        int merged_idx = (int)nodes.size();
        nodes.push_back({f1 + f2, -1, i1, i2});
        pq.push({f1 + f2, merged_idx});
    }

    int root = pq.top().second;

    // Walk tree to assign lengths
    uint8_t lengths[HUFF_SYMBOLS] = {};
    assign_lengths(nodes, root, 0, lengths);

    // Limit to 15 bits so canonical codes fit in uint32_t without overflow
    limit_lengths(lengths, 15);

    memcpy(table.code_len, lengths, sizeof(lengths));

    // Build canonical codes from lengths
    build_canonical_codes(table);

    return table;
}

// -----------------------------------------------------------------------------
// Public: huffman_encode
// -----------------------------------------------------------------------------

void huffman_encode(const uint8_t* data, size_t len,
                    const HuffTable& table, BitWriter& bw) {
    for (size_t i = 0; i < len; ++i) {
        uint8_t sym = data[i];
        int     clen = table.code_len[sym];
        if (clen == 0)
            throw std::runtime_error("huffman_encode: symbol not in table");
        bw.write_bits(table.code[sym], clen);
    }
}

// -----------------------------------------------------------------------------
// Public: huffman_decode
// -----------------------------------------------------------------------------
// We use a simple linear scan of the canonical codes.
// For production this would be a lookup table — fine for now.

std::vector<uint8_t> huffman_decode(BitReader& br, const HuffTable& table,
                                    size_t num_symbols) {
    std::vector<uint8_t> out;
    out.reserve(num_symbols);

    for (size_t i = 0; i < num_symbols; ++i) {
        uint32_t code = 0;
        int      len  = 0;
        bool     found = false;

        while (len < 16) {
            code = (code << 1) | (uint32_t)br.read_bit();
            ++len;
            // Check if this matches any symbol with this length
            for (int s = 0; s < HUFF_SYMBOLS; ++s) {
                if (table.code_len[s] == len && table.code[s] == code) {
                    out.push_back((uint8_t)s);
                    found = true;
                    break;
                }
            }
            if (found) break;
        }

        if (!found)
            throw std::runtime_error("huffman_decode: invalid code in bitstream");
    }
    return out;
}

// -----------------------------------------------------------------------------
// Public: serialize / deserialize code lengths
// -----------------------------------------------------------------------------

void huffman_write_lengths(const HuffTable& table, std::vector<uint8_t>& out) {
    for (int s = 0; s < HUFF_SYMBOLS; ++s)
        out.push_back(table.code_len[s]);
}

HuffTable huffman_read_lengths(const uint8_t lengths[HUFF_SYMBOLS]) {
    HuffTable table;
    memcpy(table.code_len, lengths, HUFF_SYMBOLS);
    memset(table.code, 0, sizeof(table.code));
    build_canonical_codes(table);
    return table;
}
