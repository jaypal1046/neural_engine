// 4-bit and 8-bit Quantization for Model Compression
// Algorithm adapted from: GGML (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/ggml-quants.c
// Modifications: Removed dependencies, optimized for AIZip brain

#include "quantization.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace Quantization {

// ============================================================================
// FP16 Conversion (IEEE 754 half-precision)
// ============================================================================

uint16_t fp32_to_fp16(float x) {
    uint32_t bits;
    std::memcpy(&bits, &x, sizeof(float));

    uint32_t sign = (bits >> 16) & 0x8000;
    int32_t exp = ((bits >> 23) & 0xFF) - 127 + 15;
    uint32_t mantissa = bits & 0x7FFFFF;

    // Handle special cases
    if (exp <= 0) {
        // Denormalized or zero
        return sign;
    } else if (exp >= 31) {
        // Infinity or NaN
        return sign | 0x7C00;
    }

    // Normal case
    return sign | (exp << 10) | (mantissa >> 13);
}

float fp16_to_fp32(uint16_t x) {
    uint32_t sign = (x & 0x8000) << 16;
    int32_t exp = (x & 0x7C00) >> 10;
    uint32_t mantissa = x & 0x03FF;

    if (exp == 0) {
        // Zero or denormalized
        if (mantissa == 0) {
            uint32_t result = sign;
            float f;
            std::memcpy(&f, &result, sizeof(float));
            return f;
        }
        // Denormalized - not handling for simplicity
        return 0.0f;
    } else if (exp == 31) {
        // Infinity or NaN
        uint32_t result = sign | 0x7F800000;
        float f;
        std::memcpy(&f, &result, sizeof(float));
        return f;
    }

    // Normal case
    uint32_t result = sign | ((exp - 15 + 127) << 23) | (mantissa << 13);
    float f;
    std::memcpy(&f, &result, sizeof(float));
    return f;
}

// ============================================================================
// Q4_0 Quantization (Symmetric, block-wise)
// ============================================================================

void quantize_q4_0(const float* x, BlockQ4_0* blocks, int n) {
    int num_blocks = n / QK4_0;

    for (int b = 0; b < num_blocks; b++) {
        const float* block_data = x + b * QK4_0;

        // Find max absolute value in block
        float amax = 0.0f;
        for (int i = 0; i < QK4_0; i++) {
            amax = std::max(amax, std::abs(block_data[i]));
        }

        // Compute scale (symmetric quantization)
        float scale = amax / 7.0f;  // 4-bit signed: -7 to +7
        float inv_scale = (scale != 0.0f) ? (1.0f / scale) : 0.0f;

        // Store scale in FP16
        blocks[b].scale = fp32_to_fp16(scale);

        // Quantize values to 4-bit
        for (int i = 0; i < QK4_0; i += 2) {
            // Quantize two values per byte
            int q0 = (int)std::round(block_data[i] * inv_scale);
            int q1 = (int)std::round(block_data[i + 1] * inv_scale);

            // Clamp to 4-bit signed range: -8 to +7
            q0 = std::max(-8, std::min(7, q0));
            q1 = std::max(-8, std::min(7, q1));

            // Pack two 4-bit values into one byte
            // q0 in low nibble, q1 in high nibble
            blocks[b].qs[i / 2] = (uint8_t)((q0 & 0xF) | ((q1 & 0xF) << 4));
        }
    }
}

void dequantize_q4_0(const BlockQ4_0* blocks, float* y, int n) {
    int num_blocks = n / QK4_0;

    for (int b = 0; b < num_blocks; b++) {
        float scale = fp16_to_fp32(blocks[b].scale);
        float* block_out = y + b * QK4_0;

        for (int i = 0; i < QK4_0; i += 2) {
            uint8_t packed = blocks[b].qs[i / 2];

            // Extract two 4-bit values
            int q0 = (int8_t)(packed << 4) >> 4;  // Sign-extend low nibble
            int q1 = (int8_t)(packed & 0xF0) >> 4; // Sign-extend high nibble

            // Dequantize
            block_out[i] = q0 * scale;
            block_out[i + 1] = q1 * scale;
        }
    }
}

// ============================================================================
// Q4_1 Quantization (Asymmetric, with min offset)
// ============================================================================

