#ifndef PRECISION_UTILS_H
#define PRECISION_UTILS_H

#include <cstdint>
#include <vector>

// Precision Conversion Utilities
// FP16/BF16 ↔ FP32 conversions for mixed precision training

namespace PrecisionUtils {

// ============================================================================
// Single Value Conversion
// ============================================================================

// FP16 (IEEE 754 Half Precision)
uint16_t fp32_to_fp16(float value);
float fp16_to_fp32(uint16_t value);

// BF16 (Brain Float 16)
uint16_t fp32_to_bf16(float value);
float bf16_to_fp32(uint16_t value);

// ============================================================================
// Array Conversion (Scalar - portable)
// ============================================================================

void fp32_array_to_fp16(const float* src, uint16_t* dst, size_t count);
void fp16_array_to_fp32(const uint16_t* src, float* dst, size_t count);

void fp32_array_to_bf16(const float* src, uint16_t* dst, size_t count);
void bf16_array_to_fp32(const uint16_t* src, float* dst, size_t count);

// ============================================================================
// Array Conversion (SIMD-optimized - F16C/AVX2)
// ============================================================================

// SIMD FP16 conversion (8x faster with F16C)
void fp32_array_to_fp16_simd(const float* src, uint16_t* dst, size_t count);
void fp16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count);

// SIMD BF16 conversion (8x faster with AVX2)
void fp32_array_to_bf16_simd(const float* src, uint16_t* dst, size_t count);
void bf16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count);

// ============================================================================
// Matrix Conversion
// ============================================================================

void fp32_matrix_to_fp16(
    const std::vector<std::vector<float>>& src,
    std::vector<uint16_t>& dst_flat  // Flattened output
);

void fp16_matrix_to_fp32(
    const std::vector<uint16_t>& src_flat,
    std::vector<std::vector<float>>& dst,
    size_t rows,
    size_t cols
);

// ============================================================================
// Testing & Validation
// ============================================================================

float fp16_round_trip_error(float original);
float bf16_round_trip_error(float original);

} // namespace PrecisionUtils

#endif // PRECISION_UTILS_H
