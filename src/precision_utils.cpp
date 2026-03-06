/*
 * Precision Conversion Utilities
 * FP16/BF16 ↔ FP32 conversions for mixed precision training
 *
 * Algorithm adapted from: NVIDIA Apex (BSD-3 License)
 * Original: https://github.com/NVIDIA/apex
 * Modifications: Pure C++ implementation, no CUDA dependency
 */

#include <cstdint>
#include <cmath>
#include <vector>
#include <limits>

namespace PrecisionUtils {

// ============================================================================
// FP16 (IEEE 754 Half Precision) Conversion
// ============================================================================
// Format: 1 sign bit, 5 exponent bits, 10 mantissa bits
// Range: ~6e-8 to ~65,504
// Precision: ~3 decimal digits

uint16_t fp32_to_fp16(float value) {
    uint32_t f32 = *reinterpret_cast<uint32_t*>(&value);

    // Extract sign, exponent, mantissa from FP32
    uint32_t sign = (f32 >> 16) & 0x8000;       // Sign bit at position 15
    int32_t exponent = ((f32 >> 23) & 0xFF) - 127;  // Remove FP32 bias
    uint32_t mantissa = f32 & 0x7FFFFF;         // 23-bit mantissa

    // Handle special cases
    if (exponent == 128) {
        // Infinity or NaN
        return sign | 0x7C00 | (mantissa != 0 ? 0x0200 : 0);  // Preserve NaN
    }

    if (exponent > 15) {
        // Overflow → Infinity
        return sign | 0x7C00;
    }

    if (exponent < -14) {
        // Underflow → Zero or denormal
        if (exponent < -24) {
            return sign;  // Too small, return signed zero
        }
        // Denormal number
        mantissa = (mantissa | 0x800000) >> (1 - exponent - 14);
        return sign | (mantissa >> 13);
    }

    // Normal number
    uint32_t fp16_exp = (exponent + 15) << 10;  // Add FP16 bias
    uint32_t fp16_mantissa = mantissa >> 13;     // Truncate to 10 bits

    return sign | fp16_exp | fp16_mantissa;
}

float fp16_to_fp32(uint16_t value) {
    uint32_t sign = (value & 0x8000) << 16;     // Sign bit
    uint32_t exponent = (value >> 10) & 0x1F;   // 5-bit exponent
    uint32_t mantissa = value & 0x3FF;          // 10-bit mantissa

    // Handle special cases
    if (exponent == 0) {
        if (mantissa == 0) {
            // Zero
            uint32_t f32 = sign;
            return *reinterpret_cast<float*>(&f32);
        }
        // Denormal number → normalize
        exponent = 1;
        while (!(mantissa & 0x400)) {
            mantissa <<= 1;
            exponent--;
        }
        mantissa &= 0x3FF;
    } else if (exponent == 31) {
        // Infinity or NaN
        uint32_t f32 = sign | 0x7F800000 | (mantissa << 13);
        return *reinterpret_cast<float*>(&f32);
    }

    // Normal number
    uint32_t fp32_exp = (exponent - 15 + 127) << 23;  // Adjust bias
    uint32_t fp32_mantissa = mantissa << 13;          // Expand to 23 bits

    uint32_t f32 = sign | fp32_exp | fp32_mantissa;
    return *reinterpret_cast<float*>(&f32);
}

// ============================================================================
// BF16 (Brain Float 16) Conversion
// ============================================================================
// Format: 1 sign bit, 8 exponent bits, 7 mantissa bits
// Range: Same as FP32 (~1.2e-38 to ~3.4e38)
// Precision: ~2 decimal digits
// Advantage: No overflow handling needed (same range as FP32)

uint16_t fp32_to_bf16(float value) {
    uint32_t f32 = *reinterpret_cast<uint32_t*>(&value);

    // Simple truncation: take upper 16 bits of FP32
    // This preserves sign + exponent + top 7 bits of mantissa
    uint16_t bf16 = (f32 >> 16) & 0xFFFF;

    // Optional: Round to nearest even (better accuracy)
    // Check bit 16 (first truncated bit)
    if ((f32 & 0x8000) && ((f32 & 0x7FFF) || (bf16 & 1))) {
        bf16++;
    }

    return bf16;
}

float bf16_to_fp32(uint16_t value) {
    // Simple expansion: BF16 is just upper 16 bits of FP32
    uint32_t f32 = static_cast<uint32_t>(value) << 16;
    return *reinterpret_cast<float*>(&f32);
}

// ============================================================================
// Batch Conversion (for arrays)
// ============================================================================

void fp32_array_to_fp16(const float* src, uint16_t* dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = fp32_to_fp16(src[i]);
    }
}

void fp16_array_to_fp32(const uint16_t* src, float* dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = fp16_to_fp32(src[i]);
    }
}

void fp32_array_to_bf16(const float* src, uint16_t* dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = fp32_to_bf16(src[i]);
    }
}

void bf16_array_to_fp32(const uint16_t* src, float* dst, size_t count) {
    for (size_t i = 0; i < count; i++) {
        dst[i] = bf16_to_fp32(src[i]);
    }
}

// ============================================================================
// 2D Matrix Conversion (for weight matrices)
// ============================================================================

