#ifndef QUANTIZATION_H
#define QUANTIZATION_H

#include <vector>
#include <cstdint>
#include <cstring>

// 4-bit and 8-bit Quantization for Model Compression
// Algorithm adapted from: GGML (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/ggml-quants.c
// Modifications: Removed dependencies, optimized for AIZip brain

namespace Quantization {

// ============================================================================
// Quantization Formats (from GGML)
// ============================================================================

// Q4_0: 4-bit quantization with block-wise scaling
// - Block size: 32 floats → 16 bytes (4 bits per weight)
// - Each block has 1 FP16 scale factor
// - Symmetric quantization: range [-scale, +scale]
// - Compression: 32 floats (128 bytes) → 18 bytes (7.1x compression)

#define QK4_0 32  // Block size for Q4_0
#define QK4_1 32  // Block size for Q4_1
#define QK5_0 32  // Block size for Q5_0
#define QK8_0 32  // Block size for Q8_0

// Q4_0 block: 32 weights in 4 bits each + 1 FP16 scale
struct BlockQ4_0 {
    uint16_t scale;      // FP16 scale factor (2 bytes)
    uint8_t qs[16];      // 32 x 4-bit quantized values (16 bytes)
                         // Total: 18 bytes
};

// Q4_1 block: Like Q4_0 but asymmetric (has min offset)
struct BlockQ4_1 {
    uint16_t scale;      // FP16 scale factor
    uint16_t min;        // FP16 min value (offset)
    uint8_t qs[16];      // 32 x 4-bit quantized values
                         // Total: 20 bytes
};

// Q5_0 block: 5-bit quantization (better quality than Q4)
struct BlockQ5_0 {
    uint16_t scale;      // FP16 scale factor
    uint8_t qh[4];       // High bits (5th bit for each value)
    uint8_t qs[16];      // 32 x 4-bit low bits
                         // Total: 22 bytes
};

// Q8_0 block: 8-bit quantization (minimal quality loss)
struct BlockQ8_0 {
    uint16_t scale;      // FP16 scale factor
    int8_t qs[32];       // 32 x 8-bit quantized values
                         // Total: 34 bytes (3.8x compression)
};

// ============================================================================
// FP16 Conversion (for scale factors)
// ============================================================================

// Convert float32 to float16 (half precision)
uint16_t fp32_to_fp16(float x);

// Convert float16 to float32
float fp16_to_fp32(uint16_t x);

// ============================================================================
// Quantization Functions
// ============================================================================

// Quantize FP32 array to Q4_0 format
// Input: x[n] (float array)
// Output: blocks[n/32] (Q4_0 blocks)
void quantize_q4_0(const float* x, BlockQ4_0* blocks, int n);

// Dequantize Q4_0 blocks back to FP32
void dequantize_q4_0(const BlockQ4_0* blocks, float* y, int n);

// Quantize to Q4_1 (asymmetric)
void quantize_q4_1(const float* x, BlockQ4_1* blocks, int n);
void dequantize_q4_1(const BlockQ4_1* blocks, float* y, int n);

// Quantize to Q5_0 (5-bit, better quality)
void quantize_q5_0(const float* x, BlockQ5_0* blocks, int n);
void dequantize_q5_0(const BlockQ5_0* blocks, float* y, int n);

// Quantize to Q8_0 (8-bit, minimal loss)
void quantize_q8_0(const float* x, BlockQ8_0* blocks, int n);
void dequantize_q8_0(const BlockQ8_0* blocks, float* y, int n);

// ============================================================================
// Quantized Matrix Operations (Fast Inference)
// ============================================================================

// Quantized matrix-vector multiplication: y = A @ x
// A: quantized weight matrix (Q4_0 format)
// x: FP32 input vector
// y: FP32 output vector
void matvec_q4_0(
    const BlockQ4_0* A_blocks,  // Quantized weight matrix
    const float* x,              // Input vector (FP32)
    float* y,                    // Output vector (FP32)
    int m,                       // Number of rows
    int k                        // Number of columns (must be multiple of 32)
);

// Optimized dot product: Q4_0 @ FP32
// Used internally for matvec
float vec_dot_q4_0_fp32(const BlockQ4_0* blocks, const float* x, int n);

// ============================================================================
// Model Quantization Utilities
// ============================================================================

// Quantize entire weight matrix (row-major)
// Returns compressed size in bytes
size_t quantize_matrix_q4_0(
    const float* weights,        // Input: FP32 weights [rows x cols]
    BlockQ4_0* output,           // Output: Quantized blocks
    int rows,
    int cols                     // Must be multiple of 32
);

// Estimate quantization error (MSE)
float quantization_error(
    const float* original,
    const float* quantized,
    int n
);

// ============================================================================
// Advanced: Mixed Precision Inference
// ============================================================================

// Mixed precision: Q4_0 weights + FP16 activations
// Faster than full FP32, minimal quality loss
struct MixedPrecisionConfig {
    bool use_fp16_activations;   // Use FP16 for intermediate results
    bool use_q4_weights;          // Use Q4_0 for weights
    bool use_q8_activations;      // Use Q8_0 for activation cache
};

// Apply mixed precision to transformer layer
void mixed_precision_forward(
    const MixedPrecisionConfig& config,
    const BlockQ4_0* weights,
    const float* input,
    float* output,
    int batch_size,
    int input_dim,
    int output_dim
);

// ============================================================================
// Statistics and Analysis
// ============================================================================

// Analyze quantization impact
struct QuantizationStats {
    float compression_ratio;     // Original size / Quantized size
    float mean_error;            // Average absolute error
    float max_error;             // Maximum absolute error
    float snr_db;                // Signal-to-noise ratio (dB)
};

QuantizationStats analyze_quantization(
    const float* original,
    int n,
    int format  // 0=Q4_0, 1=Q4_1, 2=Q5_0, 3=Q8_0
);

} // namespace Quantization

#endif // QUANTIZATION_H
