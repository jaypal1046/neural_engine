#include "compressor.h"
#include "lz77.h"
#include "huffman.h"
#include "bit_io.h"
#include "ans.h"
#include "bwt.h"
#include "ppm.h"
#include "cmix.h"

#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <sys/mman.h>
#  include <sys/stat.h>
#  include <fcntl.h>
#  include <unistd.h>
#endif

#include <fstream>
#include <vector>
#include <array>
#include <cstring>
#include <cmath>
#include <cstdio>
#include <stdexcept>
#include <algorithm>

// SHA-256 — self-contained implementation, no external deps
// -------------------------------------------------------
static const uint32_t K256[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,
    0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,
    0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,
    0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,
    0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,
    0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,
    0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,
    0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,
    0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};

static inline uint32_t rotr32(uint32_t x, int n) { return (x >> n) | (x << (32-n)); }

static void sha256_process(uint32_t state[8], const uint8_t block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; ++i)
        w[i] = ((uint32_t)block[i*4]<<24)|((uint32_t)block[i*4+1]<<16)|
               ((uint32_t)block[i*4+2]<<8)|block[i*4+3];
    for (int i = 16; i < 64; ++i) {
        uint32_t s0 = rotr32(w[i-15],7) ^ rotr32(w[i-15],18) ^ (w[i-15]>>3);
        uint32_t s1 = rotr32(w[i-2],17) ^ rotr32(w[i-2],19)  ^ (w[i-2]>>10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3],
             e=state[4],f=state[5],g=state[6],h=state[7];
    for (int i = 0; i < 64; ++i) {
        uint32_t S1  = rotr32(e,6)^rotr32(e,11)^rotr32(e,25);
        uint32_t ch  = (e&f)^((~e)&g);
        uint32_t t1  = h+S1+ch+K256[i]+w[i];
        uint32_t S0  = rotr32(a,2)^rotr32(a,13)^rotr32(a,22);
        uint32_t maj = (a&b)^(a&c)^(b&c);
        uint32_t t2  = S0+maj;
        h=g; g=f; f=e; e=d+t1; d=c; c=b; b=a; a=t1+t2;
    }
    state[0]+=a; state[1]+=b; state[2]+=c; state[3]+=d;
    state[4]+=e; state[5]+=f; state[6]+=g; state[7]+=h;
}


// -------------------------------------------------------
// Streaming SHA-256 context — hash data in chunks, no
// need to buffer the entire file in RAM.
// -------------------------------------------------------
struct Sha256Ctx {
    uint32_t state[8] = {
        0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,
        0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19
    };
    uint8_t  buf[64] = {};
    size_t   buf_len = 0;
    uint64_t total   = 0;

    void update(const uint8_t* data, size_t len) {
        total += len;
        while (len > 0) {
            size_t copy = std::min(len, (size_t)(64 - buf_len));
            memcpy(buf + buf_len, data, copy);
            buf_len += copy; data += copy; len -= copy;
            if (buf_len == 64) { sha256_process(state, buf); buf_len = 0; }
        }
    }

    std::array<uint8_t,32> finish() {
        uint8_t pad[64] = {};
        size_t rem = buf_len;
        memcpy(pad, buf, rem);
        pad[rem] = 0x80;
        if (rem < 56) {
            memset(pad + rem + 1, 0, 55 - rem);
        } else {
            memset(pad + rem + 1, 0, 63 - rem);
            sha256_process(state, pad);
            memset(pad, 0, 56);
        }
        uint64_t bit_len = total * 8;
        for (int j = 0; j < 8; ++j)
            pad[56+j] = (uint8_t)(bit_len >> (56 - j*8));
        sha256_process(state, pad);
        std::array<uint8_t,32> out;
        for (int j = 0; j < 8; ++j) {
            out[j*4+0] = (state[j]>>24)&0xFF; out[j*4+1] = (state[j]>>16)&0xFF;
            out[j*4+2] = (state[j]>> 8)&0xFF; out[j*4+3] =  state[j]     &0xFF;
        }
        return out;
    }
};

// -------------------------------------------------------
// Cross-platform read-only memory-mapped file.
// map_file()   — maps the file; caller holds MappedFile.
// unmap_file() — unmaps and closes.
// -------------------------------------------------------
struct MappedFile {
    const uint8_t* data = nullptr;
    size_t         size = 0;
#ifdef _WIN32
    HANDLE hFile    = INVALID_HANDLE_VALUE;
    HANDLE hMapping = nullptr;
#else
    int    fd       = -1;
#endif
};

