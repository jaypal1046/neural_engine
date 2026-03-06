#ifndef MINI_TRANSFORMER_H
#define MINI_TRANSFORMER_H

#include "bpe_tokenizer.h"
#include "real_embeddings.h"
#include "optimizer.h"
#include "loss.h"
#include "transformer_gradients.h"
#include "kv_cache.h"
#include "precision_utils.h"
#include "mixed_precision.h"
#include <vector>
#include <string>
#include <memory>
#include <unordered_map>

// Mini-Transformer: Real AI Language Model
// Architecture: GPT-style decoder-only transformer
// - Multi-head self-attention
// - Feed-forward networks
// - Layer normalization
// - Autoregressive text generation

struct TransformerConfig {
    int vocab_size = 32000;
    int embedding_dim = 512;      // Hidden size
    int num_layers = 6;           // Transformer blocks
    int num_heads = 8;            // Attention heads
    int ff_dim = 2048;            // Feed-forward dimension
    int max_seq_length = 512;     // Maximum context length
    float dropout = 0.1f;
    bool use_flash_attention = false;  // Enable Flash Attention v2 (O(N) memory)
};

class MiniTransformer {
public:
    MiniTransformer(const TransformerConfig& config);

    // Training
    void train(
        const std::vector<std::string>& texts,
        BPETokenizer& tokenizer,
        int epochs = 10,
        float learning_rate = 0.0001f,
        int batch_size = 32
    );

    // Inference: Generate text
    std::string generate(
        const std::string& prompt,
        BPETokenizer& tokenizer,
        int max_tokens = 50,
        float temperature = 0.8f,
        int top_k = 40
    );

    // Inference: Generate with KV-Cache (50x faster)
    std::string generate_with_cache(
        const std::string& prompt,
        BPETokenizer& tokenizer,
        int max_tokens = 50,
        float temperature = 0.8f,
        int top_k = 40
    );

    // Get next token probabilities
    std::vector<float> predict_next(const std::vector<int>& context);

    // Test forward pass with specific precision mode (for testing)
    std::vector<std::vector<float>> test_forward(
        const std::vector<int>& tokens,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    ) {
        return forward(tokens, mode);
    }

    // Persistence
    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    TransformerConfig config_;

    // Model weights (simplified - in reality would be matrices)
    struct Weights {
        // Token embeddings
        std::vector<std::vector<float>> token_embeddings;     // [vocab_size, embedding_dim]
        std::vector<std::vector<float>> position_embeddings;  // [max_seq_length, embedding_dim]

        // Transformer layers
        struct Layer {
            // Multi-head attention
            std::vector<std::vector<float>> query_weight;   // [embedding_dim, embedding_dim]
            std::vector<std::vector<float>> key_weight;
            std::vector<std::vector<float>> value_weight;
            std::vector<std::vector<float>> output_weight;

            // Feed-forward
            std::vector<std::vector<float>> ff1_weight;     // [embedding_dim, ff_dim]
            std::vector<float> ff1_bias;
            std::vector<std::vector<float>> ff2_weight;     // [ff_dim, embedding_dim]
            std::vector<float> ff2_bias;

            // Layer norm
            std::vector<float> ln1_gamma;
            std::vector<float> ln1_beta;
            std::vector<float> ln2_gamma;
            std::vector<float> ln2_beta;
        };

        std::vector<Layer> layers;

        // Output head
        std::vector<std::vector<float>> output_projection;  // [embedding_dim, vocab_size]
    };

    Weights weights_;

    // Forward pass helpers
    std::vector<std::vector<float>> forward(
        const std::vector<int>& tokens,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    );

    // KV-Cache optimized forward pass (for incremental generation)
    std::vector<std::vector<float>> forward_incremental(
        const std::vector<int>& tokens,
        KVCache::CacheManager& cache,
        bool is_prefill
    );

    // Predict next token with KV-Cache
    std::vector<float> predict_next_with_cache(
        const std::vector<int>& context,
        KVCache::CacheManager& cache,
        bool is_prefill
    );

