# Week 9: K10 - Mixed Precision Integration Session Start
**Date**: 2026-03-05 (Continued Session)
**Goal**: Integrate FP16/BF16 into forward/backward passes
**Duration**: 2-3 weeks (starting now)
**Current Progress**: 58% → Target 70%+

---

## 🎯 Session Objectives

### K10: FP16/BF16 Forward/Backward Integration

**What We Have** (Week 7 K9):
- ✅ Mixed precision framework (`train_transformer_mixed` command)
- ✅ MixedPrecisionOptimizer class
- ✅ FP16/BF16/FP32 mode selection
- ✅ Tested: 50% memory savings confirmed

**What's Missing** (K10):
- ❌ forward() doesn't support precision modes
- ❌ backward() doesn't use mixed precision
- ❌ train() doesn't manage master weights
- ❌ No automatic conversion FP32 ↔ FP16/BF16

**What We'll Build**:
- ✅ Precision-aware forward() pass
- ✅ Mixed precision backward() pass
- ✅ Master weight management in train()
- ✅ End-to-end training with FP16/BF16

---

## 📋 Day 1-2 Plan: Precision-Aware Forward Pass

### Current forward() Implementation

```cpp
// src/mini_transformer.cpp line ~541
std::vector<std::vector<float>> MiniTransformer::forward(const std::vector<int>& tokens) {
    // All operations in FP32
    auto x = embeddings;  // FP32
    auto attn_out = multi_head_attention(x, layer);  // FP32
    auto ff_out = feed_forward(attn_out, layer);  // FP32
    return x;  // FP32
}
```

### Target Implementation

```cpp
std::vector<std::vector<float>> MiniTransformer::forward(
    const std::vector<int>& tokens,
    PrecisionMode mode = PrecisionMode::FP32
) {
    // Convert weights to target precision
    if (mode == PrecisionMode::FP16) {
        convert_weights_fp16();
    }

    // Forward pass (computation in FP16/BF16)
    auto x = embeddings;  // Converted
    auto attn_out = multi_head_attention(x, layer, mode);
    auto ff_out = feed_forward(attn_out, layer, mode);

    // Convert back to FP32 for output
    return convert_to_fp32(x);
}
```

### Implementation Steps

#### Step 1: Add PrecisionMode to forward() signature (30 min)

**Modify** `include/mini_transformer.h`:
```cpp
// Add precision mode parameter
std::vector<std::vector<float>> forward(
    const std::vector<int>& tokens,
    MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecisionOptimizer::PrecisionMode::FP32
);
```

**Test**: Build compiles (other code still calls `forward(tokens)` with default FP32)

#### Step 2: Create weight conversion helpers (2-3 hours)

**Add to mini_transformer.cpp**:
```cpp
void MiniTransformer::convert_weights_to_fp16() {
    // For each layer, convert FP32 weights → FP16
    // Store in temporary FP16 buffers
}

void MiniTransformer::convert_weights_to_bf16() {
    // Similar but BF16
}

void MiniTransformer::restore_weights_to_fp32() {
    // Restore original FP32 weights after forward
}
```

**Decision**: Do we convert in-place or use separate buffers?
- **Option A**: Separate buffers (safer, more memory)
- **Option B**: In-place conversion (risky, less memory)
- **Recommendation**: Option A for now

#### Step 3: Modify attention for mixed precision (3-4 hours)

**Current**: `multi_head_attention()` works only in FP32

**Target**: Add precision parameter:
```cpp
std::vector<std::vector<float>> multi_head_attention(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    bool causal_mask,
    PrecisionMode mode  // NEW
) {
    if (mode == PrecisionMode::FP16) {
        // Use FP16 for matmul, convert back to FP32
        auto Q_fp16 = convert_to_fp16(Q);
        auto scores_fp16 = matmul_fp16(Q_fp16, K_fp16);
        auto scores_fp32 = convert_to_fp32(scores_fp16);
        // Continue in FP32 for softmax (stability)
    }
}
```

#### Step 4: Modify feed-forward for mixed precision (2-3 hours)

Similar changes to `feed_forward()`:
```cpp
std::vector<std::vector<float>> feed_forward(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    PrecisionMode mode  // NEW
) {
    // Convert to FP16/BF16 for matmul
    // Convert back to FP32 for activation (GELU)
}
```

#### Step 5: Test forward pass in each mode (2-3 hours)

**Tests**:
```bash
# Test FP32 (should match current)
output_fp32 = forward(tokens, FP32);

# Test FP16 (should be close to FP32)
output_fp16 = forward(tokens, FP16);
assert(cosine_similarity(output_fp32, output_fp16) > 0.99);

# Test BF16
output_bf16 = forward(tokens, BF16);
assert(cosine_similarity(output_fp32, output_bf16) > 0.99);
```

**Total Day 1-2**: 10-14 hours (2 full days)

---

## 🚧 Key Challenges

### Challenge 1: C++ Doesn't Have Native FP16/BF16

