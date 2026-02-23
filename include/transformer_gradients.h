#ifndef TRANSFORMER_GRADIENTS_H
#define TRANSFORMER_GRADIENTS_H

#include <vector>

// Gradient structures for transformer components
// These store gradients for backpropagation

struct AttentionGradients {
    // Gradients for attention weights
    std::vector<std::vector<float>> query_weight_grad;   // [d_model, d_model]
    std::vector<std::vector<float>> key_weight_grad;
    std::vector<std::vector<float>> value_weight_grad;
    std::vector<std::vector<float>> output_weight_grad;

    void resize(int d_model);
    void zero();
};

struct FeedForwardGradients {
    // Gradients for feed-forward weights
    std::vector<std::vector<float>> ff1_weight_grad;     // [d_model, ff_dim]
    std::vector<float> ff1_bias_grad;
    std::vector<std::vector<float>> ff2_weight_grad;     // [ff_dim, d_model]
    std::vector<float> ff2_bias_grad;

    void resize(int d_model, int ff_dim);
    void zero();
};

struct LayerNormGradients {
    // Gradients for layer norm parameters
    std::vector<float> gamma_grad;
    std::vector<float> beta_grad;

    void resize(int d_model);
    void zero();
};

struct TransformerLayerGradients {
    AttentionGradients attention;
    FeedForwardGradients feed_forward;
    LayerNormGradients ln1;
    LayerNormGradients ln2;

    void resize(int d_model, int ff_dim);
    void zero();
};

struct TransformerGradients {
    // Token and position embedding gradients
    std::vector<std::vector<float>> token_embeddings_grad;
    std::vector<std::vector<float>> position_embeddings_grad;

    // Layer gradients
    std::vector<TransformerLayerGradients> layers;

    // Output projection gradients
    std::vector<std::vector<float>> output_projection_grad;

    void resize(int vocab_size, int d_model, int num_layers, int ff_dim, int max_seq_len);
    void zero();
    void accumulate(const TransformerGradients& other);  // Add gradients for batch accumulation
};

// Backward pass functions for each component

namespace TransformerBackward {
    // Attention backward pass
    // Inputs:
    //   - input: [seq_len, d_model] - input to attention
    //   - grad_output: [seq_len, d_model] - gradient from next layer
    //   - weights: attention weights from forward pass
    //   - cached_qkv: cached Q, K, V matrices from forward pass
    // Outputs:
    //   - grad_input: [seq_len, d_model] - gradient w.r.t. input
    //   - grads: accumulated weight gradients
    void attention_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<std::vector<float>>& Q,
        const std::vector<std::vector<float>>& K,
        const std::vector<std::vector<float>>& V,
        const std::vector<std::vector<float>>& attn_scores,
        AttentionGradients& grads,
        std::vector<std::vector<float>>& grad_input,
        int d_model,
        int num_heads
    );

    // Feed-forward backward pass
    void feedforward_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<std::vector<float>>& hidden,  // Cached from forward pass
        FeedForwardGradients& grads,
        std::vector<std::vector<float>>& grad_input,
        int d_model,
        int ff_dim
    );

    // Layer normalization backward pass
    void layernorm_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<float>& gamma,
        const std::vector<float>& beta,
        LayerNormGradients& grads,
        std::vector<std::vector<float>>& grad_input
    );

    // GELU backward pass
    void gelu_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        std::vector<std::vector<float>>& grad_input
    );

    // Matrix multiplication backward
    // C = A * B
    // grad_A = grad_C * B^T
    // grad_B = A^T * grad_C
    void matmul_backward(
        const std::vector<std::vector<float>>& A,
        const std::vector<std::vector<float>>& B,
        const std::vector<std::vector<float>>& grad_C,
        std::vector<std::vector<float>>& grad_A,
        std::vector<std::vector<float>>& grad_B
    );

    // Softmax backward pass (for attention scores)
    void softmax_backward(
        const std::vector<std::vector<float>>& probs,
        const std::vector<std::vector<float>>& grad_probs,
        std::vector<std::vector<float>>& grad_logits
    );

    // Embedding backward pass
    void embedding_backward(
        const std::vector<int>& tokens,
        const std::vector<std::vector<float>>& grad_output,
        std::vector<std::vector<float>>& embedding_grad
    );
}

// Cache structure for forward pass (needed for backward)
struct TransformerCache {
    // Input sequence
    std::vector<int> tokens;

    // Per-layer caches
    struct LayerCache {
        // Attention cache
        std::vector<std::vector<float>> attn_input;
        std::vector<std::vector<float>> Q;
        std::vector<std::vector<float>> K;
        std::vector<std::vector<float>> V;
        std::vector<std::vector<float>> attn_scores;
        std::vector<std::vector<float>> attn_output;
        std::vector<std::vector<float>> attn_residual_out;

        // Feed-forward cache
        std::vector<std::vector<float>> ff_input;
        std::vector<std::vector<float>> ff_hidden;
        std::vector<std::vector<float>> ff_output;
        std::vector<std::vector<float>> ff_residual_out;
    };

    std::vector<LayerCache> layers;

    // Final output
    std::vector<std::vector<float>> final_output;

    void resize(int num_layers);
};

#endif // TRANSFORMER_GRADIENTS_H
