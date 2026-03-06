# Scale Testing Plan - Validate B1 & B2 Implementations
**Date**: 2026-03-05
**Goal**: Test KV-Cache and Flash Attention on larger models to validate real-world performance
**Duration**: 3-5 days

---

## 🎯 Objectives

1. **Validate KV-Cache**: Measure actual 10-50x speedup on production-scale models
2. **Validate Flash Attention**: Measure actual memory reduction and long-context capability
3. **Benchmark Combined**: Test KV-Cache + Flash Attention together
4. **Document Results**: Provide concrete performance numbers

---

## 📊 Current Baseline

### Existing Model (Tiny)
- **Size**: 3M parameters
- **Architecture**: 4 layers, 256 embedding, 4 heads
- **Context**: 512 tokens max
- **Training**: ~6 minutes on small corpus

**Problem**: Too small to show real speedups!
- KV-Cache overhead > benefit (cache creation cost dominates)
- Flash Attention overhead > benefit (block processing cost dominates)
- Need larger model to see advantages

---

## 🚀 Scale Testing Approach

### Test Model Sizes

| Model | Layers | Embedding | Heads | Params | Use Case |
|-------|--------|-----------|-------|--------|----------|
| **Tiny** (current) | 4 | 256 | 4 | 3M | ❌ Too small |
| **Small** (test 1) | 6 | 512 | 8 | 12M | ✅ Good for testing |
| **Medium** (test 2) | 12 | 768 | 12 | 50M | ✅ Shows real speedup |
| **Large** (future) | 24 | 1024 | 16 | 150M | 🎯 Production target |

**Decision**: Focus on **Small (12M)** and **Medium (50M)** for testing

---

## 📝 Test Plan

### Phase 1: Train Larger Model (Day 1-2)

**Objective**: Train a 12M parameter model to test optimizations

**Steps**:

1. **Prepare Corpus**:
   ```bash
   # Already created: test_scale_corpus.txt (2,370 lines)
   # This is sufficient for initial testing
   ```

2. **Configure Larger Model**:
   ```cpp
   // In neural_engine.cpp or test script
   TransformerConfig config;
   config.vocab_size = 282;          // Same tokenizer
   config.embedding_dim = 512;       // 2x larger (was 256)
   config.num_layers = 6;            // 1.5x more (was 4)
   config.num_heads = 8;             // 2x more (was 4)
   config.ff_dim = 2048;             // 2x larger (was 1024)
   config.max_seq_length = 1024;     // 2x longer (was 512)
   ```

3. **Train Model**:
   ```bash
   # Standard FP32 training
   ./bin/neural_engine.exe train_transformer test_scale_corpus.txt 7 0.002 16

   # Save as models/transformer_12M.bin
   ```

**Expected**: 20-30 minutes training time (vs 6 min for tiny model)

### Phase 2: Benchmark Standard Generation (Day 2)

**Objective**: Establish baseline performance without optimizations

**Test Cases**:

1. **Short Sequence** (100 tokens):
   ```bash
   echo "Testing standard generation (100 tokens)..."
   time ./bin/neural_engine.exe generate "The quick brown fox" 100
   ```

2. **Medium Sequence** (500 tokens):
   ```bash
   time ./bin/neural_engine.exe generate "The quick brown fox" 500
   ```

3. **Long Sequence** (1000 tokens):
   ```bash
   time ./bin/neural_engine.exe generate "The quick brown fox" 1000
   ```

**Metrics to Capture**:
- Total time (seconds)
- Tokens per second
- Memory usage (task manager or `time -v` on Linux)

### Phase 3: Benchmark KV-Cache Generation (Day 3)

**Objective**: Measure actual speedup from KV-Cache

**Same Test Cases with KV-Cache**:

1. **Short Sequence** (100 tokens):
   ```bash
   time ./bin/neural_engine.exe generate_cached "The quick brown fox" 100
   ```

2. **Medium Sequence** (500 tokens):
   ```bash
   time ./bin/neural_engine.exe generate_cached "The quick brown fox" 500
   ```

3. **Long Sequence** (1000 tokens):
   ```bash
   time ./bin/neural_engine.exe generate_cached "The quick brown fox" 1000
   ```

**Expected Results**:
- 100 tokens: 2-5x speedup (cache overhead still significant)
- 500 tokens: 10-20x speedup (cache benefit dominates)
- 1000 tokens: 20-40x speedup (approaching theoretical 50x)

