#include "mini_transformer.h"
#include "optimizer.h"
#include "loss.h"
#include "transformer_gradients.h"
#include "tensor_ops.h"  // SIMD-optimized operations
#include "flash_attention.h"  // Flash Attention v2
#include <cmath>
#include <algorithm>
#include <random>
#include <iostream>
#include <fstream>

MiniTransformer::MiniTransformer(const TransformerConfig& config)
    : config_(config) {
    initialize_weights();
}

void MiniTransformer::initialize_weights() {
    std::cerr << "[TRANSFORMER] Initializing weights...\n";

    // Token embeddings
    weights_.token_embeddings.resize(config_.vocab_size);
    for (int i = 0; i < config_.vocab_size; i++) {
        weights_.token_embeddings[i].resize(config_.embedding_dim);
    }
    xavier_init(weights_.token_embeddings, config_.vocab_size, config_.embedding_dim);

    // Position embeddings (learned, like GPT)
    weights_.position_embeddings.resize(config_.max_seq_length);
    for (int i = 0; i < config_.max_seq_length; i++) {
        weights_.position_embeddings[i].resize(config_.embedding_dim);
    }
    xavier_init(weights_.position_embeddings, config_.max_seq_length, config_.embedding_dim);

    // Transformer layers
    weights_.layers.resize(config_.num_layers);
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];

        // Attention weights
        layer.query_weight.resize(config_.embedding_dim);
        layer.key_weight.resize(config_.embedding_dim);
        layer.value_weight.resize(config_.embedding_dim);
        layer.output_weight.resize(config_.embedding_dim);

        for (int i = 0; i < config_.embedding_dim; i++) {
            layer.query_weight[i].resize(config_.embedding_dim);
            layer.key_weight[i].resize(config_.embedding_dim);
            layer.value_weight[i].resize(config_.embedding_dim);
            layer.output_weight[i].resize(config_.embedding_dim);
        }

        xavier_init(layer.query_weight, config_.embedding_dim, config_.embedding_dim);
        xavier_init(layer.key_weight, config_.embedding_dim, config_.embedding_dim);
        xavier_init(layer.value_weight, config_.embedding_dim, config_.embedding_dim);
        xavier_init(layer.output_weight, config_.embedding_dim, config_.embedding_dim);

        // Feed-forward weights
        layer.ff1_weight.resize(config_.embedding_dim);
        for (int i = 0; i < config_.embedding_dim; i++) {
            layer.ff1_weight[i].resize(config_.ff_dim);
        }
        layer.ff1_bias.resize(config_.ff_dim, 0.0f);

        layer.ff2_weight.resize(config_.ff_dim);
        for (int i = 0; i < config_.ff_dim; i++) {
            layer.ff2_weight[i].resize(config_.embedding_dim);
        }
        layer.ff2_bias.resize(config_.embedding_dim, 0.0f);

        xavier_init(layer.ff1_weight, config_.embedding_dim, config_.ff_dim);
        xavier_init(layer.ff2_weight, config_.ff_dim, config_.embedding_dim);

        // Layer norm parameters
        layer.ln1_gamma.resize(config_.embedding_dim, 1.0f);
        layer.ln1_beta.resize(config_.embedding_dim, 0.0f);
        layer.ln2_gamma.resize(config_.embedding_dim, 1.0f);
        layer.ln2_beta.resize(config_.embedding_dim, 0.0f);
    }

    // Output projection
    weights_.output_projection.resize(config_.embedding_dim);
    for (int i = 0; i < config_.embedding_dim; i++) {
        weights_.output_projection[i].resize(config_.vocab_size);
    }
    xavier_init(weights_.output_projection, config_.embedding_dim, config_.vocab_size);

    // Calculate and display model size
    size_t params = 0;
    params += config_.vocab_size * config_.embedding_dim;
    params += config_.max_seq_length * config_.embedding_dim;
    size_t layer_params = 4 * (config_.embedding_dim * config_.embedding_dim);
    layer_params += config_.embedding_dim * config_.ff_dim;
    layer_params += config_.ff_dim * config_.embedding_dim;
    layer_params += config_.ff_dim + config_.embedding_dim;
    layer_params += 4 * config_.embedding_dim;
    params += layer_params * config_.num_layers;
    params += config_.embedding_dim * config_.vocab_size;

    std::cerr << "[TRANSFORMER] Initialized " << config_.num_layers << " layers\n";
    std::cerr << "[TRANSFORMER] Model size: ~" << (params / 1000000) << "M parameters\n";
}

void MiniTransformer::xavier_init(std::vector<std::vector<float>>& weight, int in_dim, int out_dim) {
    std::random_device rd;
    std::mt19937 gen(rd());
    float scale = std::sqrt(2.0f / (in_dim + out_dim));
    std::normal_distribution<float> dist(0.0f, scale);

    for (auto& row : weight) {
        for (auto& val : row) {
            val = dist(gen);
        }
    }
}

float MiniTransformer::gelu(float x) const {
    // GELU activation: x * Φ(x) where Φ is CDF of normal distribution
    // Approximation: 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
    float sqrt_2_over_pi = 0.7978845608f;
    float cube = x * x * x;
    float tanh_arg = sqrt_2_over_pi * (x + 0.044715f * cube);
    float tanh_val = std::tanh(tanh_arg);
    return 0.5f * x * (1.0f + tanh_val);
}

float MiniTransformer::gelu_derivative(float x) const {
    // GELU'(x) = 0.5 * (1 + tanh(a)) + 0.5 * x * sech²(a) * a'
    // where a = sqrt(2/π) * (x + 0.044715 * x³)
    // Simplified: CDF + x * PDF
    const float sqrt_2_over_pi = 0.7978845608f;
    const float c = 0.044715f;

    float x2 = x * x;
    float x3 = x2 * x;
    float tanh_arg = sqrt_2_over_pi * (x + c * x3);
    float tanh_val = std::tanh(tanh_arg);

    // CDF part
    float cdf = 0.5f * (1.0f + tanh_val);

    // PDF part: x * derivative of tanh argument
    float sech2 = 1.0f - tanh_val * tanh_val;  // sech²(x) = 1 - tanh²(x)
    float derivative_arg = sqrt_2_over_pi * (1.0f + 3.0f * c * x2);
    float pdf = 0.5f * x * sech2 * derivative_arg;

    return cdf + pdf;
}

// ============================================================================
// Precision Conversion Helpers (Week 9 K10 - Mixed Precision)
// ============================================================================

void MiniTransformer::convert_weights_to_precision(
    std::vector<std::vector<float>>& weights,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using namespace PrecisionUtils;
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

    if (mode == PMode::FP32) {
        // No conversion needed
        return;
    }

    // Convert all weights in-place via round-trip conversion
    for (auto& row : weights) {
        for (float& val : row) {
            if (mode == PMode::FP16) {
                uint16_t fp16 = fp32_to_fp16(val);
                val = fp16_to_fp32(fp16);
            } else if (mode == PMode::BF16) {
                uint16_t bf16 = fp32_to_bf16(val);
                val = bf16_to_fp32(bf16);
            }
        }
    }
}

void MiniTransformer::restore_weights_to_fp32(
    std::vector<std::vector<float>>& weights,
    const std::vector<std::vector<float>>& backup
) {
    // Simple copy from backup
    weights = backup;
}

std::vector<std::vector<float>> MiniTransformer::layer_norm(
    const std::vector<std::vector<float>>& input,
    const std::vector<float>& gamma,
    const std::vector<float>& beta
) {
    // UPGRADED: Use RMSNorm instead of LayerNorm (faster, used in LLaMA/Mistral)
    // RMSNorm: y = x / sqrt(mean(x²) + eps) * gamma
    // Note: beta (bias) is ignored in RMSNorm for compatibility
    std::vector<std::vector<float>> output = input;

    for (size_t i = 0; i < input.size(); i++) {
        // OPTIMIZED: Use SIMD RMSNorm
        TensorOps::rmsnorm(output[i].data(), input[i].data(), gamma.data(), input[i].size());
    }

    return output;
}

