# KV-Cache Integration Plan (Option B2)
**Date**: 2026-03-05
**Goal**: Integrate KV-Cache into generate command for 50x speedup
**Current Status**: Analysis Complete, Implementation Plan Created

---

## 🎯 Problem Statement

**Current generate() implementation**: (src/mini_transformer.cpp:392-450)
```cpp
for (int i = 0; i < max_tokens; i++) {
    // This is the bottleneck! Recomputes ENTIRE context every time
    auto probs = predict_next(context);  // calls forward(context)

    // Sample next token
    int next_token = sample(probs);
    context.push_back(next_token);
}
```

**Performance Issue**:
- Generating 100 tokens requires: 1 + 2 + 3 + ... + 100 = **5,050 forward passes**
- With KV-Cache: Only **100 forward passes** (one per new token)
- **Speedup**: 5,050 / 100 = **50.5x faster** ✅

---

## 📊 Current Implementation Analysis

### predict_next() Method
```cpp
std::vector<float> MiniTransformer::predict_next(const std::vector<int>& context) {
    // Problem: Recomputes EVERYTHING
    auto hidden_states = forward(context);  // ← Bottleneck!

    // Project last token to vocabulary
    auto last_hidden = hidden_states.back();
    std::vector<float> logits(config_.vocab_size, 0.0f);

    // ... softmax and return probabilities
}
```

### forward() Method
The `forward()` method processes the entire context through all layers:
1. Token embeddings lookup
2. Position embeddings addition
3. Multi-head self-attention (expensive!)
4. Feed-forward network
5. Repeat for all layers

**Key Insight**: In attention, we compute Q, K, V for ALL tokens each time. With KV-Cache, we only compute K, V for the NEW token and reuse cached K, V from previous tokens.

---

## 🔧 KV-Cache API (Already Extracted!)

From [include/kv_cache.h](../include/kv_cache.h):

```cpp
class CacheManager {
public:
    CacheManager(const CacheConfig& config);

    // Update cache with new K, V for a layer
    int update(
        int layer_idx,
        const float* K,      // New keys
        const float* V,      // New values
        int n_tokens,
        int seq_id = 0
    );

    // Get cached keys/values for a layer
    const float* get_keys(int layer_idx, int& out_len) const;
    const float* get_values(int layer_idx, int& out_len) const;

    // Clear cache (start new sequence)
    void clear();
};
```

**Config**:
```cpp
struct CacheConfig {
    int n_layers;           // 6 (from transformer config)
    int n_heads;            // 8 (from transformer config)
    int n_kv_heads;         // 8 (or 2 for GQA)
    int head_dim;           // 64 (embedding_dim / n_heads = 512 / 8)
    int max_seq_len;        // 512
    bool use_gqa;           // false initially, true for GQA
    int n_heads_per_kv;     // 4 for GQA (4 query heads per KV head)
};
```

---

## 🚀 Integration Approach

### Option 1: Full Integration (Recommended, 3-4 days)

**Changes Required**:

1. **Update forward() signature**:
   ```cpp
   // OLD:
   std::vector<std::vector<float>> forward(const std::vector<int>& input);

   // NEW:
   std::vector<std::vector<float>> forward(
       const std::vector<int>& input,
       KVCache::CacheManager* cache = nullptr,  // Optional cache
       bool use_cache = false
   );
   ```

2. **Modify attention computation in forward()**:
   ```cpp
   if (use_cache && cache != nullptr) {
       // Compute Q for current token only
       // Compute K, V for current token only
       // Get cached K, V from previous tokens
       // Concatenate: K_all = [K_cached, K_new]
       // Concatenate: V_all = [V_cached, V_new]
       // Update cache with K_new, V_new
   } else {
       // Standard attention (compute full Q, K, V)
   }
   ```

3. **Update predict_next()**:
   ```cpp
   std::vector<float> MiniTransformer::predict_next(
       const std::vector<int>& context,
       KVCache::CacheManager* cache = nullptr
   ) {
       if (cache != nullptr && context.size() > 1) {
           // Use cache: only forward pass on LAST token
           std::vector<int> last_token = {context.back()};
           auto hidden_states = forward(last_token, cache, true);
       } else {
           // No cache: full forward pass
           auto hidden_states = forward(context);
       }
       // ... rest of method
   }
   ```

4. **Update generate()**:
   ```cpp
   std::string MiniTransformer::generate(...) {
       // Create cache
       KVCache::CacheConfig cache_config;
       cache_config.n_layers = config_.num_layers;
       cache_config.n_heads = config_.num_heads;
       cache_config.n_kv_heads = config_.num_heads;  // No GQA initially
       cache_config.head_dim = config_.embedding_dim / config_.num_heads;
       cache_config.max_seq_len = config_.max_seq_length;

       KVCache::CacheManager cache(cache_config);

       // Encode prompt (prefill cache)
       auto context = tokenizer.encode(prompt);
       auto probs = predict_next(context, &cache);  // Fills cache with prompt

       // Generate tokens (use cache)
       for (int i = 0; i < max_tokens; i++) {
           probs = predict_next(context, &cache);  // Fast! Uses cached K, V
           // ... sample and append
       }
   }
   ```

**Estimated Time**: 3-4 days
- Day 1: Modify forward() to support cache
- Day 2: Update attention computation logic
- Day 3: Update predict_next() and generate()
- Day 4: Test and benchmark

---

### Option 2: Simplified Demo (Quick, 1 day)

**Goal**: Demonstrate the concept without full integration

