#ifndef KV_CACHE_H
#define KV_CACHE_H

#include <vector>
#include <cstdint>
#include <memory>

// KV-Cache: Key-Value Cache for Efficient Transformer Inference
// Algorithm adapted from: llama.cpp (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/llama.cpp
// Modifications: Removed dependencies, added GQA support, ring buffer optimization

namespace KVCache {

// ============================================================================
// KV-Cache Configuration
// ============================================================================

struct CacheConfig {
    int n_layers;           // Number of transformer layers
    int n_heads;            // Number of attention heads (query heads)
    int n_kv_heads;         // Number of KV heads (for GQA/MQA)
    int head_dim;           // Dimension per head
    int max_seq_len;        // Maximum sequence length (cache size)

    // Grouped-Query Attention (GQA) settings
    bool use_gqa;           // Enable GQA (share KV across heads)
    int n_heads_per_kv;     // Query heads per KV head (e.g., 4 Q heads → 1 KV head)

    // Memory optimization
    bool use_ring_buffer;   // Ring buffer mode (constant memory)
    int sliding_window;     // Sliding window size (0 = disabled, like Mistral)

    CacheConfig()
        : n_layers(0), n_heads(0), n_kv_heads(0), head_dim(0), max_seq_len(0),
          use_gqa(false), n_heads_per_kv(1), use_ring_buffer(false), sliding_window(0) {}
};

// ============================================================================
// KV-Cache Storage (Per Layer)
// ============================================================================

// Cache storage for one transformer layer
struct LayerCache {
    // Key cache: [max_seq_len, n_kv_heads, head_dim]
    std::vector<float> keys;

    // Value cache: [max_seq_len, n_kv_heads, head_dim]
    std::vector<float> values;

    // Position tracking
    int current_pos;        // Current write position (ring buffer)
    int filled_len;         // How many positions are filled

    // Sequence management
    std::vector<int> seq_ids;  // Sequence ID for each position (for batching)

    LayerCache() : current_pos(0), filled_len(0) {}
};

// ============================================================================
// KV-Cache Manager
// ============================================================================

class CacheManager {
public:
    CacheManager(const CacheConfig& config);

    // ========================================================================
    // Cache Operations
    // ========================================================================

    // Update cache with new K, V for a layer
    // Returns: position where K, V were stored
    int update(
        int layer_idx,           // Which layer
        const float* K,          // New keys [n_tokens, n_kv_heads, head_dim]
        const float* V,          // New values [n_tokens, n_kv_heads, head_dim]
        int n_tokens,            // Number of new tokens
        int seq_id = 0           // Sequence ID (for batching)
    );

    // Get cached keys for a layer
    // Returns pointer to cached keys: [filled_len, n_kv_heads, head_dim]
    const float* get_keys(int layer_idx, int& out_len) const;

    // Get cached values for a layer
    const float* get_values(int layer_idx, int& out_len) const;

    // Get cache at specific position range [start, end)
    void get_range(
        int layer_idx,
        int start_pos,
        int end_pos,
        float* out_keys,
        float* out_values
    ) const;

    // ========================================================================
    // Grouped-Query Attention (GQA) Support
    // ========================================================================

    // Expand KV heads to match query heads (for GQA)
    // Input: K[n_kv_heads, head_dim]
    // Output: K_expanded[n_heads, head_dim]
    void expand_kv_heads(
        const float* K_compact,   // Input: [n_kv_heads, head_dim]
        float* K_expanded,        // Output: [n_heads, head_dim]
        int n_tokens
    ) const;

    // ========================================================================
    // Cache Management
    // ========================================================================

    // Clear cache for a specific layer
    void clear_layer(int layer_idx);

    // Clear all cache
    void clear_all();

    // Defragment cache (compact ring buffer)
    void defragment(int layer_idx);

    // Remove old tokens (sliding window)
    void apply_sliding_window(int layer_idx);

    // ========================================================================
    // Sequence Management (Batching)
    // ========================================================================

    // Remove sequence from cache
    void remove_sequence(int seq_id);

    // Copy cache from one sequence to another (for beam search)
    void copy_sequence(int src_seq_id, int dst_seq_id);

    // Shift cache positions (for prefix caching)
    void shift_positions(int layer_idx, int start_pos, int shift_amount);

    // ========================================================================
    // Statistics and Info
    // ========================================================================

    // Get cache usage statistics
    struct CacheStats {
        size_t total_bytes;      // Total memory used
        size_t used_bytes;       // Actually filled memory
        float utilization;       // Percentage used
        int max_position;        // Furthest position filled
    };

    CacheStats get_stats() const;

    // Get configuration
    const CacheConfig& get_config() const { return config_; }

    // Get current position for layer
    int get_position(int layer_idx) const;

private:
    CacheConfig config_;
    std::vector<LayerCache> layers_;

    // Helper: Get storage index for ring buffer
    int get_storage_index(int logical_pos) const;

    // Helper: Compute cache size for one layer
    size_t compute_layer_size() const;
};

// ============================================================================
// Attention with KV-Cache (Optimized)
// ============================================================================

// Compute attention using cached K, V
// Optimized for incremental generation (only 1 new token)
void attention_with_cache(
    const float* Q,              // Query: [n_new_tokens, n_heads, head_dim]
    const CacheManager& cache,   // KV-Cache
    int layer_idx,               // Which layer
    float* output,               // Output: [n_new_tokens, n_heads, head_dim]
    int n_new_tokens,            // Number of new tokens (usually 1)
    bool causal_mask = true      // Apply causal masking
);

// Compute attention with GQA (Grouped-Query Attention)
void attention_with_gqa(
    const float* Q,              // Query: [n_tokens, n_heads, head_dim]
    const float* K,              // Key: [n_tokens, n_kv_heads, head_dim]
    const float* V,              // Value: [n_tokens, n_kv_heads, head_dim]
    float* output,               // Output: [n_tokens, n_heads, head_dim]
    int n_tokens,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    bool causal_mask = true
);

// ============================================================================
// Multi-Query Attention (MQA) Helpers
// ============================================================================

// MQA: Special case of GQA where n_kv_heads = 1
// Used in: GPT-J, Falcon
inline bool is_mqa(const CacheConfig& config) {
    return config.use_gqa && config.n_kv_heads == 1;
}

// Check if using standard multi-head attention (MHA)
inline bool is_mha(const CacheConfig& config) {
    return !config.use_gqa || config.n_kv_heads == config.n_heads;
}

// ============================================================================
// Sliding Window Attention (Mistral-style)
// ============================================================================

// Compute attention with sliding window
// Only attends to last N tokens (sliding_window)
void attention_sliding_window(
    const float* Q,
    const CacheManager& cache,
    int layer_idx,
    float* output,
    int n_new_tokens,
    int window_size            // Sliding window size (e.g., 4096)
);

} // namespace KVCache

#endif // KV_CACHE_H
