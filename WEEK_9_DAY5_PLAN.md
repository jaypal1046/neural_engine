# Week 9 Day 5 - Training Integration (Part 1) - PLAN

**Date**: 2026-03-06
**Goal**: Implement backward pass with mixed precision support
**Target**: Training loop works with FP16/BF16, < 1% accuracy loss

---

## 🎯 Current State Analysis

### What We Have (Days 1-4)

**Forward Pass**: ✅ Complete
```cpp
// Working inference with mixed precision
auto output = transformer.forward(tokens, PrecisionMode::FP16);
// FP16: 0.08% error, BF16: 0.38% error
// 1.07x speedup with SIMD conversion
```

**Missing**: Training (Backward Pass)
- No gradient computation
- No weight updates
- No mixed precision training loop

---

## 📋 Training Integration Strategy

### NVIDIA Apex Pattern (Industry Standard)

**Three-Component System**:

1. **Master Weights** (FP32)
   - Store authoritative weights in full precision
   - Accumulate small gradient updates without precision loss
   - Backup for numerical stability

2. **Working Copy** (FP16/BF16)
   - Convert master → FP16 for forward/backward
   - Fast computation with reduced memory
   - Discard after training step

3. **Loss Scaling**
   - Multiply loss by scale factor (e.g., 1024)
   - Prevents FP16 gradient underflow
   - Scale gradients back before weight update

**Workflow**:
```
Master Weights (FP32)
    ↓ convert
Working Weights (FP16) → Forward → Loss × scale
    ↓                               ↓
    ← ← ← ← ← ← ← ← ← ← ← Backward ←
Gradients (FP16)
    ↓ scale down
Gradients (FP32) → Update Master Weights
```

---

## 🔧 Implementation Plan

### Task 1: Add Master Weights System (1.5 hours)

**File**: `include/mixed_precision_optimizer.h`

**Add Class**:
```cpp
class MixedPrecisionTrainer {
public:
    MixedPrecisionTrainer(PrecisionMode mode = PrecisionMode::FP32);

    // Master weights management
    void set_master_weights(const std::vector<std::vector<float>>& weights);
    std::vector<std::vector<float>> get_master_weights() const;

    // Convert for training
    std::vector<std::vector<float>> get_working_weights() const;
    void update_from_gradients(const std::vector<std::vector<float>>& gradients, float lr);

    // Loss scaling
    void set_loss_scale(float scale);
    float get_loss_scale() const;
    std::vector<std::vector<float>> scale_gradients(
        const std::vector<std::vector<float>>& gradients
    ) const;

private:
    PrecisionMode mode_;
    std::vector<std::vector<float>> master_weights_;  // Always FP32
    float loss_scale_;  // Default 1024 for FP16, 1.0 for FP32
};
```

**Implementation**:
```cpp
std::vector<std::vector<float>> MixedPrecisionTrainer::get_working_weights() const {
    if (mode_ == PrecisionMode::FP32) {
        return master_weights_;  // No conversion needed
    }

    // Convert FP32 master → FP16/BF16 working copy
    auto working = master_weights_;
    for (auto& row : working) {
        for (float& val : row) {
            if (mode_ == PrecisionMode::FP16) {
                val = fp16_to_fp32(fp32_to_fp16(val));
            } else if (mode_ == PrecisionMode::BF16) {
                val = bf16_to_fp32(fp32_to_bf16(val));
            }
        }
    }
    return working;
}

void MixedPrecisionTrainer::update_from_gradients(
    const std::vector<std::vector<float>>& gradients, float lr
) {
    // Scale down gradients if using loss scaling
    auto scaled_grads = gradients;
    if (loss_scale_ != 1.0f) {
        for (auto& row : scaled_grads) {
            for (float& grad : row) {
                grad /= loss_scale_;
            }
        }
    }

    // Update master weights (always FP32)
    for (size_t i = 0; i < master_weights_.size(); i++) {
        for (size_t j = 0; j < master_weights_[i].size(); j++) {
            master_weights_[i][j] -= lr * scaled_grads[i][j];
        }
    }
}
```

---

### Task 2: Implement Backward Pass Signatures (1 hour)

**File**: `include/mini_transformer.h`

**Add Methods**:
```cpp
class MiniTransformer {
public:
    // Forward pass (existing)
    std::vector<std::vector<float>> forward(
        const std::vector<int>& tokens,
        PrecisionMode mode = PrecisionMode::FP32
    );

    // NEW: Backward pass
    void backward(
        const std::vector<std::vector<float>>& output_grad,
        PrecisionMode mode = PrecisionMode::FP32
    );

    // NEW: Get gradients after backward
    std::unordered_map<std::string, std::vector<std::vector<float>>> get_gradients() const;

    // NEW: Training step
    void training_step(
        const std::vector<int>& tokens,
        const std::vector<int>& targets,
        float learning_rate,
        PrecisionMode mode = PrecisionMode::FP32
    );

private:
    // Store gradients for each layer
    std::unordered_map<std::string, std::vector<std::vector<float>>> gradients_;

    // Cache forward pass activations (needed for backward)
    std::vector<std::vector<float>> cached_embeddings_;
    std::vector<std::vector<std::vector<float>>> cached_attention_outputs_;
    std::vector<std::vector<std::vector<float>>> cached_ff_outputs_;
};
```

