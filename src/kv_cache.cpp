// KV-Cache: Key-Value Cache for Efficient Transformer Inference
// Algorithm adapted from: llama.cpp (MIT License)
// Original: https://github.com/ggerganov/llama.cpp/blob/master/llama.cpp
// Modifications: Removed dependencies, added GQA support, ring buffer optimization

#include "kv_cache.h"
#include "tensor_ops.h"
#include <cstring>
#include <algorithm>
#include <cmath>
#include <iostream>

namespace KVCache {

// ============================================================================
// CacheManager Implementation
// ============================================================================

CacheManager::CacheManager(const CacheConfig& config) : config_(config) {
    // Allocate cache for all layers
    layers_.resize(config_.n_layers);

    size_t layer_size = compute_layer_size();

    for (int i = 0; i < config_.n_layers; i++) {
        layers_[i].keys.resize(layer_size, 0.0f);
        layers_[i].values.resize(layer_size, 0.0f);
        layers_[i].seq_ids.resize(config_.max_seq_len, -1);
        layers_[i].current_pos = 0;
        layers_[i].filled_len = 0;
    }

    std::cerr << "[KV-CACHE] Initialized: " << config_.n_layers << " layers, "
              << config_.max_seq_len << " max tokens, "
              << (layer_size * sizeof(float) * 2 / 1024 / 1024) << " MB per layer\n";

    if (config_.use_gqa) {
        std::cerr << "[KV-CACHE] GQA enabled: " << config_.n_heads << " Q heads → "
                  << config_.n_kv_heads << " KV heads (ratio: "
                  << (config_.n_heads / config_.n_kv_heads) << ":1)\n";
    }
}

size_t CacheManager::compute_layer_size() const {
    // Cache layout: [max_seq_len, n_kv_heads, head_dim]
    return config_.max_seq_len * config_.n_kv_heads * config_.head_dim;
}

int CacheManager::get_storage_index(int logical_pos) const {
    if (config_.use_ring_buffer) {
        // Ring buffer: wrap around
        return logical_pos % config_.max_seq_len;
    } else {
        // Linear buffer: direct indexing
        return logical_pos;
    }
}

int CacheManager::update(int layer_idx, const float* K, const float* V, int n_tokens, int seq_id) {
    auto& layer = layers_[layer_idx];

    int start_pos = layer.current_pos;

    for (int t = 0; t < n_tokens; t++) {
        int storage_idx = get_storage_index(layer.current_pos);

        // Copy K, V for this token
        // Input layout: [n_tokens, n_kv_heads, head_dim]
        // Storage layout: [max_seq_len, n_kv_heads, head_dim]
        for (int h = 0; h < config_.n_kv_heads; h++) {
            for (int d = 0; d < config_.head_dim; d++) {
                int input_idx = (t * config_.n_kv_heads + h) * config_.head_dim + d;
                int storage_offset = (storage_idx * config_.n_kv_heads + h) * config_.head_dim + d;

                layer.keys[storage_offset] = K[input_idx];
                layer.values[storage_offset] = V[input_idx];
            }
        }

        // Track sequence ID
        layer.seq_ids[storage_idx] = seq_id;

        // Advance position
        layer.current_pos++;
        layer.filled_len = std::min(layer.filled_len + 1, config_.max_seq_len);

        // Apply sliding window if enabled
        if (config_.sliding_window > 0 && layer.filled_len > config_.sliding_window) {
            layer.filled_len = config_.sliding_window;
        }
    }

    return start_pos;
}

const float* CacheManager::get_keys(int layer_idx, int& out_len) const {
    const auto& layer = layers_[layer_idx];
    out_len = layer.filled_len;
    return layer.keys.data();
}

const float* CacheManager::get_values(int layer_idx, int& out_len) const {
    const auto& layer = layers_[layer_idx];
    out_len = layer.filled_len;
    return layer.values.data();
}

void CacheManager::get_range(int layer_idx, int start_pos, int end_pos, float* out_keys, float* out_values) const {
    const auto& layer = layers_[layer_idx];

    for (int pos = start_pos; pos < end_pos; pos++) {
        int storage_idx = get_storage_index(pos);
        int out_offset = (pos - start_pos) * config_.n_kv_heads * config_.head_dim;
        int storage_offset = storage_idx * config_.n_kv_heads * config_.head_dim;

        std::memcpy(out_keys + out_offset, layer.keys.data() + storage_offset,
                    config_.n_kv_heads * config_.head_dim * sizeof(float));
        std::memcpy(out_values + out_offset, layer.values.data() + storage_offset,
                    config_.n_kv_heads * config_.head_dim * sizeof(float));
    }
}

void CacheManager::expand_kv_heads(const float* K_compact, float* K_expanded, int n_tokens) const {
    // GQA: Expand n_kv_heads to n_heads by replicating
    // Example: 8 Q heads, 2 KV heads → each KV head serves 4 Q heads

    int n_heads_per_kv = config_.n_heads / config_.n_kv_heads;

    for (int t = 0; t < n_tokens; t++) {
        for (int kv_h = 0; kv_h < config_.n_kv_heads; kv_h++) {
            // Replicate this KV head to multiple Q heads
            for (int rep = 0; rep < n_heads_per_kv; rep++) {
                int q_head = kv_h * n_heads_per_kv + rep;

                for (int d = 0; d < config_.head_dim; d++) {
                    int src_idx = (t * config_.n_kv_heads + kv_h) * config_.head_dim + d;
                    int dst_idx = (t * config_.n_heads + q_head) * config_.head_dim + d;
                    K_expanded[dst_idx] = K_compact[src_idx];
                }
            }
        }
    }
}

void CacheManager::clear_layer(int layer_idx) {
    auto& layer = layers_[layer_idx];
    std::fill(layer.keys.begin(), layer.keys.end(), 0.0f);
    std::fill(layer.values.begin(), layer.values.end(), 0.0f);
    std::fill(layer.seq_ids.begin(), layer.seq_ids.end(), -1);
    layer.current_pos = 0;
    layer.filled_len = 0;
}

void CacheManager::clear_all() {
    for (int i = 0; i < config_.n_layers; i++) {
        clear_layer(i);
    }
}

void CacheManager::defragment(int layer_idx) {
    // Compact ring buffer to linear layout (if needed)
    if (!config_.use_ring_buffer) return;

    auto& layer = layers_[layer_idx];
    std::vector<float> temp_keys = layer.keys;
    std::vector<float> temp_values = layer.values;

    for (int i = 0; i < layer.filled_len; i++) {
        int storage_idx = get_storage_index(i);
        int offset = i * config_.n_kv_heads * config_.head_dim;
        int storage_offset = storage_idx * config_.n_kv_heads * config_.head_dim;

        if (offset != storage_offset) {
            std::memcpy(layer.keys.data() + offset, temp_keys.data() + storage_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
            std::memcpy(layer.values.data() + offset, temp_values.data() + storage_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
        }
    }

    layer.current_pos = layer.filled_len;
}

void CacheManager::apply_sliding_window(int layer_idx) {
    if (config_.sliding_window <= 0) return;

    auto& layer = layers_[layer_idx];
    if (layer.filled_len > config_.sliding_window) {
        // Keep only last N tokens
        int discard = layer.filled_len - config_.sliding_window;
        layer.current_pos -= discard;
        layer.filled_len = config_.sliding_window;
    }
}

void CacheManager::remove_sequence(int seq_id) {
    for (auto& layer : layers_) {
        for (size_t i = 0; i < layer.seq_ids.size(); i++) {
            if (layer.seq_ids[i] == seq_id) {
                layer.seq_ids[i] = -1;
            }
        }
    }
}

void CacheManager::copy_sequence(int src_seq_id, int dst_seq_id) {
    // Used for beam search: copy cache from one beam to another
    for (auto& layer : layers_) {
        for (size_t i = 0; i < layer.seq_ids.size(); i++) {
            if (layer.seq_ids[i] == src_seq_id) {
                // Find empty slot for dst
                for (size_t j = 0; j < layer.seq_ids.size(); j++) {
                    if (layer.seq_ids[j] == -1) {
                        // Copy K, V
                        int offset = i * config_.n_kv_heads * config_.head_dim;
                        int dst_offset = j * config_.n_kv_heads * config_.head_dim;

                        std::memcpy(layer.keys.data() + dst_offset,
                                    layer.keys.data() + offset,
                                    config_.n_kv_heads * config_.head_dim * sizeof(float));
                        std::memcpy(layer.values.data() + dst_offset,
                                    layer.values.data() + offset,
                                    config_.n_kv_heads * config_.head_dim * sizeof(float));

                        layer.seq_ids[j] = dst_seq_id;
                        break;
                    }
                }
            }
        }
    }
}

void CacheManager::shift_positions(int layer_idx, int start_pos, int shift_amount) {
    // Shift cache positions (for prefix caching, context shifting)
    auto& layer = layers_[layer_idx];

    if (shift_amount > 0) {
        // Shift right (make room)
        for (int i = layer.filled_len - 1; i >= start_pos; i--) {
            int src_idx = get_storage_index(i);
            int dst_idx = get_storage_index(i + shift_amount);

            int src_offset = src_idx * config_.n_kv_heads * config_.head_dim;
            int dst_offset = dst_idx * config_.n_kv_heads * config_.head_dim;

            std::memcpy(layer.keys.data() + dst_offset, layer.keys.data() + src_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
            std::memcpy(layer.values.data() + dst_offset, layer.values.data() + src_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
        }
        layer.filled_len += shift_amount;
    } else if (shift_amount < 0) {
        // Shift left (remove)
        for (int i = start_pos; i < layer.filled_len; i++) {
            int src_idx = get_storage_index(i);
            int dst_idx = get_storage_index(i + shift_amount);

            int src_offset = src_idx * config_.n_kv_heads * config_.head_dim;
            int dst_offset = dst_idx * config_.n_kv_heads * config_.head_dim;

            std::memcpy(layer.keys.data() + dst_offset, layer.keys.data() + src_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
            std::memcpy(layer.values.data() + dst_offset, layer.values.data() + src_offset,
                        config_.n_kv_heads * config_.head_dim * sizeof(float));
        }
        layer.filled_len += shift_amount;  // shift_amount is negative
    }
}

CacheManager::CacheStats CacheManager::get_stats() const {
    CacheStats stats = {};

    size_t layer_size = compute_layer_size();
    stats.total_bytes = layer_size * sizeof(float) * 2 * config_.n_layers;  // K + V

    size_t used = 0;
    int max_pos = 0;
    for (const auto& layer : layers_) {
        used += layer.filled_len * config_.n_kv_heads * config_.head_dim * sizeof(float) * 2;
        max_pos = std::max(max_pos, layer.filled_len);
    }

    stats.used_bytes = used;
    stats.utilization = (float)used / stats.total_bytes * 100.0f;
    stats.max_position = max_pos;

    return stats;
}

int CacheManager::get_position(int layer_idx) const {
    return layers_[layer_idx].current_pos;
}

// ============================================================================
// Attention with KV-Cache
// ============================================================================

void attention_with_cache(
    const float* Q,
    const CacheManager& cache,
    int layer_idx,
    float* output,
    int n_new_tokens,
    bool causal_mask
) {
    const auto& config = cache.get_config();

    // Get cached K, V
    int cached_len;
    const float* K_cached = cache.get_keys(layer_idx, cached_len);
    const float* V_cached = cache.get_values(layer_idx, cached_len);

    int total_len = cached_len;  // Only attend to cached tokens
    int head_dim = config.head_dim;

    // For each new token
    for (int t = 0; t < n_new_tokens; t++) {
        for (int h = 0; h < config.n_heads; h++) {
            // Get query for this token and head
            const float* q = Q + (t * config.n_heads + h) * head_dim;

            // Compute attention scores with all cached keys
            std::vector<float> scores(total_len);
            float scale = 1.0f / std::sqrt((float)head_dim);

            // Which KV head to use (for GQA)
            int kv_head = config.use_gqa ? (h / (config.n_heads / config.n_kv_heads)) : h;

            for (int i = 0; i < total_len; i++) {
                const float* k = K_cached + (i * config.n_kv_heads + kv_head) * head_dim;
                scores[i] = TensorOps::vec_dot(q, k, head_dim) * scale;

                // Causal mask: can't attend to future positions
                if (causal_mask && i > (cached_len - n_new_tokens + t)) {
                    scores[i] = -1e9f;
                }
            }

            // Softmax
            TensorOps::softmax(scores.data(), scores.data(), total_len);

            // Weighted sum of values
            float* out = output + (t * config.n_heads + h) * head_dim;
            std::fill_n(out, head_dim, 0.0f);

            for (int i = 0; i < total_len; i++) {
                const float* v = V_cached + (i * config.n_kv_heads + kv_head) * head_dim;
                for (int d = 0; d < head_dim; d++) {
                    out[d] += scores[i] * v[d];
                }
            }
        }
    }
}

void attention_with_gqa(
    const float* Q,
    const float* K,
    const float* V,
    float* output,
    int n_tokens,
    int n_heads,
    int n_kv_heads,
    int head_dim,
    bool causal_mask
) {
    // GQA: n_heads Q heads, n_kv_heads KV heads
    // Each KV head is shared by (n_heads / n_kv_heads) Q heads

    int n_heads_per_kv = n_heads / n_kv_heads;
    float scale = 1.0f / std::sqrt((float)head_dim);

    for (int t = 0; t < n_tokens; t++) {
        for (int h = 0; h < n_heads; h++) {
            // Which KV head does this Q head use?
            int kv_head = h / n_heads_per_kv;

            const float* q = Q + (t * n_heads + h) * head_dim;

            // Compute scores
            std::vector<float> scores(n_tokens);
            for (int i = 0; i < n_tokens; i++) {
                const float* k = K + (i * n_kv_heads + kv_head) * head_dim;
                scores[i] = TensorOps::vec_dot(q, k, head_dim) * scale;

                if (causal_mask && i > t) {
                    scores[i] = -1e9f;
                }
            }

            // Softmax
            TensorOps::softmax(scores.data(), scores.data(), n_tokens);

            // Weighted sum
            float* out = output + (t * n_heads + h) * head_dim;
            std::fill_n(out, head_dim, 0.0f);

            for (int i = 0; i < n_tokens; i++) {
                const float* v = V + (i * n_kv_heads + kv_head) * head_dim;
                for (int d = 0; d < head_dim; d++) {
                    out[d] += scores[i] * v[d];
                }
            }
        }
    }
}

void attention_sliding_window(
    const float* Q,
    const CacheManager& cache,
    int layer_idx,
    float* output,
    int n_new_tokens,
    int window_size
) {
    const auto& config = cache.get_config();

    int cached_len;
    const float* K_cached = cache.get_keys(layer_idx, cached_len);
    const float* V_cached = cache.get_values(layer_idx, cached_len);

    // Only attend to last window_size tokens
    int attend_start = std::max(0, cached_len - window_size);
    int attend_len = cached_len - attend_start;

    int head_dim = config.head_dim;
    float scale = 1.0f / std::sqrt((float)head_dim);

    for (int t = 0; t < n_new_tokens; t++) {
        for (int h = 0; h < config.n_heads; h++) {
            const float* q = Q + (t * config.n_heads + h) * head_dim;

            int kv_head = config.use_gqa ? (h / (config.n_heads / config.n_kv_heads)) : h;

            // Scores only for window
            std::vector<float> scores(attend_len);
            for (int i = 0; i < attend_len; i++) {
                int pos = attend_start + i;
                const float* k = K_cached + (pos * config.n_kv_heads + kv_head) * head_dim;
                scores[i] = TensorOps::vec_dot(q, k, head_dim) * scale;
            }

            // Softmax
            TensorOps::softmax(scores.data(), scores.data(), attend_len);

            // Weighted sum
            float* out = output + (t * config.n_heads + h) * head_dim;
            std::fill_n(out, head_dim, 0.0f);

            for (int i = 0; i < attend_len; i++) {
                int pos = attend_start + i;
                const float* v = V_cached + (pos * config.n_kv_heads + kv_head) * head_dim;
                for (int d = 0; d < head_dim; d++) {
                    out[d] += scores[i] * v[d];
                }
            }
        }
    }
}

} // namespace KVCache
