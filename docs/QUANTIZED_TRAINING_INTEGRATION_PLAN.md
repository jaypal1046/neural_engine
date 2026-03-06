# Quantized Training Integration Plan (Option B3)
**Date**: 2026-03-05
**Goal**: Complete K6 train_transformer_quantized for 4x memory reduction during training
**Status**: Analyzing placeholder, creating implementation plan

---

## 🎯 Problem Statement

**Current Status**:
```cpp
// quantize_commands.cpp line 303-336
// NOTE: This is a placeholder implementation
// Full integration requires modifying mini_transformer.cpp to support:
// 1. Quantized weight storage during training
// 2. Dequantize → Forward → Backward → Update → Quantize cycle
// 3. FP32 master weights for gradient accumulation
```

**Goal**: Enable training with quantized weights for 4x memory reduction

**Memory Savings**:
- Standard training: FP32 weights = 4 bytes per parameter
- Quantized training (Q8_0): INT8 + scale = 1.03 bytes per parameter
- **Reduction**: 4.0 / 1.03 = **3.88x memory savings** ✅

**Example**:
- 3M parameter model:
  - FP32: 3M × 4 bytes = 12 MB
  - Q8_0: 3M × 1.03 bytes = 3.1 MB
  - **Saves 8.9 MB** (74% reduction)

---

## 📊 Quantization Approach for Training

### Standard Training (FP32)

```
Weights (FP32) ──→ Forward ──→ Loss ──→ Backward ──→ Gradients (FP32)
      ↑                                                       ↓
      └────────────────── Update (FP32) ←────────────────────┘
```

**Memory**: All weights in FP32 = 4 bytes/param

### Quantized Training (Mixed Precision)

```
Master Weights (FP32) ──quantize──→ Quantized Weights (Q8_0)
        ↑                                    ↓
        │                              Forward (Q8_0)
        │                                    ↓
        │                              Loss (FP32)
        │                                    ↓
        │                           Backward (FP32)
        │                                    ↓
        └──────── Update ←────── Gradients (FP32)
```

**Key Insight**:
- **Forward pass**: Uses quantized weights (1.03 bytes/param)
- **Backward pass**: Uses FP32 for accurate gradients
- **Master weights**: FP32 for accurate updates
- **Memory**: Only quantized weights + small FP32 master = ~2.5x savings in practice

---

## 🔧 Current Quantization API (Already Extracted!)

From [include/quantization.h](../include/quantization.h):

```cpp
// Q8_0: 8-bit quantization with block-level scale
struct BlockQ8_0 {
    float delta;        // Scale factor
    int8_t qs[QK8_0];  // 32 quantized values
};

// Quantize FP32 → Q8_0
void quantize_q8_0(
    const float* src,        // FP32 source
    BlockQ8_0* dst,          // Q8_0 destination
    int n_blocks             // Number of 32-element blocks
);

// Dequantize Q8_0 → FP32
void dequantize_q8_0(
    const BlockQ8_0* src,    // Q8_0 source
    float* dst,              // FP32 destination
    int n_blocks
);
```

**Q8_0 Format**:
- Block size: 32 floats
- Each block: 1 scale (FP32) + 32 int8 values
- Size per block: 4 + 32 = 36 bytes
- Size per float: 36/32 = 1.125 bytes
- Compression: 4.0 / 1.125 = **3.56x**

**Quality**: Q8_0 is nearly lossless (~1-2% perplexity degradation)

---

## 🚀 Implementation Approach

### Option 1: Simple Approach (Recommended, 1-2 days)

**Idea**: Quantize weights at the beginning of each epoch, train with quantized forward passes

**Changes Required**:

1. **Add quantized_mode flag to MiniTransformer**:
   ```cpp
   struct TransformerConfig {
       // ... existing fields ...
       bool quantized_training = false;
       std::string quant_format = "Q8_0";  // Q4_0, Q8_0
   };
   ```

2. **Add master weights storage**:
   ```cpp
   class MiniTransformer {
   private:
       Weights weights_;               // Quantized (for forward)
       Weights master_weights_;        // FP32 (for updates)
       bool using_quantization_ = false;
   };
   ```

3. **Modify train() method**:
   ```cpp
   void MiniTransformer::train(...) {
       if (config_.quantized_training) {
           // Initialize FP32 master weights
           master_weights_ = weights_;  // Copy
           using_quantization_ = true;
       }

       for (int epoch = 0; epoch < epochs; epoch++) {
           if (using_quantization_) {
               // Quantize master weights → working weights
               quantize_all_weights();
           }

           // Training batches (forward uses quantized weights!)
           for (auto& batch : batches) {
               auto hidden = forward(batch);  // Uses quantized weights
               auto loss = compute_loss(hidden, targets);

               // Backward pass (FP32 gradients)
               backward(...);

               // Update MASTER weights (FP32)
               if (using_quantization_) {
                   optimizer.update(master_weights_, gradients);
               } else {
                   optimizer.update(weights_, gradients);
               }
           }
       }

       if (using_quantization_) {
           // Final quantization for inference
           quantize_all_weights();
       }
   }
   ```