---

### Task 3: Implement Simple Gradient Computation (1.5 hours)

**File**: `src/mini_transformer.cpp`

**Cross-Entropy Loss Gradient**:
```cpp
void MiniTransformer::backward(
    const std::vector<std::vector<float>>& output_grad,
    PrecisionMode mode
) {
    // Clear previous gradients
    gradients_.clear();

    // For now: Compute gradients only for output embeddings
    // (Attention/FF backward passes are complex - do in Day 6)

    int seq_len = output_grad.size();
    int d_model = output_grad[0].size();

    // Gradient for output projection (d_model → vocab_size)
    auto& W_out = weights_.W_out;
    std::vector<std::vector<float>> dW_out(d_model,
        std::vector<float>(config_.vocab_size, 0.0f));

    // dW_out = cached_output^T × output_grad
    for (int i = 0; i < d_model; i++) {
        for (int j = 0; j < config_.vocab_size; j++) {
            float grad_sum = 0.0f;
            for (int t = 0; t < seq_len; t++) {
                grad_sum += cached_embeddings_[t][i] * output_grad[t][j];
            }
            dW_out[i][j] = grad_sum;
        }
    }

    // Apply precision conversion to gradients
    if (mode != PrecisionMode::FP32) {
        for (auto& row : dW_out) {
            for (float& grad : row) {
                if (mode == PrecisionMode::FP16) {
                    grad = fp16_to_fp32(fp32_to_fp16(grad));
                } else if (mode == PrecisionMode::BF16) {
                    grad = bf16_to_fp32(fp32_to_bf16(grad));
                }
            }
        }
    }

    gradients_["W_out"] = dW_out;
}
```

**Training Step**:
```cpp
void MiniTransformer::training_step(
    const std::vector<int>& tokens,
    const std::vector<int>& targets,
    float learning_rate,
    PrecisionMode mode
) {
    // 1. Forward pass (cache activations)
    auto output = forward(tokens, mode);

    // 2. Compute loss and output gradient
    float loss = 0.0f;
    std::vector<std::vector<float>> output_grad(output.size(),
        std::vector<float>(output[0].size(), 0.0f));

    for (size_t t = 0; t < targets.size(); t++) {
        int target_token = targets[t];

        // Softmax + cross-entropy gradient
        float sum_exp = 0.0f;
        for (size_t i = 0; i < output[t].size(); i++) {
            sum_exp += std::exp(output[t][i]);
        }

        for (size_t i = 0; i < output[t].size(); i++) {
            float prob = std::exp(output[t][i]) / sum_exp;
            output_grad[t][i] = prob - (i == target_token ? 1.0f : 0.0f);

            if (i == target_token) {
                loss -= std::log(prob + 1e-10f);
            }
        }
    }

    // Apply loss scaling for FP16
    float loss_scale = (mode == PrecisionMode::FP16) ? 1024.0f : 1.0f;
    for (auto& row : output_grad) {
        for (float& grad : row) {
            grad *= loss_scale;
        }
    }

    // 3. Backward pass
    backward(output_grad, mode);

    // 4. Update weights (unscale gradients)
    auto grads = get_gradients();
    for (auto& [name, grad] : grads) {
        for (auto& row : grad) {
            for (float& g : row) {
                g /= loss_scale;
            }
        }
    }

    // 5. Gradient descent update
    // TODO: Integrate with MixedPrecisionTrainer for master weights
    for (auto& [name, grad] : grads) {
        if (name == "W_out") {
            for (size_t i = 0; i < weights_.W_out.size(); i++) {
                for (size_t j = 0; j < weights_.W_out[i].size(); j++) {
                    weights_.W_out[i][j] -= learning_rate * grad[i][j];
                }
            }
        }
    }
}
```

---

### Task 4: Create Training Test (1 hour)

**File**: `test_mixed_precision_training.cpp`

