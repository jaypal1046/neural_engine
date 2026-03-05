#ifndef FLASH_ATTENTION_H
#define FLASH_ATTENTION_H

#include <vector>
#include <cstdint>
#include <cmath>

// Flash Attention v2: Memory-Efficient Attention
// Algorithm adapted from: Flash Attention paper (BSD-3 License)
// Original: https://github.com/Dao-AILab/flash-attention
// Paper: https://arxiv.org/abs/2307.08691 (Flash Attention v2)
// Modifications: CPU-only implementation, removed CUDA/Triton dependencies

namespace FlashAttention {

// ============================================================================
// Flash Attention Configuration
// ============================================================================

struct FlashConfig {
    int block_size_q;      // Query block size (e.g., 64, 128)
    int block_size_kv;     // Key/Value block size (e.g., 64, 128)
    bool use_causal_mask;  // Apply causal masking (autoregressive)
    float softmax_scale;   // Scale factor (1/sqrt(d_k))

    // Advanced options
    bool use_online_softmax;  // Online softmax computation (default: true)
    bool use_recompute;       // Recompute in backward (vs cache activations)

    FlashConfig()
        : block_size_q(64), block_size_kv(64), use_causal_mask(true),
          softmax_scale(0.0f), use_online_softmax(true), use_recompute(false) {}
};

// ============================================================================
// Flash Attention Forward Pass
// ============================================================================

// Flash Attention v2 forward pass (memory-efficient)
// Input:
//   Q: [batch, seq_len, n_heads, head_dim]
//   K: [batch, seq_len, n_heads, head_dim]
//   V: [batch, seq_len, n_heads, head_dim]
// Output:
//   O: [batch, seq_len, n_heads, head_dim]
//
// Memory: O(N) instead of O(N²)
// Speed: 2-4x faster than standard attention on long sequences
void flash_attention_forward(
    const float* Q,           // Query
    const float* K,           // Key
    const float* V,           // Value
    float* O,                 // Output
    int batch,                // Batch size
    int seq_len,              // Sequence length
    int n_heads,              // Number of heads
    int head_dim,             // Head dimension
    const FlashConfig& config
);

// Flash Attention for single batch (simplified API)
void flash_attention_forward_single(
    const float* Q,           // [seq_len, n_heads, head_dim]
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
);

// ============================================================================
// Flash Attention with Grouped-Query Attention (GQA)
// ============================================================================

// Flash Attention + GQA
// Q has n_heads, K/V have n_kv_heads (where n_kv_heads < n_heads)
void flash_attention_gqa(
    const float* Q,           // [seq_len, n_heads, head_dim]
    const float* K,           // [seq_len, n_kv_heads, head_dim]
    const float* V,           // [seq_len, n_kv_heads, head_dim]
    float* O,                 // [seq_len, n_heads, head_dim]
    int seq_len,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    const FlashConfig& config
);

// ============================================================================
// Online Softmax (Core Algorithm)
// ============================================================================

// Online softmax: compute softmax without materializing full attention matrix
// Key innovation of Flash Attention
struct OnlineSoftmaxState {
    float max_val;     // Running maximum
    float sum_exp;     // Running sum of exp(x - max)

    OnlineSoftmaxState() : max_val(-1e9f), sum_exp(0.0f) {}

    // Update state with new value
    void update(float x) {
        if (x > max_val) {
            // New maximum: rescale previous sum
            float old_max = max_val;
            max_val = x;
            sum_exp = sum_exp * std::exp(old_max - max_val) + std::exp(x - max_val);
        } else {
            sum_exp += std::exp(x - max_val);
        }
    }

    // Get normalized probability
    float normalize(float x) const {
        return std::exp(x - max_val) / sum_exp;
    }
};

// ============================================================================
// Tiling for Cache Efficiency
// ============================================================================

// Tile-based attention computation
// Processes attention in small blocks that fit in cache
// Key to Flash Attention's speed
void tiled_attention_block(
    const float* Q_block,     // Query block [Bq, head_dim]
    const float* K_block,     // Key block [Bkv, head_dim]
    const float* V_block,     // Value block [Bkv, head_dim]
    float* O_block,           // Output block [Bq, head_dim]
    float* softmax_state,     // Online softmax state [Bq, 2] (max, sum)
    int Bq,                   // Query block size
    int Bkv,                  // KV block size
    int head_dim,
    float scale,
    bool causal_mask,
    int q_offset,             // Global Q offset (for causal masking)
    int kv_offset             // Global KV offset
);

// ============================================================================
// Flash Attention Variants
// ============================================================================

// Flash Attention with sliding window (Mistral-style)
void flash_attention_sliding_window(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    int window_size,          // Sliding window size
    const FlashConfig& config
);

// Flash Attention with block-sparse mask
// mask: [seq_len, seq_len] boolean mask
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
);

// ============================================================================
// Utilities
// ============================================================================

// Compute optimal block size based on available cache
struct BlockSizeConfig {
    int block_q;
    int block_kv;
};

BlockSizeConfig compute_optimal_block_size(
    int seq_len,
    int head_dim,
    size_t l2_cache_size = 256 * 1024  // 256 KB typical L2 cache
);

// Memory usage comparison
struct MemoryStats {
    size_t standard_attention;   // O(N²) memory
    size_t flash_attention;      // O(N) memory
    float reduction_factor;
};

MemoryStats compute_memory_stats(int batch, int seq_len, int n_heads, int head_dim);

// ============================================================================
// Backward Pass (for training)
// ============================================================================

// Flash Attention backward pass
// Uses recomputation to avoid storing full attention matrix
void flash_attention_backward(
    const float* grad_O,      // Gradient w.r.t. output
    const float* Q,
    const float* K,
    const float* V,
    float* grad_Q,            // Output: gradient w.r.t. Q
    float* grad_K,            // Output: gradient w.r.t. K
    float* grad_V,            // Output: gradient w.r.t. V
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
);

} // namespace FlashAttention

#endif // FLASH_ATTENTION_H