static bool map_file(const std::string& path, MappedFile& mf) {
#ifdef _WIN32
    mf.hFile = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ,
                           nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (mf.hFile == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER sz;
    if (!GetFileSizeEx(mf.hFile, &sz)) { CloseHandle(mf.hFile); return false; }
    mf.size = (size_t)sz.QuadPart;
    if (mf.size == 0) { mf.data = nullptr; return true; }
    mf.hMapping = CreateFileMappingA(mf.hFile, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mf.hMapping) { CloseHandle(mf.hFile); return false; }
    LPVOID view = MapViewOfFile(mf.hMapping, FILE_MAP_READ, 0, 0, 0);
    if (!view) { CloseHandle(mf.hMapping); CloseHandle(mf.hFile); return false; }
    mf.data = static_cast<const uint8_t*>(view);
    return true;
#else
    mf.fd = open(path.c_str(), O_RDONLY);
    if (mf.fd < 0) return false;
    struct stat st;
    if (fstat(mf.fd, &st) != 0) { close(mf.fd); return false; }
    mf.size = (size_t)st.st_size;
    if (mf.size == 0) { mf.data = nullptr; return true; }
    void* view = mmap(nullptr, mf.size, PROT_READ, MAP_PRIVATE, mf.fd, 0);
    if (view == MAP_FAILED) { close(mf.fd); return false; }
    mf.data = static_cast<const uint8_t*>(view);
    return true;
#endif
}

static void unmap_file(MappedFile& mf) {
    if (!mf.data) { mf.data = nullptr; return; }
#ifdef _WIN32
    UnmapViewOfFile(mf.data);
    if (mf.hMapping) CloseHandle(mf.hMapping);
    if (mf.hFile != INVALID_HANDLE_VALUE) CloseHandle(mf.hFile);
#else
    munmap(const_cast<uint8_t*>(mf.data), mf.size);
    if (mf.fd >= 0) close(mf.fd);
#endif
    mf.data = nullptr;
}

// -------------------------------------------------------
// Little-endian helpers
// -------------------------------------------------------
static void write_u32le(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back(x & 0xFF);
    v.push_back((x >> 8)  & 0xFF);
    v.push_back((x >> 16) & 0xFF);
    v.push_back((x >> 24) & 0xFF);
}
static uint32_t read_u32le(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

// -------------------------------------------------------
// Compress one block: data -> packed bytes
// Returns the compressed bytes (Huffman lengths + bit count + bits)
// -------------------------------------------------------

// -------------------------------------------------------
// Pre-filter: delta encoding / decoding
// -------------------------------------------------------

// delta_encode: data[i] = data[i] - data[i-stride]  (mod 256)
// Reduces entropy for audio, sensor data, and binary sequences where
// adjacent values change smoothly (adjacent differences cluster near 0).
static std::vector<uint8_t> delta_encode(const uint8_t* data, size_t len, int stride) {
    std::vector<uint8_t> out(len);
    for (size_t i = 0; i < (size_t)stride && i < len; ++i)
        out[i] = data[i];  // first stride bytes unchanged
    for (size_t i = stride; i < len; ++i)
        out[i] = (uint8_t)(data[i] - data[i - stride]);
    return out;
}

// delta_decode: inverse of delta_encode
static std::vector<uint8_t> delta_decode(const uint8_t* data, size_t len, int stride) {
    std::vector<uint8_t> out(len);
    for (size_t i = 0; i < (size_t)stride && i < len; ++i)
        out[i] = data[i];
    for (size_t i = stride; i < len; ++i)
        out[i] = (uint8_t)(data[i] + out[i - stride]);
    return out;
}

// delta16_encode: treat data as 16-bit LE samples.
// Compute difference between adjacent samples: residual = sample[i] - sample[i-1].
// Store residuals as LE int16 (wrapping). First sample stored as-is.
// This matches what FLAC/DPCM do for 16-bit PCM audio.
static std::vector<uint8_t> delta16_encode(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out(len);
    // Odd-length: copy the last byte unchanged (can't form a full sample pair)
    if (len & 1) out[len - 1] = data[len - 1];
    size_t n_samples = len / 2;
    int16_t prev = 0;
    for (size_t i = 0; i < n_samples; ++i) {
        int16_t samp;
        memcpy(&samp, data + i * 2, 2);         // read LE int16
        int16_t delta = (int16_t)(samp - prev);
        memcpy(out.data() + i * 2, &delta, 2);  // write LE int16
        prev = samp;
    }
    return out;
}

// delta16_decode: inverse of delta16_encode.
static std::vector<uint8_t> delta16_decode(const uint8_t* data, size_t len) {
    std::vector<uint8_t> out(len);
    if (len & 1) out[len - 1] = data[len - 1];
    size_t n_samples = len / 2;
    int16_t prev = 0;
    for (size_t i = 0; i < n_samples; ++i) {
        int16_t delta;
        memcpy(&delta, data + i * 2, 2);
        int16_t samp = (int16_t)(prev + delta);
        memcpy(out.data() + i * 2, &samp, 2);
        prev = samp;
    }
    return out;
}

// -------------------------------------------------------
// Pre-filter selector: pick the best filter for this block.
// Returns FILTER_NONE, FILTER_DELTA, FILTER_DELTA2, or FILTER_DELTA4.
// Also reports whether lazy matching should be used.
// -------------------------------------------------------
struct BlockProfile {
    uint8_t filter;
    bool    use_lazy;
};

static BlockProfile profile_block(const uint8_t* data, size_t len) {
    size_t probe_n = std::min(len, (size_t)8192);

    // Count byte frequencies
    uint64_t freq[256] = {};
    for (size_t i = 0; i < probe_n; ++i) freq[data[i]]++;

    int unique = 0;
    double h = 0.0, n = (double)probe_n;
    for (int b = 0; b < 256; ++b) {
        if (freq[b] == 0) continue;
        ++unique;
        double p = freq[b] / n;
        h -= p * (std::log(p) / std::log(2.0));
    }

    // Use lazy matching for medium/high entropy text-like data.
    // Greedy for genomic (low alphabet, low entropy).
    bool use_lazy = !(unique < 16 && h < 3.5);

    // Very high entropy (>= 7.5): check for PCM-like 16-bit audio before giving up.
    // PCM audio has entropy ~7.5-7.9 (random-looking bytes) but adjacent 16-bit
    // samples are strongly correlated — their difference is small.
    if (h >= 7.5) {
        // Measure autocorrelation at 16-bit sample lag (2 bytes):
        // compute mean absolute difference between adjacent LE int16 samples.
        // If the average residual is small relative to the range, it's PCM.
        if (probe_n >= 4 && (probe_n & 1) == 0) {
            // Compare entropy of 16-bit delta residuals vs raw
            // Residual byte stream after delta16: treat output as flat bytes
            uint64_t df16[256] = {};
            size_t n16 = probe_n / 2;  // number of int16 samples
            int16_t prev16 = 0;
            for (size_t i = 0; i < n16; ++i) {
                int16_t samp;
                memcpy(&samp, data + i * 2, 2);
                int16_t delta = (int16_t)(samp - prev16);
                prev16 = samp;
                // Count both bytes of the 16-bit delta residual
                df16[(uint8_t)(delta & 0xFF)]++;
                df16[(uint8_t)((delta >> 8) & 0xFF)]++;
            }
            double dh16 = 0.0, dn16 = (double)(probe_n);
            for (int b = 0; b < 256; ++b) {
                if (df16[b] == 0) continue;
                double p = df16[b] / dn16;
                dh16 -= p * (std::log(p) / std::log(2.0));
            }
            // Use delta16 if it reduces entropy by at least 1.0 bit/byte vs raw
            if (dh16 < h - 1.0)
                return {FILTER_DELTA16, use_lazy};
        }
        return {FILTER_NONE, use_lazy};
    }

    // Very low entropy (< 2.0): LZ77 already handles this perfectly (large run-length
    // matches). Delta would only interfere with LZ77's match-finding.
    if (h < 2.0) return {FILTER_NONE, use_lazy};

    // For potential delta filter candidates, compare entropy of delta residuals
    // against raw entropy. Use the filter if it meaningfully reduces entropy.
    // Test strides 1-4: covers mono/stereo audio, RGB, RGBA pixel data.
    auto delta_entropy = [&](int stride) -> double {
        size_t n2 = std::min(probe_n, len - (size_t)stride);
        if (n2 == 0) return h;
        uint64_t df[256] = {};
        for (size_t i = (size_t)stride; i < n2 + (size_t)stride; ++i)
            df[(uint8_t)(data[i] - data[i - stride])]++;
        double dh = 0.0, dn = (double)n2;
        for (int b = 0; b < 256; ++b) {
            if (df[b] == 0) continue;
            double p = df[b] / dn;
            dh -= p * (std::log(p) / std::log(2.0));
        }
        return dh;
    };

    double h1 = delta_entropy(1);
    double h2 = delta_entropy(2);
    double h3 = delta_entropy(3);
    double h4 = delta_entropy(4);

    // Pick the filter that reduces entropy the most, but only if it
    // reduces by at least 0.5 bits/byte (otherwise overhead dominates).
    double best_h = h - 0.5;  // must beat this threshold
    uint8_t best_filter = FILTER_NONE;

    if (h1 < best_h) { best_h = h1; best_filter = FILTER_DELTA; }
    if (h2 < best_h) { best_h = h2; best_filter = FILTER_DELTA2; }
    if (h3 < best_h) { best_h = h3; best_filter = FILTER_DELTA3; }
    if (h4 < best_h) { best_h = h4; best_filter = FILTER_DELTA4; }

    (void)best_h;  // suppress unused warning
    return {best_filter, use_lazy};
}

// Compress a block of bytes with a given pre-filter type.
// Returns the assembled compressed block bytes.
static std::vector<uint8_t> compress_block_with_filter(
    const uint8_t* data, size_t len, uint8_t filter_type,
    bool use_lazy, ProgressCb& progress,
    size_t global_done, size_t global_total)
{
    // Apply pre-filter if selected
    std::vector<uint8_t> filtered;
    const uint8_t* src = data;
    size_t src_len = len;

    if (filter_type == FILTER_DELTA16) {
        filtered = delta16_encode(data, len);
        src     = filtered.data();
        src_len = filtered.size();
    } else if (filter_type != FILTER_NONE) {
        int stride = (filter_type == FILTER_DELTA)  ? 1 :
                     (filter_type == FILTER_DELTA2) ? 2 :
                     (filter_type == FILTER_DELTA3) ? 3 : 4;
        filtered = delta_encode(data, len, stride);
        src     = filtered.data();
        src_len = filtered.size();
    }

    // LZ77
    auto lz_cb = [&](size_t done, size_t total) {
        if (progress)
            progress("lz77", global_done + done, global_total, 0);
        (void)total;
    };
    auto tokens    = lz77_encode(src, src_len, lz_cb, use_lazy);
    auto tok_bytes = lz77_serialize(tokens.data(), tokens.size());

    // rANS order-0 entropy coding
    uint32_t raw_freq[256] = {};
    for (uint8_t b : tok_bytes) raw_freq[b]++;

    AnsTable ans_table;
    ans_build_table(raw_freq, ans_table);

    AnsEncoder enc;
    for (int i = (int)tok_bytes.size() - 1; i >= 0; --i)
        enc.put(tok_bytes[i], ans_table);
    auto ans_bytes = enc.finish();

    // Assemble block (v7 format):
    //   1 byte:  pre-filter type
    // 512 bytes: ANS freq table
    //   4 bytes: symbol count
    //   4 bytes: encoded byte count
    //   N bytes: ANS encoded data
    std::vector<uint8_t> out;
    out.reserve(1 + 512 + 8 + ans_bytes.size());
    out.push_back(filter_type);
    ans_write_freqs(ans_table, out);
    write_u32le(out, (uint32_t)tok_bytes.size());
    write_u32le(out, (uint32_t)ans_bytes.size());
    out.insert(out.end(), ans_bytes.begin(), ans_bytes.end());

    return out;
}

static std::vector<uint8_t> compress_block(const uint8_t* data, size_t len,
                                            ProgressCb& progress,
                                            size_t global_done, size_t global_total)
{
    // Profile block: choose candidate pre-filter and lazy matching strategy
    auto profile = profile_block(data, len);

    // Always produce a no-filter result (matches v6 baseline)
    auto result = compress_block_with_filter(data, len, FILTER_NONE,
                                              profile.use_lazy, progress,
                                              global_done, global_total);

    // If entropy heuristic suggests a filter, try it and keep the smaller result.
    // This avoids regressions when the heuristic is wrong (e.g. BMP where LZ77
    // already handles runs better than delta).
    if (profile.filter != FILTER_NONE) {
        auto alt = compress_block_with_filter(data, len, profile.filter,
                                               profile.use_lazy, progress,
                                               global_done, global_total);
        if (alt.size() < result.size())
            result = std::move(alt);
    }

    return result;
}

// -------------------------------------------------------
// BWT block compressor (v8 format)
// BWT + MTF + RLE(zeros) + rANS order-0
// Block layout:
//   1 byte:  block_type (0x00=BWT_COMPRESSED, 0x01=STORED_RAW)
//   if 0x00:
//     4 bytes: BWT primary index
//   512 bytes: ANS freq table
//     4 bytes: sym_count of RLE stream
//     4 bytes: encoded byte count
//     N bytes: rANS encoded data
//   if 0x01:
//     4 bytes: raw size
//     N bytes: raw bytes
// -------------------------------------------------------
static std::vector<uint8_t> compress_block_bwt(
    const uint8_t* data, size_t len,
    ProgressCb& progress, size_t global_done, size_t global_total)
{
    if (progress) progress("bwt", global_done, global_total, 0);

    // BWT
    std::vector<uint8_t> bwt_buf(len);
    uint32_t primary_index = bwt_encode(data, len, bwt_buf.data());

    // MTF
    auto mtf_buf = mtf_encode(bwt_buf.data(), len);

    // RLE zeros
    auto rle_buf = rle_zeros_encode(mtf_buf.data(), mtf_buf.size());

    const size_t sym_count = rle_buf.size();
    const size_t raw_size  = 1 + 4 + len;  // STORED_RAW overhead

    // ---- rANS order-0 candidate ----
    uint32_t raw_freq[256] = {};
    for (uint8_t b : rle_buf) raw_freq[b]++;
    AnsTable ans0_table;
    ans_build_table(raw_freq, ans0_table);
    AnsEncoder enc0;
    for (int i = (int)sym_count - 1; i >= 0; --i)
        enc0.put(rle_buf[i], ans0_table);
    auto ans0_bytes = enc0.finish();
    size_t size_o0 = 1 + 4 + 512 + 4 + 4 + ans0_bytes.size();

    // ---- rANS order-1 candidate ----
    // Build pair frequencies (heap-allocated: 256*256*4 = 256KB)
    auto pair_freq_flat = std::make_unique<uint32_t[]>(256 * 256);
    memset(pair_freq_flat.get(), 0, 256 * 256 * sizeof(uint32_t));
    {
        uint8_t prev = 0;
        for (size_t i = 0; i < sym_count; ++i) {
            pair_freq_flat[prev * 256 + rle_buf[i]]++;
            prev = rle_buf[i];
        }
    }
    // Reinterpret as [256][256] for ans_ctx1_build
    const uint32_t (*pair_freq)[256] =
        reinterpret_cast<const uint32_t(*)[256]>(pair_freq_flat.get());
    // AnsCtx1 is ~12.5MB — must be heap-allocated to avoid stack overflow
    auto ctx1 = std::make_unique<AnsCtx1>();
    ans_ctx1_build(pair_freq, *ctx1);
    auto ans1_bytes = ans_ctx1_encode(rle_buf.data(), sym_count, *ctx1);
    size_t size_o1 = 1 + 4 + 131072 + 4 + 4 + ans1_bytes.size();

    // ---- rANS order-2 candidate (folded 2-byte context) ----
    // Only build when block is large enough to amortize the 131KB header.
    // Threshold: blocks >= 3MB so header overhead stays < ~4% of block size.
    std::vector<uint8_t> ans2_bytes;
    size_t size_o2 = SIZE_MAX;  // sentinel: not tried
    if (len >= 3 * 1024 * 1024) {
        auto ctx2 = std::make_unique<AnsCtx1>();
        ans_ctx2_build(rle_buf.data(), sym_count, *ctx2);
        ans2_bytes = ans_ctx2_encode(rle_buf.data(), sym_count, *ctx2);
        size_o2 = 1 + 4 + 131072 + 4 + 4 + ans2_bytes.size();

        // If order-2 wins, replace ctx1 with ctx2 data for writing
        if (size_o2 < size_o1) {
            ctx1 = std::move(ctx2);  // ctx1 now holds ctx2 tables for serialization
        }
    }

    // Pick smallest: raw, order-0, order-1, order-2
    size_t best_size = raw_size;
    int best_type = -1;  // -1 = STORED_RAW
    if (size_o0 < best_size) { best_size = size_o0; best_type = 0; }
    if (size_o1 < best_size) { best_size = size_o1; best_type = 1; }
    if (size_o2 < best_size) { best_size = size_o2; best_type = 2; }

    std::vector<uint8_t> out;
    if (best_type == -1) {
        // STORED_RAW (0x01)
        out.reserve(raw_size);
        out.push_back(0x01);
        write_u32le(out, (uint32_t)len);
        out.insert(out.end(), data, data + len);
    } else if (best_type == 0) {
        // BWT + rANS order-0 (0x00)
        out.reserve(size_o0);
        out.push_back(0x00);
        write_u32le(out, primary_index);
        ans_write_freqs(ans0_table, out);              // 512 bytes
        write_u32le(out, (uint32_t)sym_count);
        write_u32le(out, (uint32_t)ans0_bytes.size());
        out.insert(out.end(), ans0_bytes.begin(), ans0_bytes.end());
    } else if (best_type == 1) {
        // BWT + rANS order-1 (0x02)
        out.reserve(size_o1);
        out.push_back(0x02);
        write_u32le(out, primary_index);
        ans_ctx1_write(*ctx1, out);                    // 131072 bytes (ctx1 = order-1 tables)
        write_u32le(out, (uint32_t)sym_count);
        write_u32le(out, (uint32_t)ans1_bytes.size());
        out.insert(out.end(), ans1_bytes.begin(), ans1_bytes.end());
    } else {
        // BWT + rANS order-2 (0x03)
        out.reserve(size_o2);
        out.push_back(0x03);
        write_u32le(out, primary_index);
        ans_ctx1_write(*ctx1, out);                    // 131072 bytes (ctx1 = order-2 tables)
        write_u32le(out, (uint32_t)sym_count);
        write_u32le(out, (uint32_t)ans2_bytes.size());
        out.insert(out.end(), ans2_bytes.begin(), ans2_bytes.end());
    }
    return out;
}

// -------------------------------------------------------
// PPM block compressor (v9 format / --ultra mode)
// Block layout:
//   1 byte : block_type  (0x00=PPM_CODED, 0x01=STORED_RAW)
//   if 0x00:
//     4 bytes: sym_count
//     N bytes: range-coded stream
//   if 0x01:
//     4 bytes: raw size
//     N bytes: raw bytes
// -------------------------------------------------------
static std::vector<uint8_t> compress_block_ppm(
    const uint8_t* data, size_t len,
    ProgressCb& progress, size_t global_done, size_t global_total)
{
    if (progress) progress("ppm", global_done, global_total, 0);

    const size_t raw_size = 1 + 4 + len;  // STORED_RAW overhead

    // PPM encode
    auto coded = ppm_encode(data, len);
    size_t ppm_size = 1 + 4 + coded.size();  // block_type + sym_count + coded

    std::vector<uint8_t> out;
    if (ppm_size < raw_size) {
        // PPM wins
        out.reserve(ppm_size);
        out.push_back(0x00);                          // PPM_CODED
        write_u32le(out, (uint32_t)len);              // sym_count = original bytes
        out.insert(out.end(), coded.begin(), coded.end());
    } else {
        // Store raw
        out.reserve(raw_size);
        out.push_back(0x01);                          // STORED_RAW
        write_u32le(out, (uint32_t)len);
        out.insert(out.end(), data, data + len);
    }
    return out;
}

// -------------------------------------------------------
// CMIX block compressor (v10 format / --cmix mode)
// Block layout:
//   1 byte : block_type  (0x00=CMIX_CODED, 0x01=STORED_RAW)
//   if 0x00:
//     4 bytes: sym_count
//     N bytes: range-coded stream
//   if 0x01:
//     4 bytes: raw size
//     N bytes: raw bytes
// -------------------------------------------------------
static std::vector<uint8_t> compress_block_cmix(
    const uint8_t* data, size_t len,
    ProgressCb& progress, size_t global_done, size_t global_total)
{
    if (progress) progress("cmix", global_done, global_total, 1046);

    const size_t raw_size = 1 + 4 + len;

    auto cb = [&](size_t d, size_t t, int active) {
        if (progress) progress("cmix", global_done + d, global_total, active);
    };

    auto coded = cmix_encode(data, len, cb);
    size_t cmix_size = 1 + 4 + coded.size();

    std::vector<uint8_t> out;
    if (cmix_size < raw_size) {
        out.reserve(cmix_size);
        out.push_back(0x00);                          // CMIX_CODED
        write_u32le(out, (uint32_t)len);
        out.insert(out.end(), coded.begin(), coded.end());
    } else {
        out.reserve(raw_size);
        out.push_back(0x01);                          // STORED_RAW
        write_u32le(out, (uint32_t)len);
        out.insert(out.end(), data, data + len);
    }
    return out;
}

// -------------------------------------------------------
// compress_file  — Phase 18 streaming / memory-mapped I/O
// -------------------------------------------------------
// Input  : memory-mapped (OS paging — RAM = just the pages touched)
// Output : written block-by-block; block index patched in-place.
//
// Peak RAM = 1 uncompressed block + 1 compressed block.
// For v7: 512 KB + ~512 KB ≈ 1 MB.
// For v8: 4 MB + ~4 MB ≈ 8 MB
// Never holds the full input or output in memory.
// -------------------------------------------------------
int compress_file(const std::string& input_path,
                  const std::string& output_path,
                  ProgressCb progress,
                  CompressMode mode)
{
    const bool best_mode  = (mode == CompressMode::BEST);
    const bool ultra_mode = (mode == CompressMode::ULTRA);
    const bool cmix_mode  = (mode == CompressMode::CMIX);

    // --- 1. Memory-map the input file ---
    MappedFile mf;
    if (!map_file(input_path, mf)) {
        fprintf(stderr, "Cannot open/map input: %s\n", input_path.c_str());
        return 1;
    }
    const size_t file_size = mf.size;

    // --- 2. Compute SHA-256 by streaming over mapped pages ---
    Sha256Ctx ctx;
    if (file_size > 0)
        ctx.update(mf.data, file_size);
    auto cksum = ctx.finish();

    // --- 3. Determine block layout ---
    const uint32_t bsz = cmix_mode ? CMIX_BLOCK_SIZE
                       : ultra_mode ? PPM_BLOCK_SIZE
                       : (best_mode ? BWT_BLOCK_SIZE : BLOCK_SIZE);
    uint32_t block_count = (uint32_t)((file_size + bsz - 1) / bsz);
    if (block_count == 0) block_count = 1;

    // --- 4. Open output and write header + zeroed block index ---
    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        unmap_file(mf);
        fprintf(stderr, "Cannot write output: %s\n", output_path.c_str());
        return 1;
    }

    auto fwrite_all = [&](const void* data, size_t n) -> bool {
        return fwrite(data, 1, n, fout) == n;
    };

    // Header setup
    uint8_t version_byte = cmix_mode ? 10 : (ultra_mode ? 9 : (best_mode ? 8 : 7));
    uint8_t hdr[54] = {};
    hdr[0]='M'; hdr[1]='Z'; hdr[2]='I'; hdr[3]='P';
    hdr[4] = version_byte;
    hdr[5] = 0x00;  // mode = COMPRESSED (patched to STORED if needed)
    
    for (int i = 0; i < 8; ++i) hdr[6+i] = (uint8_t)((uint64_t)file_size >> (i*8));
    memcpy(hdr + 14, cksum.data(), 32);
    hdr[46]=(uint8_t)(block_count);     hdr[47]=(uint8_t)(block_count>>8);
    hdr[48]=(uint8_t)(block_count>>16); hdr[49]=(uint8_t)(block_count>>24);
    hdr[50]=(uint8_t)(bsz);     hdr[51]=(uint8_t)(bsz>>8);
    hdr[52]=(uint8_t)(bsz>>16); hdr[53]=(uint8_t)(bsz>>24);
    fwrite_all(hdr, 54);

    // Zeroed block index
    const long index_start = 54;
    std::vector<uint8_t> zero_index(block_count * 8, 0);
    fwrite_all(zero_index.data(), zero_index.size());

    // --- 5. Compress blocks ---
    size_t total_comp = 54 + (size_t)block_count * 8;
    size_t offset     = 0;

    for (uint32_t b = 0; b < block_count; ++b) {
        size_t blen = std::min((size_t)bsz, file_size - offset);
        const uint8_t* bdata = (file_size > 0) ? (mf.data + offset) : nullptr;

        std::vector<uint8_t> cblock;
        if (cmix_mode)
            cblock = compress_block_cmix(bdata, blen, progress, offset, file_size);
        else if (ultra_mode)
            cblock = compress_block_ppm(bdata, blen, progress, offset, file_size);
        else if (best_mode)
            cblock = compress_block_bwt(bdata, blen, progress, offset, file_size);
        else
            cblock = compress_block(bdata, blen, progress, offset, file_size);

        fwrite_all(cblock.data(), cblock.size());
        total_comp += cblock.size();

        // Patch index
        long entry_off = index_start + (long)(b * 8);
        fseek(fout, entry_off, SEEK_SET);
        uint32_t cs = (uint32_t)cblock.size();
        uint32_t os = (uint32_t)blen;
        uint8_t entry[8];
        entry[0]=cs; entry[1]=cs>>8; entry[2]=cs>>16; entry[3]=cs>>24;
        entry[4]=os; entry[5]=os>>8; entry[6]=os>>16; entry[7]=os>>24;
        fwrite_all(entry, 8);
        fseek(fout, 0, SEEK_END);

        offset += blen;
    }

    // --- 6. STORED fallback ---
    size_t total_stored = 54 + file_size;
    if (total_comp >= total_stored && file_size > 0) {
        if (mode == CompressMode::DEFAULT) {
            unmap_file(mf);
            fclose(fout);
            return compress_file(input_path, output_path, progress, CompressMode::BEST);
        }

        freopen(output_path.c_str(), "wb", fout);
        hdr[5] = 0x01;  // STORED
        uint32_t bc1 = 1;
        hdr[46]=(uint8_t)bc1;       hdr[47]=(uint8_t)(bc1>>8);
        hdr[48]=(uint8_t)(bc1>>16); hdr[49]=(uint8_t)(bc1>>24);
        uint32_t fsz32 = (uint32_t)std::min(file_size, (size_t)0xFFFFFFFFu);
        hdr[50]=(uint8_t)fsz32;       hdr[51]=(uint8_t)(fsz32>>8);
        hdr[52]=(uint8_t)(fsz32>>16); hdr[53]=(uint8_t)(fsz32>>24);
        fwrite_all(hdr, 54);
        
        constexpr size_t CHUNK = 4 * 1024 * 1024;
        for (size_t off2 = 0; off2 < file_size; off2 += CHUNK) {
            size_t n = std::min(CHUNK, file_size - off2);
            fwrite_all(mf.data + off2, n);
        }
    }

    if (progress) progress("write", file_size, file_size, 0);
    fclose(fout);
    unmap_file(mf);
    return 0;
}

// -------------------------------------------------------
// decompress_file  — Phase 18 streaming I/O
// -------------------------------------------------------
// Reads header + block index (small).
// For each block: fread() only that block's compressed bytes,
// decompress in RAM, fwrite() straight to output.
// Streams SHA-256 over output chunks — no recovered[] buffer.
//
// Peak RAM = 1 compressed block + 1 decompressed block.
// Never holds the full recovered file in memory.
// -------------------------------------------------------
int decompress_file(const std::string& input_path,
                    const std::string& output_path,
                    ProgressCb progress)
{
    // --- 1. Open compressed file ---
    FILE* fin = fopen(input_path.c_str(), "rb");
    if (!fin) { fprintf(stderr, "Cannot open input: %s\n", input_path.c_str()); return 1; }

    // --- 2. Read and parse the 54-byte header ---
    uint8_t hdr[54];
    if (fread(hdr, 1, 54, fin) != 54) {
        fclose(fin);
        fprintf(stderr, "File too small to be a valid .myzip\n");
        return 1;
    }
    if (hdr[0]!='M'||hdr[1]!='Z'||hdr[2]!='I'||hdr[3]!='P') {
        fclose(fin); fprintf(stderr, "Not an MZIP file\n"); return 1;
    }
    uint8_t  version     = hdr[4];
    if (version < 5 || version > 10) {
        fclose(fin);
        fprintf(stderr, "This decompressor handles v5-v10 (got v%d). Use Python for v4.\n", version);
        return 1;
    }
    uint8_t  mode        = hdr[5];
    uint64_t orig_size   = 0;
    for (int i = 0; i < 8; ++i) orig_size |= (uint64_t)hdr[6+i] << (i*8);
    uint8_t  cksum[32];
    memcpy(cksum, hdr + 14, 32);
    uint32_t block_count = (uint32_t)hdr[46] | ((uint32_t)hdr[47]<<8)
                         | ((uint32_t)hdr[48]<<16) | ((uint32_t)hdr[49]<<24);

    // --- 3. Open output file for streaming ---
    FILE* fout = fopen(output_path.c_str(), "wb");
    if (!fout) {
        fclose(fin);
        fprintf(stderr, "Cannot write output: %s\n", output_path.c_str());
        return 1;
    }

    Sha256Ctx sha_ctx;
    size_t    recovered_size = 0;

    // Helper: write a block to output and update streaming SHA-256
    auto emit = [&](const uint8_t* data, size_t n) {
        fwrite(data, 1, n, fout);
        sha_ctx.update(data, n);
        recovered_size += n;
    };

    if (mode == 0x01) {
        // STORED: stream raw bytes directly
        constexpr size_t CHUNK = 4 * 1024 * 1024;
        std::vector<uint8_t> buf(CHUNK);
        size_t n;
        while ((n = fread(buf.data(), 1, CHUNK, fin)) > 0)
            emit(buf.data(), n);
    } else {
        // Block-compressed: read block index first
        struct BlockInfo { uint32_t comp_size; uint32_t orig_size; };
        std::vector<BlockInfo> index(block_count);
        for (uint32_t b = 0; b < block_count; ++b) {
            uint8_t entry[8];
            if (fread(entry, 1, 8, fin) != 8) {
                fclose(fin); fclose(fout);
                fprintf(stderr, "Truncated block index\n"); return 1;
            }
            index[b].comp_size = (uint32_t)entry[0]|((uint32_t)entry[1]<<8)
                               |((uint32_t)entry[2]<<16)|((uint32_t)entry[3]<<24);
            index[b].orig_size = (uint32_t)entry[4]|((uint32_t)entry[5]<<8)
                               |((uint32_t)entry[6]<<16)|((uint32_t)entry[7]<<24);
        }

        // Decompress each block
        for (uint32_t b = 0; b < block_count; ++b) {
            uint32_t cs = index[b].comp_size;

            // Read exactly this block's compressed bytes
            std::vector<uint8_t> cbuf(cs);
            if (fread(cbuf.data(), 1, cs, fin) != cs) {
                fclose(fin); fclose(fout);
                fprintf(stderr, "Truncated compressed block %u\n", b); return 1;
            }
            const uint8_t* bp = cbuf.data();

            if (version == 5) {
                // v5: Huffman
                HuffTable table = huffman_read_lengths(bp);  bp += 256;
                uint32_t sym_count = (uint32_t)bp[0]|((uint32_t)bp[1]<<8)|((uint32_t)bp[2]<<16)|((uint32_t)bp[3]<<24);  bp += 4;
                uint32_t bit_count = (uint32_t)bp[0]|((uint32_t)bp[1]<<8)|((uint32_t)bp[2]<<16)|((uint32_t)bp[3]<<24);  bp += 4;
                size_t huff_bytes = (bit_count + 7) / 8;
                BitReader br(bp, huff_bytes, bit_count);
                auto tok = huffman_decode(br, table, sym_count);
                auto tkn = lz77_deserialize(tok.data(), tok.size());
                auto out = lz77_decode(tkn.data(), tkn.size());
                emit(out.data(), out.size());

            } else if (version == 6) {
                // v6: rANS order-0
                AnsTable tbl = ans_read_freqs(bp);  bp += 512;
                uint32_t sc = read_u32le(bp); bp += 4;
                uint32_t ec = read_u32le(bp); bp += 4;
                AnsDecoder dec; dec.init(bp, ec);
                std::vector<uint8_t> tok(sc);
                for (uint32_t i = 0; i < sc; ++i) tok[i] = dec.get(tbl);
                auto tkn = lz77_deserialize(tok.data(), tok.size());
                auto out = lz77_decode(tkn.data(), tkn.size());
                emit(out.data(), out.size());

            } else if (version == 8) {
                // v8: BWT + MTF + RLE + rANS (orders 0/1/2 or STORED_RAW)
                uint8_t block_type = *bp++;
                std::vector<uint8_t> block_out;
                if (block_type == 0x01) {
                    uint32_t raw = read_u32le(bp); bp += 4;
                    block_out.assign(bp, bp + raw);
                } else if (block_type == 0x00) {
                    uint32_t pi = read_u32le(bp); bp += 4;
                    AnsTable tbl = ans_read_freqs(bp); bp += 512;
                    uint32_t sc = read_u32le(bp); bp += 4;
                    uint32_t ec = read_u32le(bp); bp += 4;
                    AnsDecoder dec; dec.init(bp, ec);
                    std::vector<uint8_t> rle(sc);
                    for (uint32_t i = 0; i < sc; ++i) rle[i] = dec.get(tbl);
                    auto mtf = rle_zeros_decode(rle.data(), rle.size());
                    auto bwt = mtf_decode(mtf.data(), mtf.size());
                    block_out = bwt_decode(bwt.data(), bwt.size(), pi);
                } else if (block_type == 0x02) {
                    uint32_t pi = read_u32le(bp); bp += 4;
                    auto ctx = ans_ctx1_read(bp); bp += 131072;
                    uint32_t sc = read_u32le(bp); bp += 4;
                    uint32_t ec = read_u32le(bp); bp += 4;
                    auto rle = ans_ctx1_decode(bp, ec, sc, *ctx);
                    auto mtf = rle_zeros_decode(rle.data(), rle.size());
                    auto bwt = mtf_decode(mtf.data(), mtf.size());
                    block_out = bwt_decode(bwt.data(), bwt.size(), pi);
                } else if (block_type == 0x03) {
                    uint32_t pi = read_u32le(bp); bp += 4;
                    auto ctx = ans_ctx1_read(bp); bp += 131072;
                    uint32_t sc = read_u32le(bp); bp += 4;
                    uint32_t ec = read_u32le(bp); bp += 4;
                    auto rle = ans_ctx2_decode(bp, ec, sc, *ctx);
                    auto mtf = rle_zeros_decode(rle.data(), rle.size());
                    auto bwt = mtf_decode(mtf.data(), mtf.size());
                    block_out = bwt_decode(bwt.data(), bwt.size(), pi);
                } else {
                    fclose(fin); fclose(fout);
                    fprintf(stderr, "Unknown v8 block_type: 0x%02X\n", block_type); return 1;
                }
                emit(block_out.data(), block_out.size());

            } else if (version == 9 || version == 10) {
                // v9/v10
                uint8_t block_type = *bp++;
                if (block_type == 0x01) {
                    uint32_t raw_size = read_u32le(bp); bp += 4;
                    emit(bp, raw_size);
                } else if (block_type == 0x00) {
                    uint32_t sym_count = read_u32le(bp); bp += 4;
                    size_t coded_len = cs - 1 - 4;
                    if (version == 10) {
                        auto out = cmix_decode(bp, coded_len, sym_count);
                        emit(out.data(), out.size());
                    } else {
                        auto out = ppm_decode(bp, coded_len, sym_count);
                        emit(out.data(), out.size());
                    }
                } else {
                    fclose(fin); fclose(fout);
                    fprintf(stderr, "Unknown v9/v10 block type: %d\n", block_type);
                    return 1;
                }
            } else { // v7 or older
                // v7: LZ77 + delta pre-filter + rANS order-0
                uint8_t filter_type = *bp++;
                AnsTable tbl = ans_read_freqs(bp); bp += 512;
                uint32_t sc = read_u32le(bp); bp += 4;
                uint32_t ec = read_u32le(bp); bp += 4;
                AnsDecoder dec; dec.init(bp, ec);
                std::vector<uint8_t> tok(sc);
                for (uint32_t i = 0; i < sc; ++i) tok[i] = dec.get(tbl);
                auto tkn     = lz77_deserialize(tok.data(), tok.size());
                auto raw_out = lz77_decode(tkn.data(), tkn.size());
                std::vector<uint8_t> block_out;
                if (filter_type == FILTER_NONE) {
                    block_out = std::move(raw_out);
                } else if (filter_type == FILTER_DELTA16) {
                    block_out = delta16_decode(raw_out.data(), raw_out.size());
                } else {
                    int stride = (filter_type == FILTER_DELTA)  ? 1 :
                                 (filter_type == FILTER_DELTA2) ? 2 :
                                 (filter_type == FILTER_DELTA3) ? 3 : 4;
                    block_out = delta_decode(raw_out.data(), raw_out.size(), stride);
                }
                emit(block_out.data(), block_out.size());
            }

            if (progress)
                progress("decompress", recovered_size, (size_t)orig_size, 0);
        }
    }

    fclose(fin);
    fclose(fout);

    // --- 4. Verify size ---
    if (recovered_size != orig_size) {
        fprintf(stderr, "Size mismatch: expected %llu, got %zu\n",
                (unsigned long long)orig_size, recovered_size);
        return 1;
    }

    // --- 5. Verify SHA-256 (computed incrementally during emit) ---
    auto actual = sha_ctx.finish();
    if (memcmp(actual.data(), cksum, 32) != 0) {
        fprintf(stderr, "SHA-256 MISMATCH — file is corrupted!\n");
        return 1;
    }

    if (progress) progress("write", (size_t)orig_size, (size_t)orig_size, 0);
    return 0;
}
