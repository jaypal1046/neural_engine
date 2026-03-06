# Week 9 Day 6 - Full Backward Pass Implementation - PLAN

**Date**: 2026-03-06
**Goal**: Implement complete backward pass for all transformer layers
**Target**: Full training with attention + feedforward gradients, 1.5-2x speedup

---

## 🎯 Current State Analysis

### What We Have (Day 5)

**Working**:
- ✅ training_step() with output projection only
- ✅ Loss scaling (1024x for FP16)
- ✅ Training converges (79-83% loss reduction)
- ✅ 6.35x speedup (but only output layer updates)

**Missing**:
- ❌ Attention gradients (Q, K, V, O weights)
- ❌ Feedforward gradients (W1, W2, biases)
- ❌ Embedding gradients (token + position)
- ❌ Layer norm gradients (gamma, beta)

**Current Limitation**:
```cpp
// Day 5: Only output_projection updates
gradients_["output_projection"] = dW_out;

// TODO: Compute gradients for:
// - 2 layers × (Q, K, V, O, FF1, FF2) = 12 weight matrices
// - 2 layers × (ln1, ln2) = 4 layer norms
// - token_embeddings + position_embeddings = 2 embeddings
```

**Impact**: Model can't learn attention patterns or feedforward representations!

---

## 📋 Day 6 Strategy

### Backward Pass Order (Reverse of Forward)

**Forward Pass**:
```
Input tokens → Embeddings → Layer 1 (Attn + FF) → Layer 2 (Attn + FF) → Output projection → Loss
```

**Backward Pass** (chain rule):
```
Loss gradient → Output projection gradient → Layer 2 (FF + Attn) → Layer 1 (FF + Attn) → Embeddings
```

**Key Insight**: Each layer's gradient flows backward through:
1. Layer norm 2 (after feedforward)
2. Feedforward (with residual)
3. Layer norm 1 (after attention)
4. Attention (with residual)

---

## 🔧 Implementation Plan

### Task 1: Implement Feedforward Backward (1.5 hours)

**File**: `src/mini_transformer.cpp`

**Forward Pass Recap**:
```cpp
// Feedforward: x → W1 → GELU → W2 → out
// x shape: [seq_len, d_model]
// W1 shape: [d_model, ff_dim]
// hidden shape: [seq_len, ff_dim]
// W2 shape: [ff_dim, d_model]
// out shape: [seq_len, d_model]

hidden = matmul(x, W1) + b1;           // [seq_len, ff_dim]
hidden_gelu = gelu(hidden);            // [seq_len, ff_dim]
out = matmul(hidden_gelu, W2) + b2;    // [seq_len, d_model]
```

**Backward Pass**:
```cpp
void backward_feedforward(
    const std::vector<std::vector<float>>& input,        // x: [seq_len, d_model]
    const std::vector<std::vector<float>>& grad_output,  // dL/dout: [seq_len, d_model]
    const std::vector<std::vector<float>>& hidden,       // cached from forward
    const std::vector<std::vector<float>>& hidden_gelu,  // cached from forward
    const Weights::Layer& layer,
    std::unordered_map<std::string, std::vector<std::vector<float>>>& gradients
) {
    int seq_len = input.size();
    int d_model = input[0].size();
    int ff_dim = layer.ff1_weight[0].size();

    // Gradient for W2: dL/dW2 = hidden_gelu^T × grad_output
    // W2 shape: [ff_dim, d_model]
    std::vector<std::vector<float>> dW2(ff_dim, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < ff_dim; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int t = 0; t < seq_len; t++) {
                dW2[i][j] += hidden_gelu[t][i] * grad_output[t][j];
            }
        }
    }

    // Gradient for b2: dL/db2 = sum(grad_output, axis=0)
    std::vector<float> db2(d_model, 0.0f);
    for (int j = 0; j < d_model; j++) {
        for (int t = 0; t < seq_len; t++) {
            db2[j] += grad_output[t][j];
        }
    }

    // Backprop through W2: dL/dhidden_gelu = grad_output × W2^T
    std::vector<std::vector<float>> grad_hidden_gelu(seq_len, std::vector<float>(ff_dim, 0.0f));
    for (int t = 0; t < seq_len; t++) {
        for (int i = 0; i < ff_dim; i++) {
            for (int j = 0; j < d_model; j++) {
                grad_hidden_gelu[t][i] += grad_output[t][j] * layer.ff2_weight[i][j];
            }
        }
    }

    // Backprop through GELU: dL/dhidden = grad_hidden_gelu ⊙ gelu'(hidden)
    // GELU derivative: gelu'(x) = Φ(x) + x·φ(x)
    // where Φ = CDF of N(0,1), φ = PDF of N(0,1)
    std::vector<std::vector<float>> grad_hidden(seq_len, std::vector<float>(ff_dim, 0.0f));
    for (int t = 0; t < seq_len; t++) {
        for (int i = 0; i < ff_dim; i++) {
            float x = hidden[t][i];
            float gelu_grad = gelu_derivative(x);
            grad_hidden[t][i] = grad_hidden_gelu[t][i] * gelu_grad;
        }
    }

    // Gradient for W1: dL/dW1 = input^T × grad_hidden
    // W1 shape: [d_model, ff_dim]
    std::vector<std::vector<float>> dW1(d_model, std::vector<float>(ff_dim, 0.0f));
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < ff_dim; j++) {
            for (int t = 0; t < seq_len; t++) {
                dW1[i][j] += input[t][i] * grad_hidden[t][j];
            }
        }
    }

    // Gradient for b1: dL/db1 = sum(grad_hidden, axis=0)
    std::vector<float> db1(ff_dim, 0.0f);
    for (int j = 0; j < ff_dim; j++) {
        for (int t = 0; t < seq_len; t++) {
            db1[j] += grad_hidden[t][j];
        }
    }

    // Backprop to input: dL/dx = grad_hidden × W1^T
    std::vector<std::vector<float>> grad_input(seq_len, std::vector<float>(d_model, 0.0f));
    for (int t = 0; t < seq_len; t++) {
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < ff_dim; j++) {
                grad_input[t][i] += grad_hidden[t][j] * layer.ff1_weight[i][j];
            }
        }
    }

    // Store gradients
    gradients["ff1_weight"] = dW1;
    gradients["ff1_bias"] = {db1};  // Wrap in vector for consistency
    gradients["ff2_weight"] = dW2;
    gradients["ff2_bias"] = {db2};

    return grad_input;  // For backprop to previous layer
}
```