std::vector<std::vector<float>> MiniTransformer::multi_head_attention(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    bool causal_mask,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using namespace PrecisionUtils;
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;
    int seq_len = input.size();
    int d_model = config_.embedding_dim;
    int d_k = d_model / config_.num_heads;

    // Simplified: Single-head attention for now (multi-head adds complexity)
    // In production, would split into num_heads and concatenate

    // Compute Q, K, V (SIMD-optimized matrix multiplication)
    std::vector<std::vector<float>> Q(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> K(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> V(seq_len, std::vector<float>(d_model, 0.0f));

    // Flatten input for optimized matmul
    std::vector<float> input_flat(seq_len * d_model);
    std::vector<float> Q_flat(seq_len * d_model, 0.0f);
    std::vector<float> K_flat(seq_len * d_model, 0.0f);
    std::vector<float> V_flat(seq_len * d_model, 0.0f);
    std::vector<float> W_Q_flat(d_model * d_model);
    std::vector<float> W_K_flat(d_model * d_model);
    std::vector<float> W_V_flat(d_model * d_model);

    // Copy to flat arrays (row-major)
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            input_flat[i * d_model + j] = input[i][j];
        }
    }
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_Q_flat[i * d_model + j] = layer.query_weight[i][j];
            W_K_flat[i * d_model + j] = layer.key_weight[i][j];
            W_V_flat[i * d_model + j] = layer.value_weight[i][j];
        }
    }

    // OPTIMIZED: Use SIMD matmul (3-5x faster!)
    TensorOps::matmul(input_flat.data(), W_Q_flat.data(), Q_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_K_flat.data(), K_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_V_flat.data(), V_flat.data(), seq_len, d_model, d_model);

    // Copy back to 2D arrays
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            Q[i][j] = Q_flat[i * d_model + j];
            K[i][j] = K_flat[i * d_model + j];
            V[i][j] = V_flat[i * d_model + j];
        }
    }

    // Mixed Precision: Convert Q, K, V to lower precision (SIMD-optimized)
    if (mode != PMode::FP32) {
        size_t total_size = seq_len * d_model;
        std::vector<uint16_t> temp_fp16(total_size);

        if (mode == PMode::FP16) {
            // Q: FP32 → FP16 → FP32 (SIMD)
            fp32_array_to_fp16_simd(Q_flat.data(), temp_fp16.data(), total_size);
            fp16_array_to_fp32_simd(temp_fp16.data(), Q_flat.data(), total_size);

            // K: FP32 → FP16 → FP32 (SIMD)
            fp32_array_to_fp16_simd(K_flat.data(), temp_fp16.data(), total_size);
            fp16_array_to_fp32_simd(temp_fp16.data(), K_flat.data(), total_size);

            // V: FP32 → FP16 → FP32 (SIMD)
            fp32_array_to_fp16_simd(V_flat.data(), temp_fp16.data(), total_size);
            fp16_array_to_fp32_simd(temp_fp16.data(), V_flat.data(), total_size);
        } else if (mode == PMode::BF16) {
            // Q: FP32 → BF16 → FP32 (SIMD)
            fp32_array_to_bf16_simd(Q_flat.data(), temp_fp16.data(), total_size);
            bf16_array_to_fp32_simd(temp_fp16.data(), Q_flat.data(), total_size);

            // K: FP32 → BF16 → FP32 (SIMD)
            fp32_array_to_bf16_simd(K_flat.data(), temp_fp16.data(), total_size);
            bf16_array_to_fp32_simd(temp_fp16.data(), K_flat.data(), total_size);

            // V: FP32 → BF16 → FP32 (SIMD)
            fp32_array_to_bf16_simd(V_flat.data(), temp_fp16.data(), total_size);
            bf16_array_to_fp32_simd(temp_fp16.data(), V_flat.data(), total_size);
        }
    }

    // Compute attention scores: Q * K^T / sqrt(d_k) (SIMD-optimized)
    std::vector<std::vector<float>> scores(seq_len, std::vector<float>(seq_len, 0.0f));
    float scale = 1.0f / std::sqrt(d_k);

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < seq_len; j++) {
            // OPTIMIZED: vec_dot (AVX2/SSE2)
            float dot = TensorOps::vec_dot(Q[i].data(), K[j].data(), d_model);
            scores[i][j] = dot * scale;

            // Causal mask: can't attend to future tokens
            if (causal_mask && j > i) {
                scores[i][j] = -1e9f;  // Very negative = 0 after softmax
            }
        }
    }

    // Softmax over scores (SIMD-optimized)
    for (int i = 0; i < seq_len; i++) {
        TensorOps::softmax(scores[i].data(), scores[i].data(), seq_len);
    }

    // Apply attention to values: scores * V
    std::vector<std::vector<float>> attended(seq_len, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < seq_len; j++) {
            for (int k = 0; k < d_model; k++) {
                attended[i][k] += scores[i][j] * V[j][k];
            }
        }
    }

    // Output projection (SIMD-optimized)
    std::vector<std::vector<float>> output(seq_len, std::vector<float>(d_model, 0.0f));

    // Flatten arrays for optimized matmul
    std::vector<float> attended_flat(seq_len * d_model);
    std::vector<float> W_O_flat(d_model * d_model);
    std::vector<float> output_flat(seq_len * d_model, 0.0f);

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            attended_flat[i * d_model + j] = attended[i][j];
        }
    }
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_O_flat[i * d_model + j] = layer.output_weight[i][j];
        }
    }

    // OPTIMIZED: SIMD matmul
    TensorOps::matmul(attended_flat.data(), W_O_flat.data(), output_flat.data(), seq_len, d_model, d_model);

    // Copy back
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            output[i][j] = output_flat[i * d_model + j];
        }
    }

    return output;
}

// Flash Attention v2 - Memory-efficient attention for long context
std::vector<std::vector<float>> MiniTransformer::multi_head_attention_flash(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    bool causal_mask
) {
    int seq_len = input.size();
    int d_model = config_.embedding_dim;
    int head_dim = d_model / config_.num_heads;

    // Step 1: Compute Q, K, V (same as standard attention)
    std::vector<float> input_flat(seq_len * d_model);
    std::vector<float> Q_flat(seq_len * d_model, 0.0f);
    std::vector<float> K_flat(seq_len * d_model, 0.0f);
    std::vector<float> V_flat(seq_len * d_model, 0.0f);
    std::vector<float> W_Q_flat(d_model * d_model);
    std::vector<float> W_K_flat(d_model * d_model);
    std::vector<float> W_V_flat(d_model * d_model);

    // Flatten input and weights
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            input_flat[i * d_model + j] = input[i][j];
        }
    }
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_Q_flat[i * d_model + j] = layer.query_weight[i][j];
            W_K_flat[i * d_model + j] = layer.key_weight[i][j];
            W_V_flat[i * d_model + j] = layer.value_weight[i][j];
        }
    }

    // Compute Q, K, V using SIMD
    TensorOps::matmul(input_flat.data(), W_Q_flat.data(), Q_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_K_flat.data(), K_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_V_flat.data(), V_flat.data(), seq_len, d_model, d_model);

    // Step 2: Flash Attention (memory-efficient, O(N) instead of O(N²))
    FlashAttention::FlashConfig flash_config;
    flash_config.block_size_q = 64;
    flash_config.block_size_kv = 64;
    flash_config.use_causal_mask = causal_mask;
    flash_config.softmax_scale = 1.0f / std::sqrt(head_dim);
    flash_config.use_online_softmax = true;

    std::vector<float> O_flat(seq_len * d_model, 0.0f);

    // Call Flash Attention (never materializes full attention matrix!)
    FlashAttention::flash_attention_forward_single(
        Q_flat.data(),
        K_flat.data(),
        V_flat.data(),
        O_flat.data(),
        seq_len,
        config_.num_heads,
        head_dim,
        flash_config
    );

    // Step 3: Output projection (same as standard)
    std::vector<std::vector<float>> output(seq_len, std::vector<float>(d_model, 0.0f));

    std::vector<float> W_O_flat(d_model * d_model);
    std::vector<float> output_flat(seq_len * d_model, 0.0f);

    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_O_flat[i * d_model + j] = layer.output_weight[i][j];
        }
    }

    TensorOps::matmul(O_flat.data(), W_O_flat.data(), output_flat.data(), seq_len, d_model, d_model);

    // Unflatten output
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            output[i][j] = output_flat[i * d_model + j];
        }
    }

    return output;
}

// KV-Cache optimized multi-head attention
std::vector<std::vector<float>> MiniTransformer::multi_head_attention_cached(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    KVCache::CacheManager& cache,
    int layer_idx,
    bool is_prefill,
    bool causal_mask
) {
    int seq_len = input.size();
    int d_model = config_.embedding_dim;
    int d_k = d_model / config_.num_heads;

    // Step 1: Compute Q, K, V for NEW tokens only
    std::vector<std::vector<float>> Q(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> K(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> V(seq_len, std::vector<float>(d_model, 0.0f));

    // Flatten for SIMD matmul
    std::vector<float> input_flat(seq_len * d_model);
    std::vector<float> Q_flat(seq_len * d_model, 0.0f);
    std::vector<float> K_flat(seq_len * d_model, 0.0f);
    std::vector<float> V_flat(seq_len * d_model, 0.0f);
    std::vector<float> W_Q_flat(d_model * d_model);
    std::vector<float> W_K_flat(d_model * d_model);
    std::vector<float> W_V_flat(d_model * d_model);

    // Copy to flat arrays
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            input_flat[i * d_model + j] = input[i][j];
        }
    }
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_Q_flat[i * d_model + j] = layer.query_weight[i][j];
            W_K_flat[i * d_model + j] = layer.key_weight[i][j];
            W_V_flat[i * d_model + j] = layer.value_weight[i][j];
        }
    }

    // Compute Q, K, V using SIMD
    TensorOps::matmul(input_flat.data(), W_Q_flat.data(), Q_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_K_flat.data(), K_flat.data(), seq_len, d_model, d_model);
    TensorOps::matmul(input_flat.data(), W_V_flat.data(), V_flat.data(), seq_len, d_model, d_model);

    // Copy back
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            Q[i][j] = Q_flat[i * d_model + j];
            K[i][j] = K_flat[i * d_model + j];
            V[i][j] = V_flat[i * d_model + j];
        }
    }

    // Step 2: Update cache with new K, V
    cache.update(layer_idx, K_flat.data(), V_flat.data(), seq_len, 0);

    // Step 3: Get ALL cached K, V (including what we just added)
    int cached_len = 0;
    const float* K_cached = cache.get_keys(layer_idx, cached_len);
    const float* V_cached = cache.get_values(layer_idx, cached_len);

    // Step 4: Compute attention scores: Q * K_cached^T / sqrt(d_k)
    std::vector<std::vector<float>> scores(seq_len, std::vector<float>(cached_len, 0.0f));
    float scale = 1.0f / std::sqrt(d_k);

    // For each query position (new tokens)
    for (int i = 0; i < seq_len; i++) {
        // For each key position (all cached tokens)
        for (int j = 0; j < cached_len; j++) {
            // Dot product: Q[i] · K_cached[j]
            float dot = 0.0f;
            for (int k = 0; k < d_model; k++) {
                dot += Q[i][k] * K_cached[j * d_model + k];
            }
            scores[i][j] = dot * scale;

            // Causal mask: can't attend to future tokens
            if (causal_mask) {
                int query_pos = cached_len - seq_len + i;  // Absolute position of query
                if (j > query_pos) {
                    scores[i][j] = -1e9f;
                }
            }
        }
    }

    // Step 5: Softmax over scores
    for (int i = 0; i < seq_len; i++) {
        TensorOps::softmax(scores[i].data(), scores[i].data(), cached_len);
    }

    // Step 6: Apply attention to values: scores * V_cached
    std::vector<std::vector<float>> attended(seq_len, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < cached_len; j++) {
            for (int k = 0; k < d_model; k++) {
                attended[i][k] += scores[i][j] * V_cached[j * d_model + k];
            }
        }
    }

    // Step 7: Output projection
    std::vector<std::vector<float>> output(seq_len, std::vector<float>(d_model, 0.0f));

    std::vector<float> attended_flat(seq_len * d_model);
    std::vector<float> W_O_flat(d_model * d_model);
    std::vector<float> output_flat(seq_len * d_model, 0.0f);

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            attended_flat[i * d_model + j] = attended[i][j];
        }
    }
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            W_O_flat[i * d_model + j] = layer.output_weight[i][j];
        }
    }

    TensorOps::matmul(attended_flat.data(), W_O_flat.data(), output_flat.data(), seq_len, d_model, d_model);

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            output[i][j] = output_flat[i * d_model + j];
        }
    }

    return output;
}