**Problem**: C++ has `float` (FP32) and `double` (FP64), but no `half` or `bfloat16`

**Solutions**:

**Option A**: Use uint16_t and manual conversion (recommended)
```cpp
uint16_t fp32_to_fp16(float value) {
    // Extract sign, exponent, mantissa
    // Truncate mantissa to 10 bits
    // Return as uint16_t
}

float fp16_to_fp32(uint16_t value) {
    // Expand to FP32
}
```

**Option B**: Use intrinsics (AVX-512F has FP16 support)
```cpp
#include <immintrin.h>
__m256 _mm256_cvtph_ps(__m128i a);  // Convert FP16 → FP32
```

**Option C**: Use library (e.g., half.hpp)
```cpp
#include "half.hpp"
using half_float::half;
```

**Recommendation**: Start with Option A (portable), can optimize with B later

### Challenge 2: Numerical Stability

**Problem**: FP16 has limited range (max ~65,504)

**Solutions**:
- Use FP32 for softmax (overflow risk)
- Use FP32 for layer norm (numerical stability)
- Use FP16 only for matmul (most compute)

**Pattern**:
```cpp
// Matmul in FP16
auto scores_fp16 = matmul(Q_fp16, K_fp16);

// Convert to FP32 for softmax
auto scores_fp32 = convert_to_fp32(scores_fp16);
auto weights = softmax(scores_fp32);  // Stable

// Convert back to FP16 for next matmul
auto weights_fp16 = convert_to_fp16(weights);
```

### Challenge 3: Memory Management

**Problem**: Do we need separate FP16 weight storage?

**Approach 1**: Temporary conversion
```cpp
forward(tokens, FP16) {
    auto weights_fp16 = convert_weights_fp16();  // Temporary
    // ... compute ...
    // weights_fp16 destroyed after function
}
```
**Memory**: 2x during forward (FP32 + FP16 both in RAM)

**Approach 2**: In-place conversion
```cpp
forward(tokens, FP16) {
    convert_weights_in_place_fp16();  // Overwrite FP32
    // ... compute ...
    restore_weights_fp32();  // Restore
}
```
**Memory**: 1x (only one copy)
**Risk**: Must restore or lose FP32 precision

**Recommendation**: Approach 1 (safer)

---

## 📝 Implementation Checklist (Day 1-2)

### Part 1: Setup (2-3 hours)
- [ ] Add FP16/BF16 conversion functions
  - [ ] `fp32_to_fp16()`
  - [ ] `fp16_to_fp32()`
  - [ ] `fp32_to_bf16()`
  - [ ] `bf16_to_fp32()`
- [ ] Test conversions (unit tests)
- [ ] Add precision mode parameter to forward()

### Part 2: Weight Conversion (3-4 hours)
- [ ] Implement `convert_weights_to_fp16()`
- [ ] Implement `convert_weights_to_bf16()`
- [ ] Test conversion accuracy (check values)
- [ ] Measure memory usage

### Part 3: Attention Update (3-4 hours)
- [ ] Add precision mode to `multi_head_attention()`
- [ ] Implement FP16 matmul path
- [ ] Keep softmax in FP32 (stability)
- [ ] Test output matches FP32 (>99% similarity)

### Part 4: Feed-Forward Update (2-3 hours)
- [ ] Add precision mode to `feed_forward()`
- [ ] Implement FP16 matmul path
- [ ] Keep GELU in FP32 (stability)
- [ ] Test output matches FP32

### Part 5: Integration & Testing (2-3 hours)
- [ ] Update forward() to use precision-aware methods
- [ ] Test FP32 mode (should match current)
- [ ] Test FP16 mode (should be close)
- [ ] Test BF16 mode (should be close)
- [ ] Benchmark speed (should be faster)

**Total**: 12-17 hours (realistically 2-3 days with breaks)

---

## 🎯 Success Criteria (Day 1-2)

| Criterion | Target | How to Verify |
|-----------|--------|---------------|
| **Builds successfully** | 0 errors | Compile |
| **FP32 mode works** | Identical to current | Output comparison |
| **FP16 mode works** | >99% similarity to FP32 | Cosine similarity |
| **BF16 mode works** | >99% similarity to FP32 | Cosine similarity |
| **Speed improvement** | 10-20% faster | Timing benchmarks |
| **Memory usage** | 2x during forward (acceptable) | Task manager |

---

## 🚀 Quick Start (Next 30 Minutes)

Let me start by creating the FP16/BF16 conversion utilities:

### Immediate Tasks:

1. **Create** `src/precision_utils.cpp`
   - FP16/BF16 conversion functions
   - Test with sample values

2. **Modify** `include/mini_transformer.h`
   - Add precision mode to forward()
   - Add conversion method declarations

3. **Test** Build compiles

**Let's begin!** Would you like me to:
- A) Start implementing conversion functions now
- B) Review the plan first and discuss approach
- C) Something else

**Your choice?** 🚀