**Test Configuration**:
```cpp
#include "mini_transformer.h"
#include "mixed_precision_optimizer.h"
#include <iostream>
#include <chrono>

int main() {
    // Small model for testing
    TransformerConfig config;
    config.vocab_size = 100;
    config.embedding_dim = 64;
    config.num_layers = 2;
    config.num_heads = 4;
    config.ff_dim = 128;
    config.max_seq_length = 16;

    MiniTransformer transformer(config);

    // Training data: simple sequence prediction
    std::vector<int> tokens = {10, 20, 30, 40, 50};
    std::vector<int> targets = {20, 30, 40, 50, 60};

    std::cout << "=== Mixed Precision Training Test ===\n\n";

    // Test FP32 training
    std::cout << "1. FP32 Training (Baseline)\n";
    auto start_fp32 = std::chrono::high_resolution_clock::now();

    float loss_before_fp32 = 0.0f;
    for (int epoch = 0; epoch < 10; epoch++) {
        transformer.training_step(tokens, targets, 0.01f, PrecisionMode::FP32);
    }

    auto end_fp32 = std::chrono::high_resolution_clock::now();
    auto duration_fp32 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_fp32 - start_fp32).count();

    std::cout << "  Time: " << duration_fp32 << " μs\n\n";

    // Test FP16 training
    std::cout << "2. FP16 Training\n";
    MiniTransformer transformer_fp16(config);

    auto start_fp16 = std::chrono::high_resolution_clock::now();

    for (int epoch = 0; epoch < 10; epoch++) {
        transformer_fp16.training_step(tokens, targets, 0.01f, PrecisionMode::FP16);
    }

    auto end_fp16 = std::chrono::high_resolution_clock::now();
    auto duration_fp16 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_fp16 - start_fp16).count();

    std::cout << "  Time: " << duration_fp16 << " μs\n";
    std::cout << "  Speedup: " << (float)duration_fp32 / duration_fp16 << "x\n\n";

    // Test BF16 training
    std::cout << "3. BF16 Training\n";
    MiniTransformer transformer_bf16(config);

    auto start_bf16 = std::chrono::high_resolution_clock::now();

    for (int epoch = 0; epoch < 10; epoch++) {
        transformer_bf16.training_step(tokens, targets, 0.01f, PrecisionMode::BF16);
    }

    auto end_bf16 = std::chrono::high_resolution_clock::now();
    auto duration_bf16 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_bf16 - start_bf16).count();

    std::cout << "  Time: " << duration_bf16 << " μs\n";
    std::cout << "  Speedup: " << (float)duration_fp32 / duration_bf16 << "x\n\n";

    // Compare final outputs
    auto output_fp32 = transformer.forward(tokens, PrecisionMode::FP32);
    auto output_fp16 = transformer_fp16.forward(tokens, PrecisionMode::FP16);
    auto output_bf16 = transformer_bf16.forward(tokens, PrecisionMode::BF16);

    // Compute accuracy difference
    float error_fp16 = 0.0f;
    float error_bf16 = 0.0f;

    for (size_t i = 0; i < output_fp32.back().size(); i++) {
        error_fp16 += std::abs(output_fp32.back()[i] - output_fp16.back()[i]);
        error_bf16 += std::abs(output_fp32.back()[i] - output_bf16.back()[i]);
    }

    error_fp16 /= output_fp32.back().size();
    error_bf16 /= output_fp32.back().size();

    std::cout << "=== Accuracy Comparison ===\n";
    std::cout << "FP16 Error: " << error_fp16 << "\n";
    std::cout << "BF16 Error: " << error_bf16 << "\n";

    bool success = (error_fp16 < 0.01f && error_bf16 < 0.01f);
    std::cout << "\nTest: " << (success ? "✅ PASSED" : "❌ FAILED") << "\n";

    return success ? 0 : 1;
}
```

---

## 📊 Success Criteria

### Minimum (Must Have)
- ✅ Master weights system implemented
- ✅ Backward pass compiles and runs
- ✅ training_step() works with FP32/FP16/BF16
- ✅ Loss scaling applied for FP16

### Target (Should Have)
- ✅ Training converges (loss decreases)
- ✅ < 1% accuracy difference between FP32 and FP16
- ✅ Measurable speedup (>1.0x)
- ✅ Comprehensive test suite

### Stretch (Nice to Have)
- ⭐ Full attention backward pass (may defer to Day 6)
- ⭐ Dynamic loss scaler (auto-adjust scale)
- ⭐ Memory profiling (master vs working copy)

---

## 🎯 Expected Outcomes

### Performance
- Training step: 1.0-1.2x speedup with FP16 (conversion overhead)
- Full attention backward (Day 6): 1.5-2x speedup expected
- Memory: ~1.5x usage (master + working copy)

### Architecture
- Clean separation: master weights (FP32) vs working weights (FP16)
- Reusable trainer class for any model
- Foundation for advanced features (gradient clipping, momentum)

### Learning
- Understand why loss scaling is critical for FP16
- Validate gradient precision requirements
- Identify bottlenecks for Day 6 optimization

---

## ⏱️ Timeline

**Hour 1**: Implement MixedPrecisionTrainer class
- Master weights storage
- Working copy conversion
- Loss scaling helpers

