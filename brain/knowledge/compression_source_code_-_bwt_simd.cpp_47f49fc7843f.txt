// =============================================================================
// SIMD-Optimized BWT/MTF/RLE Functions
// Uses SSE2 (always available on x64) and AVX2 (detected at runtime)
// =============================================================================

#include "bwt.h"
#include <cstring>
#include <algorithm>

#ifdef _MSC_VER
#include <intrin.h>
#else
#include <x86intrin.h>
#include <cpuid.h>
#endif

// -----------------------------------------------------------------------------
// CPU Feature Detection
// -----------------------------------------------------------------------------

static bool cpu_has_sse2() {
    // SSE2 is guaranteed on all x86-64 CPUs
    return true;
}

static bool cpu_has_avx2() {
    static int cached = -1;
    if (cached >= 0) return cached;

#ifdef _MSC_VER
    int cpuInfo[4];
    __cpuid(cpuInfo, 1);
    bool hasAVX = (cpuInfo[2] & (1 << 28)) != 0;
    if (!hasAVX) { cached = 0; return false; }

    __cpuid(cpuInfo, 7);
    cached = (cpuInfo[1] & (1 << 5)) != 0;
#else
    unsigned int eax, ebx, ecx, edx;
    if (__get_cpuid(1, &eax, &ebx, &ecx, &edx)) {
        bool hasAVX = (ecx & (1 << 28)) != 0;
        if (!hasAVX) { cached = 0; return false; }
    }

    if (__get_cpuid_count(7, 0, &eax, &ebx, &ecx, &edx)) {
        cached = (ebx & (1 << 5)) != 0;
    } else {
        cached = 0;
    }
#endif

    return cached;
}

// -----------------------------------------------------------------------------
// SIMD-Optimized MTF Encode
// Uses SSE2 for fast linear search in the list
// -----------------------------------------------------------------------------

std::vector<uint8_t> mtf_encode_simd(const uint8_t* data, size_t N) {
    uint8_t list[256];
    for (int i = 0; i < 256; ++i) list[i] = (uint8_t)i;

    std::vector<uint8_t> out(N);

    // Use SSE2 for searching
    for (size_t i = 0; i < N; ++i) {
        uint8_t b = data[i];
        int rank = 0;

        // SSE2: Compare 16 bytes at a time
        #ifdef __SSE2__
        __m128i search = _mm_set1_epi8(b);

        // Search in chunks of 16
        for (int j = 0; j < 256; j += 16) {
            __m128i chunk = _mm_loadu_si128((__m128i*)(list + j));
            __m128i cmp = _mm_cmpeq_epi8(chunk, search);
            int mask = _mm_movemask_epi8(cmp);

            if (mask) {
                // Found in this chunk - count trailing zeros to find position
                #ifdef _MSC_VER
                unsigned long pos;
                _BitScanForward(&pos, mask);
                rank = j + pos;
                #else
                rank = j + __builtin_ctz(mask);
                #endif
                break;
            }
        }
        #else
        // Fallback: linear search
        while (list[rank] != b) ++rank;
        #endif

        out[i] = (uint8_t)rank;

        // Move to front - keep using memmove (it's already optimized)
        if (rank > 0) {
            memmove(list + 1, list, rank);
            list[0] = b;
        }
    }

    return out;
}

// -----------------------------------------------------------------------------
// SIMD-Optimized RLE Zero Search
// Uses SSE2/AVX2 to find runs of zeros quickly
// -----------------------------------------------------------------------------

