#pragma once
// =============================================================================
// PPM (Prediction by Partial Match) — order 1..PPMORDER context model
// with arithmetic coding (16-bit range coder).
//
// Architecture
// ------------
// Context table: std::unordered_map<uint64_t, ContextEntry>
//   key = last N bytes packed into uint64 (N = 1..PPMORDER)
//   ContextEntry = freq[256] counts + total
//
// Blending: "Escape" mechanism (simple PPM-C style).
//   For each byte, try highest order first. If context seen and has a
//   prediction, emit the symbol. If not, escape to lower order.
//   Order-0 always has all 256 symbols (initialized to 1 each).
//
// Encoding: range coder (arithmetic coding) — exact fractional bits,
//   better than Huffman and avoids rANS table overhead for adaptive models.
//
// Block format (ULTRA / v9):
//   1 byte : block_type = 0x00 (PPM_CODED)
//   4 bytes: sym_count
//   N bytes: range-coded stream (self-delimiting by sym_count)
//
// Memory usage: bounded by MAX_ENTRIES hash table entries.
//   Each entry: 256*2 + 4 = 516 bytes.
//   MAX_ENTRIES = 4M → ~2GB; use 256K → ~128MB for safety.
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <vector>

static constexpr int PPM_ORDER = 4;        // maximum context order
static constexpr int PPM_MAX_ENTRIES = (1 << 18);  // 256K context entries (~128 MB)

// Compress data using PPM + arithmetic coding. Returns coded stream.
std::vector<uint8_t> ppm_encode(const uint8_t* data, size_t len);

// Decompress sym_count bytes from coded stream.
std::vector<uint8_t> ppm_decode(const uint8_t* coded, size_t coded_len, size_t sym_count);