void quantize_q4_1(const float* x, BlockQ4_1* blocks, int n) {
    int num_blocks = n / QK4_1;

    for (int b = 0; b < num_blocks; b++) {
        const float* block_data = x + b * QK4_1;

        // Find min and max
        float min_val = block_data[0];
        float max_val = block_data[0];
        for (int i = 1; i < QK4_1; i++) {
            min_val = std::min(min_val, block_data[i]);
            max_val = std::max(max_val, block_data[i]);
        }

        // Compute scale and min (asymmetric quantization)
        float scale = (max_val - min_val) / 15.0f;  // 4-bit unsigned: 0 to 15
        float inv_scale = (scale != 0.0f) ? (1.0f / scale) : 0.0f;

        blocks[b].scale = fp32_to_fp16(scale);
        blocks[b].min = fp32_to_fp16(min_val);

        // Quantize
        for (int i = 0; i < QK4_1; i += 2) {
            int q0 = (int)std::round((block_data[i] - min_val) * inv_scale);
            int q1 = (int)std::round((block_data[i + 1] - min_val) * inv_scale);

            q0 = std::max(0, std::min(15, q0));
            q1 = std::max(0, std::min(15, q1));

            blocks[b].qs[i / 2] = (uint8_t)((q0 & 0xF) | ((q1 & 0xF) << 4));
        }
    }
}

void dequantize_q4_1(const BlockQ4_1* blocks, float* y, int n) {
    int num_blocks = n / QK4_1;

    for (int b = 0; b < num_blocks; b++) {
        float scale = fp16_to_fp32(blocks[b].scale);
        float min_val = fp16_to_fp32(blocks[b].min);
        float* block_out = y + b * QK4_1;

        for (int i = 0; i < QK4_1; i += 2) {
            uint8_t packed = blocks[b].qs[i / 2];

            uint8_t q0 = packed & 0xF;
            uint8_t q1 = (packed >> 4) & 0xF;

            block_out[i] = q0 * scale + min_val;
            block_out[i + 1] = q1 * scale + min_val;
        }
    }
}

// ============================================================================
// Q8_0 Quantization (8-bit, minimal loss)
// ============================================================================

void quantize_q8_0(const float* x, BlockQ8_0* blocks, int n) {
    int num_blocks = n / QK8_0;

    for (int b = 0; b < num_blocks; b++) {
        const float* block_data = x + b * QK8_0;

        // Find max absolute value
        float amax = 0.0f;
        for (int i = 0; i < QK8_0; i++) {
            amax = std::max(amax, std::abs(block_data[i]));
        }

        // Compute scale
        float scale = amax / 127.0f;  // 8-bit signed: -127 to +127
        float inv_scale = (scale != 0.0f) ? (1.0f / scale) : 0.0f;

        blocks[b].scale = fp32_to_fp16(scale);

        // Quantize to 8-bit
        for (int i = 0; i < QK8_0; i++) {
            int q = (int)std::round(block_data[i] * inv_scale);
            q = std::max(-127, std::min(127, q));
            blocks[b].qs[i] = (int8_t)q;
        }
    }
}

void dequantize_q8_0(const BlockQ8_0* blocks, float* y, int n) {
    int num_blocks = n / QK8_0;

    for (int b = 0; b < num_blocks; b++) {
        float scale = fp16_to_fp32(blocks[b].scale);
        float* block_out = y + b * QK8_0;

        for (int i = 0; i < QK8_0; i++) {
            block_out[i] = blocks[b].qs[i] * scale;
        }
    }
}

// ============================================================================
// Q5_0 Quantization (5-bit, better quality than Q4)
// ============================================================================

void quantize_q5_0(const float* x, BlockQ5_0* blocks, int n) {
    int num_blocks = n / QK5_0;

    for (int b = 0; b < num_blocks; b++) {
        const float* block_data = x + b * QK5_0;

        float amax = 0.0f;
        for (int i = 0; i < QK5_0; i++) {
            amax = std::max(amax, std::abs(block_data[i]));
        }

        float scale = amax / 15.0f;  // 5-bit signed: -15 to +15
        float inv_scale = (scale != 0.0f) ? (1.0f / scale) : 0.0f;

        blocks[b].scale = fp32_to_fp16(scale);
        std::memset(blocks[b].qh, 0, 4);  // Clear high bits

        for (int i = 0; i < QK5_0; i++) {
            int q = (int)std::round(block_data[i] * inv_scale);
            q = std::max(-16, std::min(15, q));

            // Split into 4-bit low + 1-bit high
            uint8_t low4 = q & 0xF;
            uint8_t high1 = (q >> 4) & 0x1;

            // Store low 4 bits
            if (i % 2 == 0) {
                blocks[b].qs[i / 2] = low4;
            } else {
                blocks[b].qs[i / 2] |= (low4 << 4);
            }

            // Store high bit
            blocks[b].qh[i / 8] |= (high1 << (i % 8));
        }
    }
}

