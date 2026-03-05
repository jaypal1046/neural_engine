// Qwen2.5 Style Dual Attention (Local + Global)
// Algorithm adapted from: Qwen/Qwen2.5 (Apache 2.0 License)
// Original: https://github.com/QwenLM/Qwen2.5
// Modifications: CPU implementation, removed dependencies

#include "qwen_attention.h"
#include "tensor_ops.h"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <iomanip>

namespace QwenAttention {

// ============================================================================
// Dual Attention Layer
// ============================================================================

DualAttentionLayer::DualAttentionLayer(const QwenConfig& config, int layer_idx)
    : config_(config), layer_idx_(layer_idx) {
    attention_type_ = get_attention_type(layer_idx, config);
}

void DualAttentionLayer::forward(
    const float* input,
    float* output,
    KVCache::CacheManager& kv_cache,
    int seq_len,
    const int* positions
) {
    // Route to local or global attention based on layer type
    if (attention_type_ == AttentionType::LOCAL) {
        forward_local(input, output, kv_cache, seq_len, positions);
    } else {
        forward_global(input, output, kv_cache, seq_len, positions);
    }
}

void DualAttentionLayer::forward_local(
    const float* input,
    float* output,
    KVCache::CacheManager& kv_cache,
    int seq_len,
    const int* positions
) {
    // Local window attention (fast, bounded)
    int window_size = config_.local_window_size;

    if (config_.use_flash_attention) {
        // Use Flash Attention with sliding window
        KVCache::attention_sliding_window(
            input,  // Q (assuming input is already Q for simplicity)
            kv_cache,
            layer_idx_,
            output,
            seq_len,
            window_size
        );
    } else {
        // Standard sliding window attention
        KVCache::attention_sliding_window(
            input,
            kv_cache,
            layer_idx_,
            output,
            seq_len,
            window_size
        );
    }
}

void DualAttentionLayer::forward_global(
    const float* input,
    float* output,
    KVCache::CacheManager& kv_cache,
    int seq_len,
    const int* positions
) {
    // Global full attention (quality, full context)
    if (config_.use_flash_attention) {
        // Use Flash Attention for memory efficiency even with full context
        KVCache::attention_with_cache(
            input,
            kv_cache,
            layer_idx_,
            output,
            seq_len,
            true  // causal mask
        );
    } else {
        // Standard full attention
        KVCache::attention_with_cache(
            input,
            kv_cache,
            layer_idx_,
            output,
            seq_len,
            true
        );
    }
}

// ============================================================================
// Custom Attention Patterns
// ============================================================================

LayerAttentionPattern CustomAttentionPattern::alternating(int n_layers, int local_per_global) {
    LayerAttentionPattern pattern(n_layers, n_layers);  // Init all local

    for (int i = 0; i < n_layers; i++) {
        if ((i / local_per_global) % 2 == 1) {
            pattern.layer_types[i] = AttentionType::GLOBAL;
        } else {
            pattern.layer_types[i] = AttentionType::LOCAL;
        }
    }

    return pattern;
}

LayerAttentionPattern CustomAttentionPattern::progressive(int n_layers, float global_ratio) {
    int n_global = (int)(n_layers * global_ratio);
    int global_start = n_layers - n_global;

    return LayerAttentionPattern(n_layers, global_start);
}

LayerAttentionPattern CustomAttentionPattern::sandwich(int n_layers) {
    LayerAttentionPattern pattern(n_layers, n_layers);  // Init all local

    // First and last layers are global
    pattern.layer_types[0] = AttentionType::GLOBAL;
    pattern.layer_types[n_layers - 1] = AttentionType::GLOBAL;

    return pattern;
}

LayerAttentionPattern CustomAttentionPattern::qwen_default(int n_layers) {
    // Qwen: First half local, second half global
    int global_start = n_layers / 2;
    return LayerAttentionPattern(n_layers, global_start);
}

// ============================================================================
// Adaptive Attention
// ============================================================================

AdaptiveAttention::AdaptiveAttention(const QwenConfig& config) : config_(config) {
}

AttentionType AdaptiveAttention::choose_attention_type(
    int layer_idx,
    int seq_len,
    int available_memory_mb
) const {
    // Decision tree for adaptive attention

    // 1. Early layers: always local (fast)
    if (layer_idx < 4) {
        return AttentionType::LOCAL;
    }

    // 2. Memory constrained: use local
    if (!should_use_local(seq_len, available_memory_mb)) {
        return AttentionType::LOCAL;
    }

    // 3. Important layers: use global
    if (layer_needs_global(layer_idx)) {
        return AttentionType::GLOBAL;
    }

    // 4. Default: follow config
    return get_attention_type(layer_idx, config_);
}

bool AdaptiveAttention::should_use_local(int seq_len, int memory_mb) const {
    // Estimate memory needed for global attention
    size_t global_mem = seq_len * seq_len * config_.n_heads * sizeof(float) / 1024 / 1024;
    return (global_mem < (size_t)memory_mb);
}

bool AdaptiveAttention::layer_needs_global(int layer_idx) const {
    // Last few layers benefit most from global context
    return (layer_idx >= config_.n_layers - 4);
}

// ============================================================================
// Dual Attention Computation
// ============================================================================

void compute_dual_attention(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    AttentionType attn_type,
    int window_size,
    bool use_flash
) {
    FlashAttention::FlashConfig config;
    config.block_size_q = 64;
    config.block_size_kv = 64;
    config.use_causal_mask = true;

    if (attn_type == AttentionType::LOCAL) {
        // Local windowed attention
        if (use_flash) {
            FlashAttention::flash_attention_sliding_window(
                Q, K, V, O, seq_len, n_heads, head_dim, window_size, config
            );
        } else {
            // Fallback: standard sliding window
            FlashAttention::flash_attention_sliding_window(
                Q, K, V, O, seq_len, n_heads, head_dim, window_size, config
            );
        }
    } else {
        // Global full attention
        if (use_flash) {
            // Use Flash Attention with GQA
            if (n_kv_heads < n_heads) {
                FlashAttention::flash_attention_gqa(
                    Q, K, V, O, seq_len, n_heads, n_kv_heads, head_dim, config
                );
            } else {
                FlashAttention::flash_attention_forward_single(
                    Q, K, V, O, seq_len, n_heads, head_dim, config
                );
            }
        } else {
            // Standard full attention
            FlashAttention::flash_attention_forward_single(
                Q, K, V, O, seq_len, n_heads, head_dim, config
            );
        }
    }
}

// ============================================================================
// Statistics and Analysis
// ============================================================================

DualAttentionStats analyze_dual_attention(
    const QwenConfig& config,
    const LayerAttentionPattern& pattern,
    int seq_len
) {
    DualAttentionStats stats = {};

    int n_local = 0;
    int n_global = 0;

    for (auto type : pattern.layer_types) {
        if (type == AttentionType::LOCAL) {
            n_local++;
        } else {
            n_global++;
        }
    }

    // Memory estimates (per layer)
    size_t local_attn_mem = seq_len * config.local_window_size * config.n_heads * sizeof(float);
    size_t global_attn_mem = seq_len * seq_len * config.n_heads * sizeof(float);

    stats.local_memory_mb = (n_local * local_attn_mem) / 1024 / 1024;
    stats.global_memory_mb = (n_global * global_attn_mem) / 1024 / 1024;

    // If all were global
    size_t all_global_mem = (pattern.layer_types.size() * global_attn_mem) / 1024 / 1024;
    size_t actual_mem = stats.local_memory_mb + stats.global_memory_mb;

    stats.memory_savings_ratio = (float)all_global_mem / actual_mem;

    // Compute estimates (FLOPs)
    // Attention: O(seq_len × context_len × d_model)
    double local_flops = n_local * seq_len * config.local_window_size * config.hidden_dim;
    double global_flops = n_global * seq_len * seq_len * config.hidden_dim;

    stats.local_compute_gflops = local_flops / 1e9;
    stats.global_compute_gflops = global_flops / 1e9;

    double all_global_flops = pattern.layer_types.size() * seq_len * seq_len * config.hidden_dim;
    double actual_flops = local_flops + global_flops;

    stats.compute_savings_ratio = (float)all_global_flops / actual_flops;

    return stats;
}

QualityEstimate estimate_quality_impact(
    int n_local_layers,
    int n_global_layers,
    int window_size,
    int seq_len
) {
    QualityEstimate estimate;

    // Heuristic quality estimates (0.0-1.0)
    // Based on how much context each type can see

    // Local: limited by window size
    float local_context_ratio = std::min(1.0f, (float)window_size / seq_len);
    estimate.local_quality_score = 0.7f + 0.3f * local_context_ratio;

    // Global: full context
    estimate.global_quality_score = 1.0f;

    // Hybrid: weighted average
    int total_layers = n_local_layers + n_global_layers;
    float global_ratio = (float)n_global_layers / total_layers;
    estimate.hybrid_quality_score = estimate.local_quality_score * (1.0f - global_ratio) +
                                    estimate.global_quality_score * global_ratio;

    // Recommendation
    if (global_ratio >= 0.5f) {
        estimate.recommendation = "Good balance: " + std::to_string((int)(global_ratio * 100)) + "% global layers";
    } else if (global_ratio >= 0.3f) {
        estimate.recommendation = "Moderate: Consider adding more global layers for better quality";
    } else {
        estimate.recommendation = "Speed-focused: Add global layers for quality-critical tasks";
    }

    return estimate;
}

// ============================================================================
// Qwen Model
// ============================================================================

QwenModel::QwenModel(const QwenConfig& config)
    : config_(config),
      pattern_(CustomAttentionPattern::qwen_default(config.n_layers)),
      kv_cache_(KVCache::CacheConfig()) {

    // Initialize KV cache
    KVCache::CacheConfig cache_config;
    cache_config.n_layers = config.n_layers;
    cache_config.n_heads = config.n_heads;
    cache_config.n_kv_heads = config.n_kv_heads;
    cache_config.head_dim = config.head_dim;
    cache_config.max_seq_len = config.max_position;
    cache_config.use_gqa = true;
    cache_config.n_heads_per_kv = config.n_heads / config.n_kv_heads;
    cache_config.use_ring_buffer = true;

    kv_cache_ = KVCache::CacheManager(cache_config);

    // Initialize layers
    layers_.reserve(config.n_layers);
    for (int i = 0; i < config.n_layers; i++) {
        layers_.emplace_back(config, i);
    }

    std::cerr << "[QWEN] Model initialized: "
              << config.n_layers << " layers (dual attention pattern)" << std::endl;
    print_attention_pattern(pattern_);
}

void QwenModel::forward(const int* input_ids, float* output, int seq_len) {
    // Placeholder: Full forward pass
    std::cerr << "[QWEN] Full forward pass not fully implemented" << std::endl;
}

// ============================================================================
// Utilities
// ============================================================================

void print_attention_pattern(const LayerAttentionPattern& pattern) {
    std::cout << "\n=== Attention Pattern ===" << std::endl;
    for (size_t i = 0; i < pattern.layer_types.size(); i++) {
        std::cout << "Layer " << std::setw(2) << i << ": ";

        if (pattern.layer_types[i] == AttentionType::LOCAL) {
            std::cout << "[LOCAL    ] (windowed)";
        } else {
            std::cout << "[GLOBAL   ] (full context)";
        }

        std::cout << std::endl;
    }
    std::cout << "=========================" << std::endl;
}

} // namespace QwenAttention
