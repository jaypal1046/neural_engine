#ifndef MINI_TRANSFORMER_H
#define MINI_TRANSFORMER_H

#include "bpe_tokenizer.h"
#include "real_embeddings.h"
#include "optimizer.h"
#include "loss.h"
#include "transformer_gradients.h"
#include <vector>
#include <string>
#include <memory>

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

    // Get next token probabilities
    std::vector<float> predict_next(const std::vector<int>& context);

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
    std::vector<std::vector<float>> forward(const std::vector<int>& tokens);
    std::vector<std::vector<float>> multi_head_attention(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer,
        bool causal_mask = true
    );
    std::vector<std::vector<float>> feed_forward(
        const std::vector<std::vector<float>>& input,
        const Weights::Layer& layer
    );
    std::vector<std::vector<float>> layer_norm(
        const std::vector<std::vector<float>>& input,
        const std::vector<float>& gamma,
        const std::vector<float>& beta
    );

    // Activation functions
    float gelu(float x) const;
    float softmax_temperature(float x, float temp) const;

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
};

#endif // MINI_TRANSFORMER_H