**GELU Derivative**:
```cpp
float gelu_derivative(float x) const {
    // GELU(x) = x·Φ(x) where Φ is standard normal CDF
    // GELU'(x) = Φ(x) + x·φ(x) where φ is standard normal PDF

    const float sqrt_2_over_pi = 0.79788456080286535587989;
    const float c = 0.044715f;

    float cdf = 0.5f * (1.0f + std::tanh(sqrt_2_over_pi * (x + c * x * x * x)));
    float pdf = sqrt_2_over_pi * (1.0f + 3.0f * c * x * x) *
                (1.0f - std::tanh(sqrt_2_over_pi * (x + c * x * x * x)) *
                 std::tanh(sqrt_2_over_pi * (x + c * x * x * x)));

    return cdf + x * pdf;
}
```

---

### Task 2: Implement Attention Backward (2 hours)

**File**: `src/mini_transformer.cpp`

**Forward Pass Recap**:
```cpp
// Multi-head attention:
// Q = input × W_Q  [seq_len, d_model]
// K = input × W_K  [seq_len, d_model]
// V = input × W_V  [seq_len, d_model]
// scores = Q × K^T / sqrt(d_k)  [seq_len, seq_len]
// attn = softmax(scores + causal_mask)  [seq_len, seq_len]
// context = attn × V  [seq_len, d_model]
// output = context × W_O  [seq_len, d_model]
```

