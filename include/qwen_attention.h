#ifndef QWEN_ATTENTION_H
#define QWEN_ATTENTION_H

#include <vector>
#include <cstdint>
#include <string>
#include "flash_attention.h"
#include "kv_cache.h"

// Qwen2.5 Style Dual Attention (Local + Global)
// Algorithm adapted from: Qwen/Qwen2.5 (Apache 2.0 License)
// Original: https://github.com/QwenLM/Qwen2.5
// Paper: Qwen2.5 Technical Report
// Modifications: CPU implementation, removed HuggingFace dependencies

namespace QwenAttention {

// ============================================================================
// Qwen Configuration
// ============================================================================

struct QwenConfig {
    // Model architecture
    int n_layers;              // Total layers (e.g., 28)
    int n_heads;               // Number of query heads (e.g., 28)
    int n_kv_heads;            // Number of KV heads for GQA (e.g., 4)
    int head_dim;              // Dimension per head (e.g., 128)
    int hidden_dim;            // Hidden dimension (e.g., 3584)
    int intermediate_size;     // Feed-forward size (e.g., 18944)

    // Dual attention configuration
    int local_window_size;     // Local attention window (e.g., 512)
    int global_start_layer;    // Where to switch to global (e.g., layer 6)
    bool use_flash_attention;  // Use Flash Attention

    // Rope settings
    float rope_theta;          // RoPE base frequency (e.g., 10000.0)

    // Vocabulary (Qwen supports 150K+ multilingual vocab)
    int vocab_size;            // Vocabulary size (e.g., 151936)
    int max_position;          // Maximum position (e.g., 32768)

    QwenConfig()
        : n_layers(28), n_heads(28), n_kv_heads(4), head_dim(128),
          hidden_dim(3584), intermediate_size(18944),
          local_window_size(512), global_start_layer(14),
          use_flash_attention(true), rope_theta(10000.0f),
          vocab_size(151936), max_position(32768) {}
};

// ============================================================================
// Attention Type per Layer
// ============================================================================

enum class AttentionType {
    LOCAL,      // Local window attention (fast, bounded context)
    GLOBAL,     // Full attention (quality, full context)
    HYBRID      // Mix of local and global
};

// Get attention type for a specific layer
inline AttentionType get_attention_type(int layer_idx, const QwenConfig& config) {
    if (layer_idx < config.global_start_layer) {
        return AttentionType::LOCAL;
    } else {
        return AttentionType::GLOBAL;
    }
}

// ============================================================================
// Dual Attention Layer
// ============================================================================

class DualAttentionLayer {
public:
    DualAttentionLayer(const QwenConfig& config, int layer_idx);

    // Forward pass (automatically chooses local vs global)
    void forward(
        const float* input,        // Input: [seq_len, hidden_dim]
        float* output,             // Output: [seq_len, hidden_dim]
        KVCache::CacheManager& kv_cache,
        int seq_len,
        const int* positions
    );

    // Get attention type for this layer
    AttentionType get_type() const { return attention_type_; }

private:
    QwenConfig config_;
    int layer_idx_;
    AttentionType attention_type_;

    // Local attention (fast, windowed)
    void forward_local(
        const float* input,
        float* output,
        KVCache::CacheManager& kv_cache,
        int seq_len,
        const int* positions
    );

    // Global attention (quality, full context)
    void forward_global(
        const float* input,
        float* output,
        KVCache::CacheManager& kv_cache,
        int seq_len,
        const int* positions
    );
};

// ============================================================================
// Hybrid Attention Strategies
// ============================================================================

// Layer-wise attention pattern
struct LayerAttentionPattern {
    std::vector<AttentionType> layer_types;  // Attention type per layer

    LayerAttentionPattern(int n_layers, int global_start_layer) {
        layer_types.resize(n_layers);
        for (int i = 0; i < n_layers; i++) {
            layer_types[i] = (i < global_start_layer) ? AttentionType::LOCAL : AttentionType::GLOBAL;
        }
    }
};

// Custom attention pattern (for experimentation)
class CustomAttentionPattern {
public:
    // Alternating pattern: L, L, G, L, L, G, ...
    static LayerAttentionPattern alternating(int n_layers, int local_per_global = 2);

    // Progressive: More global as layers increase
    static LayerAttentionPattern progressive(int n_layers, float global_ratio);

    // Sandwich: G, L, L, ..., L, L, G (global at ends)
    static LayerAttentionPattern sandwich(int n_layers);

    // Qwen default: Local first half, global second half
    static LayerAttentionPattern qwen_default(int n_layers);
};

// ============================================================================
// Adaptive Attention (Dynamic)
// ============================================================================

// Dynamically choose attention type based on:
// - Sequence length
// - Layer depth
// - Computational budget
class AdaptiveAttention {
public:
    AdaptiveAttention(const QwenConfig& config);

    // Determine attention type dynamically
    AttentionType choose_attention_type(
        int layer_idx,
        int seq_len,
        int available_memory_mb
    ) const;

private:
    QwenConfig config_;

    // Heuristics
    bool should_use_local(int seq_len, int memory_mb) const;
    bool layer_needs_global(int layer_idx) const;
};

// ============================================================================
// Memory-Efficient Dual Attention
// ============================================================================

// Compute dual attention with Flash Attention + KV-cache
void compute_dual_attention(
    const float* Q,              // Query: [seq_len, n_heads, head_dim]
    const float* K,              // Key: [seq_len, n_kv_heads, head_dim]
    const float* V,              // Value: [seq_len, n_kv_heads, head_dim]
    float* O,                    // Output: [seq_len, n_heads, head_dim]
    int seq_len,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    AttentionType attn_type,
    int window_size,
    bool use_flash
);

// ============================================================================
// Statistics and Analysis
// ============================================================================

// Analyze memory and compute trade-offs
struct DualAttentionStats {
    size_t local_memory_mb;
    size_t global_memory_mb;
    float local_compute_gflops;
    float global_compute_gflops;
    float memory_savings_ratio;
    float compute_savings_ratio;
};

DualAttentionStats analyze_dual_attention(
    const QwenConfig& config,
    const LayerAttentionPattern& pattern,
    int seq_len
);

// Estimate quality impact of local vs global
struct QualityEstimate {
    float local_quality_score;    // 0.0-1.0
    float global_quality_score;   // 0.0-1.0
    float hybrid_quality_score;   // 0.0-1.0
    std::string recommendation;
};

QualityEstimate estimate_quality_impact(
    int n_local_layers,
    int n_global_layers,
    int window_size,
    int seq_len
);

// ============================================================================
// Qwen Model Interface
// ============================================================================

class QwenModel {
public:
    QwenModel(const QwenConfig& config);

    // Forward pass through all layers
    void forward(
        const int* input_ids,
        float* output,
        int seq_len
    );

    // Get layer attention types
    const LayerAttentionPattern& get_pattern() const { return pattern_; }

    // Get KV cache
    KVCache::CacheManager& get_cache() { return kv_cache_; }

private:
    QwenConfig config_;
    LayerAttentionPattern pattern_;
    KVCache::CacheManager kv_cache_;
    std::vector<DualAttentionLayer> layers_;
};

// ============================================================================
// Utilities
// ============================================================================

// Print attention pattern visualization
void print_attention_pattern(const LayerAttentionPattern& pattern);

// Example output:
// Layer  0: [LOCAL    ] (window=512)
// Layer  1: [LOCAL    ] (window=512)
// ...
// Layer 14: [GLOBAL   ] (full context)
// Layer 15: [GLOBAL   ] (full context)

} // namespace QwenAttention

#endif // QWEN_ATTENTION_H
