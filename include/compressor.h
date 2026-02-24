#pragma once
#include <cstdint>
#include <cstddef>
#include <string>
#include <functional>
#include <vector>

// -----------------------------------------------------------------------------
// .aiz file format  (C++ multi-version, streaming block-based)
// -----------------------------------------------------------------------------
//
// HEADER (54 bytes, fixed):
//   0        4    Magic:    'MZIP'
//   4        1    Version:  7, 8, or 9  (auto-selected by mode)
//   5        1    Mode:     0x00=COMPRESSED  0x01=STORED
//   6        8    Original file size (uint64 LE)
//   14       32   SHA-256 of ENTIRE original file
//   46       4    Block count (uint32 LE)
//   50       4    Block size  (uint32 LE) — nominal uncompressed bytes per block
// = 54 bytes
//
// BLOCK INDEX (block_count * 8 bytes):
//   For each block: compressed_size (uint32 LE) + original_size (uint32 LE)
//
// v7 block layout (LZ77 + delta pre-filter + rANS order-0):
//   1 byte   Pre-filter type (FILTER_* constants below)
//   512 B    rANS freq table (order-0)
//   4 B      Symbol count
//   4 B      Encoded byte count
//   N B      rANS encoded data
//
// v8 BWT block types (first byte of each block):
//   0x00  BWT+MTF+RLE+rANS order-0
//   0x01  STORED_RAW  (4B raw_size + raw bytes)
//   0x02  BWT+MTF+RLE+rANS order-1
//   0x03  BWT+MTF+RLE+rANS order-2
//
// v9 PPM block types (first byte):
//   0x00  PPM_CODED  (4B sym_count + range-coded stream)
//   0x01  STORED_RAW (4B raw_size + raw bytes)
//
// v10 CMIX block types (first byte):
//   0x00  CMIX_CODED (4B sym_count + bit-range-coded stream)
//   0x01  STORED_RAW (4B raw_size + raw bytes)
//
// STORED global mode (header mode=0x01): raw bytes follow the 54-byte header.
// SHA-256 is present and verified in all modes.
// Backwards compat: v5 (Huffman) and v6 (rANS order-0) still decompress.
// -----------------------------------------------------------------------------

// Pre-filter types
static constexpr uint8_t FILTER_NONE    = 0;  // no pre-filter
static constexpr uint8_t FILTER_DELTA   = 1;  // byte delta stride 1
static constexpr uint8_t FILTER_DELTA2  = 2;  // byte delta stride 2 (stereo 8-bit)
static constexpr uint8_t FILTER_DELTA3  = 3;  // byte delta stride 3 (RGB)
static constexpr uint8_t FILTER_DELTA4  = 4;  // byte delta stride 4 (RGBA/16-bit stereo)
static constexpr uint8_t FILTER_DELTA16 = 5;  // 16-bit LE sample delta (mono PCM)

// Block sizes
static constexpr uint32_t BLOCK_SIZE = 512 * 1024; // 512 KB — v7 / DEFAULT
static constexpr uint32_t BWT_BLOCK_SIZE = 16 * 1024 * 1024; // 16 MB — v8 BEST
static constexpr uint32_t PPM_BLOCK_SIZE = 512 * 1024; // 512 KB — v9 ULTRA
static constexpr uint32_t CMIX_BLOCK_SIZE = 512 * 1024; // 512 KB — v10 CMIX

// Progress callback: (stage, bytes_done, bytes_total, active_models)
using ProgressCb = std::function<void(const char*, size_t, size_t, int)>;

// -----------------------------------------------------------------------------
// Compression mode — single flag replaces the old (best_mode, ultra_mode) pair.
// -----------------------------------------------------------------------------
//
//  DEFAULT  — LZ77 + delta pre-filter + rANS (v7). Per-block compare-and-pick
//             already includes STORED_RAW as a fallback. Fast; good on binary.
//
//  BEST     — Forces the BWT + MTF + RLE + rANS pipeline (v8). Slower but
//             consistently better on compressible text/code.
//
//  ULTRA    — Forces PPM order-1..4 + arithmetic (range) coding (v9).
//             Very slow. Best ratio on large, structured text.
//
// Use DEFAULT when you don't know the data type; use BEST for known text;
// use ULTRA only when ratio matters more than speed.
// -----------------------------------------------------------------------------
enum class CompressMode : int {
  DEFAULT = 0, // LZ77+delta+rANS, compare-and-pick (.aiz v7)
  BEST = 1,    // BWT+MTF+RLE+rANS                  (.aiz v8)
  ULTRA = 2,   // PPM order-4 + arithmetic coding    (.aiz v9)
  CMIX = 3,    // Neural Net Context Mixing          (.aiz v10)
};

// Compress input_path -> output_path.
// Returns 0 on success, non-zero on error.
int compress_file(const std::string& input_path,
                  const std::string& output_path,
                  ProgressCb progress = nullptr,
                  CompressMode mode = CompressMode::DEFAULT);

// Decompress input_path -> output_path.
// Auto-detects version (v5/v6/v7/v8/v9). No mode flag needed.
// Returns 0 on success, non-zero on error.
int decompress_file(const std::string& input_path,
                    const std::string& output_path,
                    ProgressCb progress = nullptr);

// -----------------------------------------------------------------------------
// Block-Based Random Access API (for knowledge modules)
// -----------------------------------------------------------------------------

// Block file info (read from compressed file header)
struct BlockFileInfo {
    uint8_t  version;        // Format version (5-10)
    uint8_t  mode;           // 0x00=compressed, 0x01=stored
    uint64_t orig_size;      // Original file size
    uint32_t block_count;    // Number of blocks
    uint32_t block_size;     // Nominal block size (uncompressed)
    uint8_t  sha256[32];     // SHA-256 checksum
};

// Block metadata (from block index)
struct BlockInfo {
    uint32_t compressed_size;   // Compressed block size in bytes
    uint32_t original_size;     // Original block size in bytes
    uint64_t file_offset;       // Byte offset in file where block data starts
};

// Open compressed file for random access.
// Returns opaque handle (FILE*) or nullptr on failure.
// Caller must call block_close() when done.
void* block_open(const std::string& file_path, BlockFileInfo& info);

// Get information about a specific block.
// Returns true on success, false on error.
bool block_get_info(void* handle, uint32_t block_index, BlockInfo& info);

// Decompress a specific block to memory.
// Returns decompressed bytes, or empty vector on error.
std::vector<uint8_t> block_decompress(void* handle, uint32_t block_index);

// Close block file handle.
void block_close(void* handle);