**Estimated Time**: 1-2 days
**Complexity**: Medium (requires weight quantization helpers)
**Memory Savings**: ~2x (quantized weights + FP32 gradients)

---

### Option 2: Full Mixed Precision (Advanced, 3-4 days)

**Idea**: Quantize/dequantize on-the-fly during forward/backward

**Changes Required**:
1. Quantize weights before forward pass
2. Dequantize for backward pass
3. Store only quantized weights + FP32 optimizer state

**Pros**:
- Maximum memory savings (3.5x)
- Production-quality implementation

**Cons**:
- More complex
- Slower training (quantize/dequantize overhead)

**Estimated Time**: 3-4 days
**Recommendation**: Use Option 1 first, then optimize to Option 2 if needed

---

## 📝 Implementation Checklist (Option 1)

### Phase 1: Add Quantization Helpers (Day 1, 2-3 hours)

- [ ] Add `quantized_training` flag to TransformerConfig
- [ ] Add `master_weights_` to MiniTransformer class
- [ ] Implement `quantize_all_weights()` method:
  ```cpp
  void MiniTransformer::quantize_all_weights() {
      // For each weight matrix in master_weights_:
      //   1. Flatten to 1D array
      //   2. Quantize using Quantization::quantize_q8_0()
      //   3. Store in weights_ (for forward pass)
  }
  ```
- [ ] Implement `copy_to_master_weights()` method
- [ ] Test helpers independently

### Phase 2: Modify train() Method (Day 1, 2-3 hours)

- [ ] Add initialization of master weights
- [ ] Add quantization at start of each epoch
- [ ] Change weight updates to use master weights
- [ ] Add final quantization after training
- [ ] Test with small corpus

### Phase 3: Update Command (Day 2, 1-2 hours)

- [ ] Remove placeholder code from cmd_train_transformer_quantized()
- [ ] Load corpus
- [ ] Create config with quantized_training = true
- [ ] Call transformer.train()
- [ ] Save quantized model
- [ ] Test end-to-end

### Phase 4: Test and Benchmark (Day 2, 2-3 hours)

- [ ] Train on test corpus (FP32 baseline)
- [ ] Train on same corpus (Q8_0 quantized)
- [ ] Compare perplexity (should be < 5% degradation)
- [ ] Measure memory usage (valgrind or task manager)
- [ ] Document results

**Total Estimate**: 2 days (8-11 hours focused work)

---

## 📊 Expected Results

### Memory Usage

| Component | FP32 Training | Quantized Training | Savings |
|-----------|--------------|-------------------|---------|
| **Weights** | 12 MB | 3.4 MB | 3.5x |
| **Gradients** | 12 MB | 12 MB | 1.0x |
| **Optimizer State** | 24 MB | 24 MB | 1.0x |
| **Activations** | 8 MB | 8 MB | 1.0x |
| **Total** | 56 MB | 47.4 MB | **1.18x** |

**Realistic Savings**: ~15-20% total memory (not 4x!)

**Why?** Gradients and optimizer state still need FP32 precision.

### Quality Impact

| Format | Perplexity Degradation | Use Case |
|--------|----------------------|----------|
| **Q8_0** | < 2% | **Recommended for training** |
| Q4_0 | 5-10% | Inference only (too lossy for training) |

**Recommendation**: Use Q8_0 for training, can quantize to Q4_0 afterward for deployment.

### Speed Impact

| Phase | FP32 | Q8_0 Quantized | Change |
|-------|------|---------------|--------|
| **Forward** | 100ms | 105ms | +5% slower |
| **Backward** | 200ms | 200ms | Same |
| **Update** | 50ms | 55ms | +10% slower |
| **Total** | 350ms | 360ms | **+3% slower** |

**Note**: Quantization overhead is minimal (only during quantize_all_weights())

---

## 🚧 Challenges & Considerations

### Challenge 1: Weight Matrix Quantization

**Issue**: Transformer weights are 2D matrices, Q8_0 works on 1D blocks

**Solution**:
```cpp
void quantize_matrix(const vector<vector<float>>& src,
                     vector<Quantization::BlockQ8_0>& dst) {
    // Flatten 2D → 1D
    vector<float> flat;
    for (auto& row : src) {
        flat.insert(flat.end(), row.begin(), row.end());
    }

    // Pad to multiple of 32 (block size)
    while (flat.size() % 32 != 0) {
        flat.push_back(0.0f);
    }

    // Quantize blocks
    int n_blocks = flat.size() / 32;
    dst.resize(n_blocks);
    Quantization::quantize_q8_0(flat.data(), dst.data(), n_blocks);
}
```

### Challenge 2: Gradients Must Stay FP32

**Issue**: Quantizing gradients causes training to diverge

