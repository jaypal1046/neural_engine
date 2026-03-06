# Week 9 Day 4 - SIMD Optimization & Benchmarking - PLAN

**Date**: 2026-03-06
**Goal**: Implement true FP16 compute with SIMD for actual speedup
**Target**: 1.5-2x speedup on larger models

---

## 🎯 Current State Analysis

### What We Have (Days 1-3)
```cpp
// Round-trip conversion (no actual speedup)
Q[i][j] = fp16_to_fp32(fp32_to_fp16(Q[i][j]));

// Problems:
// 1. Still using FP32 storage (no memory savings)
// 2. Conversion overhead (2x per value)
// 3. No SIMD FP16 instructions
// 4. Small model hides conversion cost
```

**Current Performance**: ~1.0x (no speedup)

### What We Need (Day 4)

**Option A: True FP16 Storage (Better)**
```cpp
// Store activations in FP16 format
vector<uint16_t> Q_fp16;  // Half memory
// Use F16C SIMD instructions for compute
__m128i fp16_vals = _mm_loadu_si128((__m128i*)Q_fp16.data());
__m256 fp32_vals = _mm256_cvtph_ps(fp16_vals);  // Fast SIMD convert
```

**Benefits**:
- 50% memory reduction
- SIMD conversion (8 values at once)
- Foundation for training

**Option B: SIMD-Optimized Round-Trip (Simpler)**
```cpp
// Keep FP32 storage but optimize conversion
fp32_array_to_fp16_simd(Q_flat.data(), Q_fp16.data(), size);
// Compute stays in FP32 but validates precision
```

**Benefits**:
- Simpler to implement
- Less architectural change
- Good for measuring precision impact

---

## 📋 Day 4 Strategy

### Approach: Hybrid (Best of Both)

**Phase 1**: SIMD-Optimized Conversion (2 hours)
- Implement SIMD batch conversion functions
- Use F16C/AVX2 intrinsics
- Keep FP32 storage (minimal risk)

**Phase 2**: Benchmark on Larger Model (1 hour)
- Create 10M parameter model test
- Measure FP32 vs FP16 vs BF16
- Document actual speedup

**Phase 3**: Document & Analyze (1 hour)
- Write comprehensive results
- Identify bottlenecks
- Plan Day 5 training integration

**Total**: 4 hours

---

## 🔧 Implementation Plan

### Task 1: SIMD Batch Conversion (2 hours)

**File**: `src/precision_utils.cpp`

**Add Functions**:
```cpp
// F16C SIMD conversion (8 FP32 → 8 FP16 at once)
void fp32_array_to_fp16_simd(const float* src, uint16_t* dst, size_t count);
void fp16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count);

// AVX2 for BF16 (16 values at once - simpler than FP16)
void fp32_array_to_bf16_simd(const float* src, uint16_t* dst, size_t count);
void bf16_array_to_fp32_simd(const uint16_t* src, float* dst, size_t count);
```

**Implementation**:
```cpp
#include <immintrin.h>  // F16C/AVX2

void fp32_array_to_fp16_simd(const float* src, uint16_t* dst, size_t count) {
    size_t i = 0;

    // Process 8 floats at a time (256-bit AVX)
    for (; i + 8 <= count; i += 8) {
        __m256 fp32 = _mm256_loadu_ps(&src[i]);
        __m128i fp16 = _mm256_cvtps_ph(fp32, _MM_FROUND_TO_NEAREST_INT);
        _mm_storeu_si128((__m128i*)&dst[i], fp16);
    }

    // Handle remaining elements (scalar fallback)
    for (; i < count; i++) {
        dst[i] = fp32_to_fp16(src[i]);
    }
}
```

**Expected Speedup**: 8x for conversion (8 values per instruction)

---

### Task 2: Update Attention/Feedforward (1 hour)

**File**: `src/mini_transformer.cpp`

