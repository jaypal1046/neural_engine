#pragma once
#include <cstdint>
#include <vector>
#include <array>
#include "bit_io.h"

// -----------------------------------------------------------------------------
// Huffman encoder / decoder
// -----------------------------------------------------------------------------
// Canonical Huffman — same approach as gzip/deflate:
//   1. Count symbol frequencies
//   2. Build tree by repeatedly merging lowest-freq nodes
//   3. Assign code lengths (depth in tree)
//   4. Build canonical codes from lengths (sorted, deterministic)
//   5. Store only the code lengths in the file (not the codes themselves)
//
// This means the decompressor only needs the lengths to reconstruct
// the exact same code table — compact and unambiguous.
// -----------------------------------------------------------------------------

static const int HUFF_SYMBOLS = 256;

struct HuffTable {
    // code_len[sym]  = bit length of Huffman code for symbol sym (0 = unused)
    // code[sym]      = the actual bit pattern (canonical)
    uint8_t  code_len[HUFF_SYMBOLS] = {};
    uint32_t code[HUFF_SYMBOLS]     = {};
};

// Build a Huffman table from frequency counts.
// freq[sym] = how many times symbol sym appears in the data.
// Returns a HuffTable with code_len and code filled in.
HuffTable huffman_build(const uint32_t freq[HUFF_SYMBOLS]);

// Encode `data` into `bw` using the given table.
// Caller must have already called huffman_build.
void huffman_encode(const uint8_t* data, size_t len,
                    const HuffTable& table, BitWriter& bw);

// Decode exactly `num_symbols` symbols from `br` using the given table.
// Returns decoded bytes.
std::vector<uint8_t> huffman_decode(BitReader& br, const HuffTable& table,
                                    size_t num_symbols);

// Serialize code_len array to bytes (one byte per symbol, 256 bytes total).
// This is what we store in the file.
void huffman_write_lengths(const HuffTable& table, std::vector<uint8_t>& out);

// Reconstruct a HuffTable from stored code lengths.
HuffTable huffman_read_lengths(const uint8_t lengths[HUFF_SYMBOLS]);
