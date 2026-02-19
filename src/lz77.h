#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <functional>

// -----------------------------------------------------------------------------
// LZ77 sliding-window compressor
// -----------------------------------------------------------------------------
// Tokens:
//   Literal  : flag=0x00  byte                 (2 bytes serialized)
//   Match    : flag=0x01  dist_hi dist_lo len  (4 bytes serialized)
//              dist = uint16 big-endian (1..32768)
//              len  = uint8, stored as (length - MIN_MATCH), so 0..255 = 3..258
//
// Hash table:  3-byte keys -> list of positions
//              O(1) candidate lookup per position
// -----------------------------------------------------------------------------

static constexpr int LZ_WINDOW   = 32768;  // 32 KB sliding window
static constexpr int LZ_MAX_MATCH = 258;
static constexpr int LZ_MIN_MATCH = 3;

struct LZToken {
    bool     is_match;
    uint8_t  byte;      // used when !is_match
    uint16_t dist;      // used when is_match
    uint8_t  len;       // stored length (actual = len + LZ_MIN_MATCH)
};

// Encode data into a token vector.
// progress_cb(done_bytes, total_bytes) called every 64KB if not nullptr.
// use_lazy: enable lazy matching (+ratio on text, -ratio on genomic/random data)
std::vector<LZToken> lz77_encode(
    const uint8_t* data, size_t len,
    std::function<void(size_t, size_t)> progress_cb = nullptr,
    bool use_lazy = true);

// Decode token vector back to original bytes.
std::vector<uint8_t> lz77_decode(const LZToken* tokens, size_t count);

// Serialize tokens to a flat byte stream (for Huffman input).
std::vector<uint8_t> lz77_serialize(const LZToken* tokens, size_t count);

// Deserialize flat byte stream back to tokens.
std::vector<LZToken> lz77_deserialize(const uint8_t* data, size_t len);