**Calculate Speedup**:
```
Speedup = Time_standard / Time_cached
```

### Phase 4: Benchmark Flash Attention (Day 3-4)

**Objective**: Measure memory reduction and long-context capability

**Test Long Context**:

1. **Standard Attention - 512 tokens**:
   ```bash
   # Create 512-token prompt
   cat test_scale_corpus.txt | head -c 2048 > long_prompt_512.txt

   time ./bin/neural_engine.exe generate "$(cat long_prompt_512.txt)" 50
   # Monitor memory usage
   ```

2. **Flash Attention - 512 tokens**:
   ```bash
   time ./bin/neural_engine.exe generate_flash "$(cat long_prompt_512.txt)" 50
   # Compare memory usage
   ```

3. **Flash Attention - 1024 tokens** (impossible with standard):
   ```bash
   cat test_scale_corpus.txt | head -c 4096 > long_prompt_1024.txt

   # This should work with Flash, fail/OOM with standard
   time ./bin/neural_engine.exe generate_flash "$(cat long_prompt_1024.txt)" 50
   ```

**Expected Results**:
- 512 tokens: 16x memory reduction (standard: 1MB, flash: 64KB)
- 1024 tokens: 65x memory reduction (standard: 4.2MB, flash: 64KB)
- 2048 tokens: 268x memory reduction (standard: OOM, flash: 64KB)

### Phase 5: Combined Testing (Day 4)

**Objective**: Test KV-Cache + Flash Attention together

**Implementation**:
1. Modify `generate_cached` to enable Flash Attention
2. Test on long sequences
3. Measure combined benefit

**Pseudo-code**:
```cpp
// In generate_cached implementation
config.use_flash_attention = true;  // Enable Flash too!
KVCache::CacheManager cache(config);
// ... rest of KV-Cache generation
```

**Expected**: Best of both worlds (fast + long context)

### Phase 6: Document Results (Day 5)

**Objective**: Create comprehensive performance report

**Document**:
1. All benchmark results in table format
2. Speedup graphs (if possible)
3. Memory usage comparison
4. Real-world recommendations

---

## 📊 Expected Results Summary

### KV-Cache Speedup (12M Model)

| Tokens | Standard Time | Cached Time | Speedup | Theoretical |
|--------|--------------|-------------|---------|-------------|
| 100 | 10s | 3s | **3.3x** | 5.5x |
| 500 | 250s | 15s | **16.7x** | 25.5x |
| 1000 | 1000s | 25s | **40x** | 50.5x |

**Why not theoretical?**:
- Cache creation overhead (~5-10s)
- Smaller model = less compute saved
- But still shows **massive improvement!**

### Flash Attention Memory (12M Model)

| Context | Standard Memory | Flash Memory | Reduction |
|---------|----------------|--------------|-----------|
| 512 | 1.05 MB | 64 KB | **16x** |
| 1024 | 4.2 MB | 64 KB | **66x** |
| 2048 | 16.8 MB | 64 KB | **262x** |
| 4096 | 67 MB | 64 KB | **1,048x** |

**Key**: Flash Attention memory is **constant** regardless of sequence length!

### Combined (KV-Cache + Flash)

| Context | Tokens | Time | Memory | Notes |
|---------|--------|------|--------|-------|
| 512 | 500 | 15s | 64 KB | Fast + efficient ✅ |
| 1024 | 1000 | 25s | 64 KB | Long context ✅ |
| 2048 | 1000 | 25s | 64 KB | Very long! ✅ |

**Enables**: Fast generation on very long context (previously impossible)

---

## 🔧 Implementation Steps

### Step 1: Create Test Configuration (30 min)

Create `test_configs/scale_test_config.cpp`:
```cpp
TransformerConfig get_scale_test_config() {
    TransformerConfig config;
    config.vocab_size = 282;
    config.embedding_dim = 512;
    config.num_layers = 6;
    config.num_heads = 8;
    config.ff_dim = 2048;
    config.max_seq_length = 1024;
    return config;
}
```

### Step 2: Create Benchmark Script (1 hour)

