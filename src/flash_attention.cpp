// Flash Attention v2: Memory-Efficient Attention
// Algorithm adapted from: Flash Attention paper (BSD-3 License)
// Original: https://github.com/Dao-AILab/flash-attention
// Paper: https://arxiv.org/abs/2307.08691
// Modifications: CPU-only implementation, removed CUDA dependencies

#include "flash_attention.h"
#include "tensor_ops.h"
#include <cmath>
#include <algorithm>
#include <limits>
#include <iostream>

namespace FlashAttention {

// ============================================================================
// Helper Functions
// ============================================================================

BlockSizeConfig compute_optimal_block_size(int seq_len, int head_dim, size_t l2_cache_size) {
    // Estimate memory per block:
    // Q_block: Bq × head_dim
    // K_block: Bkv × head_dim
    // V_block: Bkv × head_dim
    // O_block: Bq × head_dim
    // Scores: Bq × Bkv
    // Total: (2*Bq + 2*Bkv) * head_dim + Bq * Bkv floats

    BlockSizeConfig config;

    // Try common block sizes
    std::vector<int> candidates = {32, 64, 128, 256};

    for (int bq : candidates) {
        for (int bkv : candidates) {
            size_t mem = (2 * bq + 2 * bkv) * head_dim * sizeof(float);
            mem += bq * bkv * sizeof(float);

            if (mem <= l2_cache_size) {
                config.block_q = bq;
                config.block_kv = bkv;
            }
        }
    }

    // Fallback if nothing fits
    if (config.block_q == 0) {
        config.block_q = 32;
        config.block_kv = 32;
    }

    return config;
}

MemoryStats compute_memory_stats(int batch, int seq_len, int n_heads, int head_dim) {
    MemoryStats stats;

    // Standard attention: O(N²)
    // Stores full attention matrix: [batch, n_heads, seq_len, seq_len]
    stats.standard_attention = batch * n_heads * seq_len * seq_len * sizeof(float);

    // Flash attention: O(N)
    // Only stores: Q, K, V, O + small working buffers
    size_t qkvo = batch * seq_len * n_heads * head_dim * sizeof(float) * 4;
    size_t working = seq_len * 2 * sizeof(float);  // Softmax state
    stats.flash_attention = qkvo + working;

    stats.reduction_factor = (float)stats.standard_attention / stats.flash_attention;

    return stats;
}

// ============================================================================
// Tiled Attention Block (Core Algorithm)
// ============================================================================

void tiled_attention_block(
    const float* Q_block,
    const float* K_block,
    const float* V_block,
    float* O_block,
    float* softmax_state,  // [Bq, 2]: [max_val, sum_exp]
    int Bq,
    int Bkv,
    int head_dim,
    float scale,
    bool causal_mask,
    int q_offset,
    int kv_offset
) {
    // Compute attention scores: S = Q @ K^T
    std::vector<float> scores(Bq * Bkv);

    for (int i = 0; i < Bq; i++) {
        for (int j = 0; j < Bkv; j++) {
            const float* q = Q_block + i * head_dim;
            const float* k = K_block + j * head_dim;

            float score = TensorOps::vec_dot(q, k, head_dim) * scale;

            // Causal masking
            if (causal_mask) {
                int q_pos = q_offset + i;
                int k_pos = kv_offset + j;
                if (k_pos > q_pos) {
                    score = -1e9f;  // Mask future positions
                }
            }

            scores[i * Bkv + j] = score;
        }
    }

    // Online softmax update
    for (int i = 0; i < Bq; i++) {
        float old_max = softmax_state[i * 2];
        float old_sum = softmax_state[i * 2 + 1];

        // Find new max in this block
        float new_max = old_max;
        for (int j = 0; j < Bkv; j++) {
            new_max = std::max(new_max, scores[i * Bkv + j]);
        }

        // Rescale old output and sum if max changed
        float rescale = std::exp(old_max - new_max);
        if (old_max > -1e8f && new_max != old_max) {
            // Rescale previous output
            for (int d = 0; d < head_dim; d++) {
                O_block[i * head_dim + d] *= rescale;
            }
            old_sum *= rescale;
        }

        // Compute exp and accumulate
        float new_sum = old_sum;
        std::vector<float> exp_scores(Bkv);
        for (int j = 0; j < Bkv; j++) {
            exp_scores[j] = std::exp(scores[i * Bkv + j] - new_max);
            new_sum += exp_scores[j];
        }

        // Update output: O += exp(S - max) @ V
        for (int j = 0; j < Bkv; j++) {
            const float* v = V_block + j * head_dim;
            float weight = exp_scores[j];
            for (int d = 0; d < head_dim; d++) {
                O_block[i * head_dim + d] += weight * v[d];
            }
        }

        // Update softmax state
        softmax_state[i * 2] = new_max;
        softmax_state[i * 2 + 1] = new_sum;
    }
}

// ============================================================================
// Flash Attention Forward Pass
// ============================================================================

void flash_attention_forward_single(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
) {
    // Auto-compute scale if not set
    float scale = config.softmax_scale;
    if (scale == 0.0f) {
        scale = 1.0f / std::sqrt((float)head_dim);
    }

    int Bq = config.block_size_q;
    int Bkv = config.block_size_kv;

    // Process each head independently
    for (int h = 0; h < n_heads; h++) {
        // Initialize output to zero
        std::fill_n(O + h * seq_len * head_dim, seq_len * head_dim, 0.0f);

        // Softmax state for each query position
        std::vector<float> softmax_state(seq_len * 2, 0.0f);
        for (int i = 0; i < seq_len; i++) {
            softmax_state[i * 2] = -1e9f;  // max_val
            softmax_state[i * 2 + 1] = 0.0f;  // sum_exp
        }

        // Outer loop: query blocks
        for (int q_start = 0; q_start < seq_len; q_start += Bq) {
            int q_end = std::min(q_start + Bq, seq_len);
            int q_size = q_end - q_start;

            // Inner loop: key/value blocks
            for (int kv_start = 0; kv_start < seq_len; kv_start += Bkv) {
                int kv_end = std::min(kv_start + Bkv, seq_len);
                int kv_size = kv_end - kv_start;

                // Skip blocks that are entirely masked
                if (config.use_causal_mask && kv_start > q_end) {
                    continue;
                }

                // Extract blocks
                const float* Q_block = Q + (h * seq_len + q_start) * head_dim;
                const float* K_block = K + (h * seq_len + kv_start) * head_dim;
                const float* V_block = V + (h * seq_len + kv_start) * head_dim;
                float* O_block = O + (h * seq_len + q_start) * head_dim;
                float* state_block = softmax_state.data() + q_start * 2;

                // Process this block
                tiled_attention_block(
                    Q_block, K_block, V_block, O_block, state_block,
                    q_size, kv_size, head_dim, scale,
                    config.use_causal_mask, q_start, kv_start
                );
            }
        }

        // Final normalization
        for (int i = 0; i < seq_len; i++) {
            float sum = softmax_state[i * 2 + 1];
            if (sum > 1e-8f) {
                for (int d = 0; d < head_dim; d++) {
                    O[(h * seq_len + i) * head_dim + d] /= sum;
                }
            }
        }
    }
}

void flash_attention_forward(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int batch,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
) {
    // Process each batch independently
    size_t batch_size = seq_len * n_heads * head_dim;

    for (int b = 0; b < batch; b++) {
        flash_attention_forward_single(
            Q + b * batch_size,
            K + b * batch_size,
            V + b * batch_size,
            O + b * batch_size,
            seq_len, n_heads, head_dim, config
        );
    }
}

// ============================================================================
// Flash Attention with GQA
// ============================================================================

void flash_attention_gqa(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    const FlashConfig& config
) {
    float scale = config.softmax_scale;
    if (scale == 0.0f) {
        scale = 1.0f / std::sqrt((float)head_dim);
    }

    int n_heads_per_kv = n_heads / n_kv_heads;
    int Bq = config.block_size_q;
    int Bkv = config.block_size_kv;

    // Process each head
    for (int h = 0; h < n_heads; h++) {
        int kv_head = h / n_heads_per_kv;  // Which KV head to use

        std::fill_n(O + h * seq_len * head_dim, seq_len * head_dim, 0.0f);

        std::vector<float> softmax_state(seq_len * 2, 0.0f);
        for (int i = 0; i < seq_len; i++) {
            softmax_state[i * 2] = -1e9f;
            softmax_state[i * 2 + 1] = 0.0f;
        }

        for (int q_start = 0; q_start < seq_len; q_start += Bq) {
            int q_end = std::min(q_start + Bq, seq_len);
            int q_size = q_end - q_start;

            for (int kv_start = 0; kv_start < seq_len; kv_start += Bkv) {
                int kv_end = std::min(kv_start + Bkv, seq_len);
                int kv_size = kv_end - kv_start;

                if (config.use_causal_mask && kv_start > q_end) {
                    continue;
                }

                const float* Q_block = Q + (h * seq_len + q_start) * head_dim;
                const float* K_block = K + (kv_head * seq_len + kv_start) * head_dim;
                const float* V_block = V + (kv_head * seq_len + kv_start) * head_dim;
                float* O_block = O + (h * seq_len + q_start) * head_dim;
                float* state_block = softmax_state.data() + q_start * 2;

                tiled_attention_block(
                    Q_block, K_block, V_block, O_block, state_block,
                    q_size, kv_size, head_dim, scale,
                    config.use_causal_mask, q_start, kv_start
                );
            }
        }

        // Final normalization
        for (int i = 0; i < seq_len; i++) {
            float sum = softmax_state[i * 2 + 1];
            if (sum > 1e-8f) {
                for (int d = 0; d < head_dim; d++) {
                    O[(h * seq_len + i) * head_dim + d] /= sum;
                }
            }
        }
    }
}

// ============================================================================
// Sliding Window Flash Attention
// ============================================================================

void flash_attention_sliding_window(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    int window_size,
    const FlashConfig& config
) {
    float scale = config.softmax_scale;
    if (scale == 0.0f) {
        scale = 1.0f / std::sqrt((float)head_dim);
    }

    int Bq = config.block_size_q;
    int Bkv = config.block_size_kv;

    for (int h = 0; h < n_heads; h++) {
        std::fill_n(O + h * seq_len * head_dim, seq_len * head_dim, 0.0f);

        std::vector<float> softmax_state(seq_len * 2, 0.0f);
        for (int i = 0; i < seq_len; i++) {
            softmax_state[i * 2] = -1e9f;
            softmax_state[i * 2 + 1] = 0.0f;
        }

        for (int q_start = 0; q_start < seq_len; q_start += Bq) {
            int q_end = std::min(q_start + Bq, seq_len);
            int q_size = q_end - q_start;

            // Only attend to window
            int kv_min = std::max(0, q_start - window_size);
            int kv_max = std::min(seq_len, q_end + 1);

            for (int kv_start = kv_min; kv_start < kv_max; kv_start += Bkv) {
                int kv_end = std::min(kv_start + Bkv, kv_max);
                int kv_size = kv_end - kv_start;

                const float* Q_block = Q + (h * seq_len + q_start) * head_dim;
                const float* K_block = K + (h * seq_len + kv_start) * head_dim;
                const float* V_block = V + (h * seq_len + kv_start) * head_dim;
                float* O_block = O + (h * seq_len + q_start) * head_dim;
                float* state_block = softmax_state.data() + q_start * 2;

                tiled_attention_block(
                    Q_block, K_block, V_block, O_block, state_block,
                    q_size, kv_size, head_dim, scale,
                    config.use_causal_mask, q_start, kv_start
                );
            }
        }

        // Final normalization
        for (int i = 0; i < seq_len; i++) {
            float sum = softmax_state[i * 2 + 1];
            if (sum > 1e-8f) {
                for (int d = 0; d < head_dim; d++) {
                    O[(h * seq_len + i) * head_dim + d] /= sum;
                }
            }
        }
    }
}

// Placeholder for masked attention (TODO: implement if needed)
void flash_attention_masked(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    const bool* mask,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
) {
    // TODO: Implement block-sparse attention with custom mask
    // For now, fall back to standard flash attention
    flash_attention_forward_single(Q, K, V, O, seq_len, n_heads, head_dim, config);
}

// Placeholder for backward pass (TODO: implement for training)
void flash_attention_backward(
    const float* grad_O,
    const float* Q,
    const float* K,
    const float* V,
    float* grad_Q,
    float* grad_K,
    float* grad_V,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
) {
    // TODO: Implement Flash Attention backward pass
    // Uses recomputation to avoid storing full attention matrix
    std::cerr << "[FLASH_ATTENTION] Backward pass not yet implemented" << std::endl;
}

} // namespace FlashAttention