**Hour 2**: Add backward pass signatures
- Update mini_transformer.h
- Add gradient storage
- Cache forward activations

**Hour 3**: Implement gradient computation
- Output layer gradient
- Cross-entropy loss
- Precision conversion

**Hour 4**: Create and run training test
- Write test_mixed_precision_training.cpp
- Build and test
- Document results

**Hour 5**: Fix bugs and optimize
- Debug any convergence issues
- Tune loss scale factor
- Write Day 5 completion report

---

## 🔍 Technical Details

### Why Loss Scaling?

**Problem**: FP16 range is ±65,504
- Small gradients (< 2^-24 ≈ 6e-8) underflow to zero
- Early training: gradients often < 1e-4
- Result: Weights don't update, training stalls

**Solution**: Multiply loss by scale factor (e.g., 1024)
```cpp
// Forward: normal computation
loss = cross_entropy(output, target);

// Backward: scale loss → scale gradients
scaled_loss = loss * 1024.0f;
gradients = backward(scaled_loss);  // gradients now 1024x larger

// Update: unscale gradients
unscaled_gradients = gradients / 1024.0f;
weights -= learning_rate * unscaled_gradients;
```

**Effect**: Gradients stay in FP16 representable range
- 1e-4 × 1024 = 0.1024 (safe in FP16)
- No precision loss during backward pass
- Mathematically equivalent (scale cancels out)

### Master Weights Pattern

**Why Not Train Directly in FP16?**
- Small gradient updates lost to rounding
- Example: weight = 10.0, gradient = 0.0001, lr = 0.01
  - Update: 10.0 - 0.000001 = 10.0 (FP16 can't represent difference)
- After 1000 steps: weight unchanged!

**Solution**: Accumulate in FP32 master copy
```
Iteration 1:
  master_weight (FP32) = 10.000000
  working_weight (FP16) = 10.0
  gradient (FP16) = 0.0001
  master_weight -= 0.01 * 0.0001 = 9.999999 ✅ (updated)

Iteration 2:
  working_weight (FP16) = convert(9.999999) = 10.0
  gradient (FP16) = 0.0001
  master_weight -= 0.000001 = 9.999998 ✅ (updated)
```

### BF16 vs FP16 for Training

**FP16** (Standard for mixed precision):
- Range: ±65,504 (can overflow)
- Requires loss scaling
- Higher precision (10-bit mantissa)

**BF16** (Emerging alternative):
- Range: same as FP32 (no overflow)
- NO loss scaling needed
- Lower precision (7-bit mantissa)
- Better for training stability

**Recommendation**: Implement both, test which works better

---

## 📁 Deliverables

### Code
1. `include/mixed_precision_optimizer.h` - MixedPrecisionTrainer class
2. `src/mixed_precision_optimizer.cpp` - Implementation
3. `include/mini_transformer.h` - Updated with backward pass
4. `src/mini_transformer.cpp` - Training step implementation
5. `test_mixed_precision_training.cpp` - Comprehensive test

### Documentation
6. `WEEK_9_DAY5_COMPLETE.md` - Results and analysis
7. Updated build instructions
8. Training tutorial (how to use mixed precision)

### Metrics
9. Training speedup measurements
10. Convergence comparison (FP32 vs FP16 vs BF16)
11. Memory usage analysis
12. Gradient precision validation

---

## 🚨 Potential Issues

### Issue 1: Gradient Underflow
**Symptom**: Training doesn't converge in FP16
**Solution**: Increase loss scale (try 2048, 4096)

### Issue 2: Gradient Overflow
**Symptom**: Loss becomes NaN
**Solution**: Decrease loss scale OR use gradient clipping

### Issue 3: Master/Working Sync
**Symptom**: FP32 and FP16 give different results
**Solution**: Always convert master → working at start of each step

### Issue 4: Memory Usage
**Symptom**: OOM with large models
**Solution**: Only keep master weights for trainable params (not activations)

---

## 🎯 What's Next (Day 6)

### Full Backward Pass
- Attention backward (Q, K, V gradients)
- Feedforward backward (W1, W2 gradients)
- Embedding backward

### Optimizations
- Fused backward operations
- SIMD gradient computation
- Activation checkpointing

### Advanced Features
- Dynamic loss scaler (auto-tune scale)
- Gradient clipping
- Mixed precision Adam optimizer

**Expected**: 1.5-2x training speedup with full backward pass

---

**Status**: 📋 **PLAN COMPLETE** - Ready to implement!

**Next**: Start Hour 1 - Implement MixedPrecisionTrainer class

---

**Key Insight**: Day 5 focuses on CORRECTNESS (training works), Day 6 focuses on SPEED (full backward optimization). This incremental approach reduces debugging complexity.