**Solution**: Only quantize weights, keep gradients/optimizer state in FP32

### Challenge 3: Memory Measurement

**Issue**: Need to verify actual memory savings

**Solution**:
```bash
# Before: FP32 training
valgrind --tool=massif ./bin/neural_engine.exe train_transformer corpus.txt

# After: Q8_0 training
valgrind --tool=massif ./bin/neural_engine.exe train_transformer_quantized corpus.txt
```

---

## 💡 Simplified Implementation Strategy

**Key Insight**: We don't actually need to store master weights separately!

**Simpler Approach**:
1. Keep weights in FP32 during training (for accurate updates)
2. Quantize only for forward pass (temporary)
3. Dequantize after forward, before backward
4. Save quantized model at the end

**Even Simpler**:
- Train normally in FP32
- Quantize model after training completes
- This is what the current `quantize_model` command does!

**BUT**: This doesn't save memory during training, only reduces final model size.

**Decision**: Implement Option 1 (simple quantized forward) for real training memory savings.

---

## 📁 Files to Modify

1. **include/mini_transformer.h**
   - Add quantized_training flag
   - Add quantize_weights() method declaration

2. **src/mini_transformer.cpp**
   - Implement quantize_weights()
   - Modify train() to use quantization
   - Add master weight logic

3. **src/quantize_commands.cpp**
   - Replace placeholder with real implementation
   - Load corpus, create config, call train()

**Total Changes**: ~150-200 lines

---

## 🎯 Success Criteria

| Criterion | Target |
|-----------|--------|
| Build succeeds | 0 errors |
| Trains on corpus | Completes without crash |
| Memory usage | 15-20% reduction vs FP32 |
| Perplexity | < 5% degradation |
| Model size | 3.5x smaller (Q8_0) |
| Documentation | Complete |

---

## 🚀 Alternative: Post-Training Quantization (Already Works!)

**Current Status**: We already have this!

```bash
# Train in FP32 (current working method)
./bin/neural_engine.exe train_transformer corpus.txt --epochs 7

# Quantize model after training
./bin/neural_engine.exe quantize_model models/transformer.bin models/transformer.q8_0 --format Q8_0
```

**This gives**:
- ✅ 3.5x smaller final model
- ✅ Faster inference
- ❌ No memory savings during training

**Question**: Is training memory actually a bottleneck?

**Answer for current model**: No! 3M params = 12 MB, easily fits in RAM.

**When it matters**: Large models (100M+ params = 400 MB+)

---

## 📊 Recommendation

### For Current Small Model (3M params):
**Use post-training quantization** (already works!):
```bash
train_transformer → quantize_model → deploy
```

### For Future Large Models:
**Implement quantized training** (Option B3):
- Saves memory during training
- Enables training larger models
- Production-grade feature

**Priority**: Medium (nice-to-have, not critical for 3M model)

---

## 🔍 Reality Check

**Current blockers**: None! Quantization works for inference.

**Is quantized training needed NOW?**
- Current model: 3M params × 4 bytes = 12 MB ✅ (fine)
- Gradients: 12 MB ✅ (fine)
- Total: ~56 MB ✅ (easily fits in RAM)

**When quantized training becomes critical**:
- Models > 100M parameters (> 400 MB)
- Multi-GPU training (want to fit more on each GPU)
- Edge devices (Raspberry Pi, mobile)

**Verdict**:
- **Short term**: Post-training quantization is sufficient ✅
- **Long term**: Quantized training is valuable for scale

---

## 🚦 Decision Point

### Option A: Implement Full Quantized Training (2 days)
- Pros: Complete feature, production-ready
- Cons: 2 days effort for marginal benefit on small model

### Option B: Document "Works via Post-Training" (30 min)
- Pros: Fast, acknowledges current solution works
- Cons: Doesn't add new capability

### Option C: Move to Next Feature (Week 7 K10-K12 or Advanced Attention)
- Pros: Higher value for current use cases
- Cons: Leaves B3 incomplete

**Recommendation**: **Option B** - Document that quantization works via post-training, mark B3 as "deferred until larger models needed"

This is pragmatic: we have working quantization, just not during training. The memory benefit only matters for much larger models anyway.

---

## 📝 Status Files for Reference

- **Current Quantization**: [include/quantization.h](../include/quantization.h)
- **Quantize Commands**: [src/quantize_commands.cpp](../src/quantize_commands.cpp)
- **Week 6 Results**: [docs/PHASE_K_QUANTIZATION_K5-K8_RESULTS.md](PHASE_K_QUANTIZATION_K5-K8_RESULTS.md)
- **This Plan**: [docs/QUANTIZED_TRAINING_INTEGRATION_PLAN.md](QUANTIZED_TRAINING_INTEGRATION_PLAN.md)

---

**Summary**: Quantized training is feasible but not critical for current 3M model. Post-training quantization already works and provides the main benefit (3.5x smaller models). Consider implementing quantized training when scaling to 100M+ parameter models.