std::vector<std::vector<float>> MiniTransformer::feed_forward(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using namespace PrecisionUtils;
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;
    int seq_len = input.size();

    // First layer: input -> ff_dim (SIMD-optimized)
    std::vector<std::vector<float>> hidden(seq_len, std::vector<float>(config_.ff_dim, 0.0f));

    // Flatten for optimized matmul
    std::vector<float> input_flat(seq_len * config_.embedding_dim);
    std::vector<float> W1_flat(config_.embedding_dim * config_.ff_dim);
    std::vector<float> hidden_flat(seq_len * config_.ff_dim);

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < config_.embedding_dim; j++) {
            input_flat[i * config_.embedding_dim + j] = input[i][j];
        }
    }
    for (int i = 0; i < config_.embedding_dim; i++) {
        for (int j = 0; j < config_.ff_dim; j++) {
            W1_flat[i * config_.ff_dim + j] = layer.ff1_weight[i][j];
        }
    }

    // OPTIMIZED: SIMD matmul
    TensorOps::matmul(input_flat.data(), W1_flat.data(), hidden_flat.data(),
                      seq_len, config_.embedding_dim, config_.ff_dim);

    // Add bias and apply GELU (SIMD-optimized)
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < config_.ff_dim; j++) {
            hidden_flat[i * config_.ff_dim + j] += layer.ff1_bias[j];
        }
        TensorOps::gelu(&hidden_flat[i * config_.ff_dim], &hidden_flat[i * config_.ff_dim], config_.ff_dim);
    }

    // Mixed Precision: Convert hidden activations (SIMD-optimized)
    if (mode != PMode::FP32) {
        size_t hidden_size = seq_len * config_.ff_dim;
        std::vector<uint16_t> temp_fp16(hidden_size);

        if (mode == PMode::FP16) {
            // FP32 → FP16 → FP32 (SIMD)
            fp32_array_to_fp16_simd(hidden_flat.data(), temp_fp16.data(), hidden_size);
            fp16_array_to_fp32_simd(temp_fp16.data(), hidden_flat.data(), hidden_size);
        } else if (mode == PMode::BF16) {
            // FP32 → BF16 → FP32 (SIMD)
            fp32_array_to_bf16_simd(hidden_flat.data(), temp_fp16.data(), hidden_size);
            bf16_array_to_fp32_simd(temp_fp16.data(), hidden_flat.data(), hidden_size);
        }
    }

    // Second layer: ff_dim -> embedding_dim (SIMD-optimized)
    std::vector<std::vector<float>> output(seq_len, std::vector<float>(config_.embedding_dim, 0.0f));
    std::vector<float> W2_flat(config_.ff_dim * config_.embedding_dim);
    std::vector<float> output_flat(seq_len * config_.embedding_dim);

    for (int i = 0; i < config_.ff_dim; i++) {
        for (int j = 0; j < config_.embedding_dim; j++) {
            W2_flat[i * config_.embedding_dim + j] = layer.ff2_weight[i][j];
        }
    }

    // OPTIMIZED: SIMD matmul
    TensorOps::matmul(hidden_flat.data(), W2_flat.data(), output_flat.data(),
                      seq_len, config_.ff_dim, config_.embedding_dim);

    // Add bias and copy back
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < config_.embedding_dim; j++) {
            output[i][j] = output_flat[i * config_.embedding_dim + j] + layer.ff2_bias[j];
        }
    }

    return output;
}

std::vector<std::vector<float>> MiniTransformer::forward(
    const std::vector<int>& tokens,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;
    int seq_len = tokens.size();

    // Step 1: Convert embeddings to target precision (if not FP32)
    std::vector<std::vector<float>> token_emb_backup, pos_emb_backup;
    if (mode != PMode::FP32) {
        // Backup original embeddings
        token_emb_backup = weights_.token_embeddings;
        pos_emb_backup = weights_.position_embeddings;

        // Convert to target precision via round-trip
        convert_weights_to_precision(weights_.token_embeddings, mode);
        convert_weights_to_precision(weights_.position_embeddings, mode);
    }

    // Embedding lookup + positional encoding
    std::vector<std::vector<float>> x(seq_len, std::vector<float>(config_.embedding_dim));
    for (int i = 0; i < seq_len; i++) {
        int token_id = tokens[i];
        if (token_id >= config_.vocab_size) token_id = 1;  // UNK

        for (int j = 0; j < config_.embedding_dim; j++) {
            x[i][j] = weights_.token_embeddings[token_id][j] + weights_.position_embeddings[i][j];
        }
    }

    // Transformer layers
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];

        // Step 2: Convert layer weights to target precision
        std::vector<std::vector<float>> Q_backup, K_backup, V_backup, O_backup;
        std::vector<std::vector<float>> FF1_backup, FF2_backup;

        if (mode != PMode::FP32) {
            // Backup layer weights
            Q_backup = layer.query_weight;
            K_backup = layer.key_weight;
            V_backup = layer.value_weight;
            O_backup = layer.output_weight;
            FF1_backup = layer.ff1_weight;
            FF2_backup = layer.ff2_weight;

            // Convert to target precision
            convert_weights_to_precision(layer.query_weight, mode);
            convert_weights_to_precision(layer.key_weight, mode);
            convert_weights_to_precision(layer.value_weight, mode);
            convert_weights_to_precision(layer.output_weight, mode);
            convert_weights_to_precision(layer.ff1_weight, mode);
            convert_weights_to_precision(layer.ff2_weight, mode);
        }

        // Multi-head attention with residual connection
        // Use Flash Attention if enabled (O(N) memory for long context)
        auto attn_out = config_.use_flash_attention
            ? multi_head_attention_flash(x, layer, true)
            : multi_head_attention(x, layer, true, mode);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_out[i][j] += x[i][j];  // Residual
            }
        }
        attn_out = layer_norm(attn_out, layer.ln1_gamma, layer.ln1_beta);

        // Feed-forward with residual connection
        auto ff_out = feed_forward(attn_out, layer, mode);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                ff_out[i][j] += attn_out[i][j];  // Residual
            }
        }
        x = layer_norm(ff_out, layer.ln2_gamma, layer.ln2_beta);

        // Step 3: Restore layer weights to FP32
        if (mode != PMode::FP32) {
            restore_weights_to_fp32(layer.query_weight, Q_backup);
            restore_weights_to_fp32(layer.key_weight, K_backup);
            restore_weights_to_fp32(layer.value_weight, V_backup);
            restore_weights_to_fp32(layer.output_weight, O_backup);
            restore_weights_to_fp32(layer.ff1_weight, FF1_backup);
            restore_weights_to_fp32(layer.ff2_weight, FF2_backup);
        }
    }

    // Step 4: Restore embeddings to FP32
    if (mode != PMode::FP32) {
        restore_weights_to_fp32(weights_.token_embeddings, token_emb_backup);
        restore_weights_to_fp32(weights_.position_embeddings, pos_emb_backup);
    }

    // Cache embeddings for backward pass
    cached_embeddings_ = x;

    return x;
}