**Backward Pass** (simplified single-head):
```cpp
void backward_attention(
    const std::vector<std::vector<float>>& input,        // [seq_len, d_model]
    const std::vector<std::vector<float>>& grad_output,  // dL/doutput
    const std::vector<std::vector<float>>& Q,            // cached
    const std::vector<std::vector<float>>& K,            // cached
    const std::vector<std::vector<float>>& V,            // cached
    const std::vector<std::vector<float>>& attn_weights, // cached
    const std::vector<std::vector<float>>& context,      // cached
    const Weights::Layer& layer,
    std::unordered_map<std::string, std::vector<std::vector<float>>>& gradients
) {
    int seq_len = input.size();
    int d_model = input[0].size();

    // Gradient for W_O: dL/dW_O = context^T × grad_output
    std::vector<std::vector<float>> dW_O(d_model, std::vector<float>(d_model, 0.0f));
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int t = 0; t < seq_len; t++) {
                dW_O[i][j] += context[t][i] * grad_output[t][j];
            }
        }
    }

    // Backprop through W_O: dL/dcontext = grad_output × W_O^T
    std::vector<std::vector<float>> grad_context(seq_len, std::vector<float>(d_model, 0.0f));
    for (int t = 0; t < seq_len; t++) {
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < d_model; j++) {
                grad_context[t][i] += grad_output[t][j] * layer.output_weight[i][j];
            }
        }
    }

    // Backprop through context = attn × V
    // dL/dattn = grad_context × V^T  [seq_len, seq_len]
    // dL/dV = attn^T × grad_context  [seq_len, d_model]
    std::vector<std::vector<float>> grad_attn(seq_len, std::vector<float>(seq_len, 0.0f));
    std::vector<std::vector<float>> grad_V(seq_len, std::vector<float>(d_model, 0.0f));

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < seq_len; j++) {
            for (int k = 0; k < d_model; k++) {
                grad_attn[i][j] += grad_context[i][k] * V[j][k];
            }
        }
    }

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                grad_V[i][j] += attn_weights[k][i] * grad_context[k][j];
            }
        }
    }

    // Backprop through softmax: dL/dscores
    // For softmax: ds_i = a_i * (dL/da_i - Σ_j a_j * dL/da_j)
    std::vector<std::vector<float>> grad_scores(seq_len, std::vector<float>(seq_len, 0.0f));
    for (int i = 0; i < seq_len; i++) {
        float sum = 0.0f;
        for (int j = 0; j < seq_len; j++) {
            sum += attn_weights[i][j] * grad_attn[i][j];
        }
        for (int j = 0; j < seq_len; j++) {
            grad_scores[i][j] = attn_weights[i][j] * (grad_attn[i][j] - sum);
        }
    }

    // Backprop through scores = Q × K^T / sqrt(d_k)
    float scale = 1.0f / std::sqrt((float)d_model);
    for (auto& row : grad_scores) {
        for (auto& val : row) {
            val *= scale;
        }
    }

    // dL/dQ = grad_scores × K  [seq_len, d_model]
    // dL/dK = grad_scores^T × Q  [seq_len, d_model]
    std::vector<std::vector<float>> grad_Q(seq_len, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> grad_K(seq_len, std::vector<float>(d_model, 0.0f));

    for (int i = 0; i < seq_len; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int k = 0; k < seq_len; k++) {
                grad_Q[i][j] += grad_scores[i][k] * K[k][j];
                grad_K[i][j] += grad_scores[k][i] * Q[k][j];
            }
        }
    }

    // Gradients for W_Q, W_K, W_V: input^T × grad_{Q,K,V}
    std::vector<std::vector<float>> dW_Q(d_model, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> dW_K(d_model, std::vector<float>(d_model, 0.0f));
    std::vector<std::vector<float>> dW_V(d_model, std::vector<float>(d_model, 0.0f));

    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < d_model; j++) {
            for (int t = 0; t < seq_len; t++) {
                dW_Q[i][j] += input[t][i] * grad_Q[t][j];
                dW_K[i][j] += input[t][i] * grad_K[t][j];
                dW_V[i][j] += input[t][i] * grad_V[t][j];
            }
        }
    }

    // Backprop to input: dL/dx = dL/dQ×W_Q^T + dL/dK×W_K^T + dL/dV×W_V^T
    std::vector<std::vector<float>> grad_input(seq_len, std::vector<float>(d_model, 0.0f));
    for (int t = 0; t < seq_len; t++) {
        for (int i = 0; i < d_model; i++) {
            for (int j = 0; j < d_model; j++) {
                grad_input[t][i] += grad_Q[t][j] * layer.query_weight[i][j];
                grad_input[t][i] += grad_K[t][j] * layer.key_weight[i][j];
                grad_input[t][i] += grad_V[t][j] * layer.value_weight[i][j];
            }
        }
    }

    // Store gradients
    gradients["query_weight"] = dW_Q;
    gradients["key_weight"] = dW_K;
    gradients["value_weight"] = dW_V;
    gradients["output_weight"] = dW_O;

    return grad_input;
}
```

---

### Task 3: Update training_step() to Use Full Backward (1 hour)

**File**: `src/mini_transformer.cpp`