Create `benchmark_optimizations.bat`:
```batch
@echo off
echo ========================================
echo Benchmarking Optimizations
echo ========================================

echo.
echo Phase 1: Standard Generation Baseline
echo ----------------------------------------
echo 100 tokens:
time ./bin/neural_engine.exe generate "The quick brown fox" 100

echo 500 tokens:
time ./bin/neural_engine.exe generate "The quick brown fox" 500

echo.
echo Phase 2: KV-Cache Generation
echo ----------------------------------------
echo 100 tokens:
time ./bin/neural_engine.exe generate_cached "The quick brown fox" 100

echo 500 tokens:
time ./bin/neural_engine.exe generate_cached "The quick brown fox" 500

echo.
echo Phase 3: Flash Attention
echo ----------------------------------------
echo Long context (1024 tokens):
time ./bin/neural_engine.exe generate_flash "$(cat long_prompt_1024.txt)" 50

echo.
echo ========================================
echo Benchmarks Complete!
echo ========================================
```

### Step 3: Train Larger Model (2-3 hours)

```bash
# Modify neural_engine.cpp train_transformer to use scale_test_config
# Or manually edit config in the command

./bin/neural_engine.exe train_transformer test_scale_corpus.txt 7 0.002 16

# Wait ~30 minutes
# Model saved to models/transformer.bin (will be ~50 MB instead of 12 MB)
```

### Step 4: Run Benchmarks (1-2 hours)

```bash
./benchmark_optimizations.bat > benchmark_results.txt 2>&1

# Review results
cat benchmark_results.txt
```

### Step 5: Document (2-3 hours)

Create comprehensive results document with:
- All timing data
- Speedup calculations
- Memory measurements
- Comparison tables
- Recommendations

---

## 🎯 Success Criteria

| Metric | Target | Status |
|--------|--------|--------|
| KV-Cache speedup (500 tokens) | > 10x | 📋 To test |
| Flash Attention memory (1024 tokens) | < 100 KB | 📋 To test |
| Combined test works | Generate 1000 tokens on 1024 context | 📋 To test |
| Documentation complete | Comprehensive report | 📋 To write |

---

## ⚠️ Potential Issues

### Issue 1: Model Too Large to Train

**Problem**: 12M model takes too long or runs out of memory

**Solution**: Use 6M model instead (same config but 4 layers)

### Issue 2: KV-Cache Doesn't Show Speedup

**Problem**: Cache overhead still dominates on current hardware

**Solution**: Document that larger models (50M+) will benefit more

### Issue 3: Can't Test Very Long Context

**Problem**: Don't have good test prompts > 2K tokens

**Solution**: Repeat test corpus multiple times to create artificial long context

---

## 📝 Quick Start (Minimal Testing)

If full 3-5 days is too much, here's a **1-day quick test**:

### Quick Test Plan (1 day, ~6 hours)

1. **Use Existing Model** (skip training larger one)
   - Current 3M model is fine for proof-of-concept
   - Won't show huge speedups but will show the pattern

2. **Quick Benchmarks** (2 hours)
   ```bash
   # Standard vs KV-Cache (30 tokens)
   time ./bin/neural_engine.exe generate "test" 30
   time ./bin/neural_engine.exe generate_cached "test" 30

   # Flash Attention (256 tokens)
   time ./bin/neural_engine.exe generate_flash "$(head -c 1024 test_scale_corpus.txt)" 20
   ```

3. **Document Pattern** (4 hours)
   - Show that KV-Cache works (even if no speedup on tiny model)
   - Show that Flash Attention works (constant memory)
   - Explain that larger models will show better results
   - Mark this as "proof of concept" not full validation

**Value**: Confirms implementations work, documents expected behavior

---

## 📖 Files for Reference

- **Training Corpus**: `test_scale_corpus.txt` (2,370 lines, created)
- **Current Model**: `models/transformer.bin` (3M params)
- **Larger Model**: `models/transformer_12M.bin` (to be created)
- **This Plan**: `SCALE_TESTING_PLAN.md`

---

## 🚦 Recommendation

### Option A: Full Scale Testing (3-5 days)
**Pros**: Validates everything, shows real speedups
**Cons**: Requires training larger model (time investment)

### Option B: Quick Proof of Concept (1 day)
**Pros**: Fast, shows implementations work
**Cons**: Won't show dramatic speedups on tiny model

### Option C: Defer Until Needed (0 days)
**Pros**: Move on to other features
**Cons**: Implementations not validated at scale

**Verdict**: Recommend **Option B** (Quick PoC) to validate implementations, then move to Week 7 K10-K12 or other features. Can return for full scale testing when training production models.

---

**Status**: Ready to begin! Choose option and start testing.

**Next Step**: Run quick benchmarks on existing model OR train larger model for full validation.
