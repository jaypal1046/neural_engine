#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>

// -----------------------------------------------------------------------------
// BWT (Burrows-Wheeler Transform) pipeline for high-compression mode.
//
// Pipeline: raw block -> BWT -> MTF -> RLE(zeros) -> rANS
//
// BWT rearranges data so bytes with the same preceding context cluster together.
// MTF converts those clusters into runs of zeros (rank 0 = recently seen symbol).
// RLE compacts zero runs into (marker, count) pairs.
// rANS entropy-codes the resulting skewed symbol stream.
//
// Decompression: rANS -> RLE_decode -> MTF_decode -> BWT_decode -> original
// -----------------------------------------------------------------------------

// BWT forward transform.
// Builds suffix array (prefix-doubling, O(N log^2 N)), reads last column.
// Returns the primary index (position of the original string in sorted rotations).
// out[] must be pre-allocated to N bytes.
uint32_t bwt_encode(const uint8_t* data, size_t N, uint8_t* out);

// BWT inverse transform (L→F mapping, O(N) time and space).
// Returns the original N bytes.
std::vector<uint8_t> bwt_decode(const uint8_t* bwt, size_t N, uint32_t primary_index);

// MTF (Move-To-Front) encode.
// For each byte: output its rank in the current alphabet list [0..255],
// then move that byte to the front. Runs of same symbol → runs of 0s.
std::vector<uint8_t> mtf_encode(const uint8_t* data, size_t N);

// MTF decode (inverse of mtf_encode).
std::vector<uint8_t> mtf_decode(const uint8_t* ranks, size_t N);

// RLE encode for zero runs.
// Each run of k zeros (k >= 1) becomes: {0x00, k-1} (two bytes, k-1 in [0..255]).
// Non-zero bytes pass through unchanged.
// For runs > 256 bytes, multiple pairs are emitted.
std::vector<uint8_t> rle_zeros_encode(const uint8_t* data, size_t N);

// RLE decode (inverse of rle_zeros_encode).
// When 0x00 is seen, the next byte n means output n+1 zeros.
std::vector<uint8_t> rle_zeros_decode(const uint8_t* rle, size_t rle_len);

// -----------------------------------------------------------------------------
// SIMD-Optimized versions (Phase 16)
// Automatically use SSE2/AVX2 when available, fallback to scalar
// -----------------------------------------------------------------------------

// SIMD-optimized MTF encode (2-3x faster with SSE2)
std::vector<uint8_t> mtf_encode_optimized(const uint8_t* data, size_t N);

// SIMD-optimized RLE encode (2-4x faster on zero-heavy data)
std::vector<uint8_t> rle_zeros_encode_optimized(const uint8_t* data, size_t N);

// Fast memory copy using SIMD (SSE2/AVX2)
void fast_memcpy(void* dest, const void* src, size_t n);
