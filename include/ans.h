#pragma once
#include <cstdint>
#include <cstddef>
#include <vector>
#include <memory>

// -----------------------------------------------------------------------------
// rANS (range Asymmetric Numeral Systems) entropy coder
// -----------------------------------------------------------------------------
// Replaces Huffman coding for better compression ratio.
// Advantage: fractional bits per symbol — approaches true entropy,
// vs Huffman which must use ≥1 bit/symbol even for 99% probability symbols.
//
// State range: [ANS_M, ANS_M * 256) fits in uint32_t.
//
// Encode one symbol s with freq[s] slots out of ANS_M total:
//   1. Renormalize: while x >= freq[s]*256, emit low byte, x >>= 8
//   2. x = (x / freq[s]) * ANS_M + cumul[s] + (x % freq[s])
//
// Decode one symbol:
//   1. slot = x % ANS_M → look up sym, freq[sym], cumul[sym]
//   2. x = freq[sym] * (x / ANS_M) + slot - cumul[sym]
//   3. Renormalize: while x < ANS_M, x = (x << 8) | read_byte()
//
// The encoder processes symbols in REVERSE ORDER and emits bytes backwards.
// finish() prepends the final state and reverses the byte buffer,
// producing a forward-readable stream for the decoder.
// -----------------------------------------------------------------------------

static constexpr int      ANS_LOG_M = 14;
static constexpr uint32_t ANS_M     = 1u << ANS_LOG_M;  // 16384

// Normalized frequency table. freq[] sums to ANS_M. cumul[s] = sum(freq[0..s-1]).
// Decode table: sym[slot] and bias[slot] = slot - cumul[sym].
struct AnsTable {
    uint16_t freq[256];
    uint16_t cumul[257];
    uint8_t  sym[ANS_M];
    uint16_t bias[ANS_M];
};

// Build table from raw (unnormalized) frequency counts.
bool ans_build_table(const uint32_t raw_freq[256], AnsTable& table);

// Serialize/deserialize normalized freq[] (512 bytes: 256 x uint16 LE).
void ans_write_freqs(const AnsTable& table, std::vector<uint8_t>& out);
AnsTable ans_read_freqs(const uint8_t freqs[512]);

// Encoder: accumulates encoded bytes, then finish() reverses them.
struct AnsEncoder {
    uint32_t             state = ANS_M;  // start at lower bound
    std::vector<uint8_t> buf;            // bytes emitted during encoding (backwards)

    void put(uint8_t sym, const AnsTable& table);

    // Finalize: prepend 4-byte state, reverse buf, return forward-decode stream.
    std::vector<uint8_t> finish();
};

// Decoder: reads forward stream produced by AnsEncoder::finish().
struct AnsDecoder {
    uint32_t       state = 0;
    const uint8_t* src   = nullptr;
    size_t         pos   = 0;
    size_t         len   = 0;

    void    init(const uint8_t* data, size_t size);
    uint8_t get(const AnsTable& table);
};

// -----------------------------------------------------------------------------
// Order-1 context model: 256 separate ANS tables, one per previous-byte context.
// Encoding byte b after context c uses ctx_tables[c].
// Memory: 256 * sizeof(AnsTable) ≈ 256 * (512+514+32768+65536) bytes ≈ 25 MB.
// Serialized: 256 * 512 = 131072 bytes of freq tables per block.
// -----------------------------------------------------------------------------
struct AnsCtx1 {
    AnsTable tables[256];
};

// Build all 256 context tables from pair frequencies.
// pair_freq[c][s] = number of times byte s followed byte c in the data.
void ans_ctx1_build(const uint32_t pair_freq[256][256], AnsCtx1& ctx);

// Serialize: write 256*512 = 131072 bytes of freq tables.
void ans_ctx1_write(const AnsCtx1& ctx, std::vector<uint8_t>& out);

// Deserialize from 131072-byte buffer. Returns heap-allocated ctx (12 MB).
std::unique_ptr<AnsCtx1> ans_ctx1_read(const uint8_t* data);

// Encode tok_bytes using order-1 model (backwards, as usual for rANS).
// Returns the encoded byte stream (state prepended, bytes forward-ordered).
std::vector<uint8_t> ans_ctx1_encode(const uint8_t* data, size_t len, const AnsCtx1& ctx);

// Decode sym_count bytes from encoded stream using order-1 model.
std::vector<uint8_t> ans_ctx1_decode(const uint8_t* encoded, size_t enc_len,
                                      size_t sym_count, const AnsCtx1& ctx);

// -----------------------------------------------------------------------------
// Order-2 context model: folds 2-byte context (prev2, prev1) into one byte.
// Key = (prev2 * 5 + prev1) & 0xFF — reuses AnsCtx1 (same 256 tables, 131KB).
// Wire format is identical to order-1. Only the context key derivation differs.
// -----------------------------------------------------------------------------

// Build order-2 tables from data stream.
void ans_ctx2_build(const uint8_t* data, size_t len, AnsCtx1& ctx);

// Encode using order-2 model.
std::vector<uint8_t> ans_ctx2_encode(const uint8_t* data, size_t len, const AnsCtx1& ctx);

// Decode sym_count bytes using order-2 model.
std::vector<uint8_t> ans_ctx2_decode(const uint8_t* encoded, size_t enc_len,
                                      size_t sym_count, const AnsCtx1& ctx);
