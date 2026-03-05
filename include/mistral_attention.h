#ifndef MISTRAL_ATTENTION_H
#define MISTRAL_ATTENTION_H

#include <vector>
#include <cstdint>
#include "kv_cache.h"
#include "flash_attention.h"

// Mistral-7B Style Sliding Window Attention
// Algorithm adapted from: Mistral-src (Apache 2.0 License)
// Original: https://github.com/mistralai/mistral-src
// Paper: https://arxiv.org/abs/2310.06825
// Modifications: CPU implementation, removed HuggingFace dependencies

namespace MistralAttention {

// ============================================================================
// Mistral Configuration
// ============================================================================

struct MistralConfig {
    // Model architecture
    int n_layers;              // Number of transformer layers (e.g., 32)
    int n_heads;               // Number of query heads (e.g., 32)
    int n_kv_heads;            // Number of KV heads for GQA (e.g., 8)
    int head_dim;              // Dimension per head (e.g., 128)
    int hidden_dim;            // Hidden dimension (e.g., 4096)
    int intermediate_size;     // Feed-forward intermediate size (e.g., 14336)

    // Sliding window attention
    int sliding_window;        // Window size (e.g., 4096)
    bool use_flash_attention;  // Use Flash Attention for efficiency

    // Vocabulary
    int vocab_size;            // Vocabulary size (e.g., 32000)
    int max_position;          // Maximum position (e.g., 32768)

    // Rope settings
    float rope_theta;          // RoPE base frequency (e.g., 10000.0)

    MistralConfig()
        : n_layers(32), n_heads(32), n_kv_heads(8), head_dim(128),
          hidden_dim(4096), intermediate_size(14336),
          sliding_window(4096), use_flash_attention(true),
          vocab_size(32000), max_position(32768), rope_theta(10000.0f) {}
};

// ============================================================================
// Sliding Window Attention Layer
// ============================================================================

class SlidingWindowAttention {
public:
    SlidingWindowAttention(const MistralConfig& config);

    // Forward pass with sliding window
    // Input: [seq_len, hidden_dim]
    // Output: [seq_len, hidden_dim]
    void forward(
        const float* input,        // Input hidden states
        float* output,             // Output hidden states
        KVCache::CacheManager& kv_cache,  // KV cache
        int layer_idx,             // Which layer
        int seq_len,               // Sequence length
        const int* positions       // Token positions (for RoPE)
    );

    // Forward pass without cache (training mode)
    void forward_no_cache(
        const float* input,
        float* output,
        int seq_len,
        const int* positions
    );

private:
    MistralConfig config_;

    // Weight matrices (not allocated here, just interface)
    // In practice, these would be loaded from model weights
};

// ============================================================================
// Mistral Block (Full Transformer Layer)
// ============================================================================

class MistralBlock {
public:
    MistralBlock(const MistralConfig& config, int layer_idx);

    // Forward pass through full Mistral block
    // Includes: RMSNorm → Attention → RMSNorm → FFN
    void forward(
        const float* input,
        float* output,
        KVCache::CacheManager& kv_cache,
        int seq_len,
        const int* positions
    );

private:
    MistralConfig config_;
    int layer_idx_;
    SlidingWindowAttention attention_;
};

// ============================================================================
// Mistral Model (Full Architecture)
// ============================================================================

class MistralModel {
public:
    MistralModel(const MistralConfig& config);

    // Forward pass through full model
    void forward(
        const int* input_ids,      // Token IDs [seq_len]
        float* output,             // Output logits [seq_len, vocab_size]
        int seq_len
    );

    // Generate next token (inference mode)
    int generate_next_token(
        const int* input_ids,
        int seq_len,
        float temperature = 1.0f,
        int top_k = 50
    );

    // Get KV cache
    KVCache::CacheManager& get_cache() { return kv_cache_; }

private:
    MistralConfig config_;
    KVCache::CacheManager kv_cache_;
    std::vector<MistralBlock> blocks_;
};

// ============================================================================
// Hybrid Attention Strategies
// ============================================================================

// Attention strategy: When to use sliding window vs full attention
enum class AttentionStrategy {
    ALWAYS_SLIDING,     // Always use sliding window (Mistral default)
    FULL_THEN_SLIDING,  // Full attention up to window, then sliding
    ADAPTIVE            // Adaptive based on position
};

// Compute attention with strategy
void compute_attention_with_strategy(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    int window_size,
    AttentionStrategy strategy,
    int current_position  // For adaptive strategy
);

// ============================================================================
// Rolling Buffer Cache (Mistral-specific)
// ============================================================================

// Rolling buffer: Constant memory regardless of sequence length
// Keeps only last window_size tokens in cache
class RollingBufferCache {
public:
    RollingBufferCache(int window_size, int n_layers, int n_kv_heads, int head_dim);

    // Update cache with new K, V (rolls out old tokens)
    void update(
        int layer_idx,
        const float* K_new,  // New keys [1, n_kv_heads, head_dim]
        const float* V_new,  // New values [1, n_kv_heads, head_dim]
        int position         // Global position
    );

    // Get cached K, V for attention computation
    void get_kv(
        int layer_idx,
        float* K_out,        // Output: [window_size, n_kv_heads, head_dim]
        float* V_out,
        int& valid_len       // How many positions are valid
    ) const;

    // Clear cache
    void clear();

private:
    int window_size_;
    int n_layers_;
    int n_kv_heads_;
    int head_dim_;

    std::vector<std::vector<float>> keys_;   // [n_layers][window * n_kv_heads * head_dim]
    std::vector<std::vector<float>> values_;
    std::vector<int> positions_;             // [n_layers] - current write position
    std::vector<int> filled_;                // [n_layers] - how many positions filled
};

// ============================================================================
// Utilities
// ============================================================================

// Compute memory savings of sliding window vs full attention
struct SlidingWindowStats {
    size_t full_attention_memory;      // O(N²) for full attention
    size_t sliding_window_memory;      // O(N × W) for sliding window
    float memory_reduction;
    int effective_context;             // How much context actually used
};

SlidingWindowStats compute_sliding_window_stats(
    int seq_len,
    int window_size,
    int n_heads,
    int head_dim
);

// Determine if a query position should attend to a key position
// given sliding window constraint
inline bool should_attend(int q_pos, int k_pos, int window_size, bool causal = true) {
    if (causal && k_pos > q_pos) {
        return false;  // Can't attend to future
    }

    // Sliding window: only attend to last window_size tokens
    return (q_pos - k_pos) < window_size;
}

// Create attention mask for sliding window
// Output: [seq_len, seq_len] boolean mask
void create_sliding_window_mask(
    bool* mask,
    int seq_len,
    int window_size,
    bool causal = true
);

} // namespace MistralAttention

#endif // MISTRAL_ATTENTION_H