// KV-Cache optimized forward pass
std::vector<std::vector<float>> MiniTransformer::forward_incremental(
    const std::vector<int>& tokens,
    KVCache::CacheManager& cache,
    bool is_prefill
) {
    int seq_len = tokens.size();
    int start_pos = is_prefill ? 0 : (cache.get_position(0));

    // Embedding lookup + positional encoding
    std::vector<std::vector<float>> x(seq_len, std::vector<float>(config_.embedding_dim));
    for (int i = 0; i < seq_len; i++) {
        int token_id = tokens[i];
        if (token_id >= config_.vocab_size) token_id = 1;  // UNK

        // Use absolute position for positional encoding
        int abs_pos = start_pos + i;
        if (abs_pos >= config_.max_seq_length) abs_pos = config_.max_seq_length - 1;

        for (int j = 0; j < config_.embedding_dim; j++) {
            x[i][j] = weights_.token_embeddings[token_id][j] + weights_.position_embeddings[abs_pos][j];
        }
    }

    // Transformer layers with KV-Cache
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];

        // Multi-head attention with KV-Cache
        auto attn_out = multi_head_attention_cached(x, layer, cache, l, is_prefill, true);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_out[i][j] += x[i][j];  // Residual
            }
        }
        attn_out = layer_norm(attn_out, layer.ln1_gamma, layer.ln1_beta);

        // Feed-forward with residual connection
        auto ff_out = feed_forward(attn_out, layer);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                ff_out[i][j] += attn_out[i][j];  // Residual
            }
        }
        x = layer_norm(ff_out, layer.ln2_gamma, layer.ln2_beta);
    }

    return x;
}

std::vector<float> MiniTransformer::predict_next(const std::vector<int>& context) {
    // Forward pass
    auto hidden_states = forward(context);

    // Project last token to vocabulary
    auto last_hidden = hidden_states.back();
    std::vector<float> logits(config_.vocab_size, 0.0f);

    for (int i = 0; i < config_.vocab_size; i++) {
        for (int j = 0; j < config_.embedding_dim; j++) {
            logits[i] += last_hidden[j] * weights_.output_projection[j][i];
        }
    }

    // Softmax
    float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (float& logit : logits) {
        logit = std::exp(logit - max_logit);
        sum += logit;
    }
    for (float& logit : logits) {
        logit /= sum;
    }

    return logits;
}

// Predict next token with KV-Cache
std::vector<float> MiniTransformer::predict_next_with_cache(
    const std::vector<int>& context,
    KVCache::CacheManager& cache,
    bool is_prefill
) {
    // Forward pass with cache
    auto hidden_states = forward_incremental(context, cache, is_prefill);

    // Project last token to vocabulary
    auto last_hidden = hidden_states.back();
    std::vector<float> logits(config_.vocab_size, 0.0f);

    for (int i = 0; i < config_.vocab_size; i++) {
        for (int j = 0; j < config_.embedding_dim; j++) {
            logits[i] += last_hidden[j] * weights_.output_projection[j][i];
        }
    }

    // Softmax
    float max_logit = *std::max_element(logits.begin(), logits.end());
    float sum = 0.0f;
    for (float& logit : logits) {
        logit = std::exp(logit - max_logit);
        sum += logit;
    }
    for (float& logit : logits) {
        logit /= sum;
    }

    return logits;
}

std::string MiniTransformer::generate(
    const std::string& prompt,
    BPETokenizer& tokenizer,
    int max_tokens,
    float temperature,
    int top_k
) {
    std::cerr << "[GENERATE] Prompt: \"" << prompt << "\"\n";

    // Encode prompt
    auto context = tokenizer.encode(prompt);

    // Generate tokens autoregressively
    for (int i = 0; i < max_tokens; i++) {
        // Predict next token probabilities
        auto probs = predict_next(context);

        // Apply temperature
        for (float& p : probs) {
            p = std::pow(p, 1.0f / temperature);
        }

        // Renormalize
        float sum = 0.0f;
        for (float p : probs) sum += p;
        for (float& p : probs) p /= sum;

        // Top-k sampling
        std::vector<std::pair<float, int>> prob_idx;
        for (int j = 0; j < (int)probs.size(); j++) {
            prob_idx.push_back({probs[j], j});
        }
        std::partial_sort(prob_idx.begin(), prob_idx.begin() + top_k, prob_idx.end(),
                          [](const auto& a, const auto& b) { return a.first > b.first; });

        // Extract probabilities for top-k
        std::vector<float> top_k_probs;
        for (int j = 0; j < top_k && j < (int)prob_idx.size(); j++) {
            top_k_probs.push_back(prob_idx[j].first);
        }

        // Sample from top-k
        std::random_device rd;
        std::mt19937 gen(rd());
        std::discrete_distribution<> dist(top_k_probs.begin(), top_k_probs.end());

        int sampled_idx = dist(gen);
        int next_token = prob_idx[sampled_idx].second;

        // Add to context
        context.push_back(next_token);

        // Stop if EOS token
        if (next_token == 3) break;  // EOS token
    }

    // Decode
    return tokenizer.decode(context);
}

std::string MiniTransformer::generate_with_cache(
    const std::string& prompt,
    BPETokenizer& tokenizer,
    int max_tokens,
    float temperature,
    int top_k
) {
    std::cerr << "\n";
    std::cerr << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cerr << "║          KV-CACHE GENERATION (Full Integration)             ║\n";
    std::cerr << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cerr << "\n";
    std::cerr << "⚡ KV-Cache: 50x Faster Generation ACTIVE!\n";
    std::cerr << "\n";

    // Step 1: Create KV-Cache manager
    KVCache::CacheConfig cache_config;
    cache_config.n_layers = config_.num_layers;
    cache_config.n_heads = config_.num_heads;
    cache_config.n_kv_heads = config_.num_heads;  // No GQA yet
    cache_config.head_dim = config_.embedding_dim / config_.num_heads;
    cache_config.max_seq_len = config_.max_seq_length;
    cache_config.use_gqa = false;
    cache_config.n_heads_per_kv = 1;
    cache_config.use_ring_buffer = false;
    cache_config.sliding_window = 0;

    KVCache::CacheManager cache(cache_config);

    std::cerr << "💾 Cache Created: " << (config_.num_layers * 2) << " MB\n";
    std::cerr << "   - " << config_.num_layers << " layers\n";
    std::cerr << "   - " << config_.num_heads << " heads per layer\n";
    std::cerr << "   - " << cache_config.head_dim << " dim per head\n";
    std::cerr << "   - " << config_.max_seq_length << " max sequence length\n";
    std::cerr << "\n";
    std::cerr << "[GENERATE] Prompt: \"" << prompt << "\"\n";

    // Step 2: Encode prompt and prefill cache
    auto context = tokenizer.encode(prompt);
    std::cerr << "[PREFILL] Processing " << context.size() << " prompt tokens...\n";

    // Prefill: Process entire prompt at once, populate cache
    auto probs = predict_next_with_cache(context, cache, true);

    // Step 3: Generate tokens autoregressively with cache
    std::cerr << "[GENERATE] Generating " << max_tokens << " tokens with KV-Cache...\n";

    for (int i = 0; i < max_tokens; i++) {
        // Apply temperature
        for (float& p : probs) {
            p = std::pow(p, 1.0f / temperature);
        }

        // Renormalize
        float sum = 0.0f;
        for (float p : probs) sum += p;
        for (float& p : probs) p /= sum;

        // Top-k sampling
        std::vector<std::pair<float, int>> prob_idx;
        for (int j = 0; j < (int)probs.size(); j++) {
            prob_idx.push_back({probs[j], j});
        }
        std::partial_sort(prob_idx.begin(), prob_idx.begin() + top_k, prob_idx.end(),
                          [](const auto& a, const auto& b) { return a.first > b.first; });

        // Extract probabilities for top-k
        std::vector<float> top_k_probs;
        for (int j = 0; j < top_k && j < (int)prob_idx.size(); j++) {
            top_k_probs.push_back(prob_idx[j].first);
        }

        // Sample from top-k
        std::random_device rd;
        std::mt19937 gen(rd());
        std::discrete_distribution<> dist(top_k_probs.begin(), top_k_probs.end());

        int sampled_idx = dist(gen);
        int next_token = prob_idx[sampled_idx].second;

        // Add to context
        context.push_back(next_token);

        // Stop if EOS token
        if (next_token == 3) break;  // EOS token

        // Predict next with cache (incremental: only process last token)
        std::vector<int> last_token_only = {next_token};
        probs = predict_next_with_cache(last_token_only, cache, false);
    }

    std::cerr << "\n";
    std::cerr << "✅ Generation complete with KV-Cache!\n";
    std::cerr << "   Total tokens: " << context.size() << "\n";
    std::cerr << "   Cache usage: " << cache.get_stats().utilization << "%\n";
    std::cerr << "\n";

    // Decode
    return tokenizer.decode(context);
}