std::vector<uint8_t> rle_zeros_encode_simd(const uint8_t* data, size_t N) {
    std::vector<uint8_t> out;
    out.reserve(N);

    size_t i = 0;

    #ifdef __SSE2__
    __m128i zero_vec = _mm_setzero_si128();

    while (i < N) {
        if (data[i] != 0) {
            out.push_back(data[i]);
            ++i;
        } else {
            // Count run of zeros using SIMD
            size_t run_start = i;
            size_t k = 0;

            // Process 16 bytes at a time
            while (i + 16 <= N && k < 256) {
                __m128i chunk = _mm_loadu_si128((__m128i*)(data + i));
                __m128i cmp = _mm_cmpeq_epi8(chunk, zero_vec);
                int mask = _mm_movemask_epi8(cmp);

                if (mask == 0xFFFF) {
                    // All 16 bytes are zero
                    i += 16;
                    k += 16;
                } else if (mask == 0) {
                    // No zeros in this chunk
                    break;
                } else {
                    // Partial zeros - count how many
                    #ifdef _MSC_VER
                    unsigned long pos;
                    _BitScanForward(&pos, ~mask);
                    size_t count = pos;
                    #else
                    size_t count = __builtin_ctz(~mask);
                    #endif
                    i += count;
                    k += count;
                    break;
                }
            }

            // Finish counting any remaining zeros (scalar)
            while (i < N && data[i] == 0 && k < 256) {
                ++k;
                ++i;
            }

            if (k > 0) {
                out.push_back(0x00);
                out.push_back((uint8_t)(k - 1));
            }
        }
    }
    #else
    // Fallback to scalar version
    while (i < N) {
        if (data[i] != 0) {
            out.push_back(data[i]);
            ++i;
        } else {
            size_t k = 0;
            while (i < N && data[i] == 0 && k < 256) {
                ++k;
                ++i;
            }
            out.push_back(0x00);
            out.push_back((uint8_t)(k - 1));
        }
    }
    #endif

    return out;
}

// -----------------------------------------------------------------------------
// Fast memory operations using SIMD
// -----------------------------------------------------------------------------

void fast_memcpy(void* dest, const void* src, size_t n) {
    #ifdef __AVX2__
    if (cpu_has_avx2() && n >= 32) {
        uint8_t* d = (uint8_t*)dest;
        const uint8_t* s = (const uint8_t*)src;

        // Copy 32 bytes at a time with AVX2
        size_t avx_chunks = n / 32;
        for (size_t i = 0; i < avx_chunks; ++i) {
            __m256i chunk = _mm256_loadu_si256((__m256i*)(s + i * 32));
            _mm256_storeu_si256((__m256i*)(d + i * 32), chunk);
        }

        // Copy remaining bytes
        size_t remaining = n % 32;
        if (remaining > 0) {
            memcpy(d + avx_chunks * 32, s + avx_chunks * 32, remaining);
        }
        return;
    }
    #endif

    #ifdef __SSE2__
    if (n >= 16) {
        uint8_t* d = (uint8_t*)dest;
        const uint8_t* s = (const uint8_t*)src;

        // Copy 16 bytes at a time with SSE2
        size_t sse_chunks = n / 16;
        for (size_t i = 0; i < sse_chunks; ++i) {
            __m128i chunk = _mm_loadu_si128((__m128i*)(s + i * 16));
            _mm_storeu_si128((__m128i*)(d + i * 16), chunk);
        }

        // Copy remaining bytes
        size_t remaining = n % 16;
        if (remaining > 0) {
            memcpy(d + sse_chunks * 16, s + sse_chunks * 16, remaining);
        }
        return;
    }
    #endif

    // Fallback
    memcpy(dest, src, n);
}

// -----------------------------------------------------------------------------
// Export: Choose best implementation at runtime
// -----------------------------------------------------------------------------

std::vector<uint8_t> mtf_encode_optimized(const uint8_t* data, size_t N) {
    #ifdef __SSE2__
    if (cpu_has_sse2()) {
        return mtf_encode_simd(data, N);
    }
    #endif
    return mtf_encode(data, N);  // Fallback to scalar
}

std::vector<uint8_t> rle_zeros_encode_optimized(const uint8_t* data, size_t N) {
    #ifdef __SSE2__
    if (cpu_has_sse2()) {
        return rle_zeros_encode_simd(data, N);
    }
    #endif
    return rle_zeros_encode(data, N);  // Fallback to scalar
}