**Current** (scalar):
```cpp
for (int i = 0; i < seq_len; i++) {
    for (int j = 0; j < d_model; j++) {
        Q[i][j] = fp16_to_fp32(fp32_to_fp16(Q[i][j]));
    }
}
```

**Optimized** (SIMD):
```cpp
// Flatten Q for SIMD processing
std::vector<float> Q_flat(seq_len * d_model);
std::vector<uint16_t> Q_fp16(seq_len * d_model);

// Copy to flat array
for (int i = 0; i < seq_len; i++) {
    std::copy(Q[i].begin(), Q[i].end(), Q_flat.begin() + i * d_model);
}

// SIMD round-trip
fp32_array_to_fp16_simd(Q_flat.data(), Q_fp16.data(), seq_len * d_model);
fp16_array_to_fp32_simd(Q_fp16.data(), Q_flat.data(), seq_len * d_model);

// Copy back
for (int i = 0; i < seq_len; i++) {
    std::copy(Q_flat.begin() + i * d_model, Q_flat.begin() + (i + 1) * d_model, Q[i].begin());
}
```

**Expected**: Minimal overhead, validates SIMD works

---

### Task 3: Create Large Model Benchmark (1 hour)

**File**: `test_mixed_precision_benchmark.cpp`

**Test Configuration**:
```cpp
// Small model (current)
config.embedding_dim = 128;
config.num_layers = 2;
// ~0.3M params, too fast to measure

// Large model (new)
config.embedding_dim = 768;   // GPT-2 size
config.num_layers = 12;        // Deeper
config.num_heads = 12;
config.ff_dim = 3072;
// ~10M params, measurable timing
```

**Benchmark Sequence**:
```cpp
// Short sequence (fast)
std::vector<int> tokens_short(16, 10);   // 16 tokens

// Medium sequence (realistic)
std::vector<int> tokens_medium(128, 10);  // 128 tokens

// Long sequence (stress test)
std::vector<int> tokens_long(512, 10);    // 512 tokens
```

**Metrics to Measure**:
1. **Time**: FP32 vs FP16 vs BF16
2. **Speedup**: FP16/FP32, BF16/FP32
3. **Memory**: Peak usage (rough estimate)
4. **Accuracy**: Error vs FP32

---

### Task 4: Document Results (1 hour)

**Expected Results**:

**Small Model (0.3M params)**:
- FP32: ~10ms
- FP16: ~10ms (no speedup - conversion overhead)
- Conclusion: Too small to benefit

**Large Model (10M params)**:
- FP32: ~200ms
- FP16: ~150ms (1.3x speedup from SIMD conversion)
- BF16: ~160ms (1.25x speedup)
- Conclusion: Measurable benefit, but not 2x yet

**Why Not 2x Yet**:
- Still doing FP32 compute (matmul in FP32)
- SIMD only helps conversion
- Need FP16 matmul for full speedup (future)

**What We Learn**:
- SIMD conversion works
- Larger models benefit more
- Ready for training integration

---

## 🚀 Alternative: Full FP16 Storage (Optional)

**If Time Permits** (add 2 hours):

### True FP16 Compute Path

**Changes Required**:
1. Store Q, K, V in `vector<uint16_t>` (not `vector<float>`)
2. Convert to FP32 only for matmul
3. Keep intermediate results in FP16

**Implementation**:
```cpp
// Store in FP16
std::vector<uint16_t> Q_fp16(seq_len * d_model);
std::vector<uint16_t> K_fp16(seq_len * d_model);
std::vector<uint16_t> V_fp16(seq_len * d_model);

// Compute Q = input * W_Q (still FP32 matmul)
TensorOps::matmul(input_flat, W_Q_flat, Q_fp32_tmp, ...);

// Convert result to FP16 for storage
fp32_array_to_fp16_simd(Q_fp32_tmp.data(), Q_fp16.data(), size);

// For attention scores: Q * K^T
// Convert FP16 → FP32 for matmul
fp16_array_to_fp32_simd(Q_fp16.data(), Q_fp32.data(), size);
fp16_array_to_fp32_simd(K_fp16.data(), K_fp32.data(), size);
// Compute in FP32
float dot = vec_dot(Q_fp32, K_fp32, d_model);
```