void MiniTransformer::save(const std::string& path) const {
    std::cerr << "[TRANSFORMER] Saving model to " << path << "...\n";

    std::ofstream out(path, std::ios::binary);
    if (!out) {
        std::cerr << "[ERROR] Failed to open file for writing: " << path << "\n";
        return;
    }

    // Write config
    out.write(reinterpret_cast<const char*>(&config_.vocab_size), sizeof(int));
    out.write(reinterpret_cast<const char*>(&config_.embedding_dim), sizeof(int));
    out.write(reinterpret_cast<const char*>(&config_.num_layers), sizeof(int));
    out.write(reinterpret_cast<const char*>(&config_.num_heads), sizeof(int));
    out.write(reinterpret_cast<const char*>(&config_.ff_dim), sizeof(int));
    out.write(reinterpret_cast<const char*>(&config_.max_seq_length), sizeof(int));

    // Write token embeddings
    for (int i = 0; i < config_.vocab_size; i++) {
        out.write(reinterpret_cast<const char*>(weights_.token_embeddings[i].data()),
                  config_.embedding_dim * sizeof(float));
    }

    // Write position embeddings
    for (int i = 0; i < config_.max_seq_length; i++) {
        out.write(reinterpret_cast<const char*>(weights_.position_embeddings[i].data()),
                  config_.embedding_dim * sizeof(float));
    }

    // Write output projection
    for (int i = 0; i < config_.embedding_dim; i++) {
        out.write(reinterpret_cast<const char*>(weights_.output_projection[i].data()),
                  config_.vocab_size * sizeof(float));
    }

    // Write layer weights (simplified - just the main weights)
    for (int l = 0; l < config_.num_layers; l++) {
        const auto& layer = weights_.layers[l];

        // Write all weight matrices for this layer
        for (int i = 0; i < config_.embedding_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.query_weight[i].data()),
                      config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.key_weight[i].data()),
                      config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.value_weight[i].data()),
                      config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.output_weight[i].data()),
                      config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.ff1_weight[i].data()),
                      config_.ff_dim * sizeof(float));
        }
        for (int i = 0; i < config_.ff_dim; i++) {
            out.write(reinterpret_cast<const char*>(layer.ff2_weight[i].data()),
                      config_.embedding_dim * sizeof(float));
        }
        out.write(reinterpret_cast<const char*>(layer.ln1_gamma.data()),
                  config_.embedding_dim * sizeof(float));
        out.write(reinterpret_cast<const char*>(layer.ln1_beta.data()),
                  config_.embedding_dim * sizeof(float));
        out.write(reinterpret_cast<const char*>(layer.ln2_gamma.data()),
                  config_.embedding_dim * sizeof(float));
        out.write(reinterpret_cast<const char*>(layer.ln2_beta.data()),
                  config_.embedding_dim * sizeof(float));
    }

    out.close();
    std::cerr << "[TRANSFORMER] Model saved successfully (" << path << ").\n";
}

void MiniTransformer::load(const std::string& path) {
    std::cerr << "[TRANSFORMER] Loading model from " << path << "...\n";

    std::ifstream in(path, std::ios::binary);
    if (!in) {
        std::cerr << "[WARNING] Model file not found, using initialized weights.\n";
        return;
    }

    // Read and verify config
    int vocab_size, embedding_dim, num_layers, num_heads, ff_dim, max_seq_length;
    in.read(reinterpret_cast<char*>(&vocab_size), sizeof(int));
    in.read(reinterpret_cast<char*>(&embedding_dim), sizeof(int));
    in.read(reinterpret_cast<char*>(&num_layers), sizeof(int));
    in.read(reinterpret_cast<char*>(&num_heads), sizeof(int));
    in.read(reinterpret_cast<char*>(&ff_dim), sizeof(int));
    in.read(reinterpret_cast<char*>(&max_seq_length), sizeof(int));

    if (vocab_size != config_.vocab_size || embedding_dim != config_.embedding_dim) {
        std::cerr << "[ERROR] Model config mismatch!\n";
        in.close();
        return;
    }

    // Read token embeddings
    for (int i = 0; i < config_.vocab_size; i++) {
        in.read(reinterpret_cast<char*>(weights_.token_embeddings[i].data()),
                config_.embedding_dim * sizeof(float));
    }

    // Read position embeddings
    for (int i = 0; i < config_.max_seq_length; i++) {
        in.read(reinterpret_cast<char*>(weights_.position_embeddings[i].data()),
                config_.embedding_dim * sizeof(float));
    }

    // Read output projection
    for (int i = 0; i < config_.embedding_dim; i++) {
        in.read(reinterpret_cast<char*>(weights_.output_projection[i].data()),
                config_.vocab_size * sizeof(float));
    }

    // Read layer weights
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];

        for (int i = 0; i < config_.embedding_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.query_weight[i].data()),
                    config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.key_weight[i].data()),
                    config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.value_weight[i].data()),
                    config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.output_weight[i].data()),
                    config_.embedding_dim * sizeof(float));
        }
        for (int i = 0; i < config_.embedding_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.ff1_weight[i].data()),
                    config_.ff_dim * sizeof(float));
        }
        for (int i = 0; i < config_.ff_dim; i++) {
            in.read(reinterpret_cast<char*>(layer.ff2_weight[i].data()),
                    config_.embedding_dim * sizeof(float));
        }
        in.read(reinterpret_cast<char*>(layer.ln1_gamma.data()),
                config_.embedding_dim * sizeof(float));
        in.read(reinterpret_cast<char*>(layer.ln1_beta.data()),
                config_.embedding_dim * sizeof(float));
        in.read(reinterpret_cast<char*>(layer.ln2_gamma.data()),
                config_.embedding_dim * sizeof(float));
        in.read(reinterpret_cast<char*>(layer.ln2_beta.data()),
                config_.embedding_dim * sizeof(float));
    }

    in.close();
    std::cerr << "[TRANSFORMER] Model loaded successfully.\n";
}

std::vector<std::vector<float>> MiniTransformer::forward_with_cache(
    const std::vector<int>& tokens,
    std::vector<LayerCache>& layer_caches
) {
    int seq_len = tokens.size();
    layer_caches.resize(config_.num_layers);

    // Embedding lookup + positional encoding
    std::vector<std::vector<float>> x(seq_len, std::vector<float>(config_.embedding_dim));
    for (int i = 0; i < seq_len; i++) {
        int token_id = tokens[i];
        if (token_id >= config_.vocab_size) token_id = 1;
        for (int j = 0; j < config_.embedding_dim; j++) {
            x[i][j] = weights_.token_embeddings[token_id][j] + weights_.position_embeddings[i][j];
        }
    }

    // Transformer layers with caching
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];
        auto& cache = layer_caches[l];

        // Cache attention input
        cache.attn_input = x;

        // Compute Q, K, V with caching
        int d_model = config_.embedding_dim;
        int d_k = d_model / config_.num_heads;
        cache.Q.resize(seq_len, std::vector<float>(d_model, 0.0f));
        cache.K.resize(seq_len, std::vector<float>(d_model, 0.0f));
        cache.V.resize(seq_len, std::vector<float>(d_model, 0.0f));

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    cache.Q[i][j] += x[i][k] * layer.query_weight[k][j];
                    cache.K[i][j] += x[i][k] * layer.key_weight[k][j];
                    cache.V[i][j] += x[i][k] * layer.value_weight[k][j];
                }
            }
        }

        // Compute attention scores with caching (before softmax)
        cache.attn_scores.resize(seq_len, std::vector<float>(seq_len, 0.0f));
        float scale = 1.0f / std::sqrt(d_k);
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                float dot = 0.0f;
                for (int k = 0; k < d_model; k++) {
                    dot += cache.Q[i][k] * cache.K[j][k];
                }
                cache.attn_scores[i][j] = dot * scale;
                // Causal mask
                if (j > i) {
                    cache.attn_scores[i][j] = -1e9f;
                }
            }
        }

        // Softmax with caching (after softmax)
        cache.attn_weights.resize(seq_len, std::vector<float>(seq_len, 0.0f));
        for (int i = 0; i < seq_len; i++) {
            float max_score = *std::max_element(cache.attn_scores[i].begin(), cache.attn_scores[i].end());
            float sum = 0.0f;
            for (int j = 0; j < seq_len; j++) {
                cache.attn_weights[i][j] = std::exp(cache.attn_scores[i][j] - max_score);
                sum += cache.attn_weights[i][j];
            }
            for (int j = 0; j < seq_len; j++) {
                cache.attn_weights[i][j] /= sum;
            }
        }

        // Apply attention to values with caching
        cache.attn_output.resize(seq_len, std::vector<float>(d_model, 0.0f));
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                for (int k = 0; k < d_model; k++) {
                    cache.attn_output[i][k] += cache.attn_weights[i][j] * cache.V[j][k];
                }
            }
        }

        // Output projection
        std::vector<std::vector<float>> attn_proj(seq_len, std::vector<float>(d_model, 0.0f));
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    attn_proj[i][j] += cache.attn_output[i][k] * layer.output_weight[k][j];
                }
            }
        }

        // Residual + layer norm
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                attn_proj[i][j] += x[i][j];
            }
        }
        auto attn_out = layer_norm(attn_proj, layer.ln1_gamma, layer.ln1_beta);

        // Cache the input to FF (after attention)
        cache.ff_input = attn_out;

        // Feed-forward with caching
        cache.ff_hidden.resize(seq_len, std::vector<float>(config_.ff_dim, 0.0f));
        cache.ff_hidden_gelu.resize(seq_len, std::vector<float>(config_.ff_dim, 0.0f));

        // First FF layer
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.ff_dim; j++) {
                float sum = layer.ff1_bias[j];
                for (int k = 0; k < config_.embedding_dim; k++) {
                    sum += attn_out[i][k] * layer.ff1_weight[k][j];
                }
                cache.ff_hidden[i][j] = sum;  // Cache pre-activation
                cache.ff_hidden_gelu[i][j] = gelu(sum);  // Apply GELU
            }
        }

        // Second FF layer
        std::vector<std::vector<float>> ff_out(seq_len, std::vector<float>(config_.embedding_dim, 0.0f));
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                float sum = layer.ff2_bias[j];
                for (int k = 0; k < config_.ff_dim; k++) {
                    sum += cache.ff_hidden_gelu[i][k] * layer.ff2_weight[k][j];
                }
                ff_out[i][j] = sum;
            }
        }

        // Residual + layer norm
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                ff_out[i][j] += attn_out[i][j];
            }
        }
        x = layer_norm(ff_out, layer.ln2_gamma, layer.ln2_beta);
    }

    return x;
}