    std::vector<std::vector<float>> multi_head_attention(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer,
        bool causal_mask = true,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    );

    // Flash Attention v2 (memory-efficient, long context)
    std::vector<std::vector<float>> multi_head_attention_flash(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer,
        bool causal_mask = true
    );

    // KV-Cache optimized attention
    std::vector<std::vector<float>> multi_head_attention_cached(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer,
        KVCache::CacheManager& cache,
        int layer_idx,
        bool is_prefill,
        bool causal_mask = true
    );

    std::vector<std::vector<float>> feed_forward(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    );
    std::vector<std::vector<float>> layer_norm(
        const std::vector<std::vector<float>>& input,
        const std::vector<float>& gamma,
        const std::vector<float>& beta
    );

    // Activation functions
    float gelu(float x) const;
    float gelu_derivative(float x) const;  // For backward pass
    float softmax_temperature(float x, float temp) const;

    // Precision conversion helpers (for mixed precision forward pass)
    void convert_weights_to_precision(
        std::vector<std::vector<float>>& weights,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode
    );
    void restore_weights_to_fp32(
        std::vector<std::vector<float>>& weights,
        const std::vector<std::vector<float>>& backup
    );

    // Initialization
    void initialize_weights();
    void xavier_init(std::vector<std::vector<float>>& weight, int in_dim, int out_dim);

    // Training helpers
    void zero_gradients(TransformerGradients& grads);
    void scale_gradients(TransformerGradients& grads, float scale);
    void update_all_weights(AdamOptimizer& optimizer, TransformerGradients& grads);

    // Forward/backward with caching for training
    struct LayerCache {
        // Attention caching
        std::vector<std::vector<float>> attn_input;  // Input to attention
        std::vector<std::vector<float>> Q, K, V;     // Query, Key, Value matrices
        std::vector<std::vector<float>> attn_scores; // Pre-softmax scores
        std::vector<std::vector<float>> attn_weights; // Post-softmax attention
        std::vector<std::vector<float>> attn_output;  // Attention @ V

        // Feed-forward caching
        std::vector<std::vector<float>> ff_input;     // Input to FF
        std::vector<std::vector<float>> ff_hidden;    // Pre-GELU
        std::vector<std::vector<float>> ff_hidden_gelu; // Post-GELU
    };

    std::vector<std::vector<float>> forward_with_cache(
        const std::vector<int>& tokens,
        std::vector<LayerCache>& layer_caches
    );

    void backward_feedforward_simple(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<std::vector<float>>& ff_hidden_pre_gelu,
        const Weights::Layer& layer,
        FeedForwardGradients& ff_grads,
        std::vector<std::vector<float>>& grad_input
    );

    void backward_attention(
        const LayerCache& cache,
        const std::vector<std::vector<float>>& grad_output,
        const Weights::Layer& layer,
        AttentionGradients& attn_grads,
        std::vector<std::vector<float>>& grad_input
    );

public:
    // ========================================================================
    // Mixed Precision Training API (Week 9 Day 5)
    // ========================================================================

    // Training step with mixed precision support
    float training_step(
        const std::vector<int>& tokens,
        const std::vector<int>& targets,
        float learning_rate,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    );

    // Backward pass with mixed precision
    void backward(
        const std::vector<std::vector<float>>& output_grad,
        MixedPrecision::MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP32
    );

    // Get gradients after backward pass
    std::unordered_map<std::string, std::vector<std::vector<float>>> get_gradients() const;

private:
    // Gradient storage
    std::unordered_map<std::string, std::vector<std::vector<float>>> gradients_;

    // Cache forward pass activations (needed for backward)
    std::vector<std::vector<float>> cached_embeddings_;
    std::vector<std::vector<std::vector<float>>> cached_layer_outputs_;
    std::vector<std::vector<float>> cached_final_output_;
};

#endif // MINI_TRANSFORMER_H