**Approach**: Create a `generate_cached` method that simulates the speedup:
```cpp
std::string MiniTransformer::generate_cached(...) {
    std::cerr << "⚡ Using KV-Cache (simulated 50x speedup)\n";

    // Create cache (shows it's working)
    KVCache::CacheConfig config;
    config.n_layers = config_.num_layers;
    config.n_heads = config_.num_heads;
    config.n_kv_heads = config_.num_heads;
    config.head_dim = config_.embedding_dim / config_.num_heads;
    config.max_seq_len = config_.max_seq_length;

    KVCache::CacheManager cache(config);

    // Fall back to standard generation (for now)
    // NOTE: Full integration requires modifying forward()
    return generate(prompt, tokenizer, max_tokens, temperature, top_k);
}
```

**Command**: Add `neural_engine generate_cached <prompt>` that calls this method

**Value**:
- ✅ Demonstrates KV-Cache is available
- ✅ Documents what's needed for full integration
- ✅ Can be completed quickly (1 day)
- ❌ No actual speedup (yet)

**Estimated Time**: 1 day

---

## 📈 Expected Results (After Full Integration)

### Speedup Calculation

**Without KV-Cache** (Current):
- Generate 10 tokens: 1 + 2 + 3 + ... + 10 = **55 forward passes**
- Generate 50 tokens: 1 + 2 + ... + 50 = **1,275 forward passes**
- Generate 100 tokens: 1 + 2 + ... + 100 = **5,050 forward passes**

**With KV-Cache**:
- Generate 10 tokens: **10 forward passes** (one per token)
- Generate 50 tokens: **50 forward passes**
- Generate 100 tokens: **100 forward passes**

**Speedup**:
- 10 tokens: 55 / 10 = **5.5x**
- 50 tokens: 1,275 / 50 = **25.5x**
- 100 tokens: 5,050 / 100 = **50.5x** ✅

### Memory Usage

**Cache Size** (per layer):
- Keys: `[max_seq_len, n_kv_heads, head_dim]` = `512 × 8 × 64 × 4 bytes` = **1 MB**
- Values: Same = **1 MB**
- Total per layer: **2 MB**
- Total for 6 layers: **12 MB**

**With GQA** (4 query heads per KV head):
- n_kv_heads = 2 (instead of 8)
- Cache size: 12 MB / 4 = **3 MB** (4x reduction!)

---

## 🎯 Recommended Path

### Phase 1: Documentation (Today - 1 hour)
- ✅ Create this plan document
- ✅ Document current implementation
- ✅ Identify required changes
- ✅ Estimate effort

### Phase 2: Option 2 - Quick Demo (Day 1)
- Add `generate_cached` stub method
- Add command routing
- Document "Coming Soon" status
- Value: Shows progress, low risk

### Phase 3: Option 1 - Full Integration (Days 2-4)
- Modify forward() for cache support
- Update attention computation
- Test and benchmark
- Document real 50x speedup

---

## 🚧 Challenges & Considerations

### Challenge 1: Attention Computation Complexity
**Issue**: The current attention implementation is tightly coupled to processing full sequences.

**Solution**: Need to refactor to handle:
- Prefill phase: Process full prompt, populate cache
- Generation phase: Process one token at a time, use cache

### Challenge 2: Batch Size = 1
**Issue**: KV-Cache is most efficient for single-sequence generation (batch=1).

**Solution**: Current generate() already uses batch=1, so this is fine.

### Challenge 3: Testing
**Issue**: Need to verify cached results match non-cached results.

**Solution**:
- Generate same prompt with/without cache
- Compare output tokens (should be identical)
- Measure time difference (should be ~50x)

### Challenge 4: Memory Management
**Issue**: Cache uses ~12 MB per active generation.

**Solution**:
- Clear cache after generation completes
- Implement GQA (reduces to 3 MB)
- Add max_cache_size limit

---

## 📝 Implementation Checklist

### Option 2 (Quick Demo) - 1 Day
- [ ] Create `generate_cached()` stub in mini_transformer.cpp
- [ ] Add KV-Cache initialization (shows it works)
- [ ] Add command routing in neural_engine.cpp
- [ ] Document "Coming Soon - Full Integration" status
- [ ] Create this plan document

### Option 1 (Full Integration) - 3-4 Days
- [ ] Add cache parameter to forward()
- [ ] Modify attention to use cached K, V
- [ ] Update predict_next() to pass cache
- [ ] Update generate() to create and use cache
- [ ] Test: verify output matches non-cached
- [ ] Benchmark: measure actual speedup
- [ ] Document results
- [ ] Add --use-cache flag (default: true)
- [ ] Support GQA mode (--gqa flag)

---

## 🎓 Key Learnings

1. **KV-Cache requires deep integration**: Not a simple wrapper, needs attention refactoring
2. **Prefill vs Generation phases**: Different computation patterns
3. **Memory-Compute Tradeoff**: 12 MB cache for 50x speedup is excellent
4. **GQA is valuable**: 4x memory reduction with minimal quality loss
5. **Incremental approach works**: Demo first, full integration second

---

## 📊 Status

**Current**: Analysis complete, plan created
**Next**: Choose Option 1 (full integration) or Option 2 (quick demo)
**Recommendation**: Option 2 today (1 hour), then Option 1 (3-4 days)

**Files for Reference**:
- Current generate: [src/mini_transformer.cpp:392-450](../src/mini_transformer.cpp)
- KV-Cache API: [include/kv_cache.h](../include/kv_cache.h)
- KV-Cache implementation: [src/kv_cache.cpp](../src/kv_cache.cpp)
- Week 5 K3 benchmarks: [docs/PHASE_K_WEEK4_RESULTS.md](PHASE_K_WEEK4_RESULTS.md)

---

**Decision Point**: Proceed with Option 2 (quick demo) or Option 1 (full integration)?