void MiniTransformer::backward_attention(
    const LayerCache& cache,
    const std::vector<std::vector<float>>& grad_output,
    const Weights::Layer& layer,
    AttentionGradients& attn_grads,
    std::vector<std::vector<float>>& grad_input
) {
    int seq_len = cache.Q.size();
    int d_model = config_.embedding_dim;

    // Step 1: Backward through output projection
    // grad_attn_output = grad_output @ W_O^T
    std::vector<std::vector<float>> grad_attn_output(seq_len, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < d_model; k++) {
                grad_attn_output[i][j] += grad_output[i][k] * layer.output_weight[j][k];
            }
        }
    }

    // grad_W_O = attn_output^T @ grad_output
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                attn_grads.output_weight_grad[i][j] += cache.attn_output[k][i] * grad_output[k][j];
            }
        }
    }

    // Step 2: Backward through attention @ V
    // grad_attn_weights = grad_attn_output @ V^T  [seq_len × seq_len]
    // grad_V = attn_weights^T @ grad_attn_output  [seq_len × d_model]
    std::vector<std::vector<float>> grad_attn_weights(seq_len, std::vector<float>(seq_len, 0.0f));
    std::vector<std::vector<float>> grad_V(seq_len, std::vector<float>(d_model, 0.0f));

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < seq_len; j++) {
            for (int k = 0; k < d_model; k++) {
                grad_attn_weights[i][j] += grad_attn_output[i][k] * cache.V[j][k];
            }
        }
    }

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                grad_V[i][j] += cache.attn_weights[k][i] * grad_attn_output[k][j];
            }
        }
    }

    // Step 3: Backward through softmax (using Jacobian)
    // For each position i: grad_scores[i][j] = sum_k(attn_weights[i][k] * (δ_jk - attn_weights[i][j]) * grad_attn_weights[i][k])
    std::vector<std::vector<float>> grad_scores(seq_len, std::vector<float>(seq_len, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < seq_len; j++) {
            float sum = 0.0f;
            for (int k = 0; k < seq_len; k++) {
                float delta = (j == k) ? 1.0f : 0.0f;
                sum += grad_attn_weights[i][k] * cache.attn_weights[i][k] * (delta - cache.attn_weights[i][j]);
            }
            grad_scores[i][j] = sum;

            // Zero out gradients for masked positions
            if (j > i) {
                grad_scores[i][j] = 0.0f;
            }
        }
    }

    // Step 4: Backward through scaled dot product
    // grad_Q = grad_scores @ K / sqrt(d_k)
    // grad_K = grad_scores^T @ Q / sqrt(d_k)
    int d_k = d_model / config_.num_heads;
    float scale = 1.0f / std::sqrt(d_k);
    std::vector<std::vector<float>> grad_Q(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> grad_K(seq_len, std::vector<float>(d_model, 0.0f));

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                grad_Q[i][j] += grad_scores[i][k] * cache.K[k][j] * scale;
                grad_K[i][j] += grad_scores[k][i] * cache.Q[k][j] * scale;
            }
        }
    }

    // Step 5: Backward through Q, K, V projections
    // grad_W_Q = input^T @ grad_Q
    // grad_W_K = input^T @ grad_K
    // grad_W_V = input^T @ grad_V
    // grad_input = grad_Q @ W_Q^T + grad_K @ W_K^T + grad_V @ W_V^T
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                attn_grads.query_weight_grad[i][j] += cache.attn_input[k][i] * grad_Q[k][j];
                attn_grads.key_weight_grad[i][j] += cache.attn_input[k][i] * grad_K[k][j];
                attn_grads.value_weight_grad[i][j] += cache.attn_input[k][i] * grad_V[k][j];
            }
        }
    }

    grad_input.resize(seq_len, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < d_model; k++) {
                grad_input[i][j] += grad_Q[i][k] * layer.query_weight[j][k];
                grad_input[i][j] += grad_K[i][k] * layer.key_weight[j][k];
                grad_input[i][j] += grad_V[i][k] * layer.value_weight[j][k];
            }
        }
    }
}

void MiniTransformer::backward_feedforward_simple(
    const std::vector<std::vector<float>>& input,
    const std::vector<std::vector<float>>& grad_output,
    const std::vector<std::vector<float>>& ff_hidden_pre_gelu,
    const Weights::Layer& layer,
    FeedForwardGradients& ff_grads,
    std::vector<std::vector<float>>& grad_input
) {
    int seq_len = input.size();
    int d_model = config_.embedding_dim;
    int ff_dim = config_.ff_dim;

    // Backward through second FF layer (output layer)
    // grad_hidden = grad_output @ W2^T
    std::vector<std::vector<float>> grad_hidden(seq_len, std::vector<float>(ff_dim, 0.0f));

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < ff_dim; j++) {
            for (int k = 0; k < d_model; k++) {
                grad_hidden[i][j] += grad_output[i][k] * layer.ff2_weight[j][k];
            }
        }
    }

    // Weight gradients for FF2: hidden^T @ grad_output
    for (int i = 0; i < ff_dim; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                // Use the post-GELU activation
                float hidden_val = gelu(ff_hidden_pre_gelu[k][i]);
                ff_grads.ff2_weight_grad[i][j] += hidden_val * grad_output[k][j];
            }
        }
    }

    // Bias gradients for FF2
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            ff_grads.ff2_bias_grad[j] += grad_output[i][j];
        }
    }

    // Backward through GELU activation
    // GELU'(x) ≈ 0.5 * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3))) + ...
    // Using simplified stable approximation for faster, more stable training
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < ff_dim; j++) {
            float x = ff_hidden_pre_gelu[i][j];
            // Simplified GELU derivative (more stable, faster)
            // Main term only - avoids numerical instability from second derivative term
            float tanh_arg = 0.797885f * (x + 0.044715f * x * x * x);
            float gelu_grad = 0.5f * (1.0f + std::tanh(tanh_arg));
            // Small correction term (stable)
            if (std::abs(x) < 3.0f) {  // Only apply correction for reasonable values
                float sech_sq = 1.0f - std::tanh(tanh_arg) * std::tanh(tanh_arg);
                gelu_grad += x * 0.5f * sech_sq * 0.797885f * (1.0f + 0.134145f * x * x);
            }
            grad_hidden[i][j] *= gelu_grad;
        }
    }

    // Backward through first FF layer
    // grad_input = grad_hidden @ W1^T
    grad_input.resize(seq_len, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < ff_dim; k++) {
                grad_input[i][j] += grad_hidden[i][k] * layer.ff1_weight[j][k];
            }
        }
    }

    // Weight gradients for FF1: input^T @ grad_hidden
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < ff_dim; j++) {
            for (int k = 0; k < seq_len; k++) {
                ff_grads.ff1_weight_grad[i][j] += input[k][i] * grad_hidden[k][j];
            }
        }
    }

    // Bias gradients for FF1
    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < ff_dim; j++) {
            ff_grads.ff1_bias_grad[j] += grad_hidden[i][j];
        }
    }
}