void dequantize_q5_0(const BlockQ5_0* blocks, float* y, int n) {
    int num_blocks = n / QK5_0;

    for (int b = 0; b < num_blocks; b++) {
        float scale = fp16_to_fp32(blocks[b].scale);
        float* block_out = y + b * QK5_0;

        for (int i = 0; i < QK5_0; i++) {
            // Reconstruct 5-bit value
            uint8_t low4 = (blocks[b].qs[i / 2] >> ((i % 2) * 4)) & 0xF;
            uint8_t high1 = (blocks[b].qh[i / 8] >> (i % 8)) & 0x1;
            int q = (int8_t)((high1 << 4) | low4);

            // Sign extend from 5 bits
            if (q & 0x10) q |= 0xE0;

            block_out[i] = q * scale;
        }
    }
}

// ============================================================================
// Quantized Matrix-Vector Multiplication (Fast Inference)
// ============================================================================

float vec_dot_q4_0_fp32(const BlockQ4_0* blocks, const float* x, int n) {
    int num_blocks = n / QK4_0;
    float sum = 0.0f;

    for (int b = 0; b < num_blocks; b++) {
        float scale = fp16_to_fp32(blocks[b].scale);
        const float* x_block = x + b * QK4_0;

        float block_sum = 0.0f;
        for (int i = 0; i < QK4_0; i += 2) {
            uint8_t packed = blocks[b].qs[i / 2];

            int8_t q0 = (int8_t)(packed << 4) >> 4;
            int8_t q1 = (int8_t)(packed & 0xF0) >> 4;

            block_sum += q0 * x_block[i];
            block_sum += q1 * x_block[i + 1];
        }

        sum += block_sum * scale;
    }

    return sum;
}

void matvec_q4_0(const BlockQ4_0* A_blocks, const float* x, float* y, int m, int k) {
    int k_blocks = k / QK4_0;

    for (int row = 0; row < m; row++) {
        const BlockQ4_0* row_blocks = A_blocks + row * k_blocks;
        y[row] = vec_dot_q4_0_fp32(row_blocks, x, k);
    }
}

// ============================================================================
// Model Quantization Utilities
// ============================================================================

size_t quantize_matrix_q4_0(const float* weights, BlockQ4_0* output, int rows, int cols) {
    for (int i = 0; i < rows; i++) {
        quantize_q4_0(weights + i * cols, output + i * (cols / QK4_0), cols);
    }
    return rows * (cols / QK4_0) * sizeof(BlockQ4_0);
}

float quantization_error(const float* original, const float* quantized, int n) {
    float mse = 0.0f;
    for (int i = 0; i < n; i++) {
        float diff = original[i] - quantized[i];
        mse += diff * diff;
    }
    return std::sqrt(mse / n);
}

QuantizationStats analyze_quantization(const float* original, int n, int format) {
    QuantizationStats stats = {};

    std::vector<float> dequantized(n);

    if (format == 0) {  // Q4_0
        std::vector<BlockQ4_0> blocks(n / QK4_0);
        quantize_q4_0(original, blocks.data(), n);
        dequantize_q4_0(blocks.data(), dequantized.data(), n);
        stats.compression_ratio = (float)(n * sizeof(float)) / (blocks.size() * sizeof(BlockQ4_0));
    } else if (format == 3) {  // Q8_0
        std::vector<BlockQ8_0> blocks(n / QK8_0);
        quantize_q8_0(original, blocks.data(), n);
        dequantize_q8_0(blocks.data(), dequantized.data(), n);
        stats.compression_ratio = (float)(n * sizeof(float)) / (blocks.size() * sizeof(BlockQ8_0));
    }

    // Compute errors
    float sum_error = 0.0f;
    float sum_original_sq = 0.0f;
    float sum_noise_sq = 0.0f;
    stats.max_error = 0.0f;

    for (int i = 0; i < n; i++) {
        float error = std::abs(original[i] - dequantized[i]);
        sum_error += error;
        sum_original_sq += original[i] * original[i];
        sum_noise_sq += error * error;
        stats.max_error = std::max(stats.max_error, error);
    }

    stats.mean_error = sum_error / n;
    stats.snr_db = 10.0f * std::log10(sum_original_sq / sum_noise_sq);

    return stats;
}

} // namespace Quantization