**Enhanced backward() Method**:
```cpp
void MiniTransformer::backward(
    const std::vector<std::vector<float>>& output_grad,
    PrecisionMode mode
) {
    gradients_.clear();

    // Start from output projection (Day 5 code - keep this)
    compute_output_projection_gradient(output_grad, mode);

    // Get gradient flowing back into final layer output
    std::vector<std::vector<float>> grad_hidden = backprop_through_output_projection(output_grad);

    // Backward through each layer (in reverse order)
    for (int l = config_.num_layers - 1; l >= 0; l--) {
        auto& layer = weights_.layers[l];
        std::string prefix = "layer_" + std::to_string(l) + "_";

        // Backward through layer norm 2 (after feedforward)
        auto grad_ff_out = backward_layer_norm(grad_hidden, layer.ln2_gamma, layer.ln2_beta);

        // Add residual gradient
        auto grad_attn_out = grad_ff_out;  // Residual shortcut

        // Backward through feedforward
        auto grad_ff_in = backward_feedforward(
            cached_layer_inputs_[l],
            grad_ff_out,
            cached_ff_hidden_[l],
            cached_ff_gelu_[l],
            layer,
            gradients_,
            prefix
        );

        // Accumulate with residual
        for (int i = 0; i < grad_ff_in.size(); i++) {
            for (int j = 0; j < grad_ff_in[i].size(); j++) {
                grad_attn_out[i][j] += grad_ff_in[i][j];
            }
        }

        // Backward through layer norm 1 (after attention)
        auto grad_attn = backward_layer_norm(grad_attn_out, layer.ln1_gamma, layer.ln1_beta);

        // Add residual gradient
        auto grad_layer_input = grad_attn;

        // Backward through attention
        auto grad_attn_in = backward_attention(
            cached_layer_inputs_[l],
            grad_attn,
            cached_Q_[l],
            cached_K_[l],
            cached_V_[l],
            cached_attn_weights_[l],
            cached_context_[l],
            layer,
            gradients_,
            prefix
        );

        // Accumulate with residual
        for (int i = 0; i < grad_attn_in.size(); i++) {
            for (int j = 0; j < grad_attn_in[i].size(); j++) {
                grad_layer_input[i][j] += grad_attn_in[i][j];
            }
        }

        // This becomes input gradient for previous layer
        grad_hidden = grad_layer_input;
    }

    // Backward through embeddings (optional for now)
    // grad_hidden now contains dL/dembeddings
    // Can compute dL/dtoken_embeddings and dL/dposition_embeddings
}
```

**Weight Update**:
```cpp
// Update all weights in training_step()
for (auto& [name, grad] : gradients_) {
    if (name == "output_projection") {
        update_matrix(weights_.output_projection, grad, learning_rate);
    } else if (name.find("layer_") == 0) {
        // Parse layer index
        int layer_idx = std::stoi(name.substr(6, name.find("_", 6) - 6));
        auto& layer = weights_.layers[layer_idx];

        if (name.find("query_weight") != std::string::npos) {
            update_matrix(layer.query_weight, grad, learning_rate);
        } else if (name.find("key_weight") != std::string::npos) {
            update_matrix(layer.key_weight, grad, learning_rate);
        }
        // ... similar for other weights
    }
}
```

---

### Task 4: Add Forward Pass Caching (30 min)

**Modify forward() to cache all intermediate values**:
```cpp
// Clear caches at start of forward
cached_layer_inputs_.clear();
cached_Q_.clear();
cached_K_.clear();
cached_V_.clear();
cached_attn_weights_.clear();
cached_context_.clear();
cached_ff_hidden_.clear();
cached_ff_gelu_.clear();

// During forward pass, cache everything
for (int l = 0; l < config_.num_layers; l++) {
    cached_layer_inputs_.push_back(x);  // Input to this layer

    // In attention
    cached_Q_.push_back(Q);
    cached_K_.push_back(K);
    cached_V_.push_back(V);
    cached_attn_weights_.push_back(attn);
    cached_context_.push_back(context);

    // In feedforward
    cached_ff_hidden_.push_back(hidden);
    cached_ff_gelu_.push_back(hidden_gelu);
}
```

---

## 📊 Success Criteria

### Minimum (Must Have)
- ✅ Feedforward backward implemented
- ✅ Attention backward implemented
- ✅ All gradients computed correctly
- ✅ Weight updates for all layers
- ✅ Training still converges

### Target (Should Have)
- ✅ 1.5-2x training speedup vs Day 5
- ✅ Loss decreases faster (more parameters updating)
- ✅ < 1% accuracy difference FP32 vs FP16
- ✅ No gradient explosions or NaN

### Stretch (Nice to Have)
- ⭐ Gradient clipping
- ⭐ Layer norm backward
- ⭐ Embedding backward

---

## 🎯 Expected Outcomes

### Performance
- Training step: 1.5-2x overall speedup with FP16
- Loss convergence: Faster (all layers learning)
- Memory: Same as Day 5 (gradients computed on-the-fly)

### Code Quality
- Clean separation: forward caching, backward computation
- Reusable helpers: backward_attention, backward_feedforward
- Comprehensive testing

---

## ⏱️ Timeline

**Hour 1**: Implement feedforward backward + GELU derivative
**Hour 2**: Implement attention backward (Q, K, V gradients)
**Hour 3**: Implement softmax backward + scores backprop
**Hour 4**: Integrate all gradients into training_step
**Hour 5**: Test, debug, measure performance

---

**Status**: 📋 **PLAN COMPLETE** - Ready to implement!

**Next**: Start Hour 1 - Implement feedforward backward pass

---

**Key Insight**: Backward pass is just forward pass in reverse with transposed matrices and cached activations. Chain rule makes each layer's gradient depend on the next layer's gradient.