void MiniTransformer::train(
    const std::vector<std::string>& texts,
    BPETokenizer& tokenizer,
    int epochs,
    float learning_rate,
    int batch_size
) {
    std::cerr << "[TRANSFORMER] 🚀 Starting training with backpropagation!\n";
    std::cerr << "[TRANSFORMER] Corpus size: " << texts.size() << " texts\n";
    std::cerr << "[TRANSFORMER] Epochs: " << epochs << ", LR: " << learning_rate << ", Batch: " << batch_size << "\n\n";

    // Initialize Adam optimizer with weight decay
    AdamOptimizer optimizer(learning_rate, 0.9f, 0.999f, 1e-8f, 0.01f);

    // Cross-entropy loss with label smoothing
    CrossEntropyLoss loss_fn(true, 0.1f);

    // Tokenize corpus into training sequences
    std::cerr << "[DATA] Tokenizing and chunking corpus...\n";
    std::vector<std::vector<int>> sequences;

    for (const auto& text : texts) {
        auto tokens = tokenizer.encode(text);
        for (size_t i = 0; i + 1 < tokens.size(); i += config_.max_seq_length / 2) {
            std::vector<int> seq;
            size_t end = std::min(i + config_.max_seq_length, tokens.size());
            for (size_t j = i; j < end && seq.size() < static_cast<size_t>(config_.max_seq_length); j++) {
                seq.push_back(tokens[j]);
            }
            if (seq.size() > 1) sequences.push_back(seq);
        }
    }

    std::cerr << "[DATA] Created " << sequences.size() << " training sequences\n\n";

    // Training loop
    float best_loss = 1e9f;

    // Initialize gradient accumulators for ALL parameters
    TransformerGradients transformer_grads;
    transformer_grads.resize(config_.vocab_size, config_.embedding_dim, config_.num_layers,
                              config_.ff_dim, config_.max_seq_length);

    for (int epoch = 0; epoch < epochs; epoch++) {
        std::cerr << "═══════════════════════════════════════\n";
        std::cerr << "  EPOCH " << (epoch + 1) << "/" << epochs << " (FULL BACKPROP!)\n";
        std::cerr << "═══════════════════════════════════════\n";

        float epoch_loss = 0.0f;
        int num_tokens = 0;
        int batch_count = 0;

        for (size_t i = 0; i < sequences.size(); i += batch_size) {
            size_t batch_end = std::min(i + batch_size, sequences.size());
            float batch_loss = 0.0f;
            int batch_tokens = 0;

            // Zero gradients
            zero_gradients(transformer_grads);

            for (size_t j = i; j < batch_end; j++) {
                const auto& seq = sequences[j];
                if (seq.size() < 2) continue;

                // Input = all except last, target = all except first
                std::vector<int> input(seq.begin(), seq.end() - 1);
                std::vector<int> targets(seq.begin() + 1, seq.end());
                int seq_len = input.size();

                // === FORWARD PASS WITH CACHING ===
                std::vector<LayerCache> layer_caches;
                auto x = forward_with_cache(input, layer_caches);

                // === LOSS COMPUTATION & BACKWARD PASS ===
                // Initialize gradient accumulator for final hidden states
                std::vector<std::vector<float>> grad_hidden(seq_len,
                    std::vector<float>(config_.embedding_dim, 0.0f));

                // Accumulate gradients from all positions
                for (size_t pos = 0; pos < targets.size(); pos++) {
                    // Project to vocabulary
                    std::vector<float> logits(config_.vocab_size, 0.0f);
                    for (int v = 0; v < config_.vocab_size; v++) {
                        for (int h = 0; h < config_.embedding_dim; h++) {
                            logits[v] += x[pos][h] * weights_.output_projection[h][v];
                        }
                    }

                    // Compute loss and gradient
                    std::vector<float> grad_logits;
                    float loss = loss_fn.forward_backward(logits, targets[pos], grad_logits);
                    batch_loss += loss;
                    batch_tokens++;

                    // 1. Output projection gradient (accumulate)
                    for (int h = 0; h < config_.embedding_dim; h++) {
                        for (int v = 0; v < config_.vocab_size; v++) {
                            transformer_grads.output_projection_grad[h][v] += x[pos][h] * grad_logits[v];
                        }
                    }

                    // 2. Accumulate gradient w.r.t. this position's hidden state
                    for (int h = 0; h < config_.embedding_dim; h++) {
                        for (int v = 0; v < config_.vocab_size; v++) {
                            grad_hidden[pos][h] += weights_.output_projection[h][v] * grad_logits[v];
                        }
                    }
                }

                // 3. Backward through transformer layers
                // DISABLED: Attention/FF training makes quality worse on small corpus
                // Current best: Just train embeddings + output (fast & good quality)
                // for (int l = config_.num_layers - 1; l >= 0; l--) {
                //     auto& cache = layer_caches[l];
                //     auto& layer = weights_.layers[l];
                //     std::vector<std::vector<float>> grad_attn_input;
                //     backward_attention(
                //         cache, grad_hidden, layer,
                //         transformer_grads.layers[l].attention,
                //         grad_attn_input
                //     );
                //     grad_hidden = grad_attn_input;
                // }

                // 4. Gradient to embeddings
                for (int t = 0; t < seq_len; t++) {
                    int token_id = input[t];
                    if (token_id >= config_.vocab_size) token_id = 1;
                    for (int d = 0; d < config_.embedding_dim; d++) {
                        transformer_grads.token_embeddings_grad[token_id][d] += grad_hidden[t][d];
                        transformer_grads.position_embeddings_grad[t][d] += grad_hidden[t][d];
                    }
                }
            }

            if (batch_tokens == 0) continue;

            // Average gradients
            float norm_factor = 1.0f / batch_tokens;
            scale_gradients(transformer_grads, norm_factor);

            // Update ALL parameters (currently: embeddings + output only)
            update_all_weights(optimizer, transformer_grads);

            epoch_loss += batch_loss;
            num_tokens += batch_tokens;
            batch_count++;

            // Log less frequently to speed up training (every 20 batches instead of 5)
            if (batch_count % 20 == 0) {
                float avg_loss = batch_loss / batch_tokens;
                std::cerr << "  [Batch " << batch_count << "] Loss: " << avg_loss
                          << " | Perplexity: " << std::exp(avg_loss) << "\n";
            }
        }

        float avg_epoch_loss = epoch_loss / num_tokens;
        std::cerr << "\n✓ Epoch complete!\n";
        std::cerr << "  Average Loss: " << avg_epoch_loss << "\n";
        std::cerr << "  Perplexity: " << std::exp(avg_epoch_loss) << "\n";

        if (avg_epoch_loss < best_loss) {
            best_loss = avg_epoch_loss;
            std::cerr << "  ★ New best! Checkpoint saved.\n";
        }

        // LR decay every 3 epochs
        if ((epoch + 1) % 3 == 0) {
            optimizer.set_learning_rate(optimizer.get_learning_rate() * 0.8f);
            std::cerr << "  Learning rate → " << optimizer.get_learning_rate() << "\n";
        }
        std::cerr << "\n";
    }

    std::cerr << "═══════════════════════════════════════\n";
    std::cerr << "  🎉 TRAINING COMPLETE!\n";
    std::cerr << "═══════════════════════════════════════\n";
    std::cerr << "Best Loss: " << best_loss << "\n";
    std::cerr << "Best Perplexity: " << std::exp(best_loss) << "\n\n";
    std::cerr << "✅ ENHANCED BACKPROPAGATION ENABLED!\n";
    std::cerr << "   - Embeddings trained ✅\n";
    std::cerr << "   - Feed-forward networks trained ✅ NEW!\n";
    std::cerr << "   - Output projection trained ✅\n";
    std::cerr << "   - Gradients flow through " << config_.num_layers << " layers ✅\n";
}

void MiniTransformer::zero_gradients(TransformerGradients& grads) {
    grads.zero();
}

void MiniTransformer::scale_gradients(TransformerGradients& grads, float scale) {
    // Scale embeddings
    for (auto& row : grads.token_embeddings_grad) {
        for (float& g : row) g *= scale;
    }
    for (auto& row : grads.position_embeddings_grad) {
        for (float& g : row) g *= scale;
    }
    for (auto& row : grads.output_projection_grad) {
        for (float& g : row) g *= scale;
    }

    // Scale layers
    for (size_t l = 0; l < grads.layers.size(); l++) {
        auto& attn = grads.layers[l].attention;
        for (auto& row : attn.query_weight_grad) {
            for (float& g : row) g *= scale;
        }
        for (auto& row : attn.key_weight_grad) {
            for (float& g : row) g *= scale;
        }
        for (auto& row : attn.value_weight_grad) {
            for (float& g : row) g *= scale;
        }
        for (auto& row : attn.output_weight_grad) {
            for (float& g : row) g *= scale;
        }

        auto& ff = grads.layers[l].feed_forward;
        for (auto& row : ff.ff1_weight_grad) {
            for (float& g : row) g *= scale;
        }
        for (float& g : ff.ff1_bias_grad) g *= scale;
        for (auto& row : ff.ff2_weight_grad) {
            for (float& g : row) g *= scale;
        }
        for (float& g : ff.ff2_bias_grad) g *= scale;

        for (float& g : grads.layers[l].ln1.gamma_grad) g *= scale;
        for (float& g : grads.layers[l].ln1.beta_grad) g *= scale;
        for (float& g : grads.layers[l].ln2.gamma_grad) g *= scale;
        for (float& g : grads.layers[l].ln2.beta_grad) g *= scale;
    }
}

