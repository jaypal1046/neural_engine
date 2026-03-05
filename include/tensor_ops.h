#ifndef TENSOR_OPS_H
#define TENSOR_OPS_H

#include <vector>
#include <cstdint>
#include <cmath>

// High-Performance Tensor Operations
// Algorithm adapted from: GGML (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/ggml.c
// Modifications: Removed dependencies, optimized for AIZip brain, added pure C++ interface

namespace TensorOps {

// CPU Feature Detection
struct CPUFeatures {
    bool avx;
    bool avx2;
    bool avx512;
    bool fma;
    bool sse2;
    bool sse42;
    bool neon;  // ARM NEON (future mobile support)
};

CPUFeatures detect_cpu_features();

// ============================================================================
// Matrix Operations (SIMD Optimized)
// ============================================================================

// Matrix multiplication: C = A @ B
// A: [m x k], B: [k x n], C: [m x n]
// Uses best available SIMD instruction set (AVX2 > SSE2 > scalar)
void matmul(
    const float* A, const float* B, float* C,
    int m, int k, int n
);

// Matrix multiplication with transposed B: C = A @ B^T
// Faster for attention computation (Q @ K^T)
void matmul_transposed(
    const float* A, const float* B, float* C,
    int m, int k, int n
);

// Vector dot product (SIMD optimized)
// Returns sum(a[i] * b[i])
float vec_dot(const float* a, const float* b, int n);

// Matrix-vector multiplication: y = A @ x
// A: [m x n], x: [n], y: [m]
void matvec(
    const float* A, const float* x, float* y,
    int m, int n
);

// Element-wise operations
void vec_add(float* dst, const float* a, const float* b, int n);
void vec_sub(float* dst, const float* a, const float* b, int n);
void vec_mul(float* dst, const float* a, const float* b, int n);
void vec_scale(float* dst, const float* src, float scale, int n);

// ============================================================================
// Normalization Layers
// ============================================================================

// RMSNorm: Root Mean Square Normalization (better than LayerNorm)
// Algorithm from: LLaMA paper (https://arxiv.org/abs/2302.13971)
// y = x / sqrt(mean(x^2) + eps) * weight
// Faster than LayerNorm (no mean subtraction, no bias)
void rmsnorm(
    float* output,
    const float* input,
    const float* weight,
    int size,
    float eps = 1e-6f
);

// LayerNorm (for compatibility)
// y = (x - mean(x)) / sqrt(var(x) + eps) * gamma + beta
void layernorm(
    float* output,
    const float* input,
    const float* gamma,
    const float* beta,
    int size,
    float eps = 1e-6f
);

// ============================================================================
// Rotary Position Embeddings (RoPE)
// ============================================================================

// RoPE: Rotary Position Embeddings
// Algorithm from: RoFormer paper (https://arxiv.org/abs/2104.09864)
// Used in LLaMA, Mistral, Qwen, Phi-3
// Better than absolute position embeddings (generalizes to longer sequences)

struct RoPEConfig {
    int dim;              // Embedding dimension
    int max_seq_len;      // Maximum sequence length
    float theta;          // Base frequency (default: 10000.0)
    float scaling_factor; // For long context (default: 1.0)

    // Long RoPE (Phi-3 style) - extends context length
    bool use_long_rope;
    float original_max_len; // Original context length (e.g., 4096)
    float extended_max_len; // Extended length (e.g., 128K)

    RoPEConfig()
        : dim(0), max_seq_len(0), theta(10000.0f), scaling_factor(1.0f),
          use_long_rope(false), original_max_len(0), extended_max_len(0) {}
};

// RoPE cache for precomputed sin/cos tables
class RoPECache {
public:
    RoPECache(const RoPEConfig& config);

    // Apply RoPE to query or key vectors
    // input: [seq_len, n_heads, head_dim]
    // positions: token positions [seq_len]
    void apply(
        float* output,
        const float* input,
        const int* positions,
        int seq_len,
        int n_heads,
        int head_dim
    );

    // Apply RoPE with interleaved format (Q and K together)
    void apply_interleaved(
        float* qk,  // [seq_len, n_heads, 2, head_dim] (Q and K interleaved)
        const int* positions,
        int seq_len,
        int n_heads,
        int head_dim
    );

private:
    RoPEConfig config_;
    std::vector<float> cos_cache_;  // [max_seq_len, dim/2]
    std::vector<float> sin_cache_;  // [max_seq_len, dim/2]

    void precompute_freqs();
    void apply_long_rope_scaling();
};

// ============================================================================
// Activation Functions (SIMD Optimized)
// ============================================================================

// GELU: Gaussian Error Linear Unit
// Used in GPT-2, BERT (exact version)
void gelu(float* dst, const float* src, int n);

// GELU (fast approximation)
// ~2x faster, negligible quality loss
void gelu_fast(float* dst, const float* src, int n);

// SiLU (Swish): x * sigmoid(x)
// Used in LLaMA, Mistral
void silu(float* dst, const float* src, int n);

// ReLU: max(0, x)
void relu(float* dst, const float* src, int n);

// Softmax: exp(x) / sum(exp(x))
// Numerically stable version (subtract max before exp)
void softmax(float* dst, const float* src, int n);

// Softmax with temperature scaling
void softmax_temperature(float* dst, const float* src, int n, float temperature);

// ============================================================================
// Utility Functions
// ============================================================================

// Copy tensor
void copy(float* dst, const float* src, int n);

// Fill tensor with constant
void fill(float* dst, float value, int n);

// Compute L2 norm
float l2_norm(const float* x, int n);

// Gradient clipping by norm
void clip_by_norm(float* grad, int n, float max_norm);

// ============================================================================
// Internal SIMD Implementations (dispatched automatically)
// ============================================================================

namespace internal {
    // Scalar fallback (always available)
    float vec_dot_scalar(const float* a, const float* b, int n);
    void matmul_scalar(const float* A, const float* B, float* C, int m, int k, int n);

    // SSE2 (guaranteed on x86_64)
    float vec_dot_sse2(const float* a, const float* b, int n);
    void matmul_sse2(const float* A, const float* B, float* C, int m, int k, int n);

    // AVX2 (runtime detection)
    float vec_dot_avx2(const float* a, const float* b, int n);
    void matmul_avx2(const float* A, const float* B, float* C, int m, int k, int n);

    // AVX-512 (future optimization)
    float vec_dot_avx512(const float* a, const float* b, int n);
    void matmul_avx512(const float* A, const float* B, float* C, int m, int k, int n);
}

} // namespace TensorOps

#endif // TENSOR_OPS_H
