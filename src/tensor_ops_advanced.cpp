// Advanced Tensor Operations: RMSNorm, RoPE, Activations
// Algorithm adapted from: GGML (MIT License) and LLaMA/Mistral implementations
// Original: https://github.com/ggerganov/llama.cpp

#include "tensor_ops.h"
#include <cmath>
#include <algorithm>

#ifdef __AVX2__
#include <immintrin.h>
#endif

namespace TensorOps {

// ============================================================================
// Normalization Layers
// ============================================================================

void rmsnorm(float* output, const float* input, const float* weight, int size, float eps) {
    // RMSNorm: y = x / sqrt(mean(x^2) + eps) * weight
    // Used in LLaMA, Mistral (faster than LayerNorm, no bias)

    // Compute mean of squares
    float sum_sq = 0.0f;
    for (int i = 0; i < size; i++) {
        sum_sq += input[i] * input[i];
    }
    float rms = std::sqrt(sum_sq / size + eps);

    // Normalize and scale
    float inv_rms = 1.0f / rms;
    for (int i = 0; i < size; i++) {
        output[i] = input[i] * inv_rms * weight[i];
    }
}

void layernorm(float* output, const float* input, const float* gamma, const float* beta, int size, float eps) {
    // Standard LayerNorm: y = (x - mean) / sqrt(var + eps) * gamma + beta

    // Compute mean
    float sum = 0.0f;
    for (int i = 0; i < size; i++) {
        sum += input[i];
    }
    float mean = sum / size;

    // Compute variance
    float sum_sq_diff = 0.0f;
    for (int i = 0; i < size; i++) {
        float diff = input[i] - mean;
        sum_sq_diff += diff * diff;
    }
    float variance = sum_sq_diff / size;
    float inv_std = 1.0f / std::sqrt(variance + eps);

    // Normalize, scale, and shift
    for (int i = 0; i < size; i++) {
        output[i] = (input[i] - mean) * inv_std * gamma[i] + beta[i];
    }
}

// ============================================================================
// Rotary Position Embeddings (RoPE)
// ============================================================================

RoPECache::RoPECache(const RoPEConfig& config) : config_(config) {
    precompute_freqs();
    if (config_.use_long_rope) {
        apply_long_rope_scaling();
    }
}

void RoPECache::precompute_freqs() {
    int head_dim = config_.dim;
    int half_dim = head_dim / 2;

    cos_cache_.resize(config_.max_seq_len * half_dim);
    sin_cache_.resize(config_.max_seq_len * half_dim);

    for (int pos = 0; pos < config_.max_seq_len; pos++) {
        for (int i = 0; i < half_dim; i++) {
            // Compute frequency: theta^(-2i/d)
            float freq = 1.0f / std::pow(config_.theta, (2.0f * i) / head_dim);
            freq *= config_.scaling_factor;  // Apply scaling if needed

            float angle = pos * freq;

            cos_cache_[pos * half_dim + i] = std::cos(angle);
            sin_cache_[pos * half_dim + i] = std::sin(angle);
        }
    }
}

void RoPECache::apply_long_rope_scaling() {
    // Long RoPE (Phi-3 style): extends context length via frequency interpolation
    // Paper: https://arxiv.org/abs/2309.16039

    if (config_.extended_max_len <= config_.original_max_len) {
        return;  // No scaling needed
    }

    float scale = config_.original_max_len / config_.extended_max_len;
    int half_dim = config_.dim / 2;

    // Apply linear interpolation to frequencies
    for (int pos = 0; pos < config_.max_seq_len; pos++) {
        for (int i = 0; i < half_dim; i++) {
            // Recompute with scaled frequency
            float freq = 1.0f / std::pow(config_.theta, (2.0f * i) / config_.dim);
            freq *= scale;  // Long RoPE scaling

            float angle = pos * freq;

            cos_cache_[pos * half_dim + i] = std::cos(angle);
            sin_cache_[pos * half_dim + i] = std::sin(angle);
        }
    }
}

void RoPECache::apply(float* output, const float* input, const int* positions, int seq_len, int n_heads, int head_dim) {
    // Apply RoPE to input tensor: [seq_len, n_heads, head_dim]
    // Algorithm: for each pair (x[2i], x[2i+1]), apply rotation matrix

    int half_dim = head_dim / 2;

    for (int s = 0; s < seq_len; s++) {
        int pos = positions[s];
        if (pos >= config_.max_seq_len) {
            pos = config_.max_seq_len - 1;  // Clamp to max
        }

        for (int h = 0; h < n_heads; h++) {
            const float* in_head = input + (s * n_heads + h) * head_dim;
            float* out_head = output + (s * n_heads + h) * head_dim;

            // Apply rotation to each pair of dimensions
            for (int i = 0; i < half_dim; i++) {
                float cos_val = cos_cache_[pos * half_dim + i];
                float sin_val = sin_cache_[pos * half_dim + i];

                float x0 = in_head[2 * i];
                float x1 = in_head[2 * i + 1];

                // Rotation matrix: [cos, -sin; sin, cos]
                out_head[2 * i] = x0 * cos_val - x1 * sin_val;
                out_head[2 * i + 1] = x0 * sin_val + x1 * cos_val;
            }
        }
    }
}

void RoPECache::apply_interleaved(float* qk, const int* positions, int seq_len, int n_heads, int head_dim) {
    // Apply RoPE to interleaved Q and K tensors
    // Layout: [seq_len, n_heads, 2, head_dim] where 2 = Q and K

    int half_dim = head_dim / 2;

    for (int s = 0; s < seq_len; s++) {
        int pos = positions[s];
        if (pos >= config_.max_seq_len) {
            pos = config_.max_seq_len - 1;
        }

        for (int h = 0; h < n_heads; h++) {
            for (int qk_idx = 0; qk_idx < 2; qk_idx++) {  // 0=Q, 1=K
                float* head_ptr = qk + ((s * n_heads + h) * 2 + qk_idx) * head_dim;

                for (int i = 0; i < half_dim; i++) {
                    float cos_val = cos_cache_[pos * half_dim + i];
                    float sin_val = sin_cache_[pos * half_dim + i];

                    float x0 = head_ptr[2 * i];
                    float x1 = head_ptr[2 * i + 1];

                    head_ptr[2 * i] = x0 * cos_val - x1 * sin_val;
                    head_ptr[2 * i + 1] = x0 * sin_val + x1 * cos_val;
                }
            }
        }
    }
}

// ============================================================================
// Activation Functions
// ============================================================================

void gelu(float* dst, const float* src, int n) {
    // GELU(x) = 0.5 * x * (1 + tanh(sqrt(2/pi) * (x + 0.044715 * x^3)))
    const float c1 = 0.7978845608f;  // sqrt(2/pi)
    const float c2 = 0.044715f;

    for (int i = 0; i < n; i++) {
        float x = src[i];
        float x3 = x * x * x;
        float inner = c1 * (x + c2 * x3);
        dst[i] = 0.5f * x * (1.0f + std::tanh(inner));
    }
}

void gelu_fast(float* dst, const float* src, int n) {
    // Fast GELU approximation: GELU(x) ≈ x * sigmoid(1.702 * x)
    const float alpha = 1.702f;

    for (int i = 0; i < n; i++) {
        float x = src[i];
        float sigmoid = 1.0f / (1.0f + std::exp(-alpha * x));
        dst[i] = x * sigmoid;
    }
}

void silu(float* dst, const float* src, int n) {
    // SiLU(x) = x * sigmoid(x) = x / (1 + exp(-x))
    for (int i = 0; i < n; i++) {
        float x = src[i];
        dst[i] = x / (1.0f + std::exp(-x));
    }
}

void relu(float* dst, const float* src, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = std::max(0.0f, src[i]);
    }
}

void softmax(float* dst, const float* src, int n) {
    // Numerically stable softmax: subtract max before exp
    float max_val = *std::max_element(src, src + n);

    float sum_exp = 0.0f;
    for (int i = 0; i < n; i++) {
        dst[i] = std::exp(src[i] - max_val);
        sum_exp += dst[i];
    }

    float inv_sum = 1.0f / sum_exp;
    for (int i = 0; i < n; i++) {
        dst[i] *= inv_sum;
    }
}

void softmax_temperature(float* dst, const float* src, int n, float temperature) {
    // Softmax with temperature scaling
    float max_val = *std::max_element(src, src + n);

    float sum_exp = 0.0f;
    for (int i = 0; i < n; i++) {
        dst[i] = std::exp((src[i] - max_val) / temperature);
        sum_exp += dst[i];
    }

    float inv_sum = 1.0f / sum_exp;
    for (int i = 0; i < n; i++) {
        dst[i] *= inv_sum;
    }
}

} // namespace TensorOps