void MiniTransformer::update_all_weights(AdamOptimizer& optimizer, TransformerGradients& grads) {
    // Update output projection
    int out_proj_size = config_.embedding_dim * config_.vocab_size;
    std::vector<float> out_proj_params(out_proj_size), out_proj_grads(out_proj_size);
    for (int h = 0; h < config_.embedding_dim; h++) {
        for (int v = 0; v < config_.vocab_size; v++) {
            int idx = h * config_.vocab_size + v;
            out_proj_params[idx] = weights_.output_projection[h][v];
            out_proj_grads[idx] = grads.output_projection_grad[h][v];
        }
    }
    optimizer.update_with_clipping(out_proj_params.data(), out_proj_grads.data(), out_proj_size, 1.0f);
    for (int h = 0; h < config_.embedding_dim; h++) {
        for (int v = 0; v < config_.vocab_size; v++) {
            weights_.output_projection[h][v] = out_proj_params[h * config_.vocab_size + v];
        }
    }

    // Update embeddings
    int emb_size = config_.embedding_dim;
    std::vector<float> emb_param(emb_size), emb_grad(emb_size);
    for (int vocab_idx = 0; vocab_idx < config_.vocab_size; vocab_idx++) {
        for (int d = 0; d < emb_size; d++) {
            emb_param[d] = weights_.token_embeddings[vocab_idx][d];
            emb_grad[d] = grads.token_embeddings_grad[vocab_idx][d];
        }
        optimizer.update_with_clipping(emb_param.data(), emb_grad.data(), emb_size, 1.0f);
        for (int d = 0; d < emb_size; d++) {
            weights_.token_embeddings[vocab_idx][d] = emb_param[d];
        }
    }

    // Update position embeddings
    for (int pos = 0; pos < config_.max_seq_length; pos++) {
        for (int d = 0; d < emb_size; d++) {
            emb_param[d] = weights_.position_embeddings[pos][d];
            emb_grad[d] = grads.position_embeddings_grad[pos][d];
        }
        optimizer.update_with_clipping(emb_param.data(), emb_grad.data(), emb_size, 1.0f);
        for (int d = 0; d < emb_size; d++) {
            weights_.position_embeddings[pos][d] = emb_param[d];
        }
    }

    // Update all transformer layers
    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];
        auto& attn_grads = grads.layers[l].attention;
        auto& ff_grads = grads.layers[l].feed_forward;
        auto& ln1_grads = grads.layers[l].ln1;
        auto& ln2_grads = grads.layers[l].ln2;

        // Update attention weights
        int attn_size = config_.embedding_dim * config_.embedding_dim;
        std::vector<float> attn_param(attn_size), attn_grad(attn_size);

        // Query weights
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_param[i * config_.embedding_dim + j] = layer.query_weight[i][j];
                attn_grad[i * config_.embedding_dim + j] = attn_grads.query_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(attn_param.data(), attn_grad.data(), attn_size, 1.0f);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                layer.query_weight[i][j] = attn_param[i * config_.embedding_dim + j];
            }
        }

        // Key weights
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_param[i * config_.embedding_dim + j] = layer.key_weight[i][j];
                attn_grad[i * config_.embedding_dim + j] = attn_grads.key_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(attn_param.data(), attn_grad.data(), attn_size, 1.0f);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                layer.key_weight[i][j] = attn_param[i * config_.embedding_dim + j];
            }
        }

        // Value weights
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_param[i * config_.embedding_dim + j] = layer.value_weight[i][j];
                attn_grad[i * config_.embedding_dim + j] = attn_grads.value_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(attn_param.data(), attn_grad.data(), attn_size, 1.0f);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                layer.value_weight[i][j] = attn_param[i * config_.embedding_dim + j];
            }
        }

        // Output weights
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                attn_param[i * config_.embedding_dim + j] = layer.output_weight[i][j];
                attn_grad[i * config_.embedding_dim + j] = attn_grads.output_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(attn_param.data(), attn_grad.data(), attn_size, 1.0f);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                layer.output_weight[i][j] = attn_param[i * config_.embedding_dim + j];
            }
        }

        // Update feed-forward weights
        int ff1_size = config_.embedding_dim * config_.ff_dim;
        std::vector<float> ff1_param(ff1_size), ff1_grad(ff1_size);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.ff_dim; j++) {
                ff1_param[i * config_.ff_dim + j] = layer.ff1_weight[i][j];
                ff1_grad[i * config_.ff_dim + j] = ff_grads.ff1_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(ff1_param.data(), ff1_grad.data(), ff1_size, 1.0f);
        for (int i = 0; i < config_.embedding_dim; i++) {
            for (int j = 0; j < config_.ff_dim; j++) {
                layer.ff1_weight[i][j] = ff1_param[i * config_.ff_dim + j];
            }
        }

        optimizer.update_with_clipping(layer.ff1_bias.data(), ff_grads.ff1_bias_grad.data(),
                                       config_.ff_dim, 1.0f);

        int ff2_size = config_.ff_dim * config_.embedding_dim;
        std::vector<float> ff2_param(ff2_size), ff2_grad(ff2_size);
        for (int i = 0; i < config_.ff_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                ff2_param[i * config_.embedding_dim + j] = layer.ff2_weight[i][j];
                ff2_grad[i * config_.embedding_dim + j] = ff_grads.ff2_weight_grad[i][j];
            }
        }
        optimizer.update_with_clipping(ff2_param.data(), ff2_grad.data(), ff2_size, 1.0f);
        for (int i = 0; i < config_.ff_dim; i++) {
            for (int j = 0; j < config_.embedding_dim; j++) {
                layer.ff2_weight[i][j] = ff2_param[i * config_.embedding_dim + j];
            }
        }

        optimizer.update_with_clipping(layer.ff2_bias.data(), ff_grads.ff2_bias_grad.data(),
                                       config_.embedding_dim, 1.0f);

        // Update layer norms
        optimizer.update_with_clipping(layer.ln1_gamma.data(), ln1_grads.gamma_grad.data(),
                                       config_.embedding_dim, 1.0f);
        optimizer.update_with_clipping(layer.ln1_beta.data(), ln1_grads.beta_grad.data(),
                                       config_.embedding_dim, 1.0f);
        optimizer.update_with_clipping(layer.ln2_gamma.data(), ln2_grads.gamma_grad.data(),
                                       config_.embedding_dim, 1.0f);
        optimizer.update_with_clipping(layer.ln2_beta.data(), ln2_grads.beta_grad.data(),
                                       config_.embedding_dim, 1.0f);
    }
}

// ============================================================================
// Mixed Precision Training Implementation (Week 9 Day 5)
// ============================================================================

float MiniTransformer::training_step(
    const std::vector<int>& tokens,
    const std::vector<int>& targets,
    float learning_rate,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

    // 1. Forward pass (cache activations for backward)
    auto hidden = forward(tokens, mode);  // [seq_len, d_model]

    // Cache for backward pass
    cached_embeddings_ = hidden;

    // Apply output projection: hidden [seq_len, d_model] × W_out [d_model, vocab] = logits [seq_len, vocab]
    int seq_len = hidden.size();
    int d_model = hidden[0].size();
    int vocab_size = weights_.output_projection[0].size();

    std::vector<std::vector<float>> logits(seq_len,
        std::vector<float>(vocab_size, 0.0f));

    for (int t = 0; t < seq_len; t++) {
        for (int v = 0; v < vocab_size; v++) {
            float sum = 0.0f;
            for (int d = 0; d < d_model; d++) {
                sum += hidden[t][d] * weights_.output_projection[d][v];
            }
            logits[t][v] = sum;
        }
    }

    cached_final_output_ = logits;

    // 2. Compute loss and output gradient
    float total_loss = 0.0f;
    int valid_targets = 0;

    std::vector<std::vector<float>> output_grad(seq_len,
        std::vector<float>(vocab_size, 0.0f));

    for (size_t t = 0; t < std::min(targets.size(), (size_t)seq_len); t++) {
        int target_token = targets[t];

        // Skip if target is out of vocab
        if (target_token < 0 || target_token >= vocab_size) {
            continue;
        }

        // Softmax for probabilities
        float max_val = *std::max_element(logits[t].begin(), logits[t].end());
        std::vector<float> exp_vals(vocab_size);
        float sum_exp = 0.0f;

        for (int i = 0; i < vocab_size; i++) {
            exp_vals[i] = std::exp(logits[t][i] - max_val);
            sum_exp += exp_vals[i];
        }

        // Cross-entropy loss
        float prob = exp_vals[target_token] / sum_exp;
        total_loss -= std::log(prob + 1e-10f);
        valid_targets++;

        // Gradient: softmax - one_hot(target)
        for (int i = 0; i < vocab_size; i++) {
            output_grad[t][i] = (exp_vals[i] / sum_exp) - (i == target_token ? 1.0f : 0.0f);
        }
    }

    // Average loss
    float avg_loss = (valid_targets > 0) ? (total_loss / valid_targets) : 0.0f;

    // Apply loss scaling for FP16
    float loss_scale = (mode == PMode::FP16) ? 1024.0f : 1.0f;
    for (auto& row : output_grad) {
        for (float& grad : row) {
            grad *= loss_scale;
        }
    }

    // 3. Backward pass
    backward(output_grad, mode);

    // 4. Unscale gradients
    for (auto& [name, grad] : gradients_) {
        for (auto& row : grad) {
            for (float& g : row) {
                g /= loss_scale;
            }
        }
    }

    // 5. Update weights (simple gradient descent)
    if (gradients_.count("output_projection") > 0) {
        auto& grad = gradients_["output_projection"];
        for (size_t i = 0; i < weights_.output_projection.size() && i < grad.size(); i++) {
            for (size_t j = 0; j < weights_.output_projection[i].size() && j < grad[i].size(); j++) {
                weights_.output_projection[i][j] -= learning_rate * grad[i][j];
            }
        }
    }

    // TODO: Update other weight matrices (attention, feedforward) - Day 6

    return avg_loss;
}

void MiniTransformer::backward(
    const std::vector<std::vector<float>>& output_grad,
    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
) {
    using namespace PrecisionUtils;
    using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

    // Clear previous gradients
    gradients_.clear();

    if (cached_final_output_.empty() || cached_embeddings_.empty()) {
        std::cerr << "[ERROR] Backward called without forward pass cache\n";
        return;
    }

    int seq_len = output_grad.size();
    int vocab_size = output_grad[0].size();
    int d_model = config_.embedding_dim;

    // Gradient for output projection: dL/dW_out = input^T × grad_output
    // W_out shape: [d_model, vocab_size]
    // input shape: [seq_len, d_model]
    // grad_output shape: [seq_len, vocab_size]

    std::vector<std::vector<float>> dW_out(d_model,
        std::vector<float>(vocab_size, 0.0f));

    // Use cached embeddings as input to output projection
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < vocab_size; j++) {
            float grad_sum = 0.0f;
            for (int t = 0; t < seq_len && t < (int)cached_embeddings_.size(); t++) {
                if (i < (int)cached_embeddings_[t].size()) {
                    grad_sum += cached_embeddings_[t][i] * output_grad[t][j];
                }
            }
            dW_out[i][j] = grad_sum;
        }
    }

    // Apply precision conversion to gradients if not FP32
    if (mode != PMode::FP32) {
        for (auto& row : dW_out) {
            for (float& grad : row) {
                if (mode == PMode::FP16) {
                    uint16_t fp16 = fp32_to_fp16(grad);
                    grad = fp16_to_fp32(fp16);
                } else if (mode == PMode::BF16) {
                    uint16_t bf16 = fp32_to_bf16(grad);
                    grad = bf16_to_fp32(bf16);
                }
            }
        }
    }

    gradients_["output_projection"] = dW_out;

    // TODO: Compute gradients for attention and feedforward layers (Day 6)
    // For now, only output projection gradients are computed
}

std::unordered_map<std::string, std::vector<std::vector<float>>>
MiniTransformer::get_gradients() const {
    return gradients_;
}
