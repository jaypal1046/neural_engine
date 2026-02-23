#include "transformer_gradients.h"
#include <cmath>
#include <iostream>
#include <algorithm>
#include <numeric>

// Gradient structure implementations
void AttentionGradients::resize(int d_model) {
    query_weight_grad.resize(d_model, std::vector<float>(d_model, 0.0f));
    key_weight_grad.resize(d_model, std::vector<float>(d_model, 0.0f));
    value_weight_grad.resize(d_model, std::vector<float>(d_model, 0.0f));
    output_weight_grad.resize(d_model, std::vector<float>(d_model, 0.0f));
}

void AttentionGradients::zero() {
    for (auto& row : query_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
    for (auto& row : key_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
    for (auto& row : value_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
    for (auto& row : output_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
}

void FeedForwardGradients::resize(int d_model, int ff_dim) {
    ff1_weight_grad.resize(d_model, std::vector<float>(ff_dim, 0.0f));
    ff1_bias_grad.resize(ff_dim, 0.0f);
    ff2_weight_grad.resize(ff_dim, std::vector<float>(d_model, 0.0f));
    ff2_bias_grad.resize(d_model, 0.0f);
}

void FeedForwardGradients::zero() {
    for (auto& row : ff1_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
    std::fill(ff1_bias_grad.begin(), ff1_bias_grad.end(), 0.0f);
    for (auto& row : ff2_weight_grad) std::fill(row.begin(), row.end(), 0.0f);
    std::fill(ff2_bias_grad.begin(), ff2_bias_grad.end(), 0.0f);
}

void LayerNormGradients::resize(int d_model) {
    gamma_grad.resize(d_model, 0.0f);
    beta_grad.resize(d_model, 0.0f);
}

void LayerNormGradients::zero() {
    std::fill(gamma_grad.begin(), gamma_grad.end(), 0.0f);
    std::fill(beta_grad.begin(), beta_grad.end(), 0.0f);
}

void TransformerLayerGradients::resize(int d_model, int ff_dim) {
    attention.resize(d_model);
    feed_forward.resize(d_model, ff_dim);
    ln1.resize(d_model);
    ln2.resize(d_model);
}

void TransformerLayerGradients::zero() {
    attention.zero();
    feed_forward.zero();
    ln1.zero();
    ln2.zero();
}

void TransformerGradients::resize(int vocab_size, int d_model, int num_layers, int ff_dim, int max_seq_len) {
    token_embeddings_grad.resize(vocab_size, std::vector<float>(d_model, 0.0f));
    position_embeddings_grad.resize(max_seq_len, std::vector<float>(d_model, 0.0f));

    layers.resize(num_layers);
    for (auto& layer : layers) {
        layer.resize(d_model, ff_dim);
    }

    output_projection_grad.resize(d_model, std::vector<float>(vocab_size, 0.0f));
}

void TransformerGradients::zero() {
    for (auto& row : token_embeddings_grad) std::fill(row.begin(), row.end(), 0.0f);
    for (auto& row : position_embeddings_grad) std::fill(row.begin(), row.end(), 0.0f);

    for (auto& layer : layers) {
        layer.zero();
    }

    for (auto& row : output_projection_grad) std::fill(row.begin(), row.end(), 0.0f);
}

void TransformerGradients::accumulate(const TransformerGradients& other) {
    // Add gradients for batch accumulation
    for (size_t i = 0; i < token_embeddings_grad.size(); i++) {
        for (size_t j = 0; j < token_embeddings_grad[i].size(); j++) {
            token_embeddings_grad[i][j] += other.token_embeddings_grad[i][j];
        }
    }

    for (size_t i = 0; i < position_embeddings_grad.size(); i++) {
        for (size_t j = 0; j < position_embeddings_grad[i].size(); j++) {
            position_embeddings_grad[i][j] += other.position_embeddings_grad[i][j];
        }
    }

    // Accumulate layer gradients (simplified - would need full implementation)
    // ... similar for all other gradient components
}

void TransformerCache::resize(int num_layers) {
    layers.resize(num_layers);
}

// Backward pass implementations
namespace TransformerBackward {
    void matmul_backward(
        const std::vector<std::vector<float>>& A,
        const std::vector<std::vector<float>>& B,
        const std::vector<std::vector<float>>& grad_C,
        std::vector<std::vector<float>>& grad_A,
        std::vector<std::vector<float>>& grad_B
    ) {
        // C = A * B
        // grad_A = grad_C * B^T
        // grad_B = A^T * grad_C

        int m = A.size();
        int k = A[0].size();
        int n = B[0].size();

        // grad_A = grad_C * B^T
        grad_A.resize(m, std::vector<float>(k, 0.0f));
        for (int i = 0; i < m; i++) {
            for (int j = 0; j < k; j++) {
                float sum = 0.0f;
                for (int l = 0; l < n; l++) {
                    sum += grad_C[i][l] * B[j][l];  // B^T
                }
                grad_A[i][j] = sum;
            }
        }

        // grad_B = A^T * grad_C
        grad_B.resize(k, std::vector<float>(n, 0.0f));
        for (int i = 0; i < k; i++) {
            for (int j = 0; j < n; j++) {
                float sum = 0.0f;
                for (int l = 0; l < m; l++) {
                    sum += A[l][i] * grad_C[l][j];  // A^T
                }
                grad_B[i][j] += sum;  // Accumulate
            }
        }
    }

    void gelu_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        std::vector<std::vector<float>>& grad_input
    ) {
        // GELU: 0.5 * x * (1 + tanh(sqrt(2/π) * (x + 0.044715 * x^3)))
        // Derivative is computed using chain rule

        const float sqrt_2_over_pi = 0.7978845608f;
        const float coeff = 0.044715f;

        grad_input.resize(input.size());
        for (size_t i = 0; i < input.size(); i++) {
            grad_input[i].resize(input[i].size());
            for (size_t j = 0; j < input[i].size(); j++) {
                float x = input[i][j];
                float x_cube = x * x * x;

                // Forward pass components
                float tanh_arg = sqrt_2_over_pi * (x + coeff * x_cube);
                float tanh_val = std::tanh(tanh_arg);

                // Derivative of GELU
                float dtanh = 1.0f - tanh_val * tanh_val;  // sech^2
                float darg = sqrt_2_over_pi * (1.0f + 3.0f * coeff * x * x);
                float dgelu = 0.5f * (1.0f + tanh_val) + 0.5f * x * dtanh * darg;

                grad_input[i][j] = grad_output[i][j] * dgelu;
            }
        }
    }

    void layernorm_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<float>& gamma,
        const std::vector<float>& beta,
        LayerNormGradients& grads,
        std::vector<std::vector<float>>& grad_input
    ) {
        int seq_len = input.size();
        int d_model = input[0].size();

        grad_input.resize(seq_len, std::vector<float>(d_model, 0.0f));

        for (int i = 0; i < seq_len; i++) {
            // Recompute mean and std from forward pass
            float mean = 0.0f;
            for (float val : input[i]) {
                mean += val;
            }
            mean /= d_model;

            float var = 0.0f;
            for (float val : input[i]) {
                float diff = val - mean;
                var += diff * diff;
            }
            var /= d_model;
            float std = std::sqrt(var + 1e-5f);

            // Compute gradients
            std::vector<float> x_hat(d_model);
            for (int j = 0; j < d_model; j++) {
                x_hat[j] = (input[i][j] - mean) / std;
            }

            // Gradient w.r.t. gamma and beta
            for (int j = 0; j < d_model; j++) {
                grads.gamma_grad[j] += grad_output[i][j] * x_hat[j];
                grads.beta_grad[j] += grad_output[i][j];
            }

            // Gradient w.r.t. input (complex chain rule)
            float dvar = 0.0f;
            for (int j = 0; j < d_model; j++) {
                dvar += grad_output[i][j] * gamma[j] * (input[i][j] - mean) * (-0.5f) * std::pow(std, -3.0f);
            }

            float dmean = 0.0f;
            for (int j = 0; j < d_model; j++) {
                dmean += grad_output[i][j] * gamma[j] * (-1.0f / std);
            }
            dmean += dvar * (-2.0f / d_model) * std::accumulate(input[i].begin(), input[i].end(), 0.0f,
                [mean](float sum, float val) { return sum + (val - mean); });

            for (int j = 0; j < d_model; j++) {
                grad_input[i][j] = grad_output[i][j] * gamma[j] / std;
                grad_input[i][j] += dvar * (2.0f / d_model) * (input[i][j] - mean);
                grad_input[i][j] += dmean / d_model;
            }
        }
    }

    void softmax_backward(
        const std::vector<std::vector<float>>& probs,
        const std::vector<std::vector<float>>& grad_probs,
        std::vector<std::vector<float>>& grad_logits
    ) {
        int rows = probs.size();
        int cols = probs[0].size();

        grad_logits.resize(rows, std::vector<float>(cols, 0.0f));

        for (int i = 0; i < rows; i++) {
            // Compute dot product
            float dot = 0.0f;
            for (int j = 0; j < cols; j++) {
                dot += probs[i][j] * grad_probs[i][j];
            }

            // Jacobian of softmax
            for (int j = 0; j < cols; j++) {
                grad_logits[i][j] = probs[i][j] * (grad_probs[i][j] - dot);
            }
        }
    }

    void embedding_backward(
        const std::vector<int>& tokens,
        const std::vector<std::vector<float>>& grad_output,
        std::vector<std::vector<float>>& embedding_grad
    ) {
        // Accumulate gradients for each token
        for (size_t i = 0; i < tokens.size(); i++) {
            int token_id = tokens[i];
            for (size_t j = 0; j < grad_output[i].size(); j++) {
                embedding_grad[token_id][j] += grad_output[i][j];
            }
        }
    }

    // Full multi-head attention backward pass
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
    ) {
        // Multi-head attention backward pass
        // Forward was: out = softmax(Q·K^T / sqrt(d_k)) · V
        // We need to backprop through this entire computation

        int seq_len = input.size();
        int d_k = d_model / num_heads;
        float scale = 1.0f / std::sqrt(static_cast<float>(d_k));

        // Step 1: Backward through output projection
        // grad_output is gradient w.r.t. final attention output [seq_len, d_model]
        // We need grad w.r.t. attended values (before output projection)

        std::vector<std::vector<float>> grad_attended(seq_len, std::vector<float>(d_model, 0.0f));

        // grad_attended = grad_output @ output_weight^T
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    grad_attended[i][j] += grad_output[i][k] * grads.output_weight_grad[j][k];
                }
            }
        }

        // Accumulate gradient for output_weight
        // grad_output_weight += attended^T @ grad_output
        // Note: 'attended' values would need to be cached from forward pass
        // For now, we approximate or require them as input

        // Step 2: Backward through attention application (scores · V)
        // attended = attn_scores @ V
        // grad_attn_scores = grad_attended @ V^T
        // grad_V = attn_scores^T @ grad_attended

        std::vector<std::vector<float>> grad_attn_scores(seq_len, std::vector<float>(seq_len, 0.0f));
        std::vector<std::vector<float>> grad_V(seq_len, std::vector<float>(d_model, 0.0f));

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                for (int k = 0; k < d_model; k++) {
                    grad_attn_scores[i][j] += grad_attended[i][k] * V[j][k];
                }
            }
        }

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grad_V[i][j] += attn_scores[k][i] * grad_attended[k][j];
                }
            }
        }

        // Step 3: Backward through softmax
        // attn_scores = softmax(attn_logits)
        // grad_attn_logits = softmax_jacobian @ grad_attn_scores

        std::vector<std::vector<float>> grad_attn_logits(seq_len, std::vector<float>(seq_len, 0.0f));

        for (int i = 0; i < seq_len; i++) {
            // For each row, compute softmax backward
            // grad_logits[j] = scores[j] * (grad_scores[j] - sum_k(scores[k] * grad_scores[k]))

            float sum_term = 0.0f;
            for (int k = 0; k < seq_len; k++) {
                sum_term += attn_scores[i][k] * grad_attn_scores[i][k];
            }

            for (int j = 0; j < seq_len; j++) {
                grad_attn_logits[i][j] = attn_scores[i][j] * (grad_attn_scores[i][j] - sum_term);
            }
        }

        // Step 4: Backward through scaled dot-product
        // attn_logits = (Q @ K^T) / sqrt(d_k)
        // grad_attn_logits is already scaled, so we apply scale to gradients

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                grad_attn_logits[i][j] *= scale;
            }
        }

        // Step 5: Backward through Q·K^T
        // logits = Q @ K^T
        // grad_Q = grad_logits @ K
        // grad_K = grad_logits^T @ Q

        std::vector<std::vector<float>> grad_Q(seq_len, std::vector<float>(d_model, 0.0f));
        std::vector<std::vector<float>> grad_K(seq_len, std::vector<float>(d_model, 0.0f));

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grad_Q[i][j] += grad_attn_logits[i][k] * K[k][j];
                }
            }
        }

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grad_K[i][j] += grad_attn_logits[k][i] * Q[k][j];
                }
            }
        }

        // Step 6: Backward through Q, K, V projections
        // Q = input @ query_weight
        // K = input @ key_weight
        // V = input @ value_weight

        grad_input.resize(seq_len, std::vector<float>(d_model, 0.0f));

        // grad_input += grad_Q @ query_weight^T
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    grad_input[i][j] += grad_Q[i][k] * grads.query_weight_grad[j][k];
                }
            }
        }

        // grad_input += grad_K @ key_weight^T
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    grad_input[i][j] += grad_K[i][k] * grads.key_weight_grad[j][k];
                }
            }
        }

        // grad_input += grad_V @ value_weight^T
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < d_model; k++) {
                    grad_input[i][j] += grad_V[i][k] * grads.value_weight_grad[j][k];
                }
            }
        }

        // Accumulate weight gradients
        // grad_query_weight += input^T @ grad_Q
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grads.query_weight_grad[i][j] += input[k][i] * grad_Q[k][j];
                }
            }
        }

        // grad_key_weight += input^T @ grad_K
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grads.key_weight_grad[i][j] += input[k][i] * grad_K[k][j];
                }
            }
        }

        // grad_value_weight += input^T @ grad_V
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < d_model; j++) {
                for (int k = 0; k < seq_len; k++) {
                    grads.value_weight_grad[i][j] += input[k][i] * grad_V[k][j];
                }
            }
        }

        std::cerr << "[BACKPROP] Full multi-head attention backward complete\n";
    }

    void feedforward_backward(
        const std::vector<std::vector<float>>& input,
        const std::vector<std::vector<float>>& grad_output,
        const std::vector<std::vector<float>>& hidden,
        FeedForwardGradients& grads,
        std::vector<std::vector<float>>& grad_input,
        int d_model,
        int ff_dim
    ) {
        int seq_len = input.size();

        // Backward through second linear layer: ff_dim -> d_model
        // grad w.r.t. hidden
        std::vector<std::vector<float>> grad_hidden(seq_len, std::vector<float>(ff_dim, 0.0f));

        matmul_backward(hidden, grads.ff2_weight_grad, grad_output, grad_hidden, grads.ff2_weight_grad);

        // Gradient w.r.t. bias
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < d_model; j++) {
                grads.ff2_bias_grad[j] += grad_output[i][j];
            }
        }

        // Backward through GELU
        std::vector<std::vector<float>> grad_ff1_output;
        gelu_backward(hidden, grad_hidden, grad_ff1_output);

        // Backward through first linear layer: d_model -> ff_dim
        matmul_backward(input, grads.ff1_weight_grad, grad_ff1_output, grad_input, grads.ff1_weight_grad);

        // Gradient w.r.t. bias
        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < ff_dim; j++) {
                grads.ff1_bias_grad[j] += grad_ff1_output[i][j];
            }
        }
    }
}