void fp32_matrix_to_fp16(
    const std::vector<std::vector<float>>& src,
    std::vector<uint16_t>& dst_flat  // Flattened output
) {
    size_t rows = src.size();
    size_t cols = src[0].size();
    dst_flat.resize(rows * cols);

    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            dst_flat[i * cols + j] = fp32_to_fp16(src[i][j]);
        }
    }
}

void fp16_matrix_to_fp32(
    const std::vector<uint16_t>& src_flat,
    std::vector<std::vector<float>>& dst,
    size_t rows,
    size_t cols
) {
    dst.resize(rows, std::vector<float>(cols));

    for (size_t i = 0; i < rows; i++) {
        for (size_t j = 0; j < cols; j++) {
            dst[i][j] = fp16_to_fp32(src_flat[i * cols + j]);
        }
    }
}

// ============================================================================
// Accuracy Testing
// ============================================================================

float fp16_round_trip_error(float original) {
    uint16_t fp16 = fp32_to_fp16(original);
    float recovered = fp16_to_fp32(fp16);
    return std::abs(original - recovered);
}

float bf16_round_trip_error(float original) {
    uint16_t bf16 = fp32_to_bf16(original);
    float recovered = bf16_to_fp32(bf16);
    return std::abs(original - recovered);
}

// ============================================================================
// SIMD-Optimized Array Conversion (F16C/AVX2)
// ============================================================================

#ifdef __F16C__
#include <immintrin.h>  // F16C, AVX2 intrinsics

void fp32_array_to_fp16_simd(const float* src, uint16_t* dst, size_t count) {
    size_t i = 0;

    // Process 8 floats at a time with F16C (256-bit AVX)
    for (; i + 8 <= count; i += 8) {
        __m256 fp32 = _mm256_loadu_ps(&src[i]);
        __m128i fp16 = _mm256_cvtps_ph(fp32, _MM_FROUND_TO_NEAREST_INT);
        _mm_storeu_si128((__m128i*)&dst[i], fp16);
    }

    // Scalar fallback for remaining elements
    for (; i < count; i++) {
        dst[i] = fp32_to_fp16(src[i]);
    }
}

void fp16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count) {
    size_t i = 0;

    // Process 8 FP16 values at a time with F16C
    for (; i + 8 <= count; i += 8) {
        __m128i fp16 = _mm_loadu_si128((const __m128i*)&src[i]);
        __m256 fp32 = _mm256_cvtph_ps(fp16);
        _mm256_storeu_ps(&dst[i], fp32);
    }

    // Scalar fallback for remaining elements
    for (; i < count; i++) {
        dst[i] = fp16_to_fp32(src[i]);
    }
}

#else
// No F16C support - fallback to scalar implementation
void fp32_array_to_fp16_simd(const float* src, uint16_t* dst, size_t count) {
    fp32_array_to_fp16(src, dst, count);  // Use scalar version
}

void fp16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count) {
    fp16_array_to_fp32(src, dst, count);  // Use scalar version
}
#endif

// ============================================================================
// SIMD-Optimized BF16 Conversion (AVX2)
// ============================================================================

#ifdef __AVX2__

void fp32_array_to_bf16_simd(const float* src, uint16_t* dst, size_t count) {
    size_t i = 0;

    // Process 8 floats at a time with AVX2
    for (; i + 8 <= count; i += 8) {
        __m256 fp32 = _mm256_loadu_ps(&src[i]);
        __m256i i32 = _mm256_castps_si256(fp32);

        // Shift right 16 bits to get upper half (BF16)
        __m256i shifted = _mm256_srli_epi32(i32, 16);

        // Pack 8x32-bit → 8x16-bit
        // Need to rearrange for packing
        __m128i lo = _mm256_castsi256_si128(shifted);
        __m128i hi = _mm256_extracti128_si256(shifted, 1);
        __m128i packed = _mm_packus_epi32(lo, hi);

        _mm_storeu_si128((__m128i*)&dst[i], packed);
    }

    // Scalar fallback for remaining elements
    for (; i < count; i++) {
        dst[i] = fp32_to_bf16(src[i]);
    }
}

void bf16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count) {
    size_t i = 0;

    // Process 8 BF16 values at a time with AVX2
    for (; i + 8 <= count; i += 8) {
        __m128i bf16 = _mm_loadu_si128((const __m128i*)&src[i]);

        // Unpack 8x16-bit → 8x32-bit
        __m128i lo_16 = _mm_unpacklo_epi16(_mm_setzero_si128(), bf16);
        __m128i hi_16 = _mm_unpackhi_epi16(_mm_setzero_si128(), bf16);

        // Combine into 256-bit register
        __m256i i32 = _mm256_set_m128i(hi_16, lo_16);
        __m256 fp32 = _mm256_castsi256_ps(i32);

        _mm256_storeu_ps(&dst[i], fp32);
    }

    // Scalar fallback for remaining elements
    for (; i < count; i++) {
        dst[i] = bf16_to_fp32(src[i]);
    }
}

#else
// No AVX2 support - fallback to scalar implementation
void fp32_array_to_bf16_simd(const float* src, uint16_t* dst, size_t count) {
    fp32_array_to_bf16(src, dst, count);  // Use scalar version
}

void bf16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count) {
    bf16_array_to_fp32(src, dst, count);  // Use scalar version
}
#endif

} // namespace PrecisionUtils