**Benefits**:
- 50% memory reduction (real savings)
- Foundation for FP16 matmul
- Production-ready architecture

**Risks**:
- More complex debugging
- Might find accuracy issues
- Requires more testing

**Decision**: Do SIMD conversion first, then evaluate if time permits

---

## 📊 Success Criteria

### Minimum (Must Have)
- ✅ SIMD conversion functions implemented
- ✅ 8x faster conversion (8 values per instruction)
- ✅ Large model benchmark created
- ✅ Measurable speedup documented (>1.2x)

### Target (Should Have)
- ✅ 1.5x overall speedup on 10M model
- ✅ Memory profiling data
- ✅ Comprehensive benchmark results
- ✅ Clear path to 2x speedup

### Stretch (Nice to Have)
- ⭐ True FP16 storage implemented
- ⭐ 2x speedup achieved
- ⭐ Production-ready optimization

---

## 🔍 Technical Details

### F16C Instructions (Hardware Support)

**Required CPU**:
- Intel: Ivy Bridge (2012) or newer
- AMD: Piledriver (2012) or newer
- **Check**: Most modern CPUs support this

**Detection**:
```cpp
bool has_f16c() {
    #ifdef __F16C__
        return true;
    #else
        // Runtime check
        __builtin_cpu_supports("f16c");
    #endif
}
```

**Compile Flags**:
```bash
g++ -mf16c -mavx2 ...
```

### AVX2 for BF16

**Conversion** (simpler than FP16):
```cpp
// BF16 = upper 16 bits of FP32
// Can process 8 floats at once with AVX2

__m256i fp32_to_bf16_avx2(__m256 fp32) {
    // Shift right 16 bits, extract upper half
    __m256i i32 = _mm256_castps_si256(fp32);
    __m256i upper = _mm256_srli_epi32(i32, 16);
    // Pack 8x32-bit → 8x16-bit
    return _mm256_packus_epi32(upper, upper);
}
```

---

## 📝 Deliverables

### Code
1. SIMD conversion functions (precision_utils.cpp)
2. Updated attention/feedforward (mini_transformer.cpp)
3. Large model benchmark (test_mixed_precision_benchmark.cpp)

### Documentation
4. Benchmark results (BENCHMARK_RESULTS_DAY4.md)
5. Day 4 completion report (WEEK_9_DAY4_COMPLETE.md)
6. Updated summary (WEEK_9_DAYS_1-4_SUMMARY.md)

### Metrics
7. Speedup measurements (FP32 vs FP16 vs BF16)
8. Memory usage analysis
9. Accuracy validation

---

## ⏱️ Timeline

**Hour 1**: Implement SIMD conversion functions
- Write fp32_to_fp16_simd
- Write fp16_to_fp32_simd
- Test and validate

**Hour 2**: Integrate into attention/feedforward
- Update multi_head_attention
- Update feed_forward
- Build and test

**Hour 3**: Create and run large model benchmark
- Write benchmark code
- Run tests on 10M model
- Collect performance data

**Hour 4**: Document and analyze
- Write benchmark results
- Analyze bottlenecks
- Plan Days 5-7

---

## 🎯 Expected Outcomes

### Performance
- SIMD conversion: 8x faster than scalar
- Large model: 1.3-1.5x overall speedup
- Small model: ~1.0x (conversion overhead)

### Architecture
- SIMD infrastructure in place
- Ready for FP16 matmul (future)
- Foundation for training (Days 5-6)

### Learning
- Understand where bottlenecks are
- Validate SIMD approach
- Identify next optimizations

---

**Status**: 📋 **PLAN COMPLETE** - Ready to implement!

**Next**: Start Hour 1 - Implement SIMD conversion functions
