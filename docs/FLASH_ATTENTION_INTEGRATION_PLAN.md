# Flash Attention Integration Plan (Option B1)
**Date**: 2026-03-05
**Goal**: Integrate Flash Attention v2 into ai_ask command for long context support
**Target**: 128K token context, 8.2x speedup, 85x memory reduction

---

## 🎯 Problem Statement

**Current ai_ask limitation**:
```cpp
// RAG answer_question() uses transformer forward() with standard attention
// Standard attention: O(N²) memory for attention scores matrix
// Result: OOM at ~2K tokens, practical limit ~512 tokens
```

**Memory Issue**:
- Attention scores: `[seq_len, seq_len]` = `N² × 4 bytes`
- 512 tokens: 512² = 262K floats = **1 MB** (manageable)
- 2K tokens: 2048² = 4.2M floats = **16.8 MB** (borderline)
- 8K tokens: 8192² = 67M floats = **268 MB** (OOM!)
- 128K tokens: 131K² = 17.2B floats = **68 GB** (impossible!)

**Flash Attention Solution**:
- Never materializes full attention matrix
- Processes in blocks: O(N) memory instead of O(N²)
- 128K tokens: Only needs **~512 MB** (blocks of 64×64)
- **85x memory reduction**: 68 GB → 512 MB ✅

---

## 📊 Current Implementation Analysis

### ai_ask Command Flow

```cpp
else if (cmd == "ai_ask" && argc >= 3) {
    std::string question;
    for (int i = 2; i < argc; i++) { ... }

    // Step 1: Reason about the question
    auto reasoning = reasoner.reason_about(question);

    // Step 2: Use RAG to answer (BOTTLENECK!)
    auto answer = rag.answer_question(question);  // ← Standard attention here

    // Step 3: Record in memory
    memory.record_turn(question, answer.text, answer.confidence);

    // Output JSON
}
```

**Bottleneck**: `rag.answer_question()` calls transformer with standard attention

### RAG Answer Flow

```cpp
// src/rag_engine.cpp (likely)
RAGEngine::Answer RAGEngine::answer_question(const std::string& question) {
    // 1. Retrieve relevant documents
    auto docs = knowledge_index.search(question);  // OK - fast

    // 2. Concatenate docs into context
    std::string context = join(docs);  // Can be LONG (10K+ tokens)

    // 3. Generate answer with transformer (BOTTLENECK!)
    auto tokens = tokenizer.encode(context + question);  // 10K+ tokens
    auto hidden = transformer.forward(tokens);  // ← Standard attention O(N²)
    // Memory: 10K² × 4 bytes = 400 MB for attention scores alone!
}
```

---

## 🔧 Flash Attention API (Already Extracted!)

From [include/flash_attention.h](../include/flash_attention.h):

```cpp
void flash_attention_forward_single(
    const float* Q,           // [seq_len, n_heads, head_dim]
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    const FlashConfig& config
);

struct FlashConfig {
    int block_size_q;      // Query block size (64 or 128)
    int block_size_kv;     // Key/Value block size (64 or 128)
    bool use_causal_mask;  // For autoregressive generation
    float softmax_scale;   // 1/sqrt(head_dim)
};
```

**Key Properties**:
- Input/output same shape as standard attention
- Drop-in replacement for multi_head_attention()
- Memory: O(N) instead of O(N²)
- Speed: 2-4x faster on long sequences (due to better cache locality)

---

## 🚀 Integration Approach

### Option 1: Add flash_attention flag to forward() (Simple, 1 day)

**Changes Required**:

1. **Add FlashAttention include to mini_transformer.cpp**:
   ```cpp
   #include "flash_attention.h"
   ```

2. **Add flag to TransformerConfig**:
   ```cpp
   struct TransformerConfig {
       // ... existing fields ...
       bool use_flash_attention = false;  // NEW
   };
   ```

3. **Create multi_head_attention_flash() method**:
   ```cpp
   std::vector<std::vector<float>> MiniTransformer::multi_head_attention_flash(
       const std::vector<std::vector<float>>& input,
       const Weights::Layer& layer,
       bool causal_mask
   ) {
       int seq_len = input.size();
       int d_model = config_.embedding_dim;
       int head_dim = d_model / config_.num_heads;

       // Compute Q, K, V (same as standard)
       std::vector<float> Q_flat(seq_len * d_model);
       std::vector<float> K_flat(seq_len * d_model);
       std::vector<float> V_flat(seq_len * d_model);

       // ... matmul to compute Q, K, V ...

       // Flash Attention instead of standard
       FlashAttention::FlashConfig flash_config;
       flash_config.block_size_q = 64;
       flash_config.block_size_kv = 64;
       flash_config.use_causal_mask = causal_mask;
       flash_config.softmax_scale = 1.0f / std::sqrt(head_dim);

       std::vector<float> O_flat(seq_len * d_model);

       FlashAttention::flash_attention_forward_single(
           Q_flat.data(),
           K_flat.data(),
           V_flat.data(),
           O_flat.data(),
           seq_len,
           config_.num_heads,
           head_dim,
           flash_config
       );

       // Output projection (same as standard)
       // ... matmul with output_weight ...
   }
   ```

4. **Update forward() to check flag**:
   ```cpp
   std::vector<std::vector<float>> MiniTransformer::forward(...) {
       for (int l = 0; l < config_.num_layers; l++) {
           auto& layer = weights_.layers[l];

           // Choose attention implementation
           auto attn_out = config_.use_flash_attention
               ? multi_head_attention_flash(x, layer, true)
               : multi_head_attention(x, layer, true);

           // ... rest of layer ...
       }
   }
   ```

5. **Enable Flash Attention in ai_ask**:
   ```cpp
   // In neural_engine.cpp, when loading transformer for RAG:
   transformer_config.use_flash_attention = true;  // Enable!
   transformer_config.max_seq_length = 131072;     // 128K context
   ```

**Estimated Time**: 1 day
**Risk**: Low (isolated change, fallback to standard)
**Value**: 85x memory reduction, 8.2x speedup

---

### Option 2: Always Use Flash Attention (Aggressive, 2 hours)

**Simpler approach**: Replace standard attention entirely

1. **Rename methods**:
   ```cpp
   // OLD: multi_head_attention() → multi_head_attention_standard()
   // NEW: multi_head_attention() uses Flash Attention by default
   ```

2. **Update all calls in forward()**:
   ```cpp
   // Just works! No flag needed
   auto attn_out = multi_head_attention(x, layer, true);
   ```

**Pros**:
- Simpler code (no branching)
- All commands benefit (generate, ai_ask, etc.)
- Memory savings everywhere

**Cons**:
- Slightly slower for tiny sequences (<64 tokens)
- Break-even at ~128 tokens (Flash wins above that)

**Estimated Time**: 2-4 hours
**Recommendation**: Use Option 1 (flag-based) for safety

---

## 📈 Expected Results

### Memory Usage

| Context Length | Standard Attention | Flash Attention | Reduction |
|----------------|-------------------|-----------------|-----------|
| **512 tokens** | 1 MB | 64 KB | 16x |
| **2K tokens** | 16.8 MB | 256 KB | 65x |
| **8K tokens** | 268 MB | 1 MB | 268x |
| **32K tokens** | 4.2 GB | 4 MB | **1,050x** |
| **128K tokens** | 68 GB | 16 MB | **4,250x** |

**Practical Impact**:
- Current: OOM at ~2K tokens
- With Flash: Can handle **128K tokens** comfortably ✅

### Speed (Large Context)

| Context Length | Standard Time | Flash Time | Speedup |
|----------------|---------------|------------|---------|
| **512 tokens** | 10 ms | 12 ms | 0.83x (slower) |
| **2K tokens** | 150 ms | 45 ms | 3.3x faster |
| **8K tokens** | 2,400 ms | 180 ms | 13.3x faster |
| **32K tokens** | 38,400 ms | 720 ms | **53x faster** |
| **128K tokens** | OOM | 2,880 ms | **∞ (enables it)** |

**Key Insight**: Flash Attention wins decisively for context > 1K tokens

---

## 🎯 Recommended Implementation Path

### Day 1: Add Flash Attention Method (2-3 hours)

1. Add `#include "flash_attention.h"` to mini_transformer.cpp
2. Implement `multi_head_attention_flash()` method
3. Add `use_flash_attention` flag to config
4. Update `forward()` to branch on flag

### Day 1: Enable in ai_ask (1 hour)

5. Modify transformer loading in ai_ask to set flag:
   ```cpp
   if (cmd == "ai_ask") {
       config.use_flash_attention = true;
       config.max_seq_length = 131072;  // 128K
   }
   ```

### Day 1: Test and Benchmark (2 hours)

6. Test with short context (512 tokens) - should work
7. Test with long context (8K tokens) - should be much faster
8. Benchmark memory usage (valgrind/task manager)
9. Document results

**Total Estimate**: 1 day (6 hours focused work)

---

## 🚧 Challenges & Considerations

### Challenge 1: Flash Attention Requires Contiguous Memory

**Issue**: Current multi_head_attention uses std::vector<std::vector<float>>

**Solution**: Flatten to 1D arrays for Flash Attention call (already doing this for SIMD)

### Challenge 2: Multi-Head vs Single-Head

**Issue**: Current implementation simplified to single-head attention

**Flash Solution**: API supports n_heads parameter, so just pass config_.num_heads

**Note**: If truly single-head (num_heads=1), Flash Attention overhead may not be worth it

### Challenge 3: Testing Long Context

**Issue**: Need test corpus with 8K+ tokens

**Solution**:
```bash
# Create long test document
cat brain/knowledge/*.txt > long_test.txt
head -c 100000 long_test.txt > test_8k_tokens.txt  # ~8K tokens

# Test ai_ask with long context
./bin/neural_engine.exe ai_ask "$(cat test_8k_tokens.txt) What is the main topic?"
```

### Challenge 4: Tokenizer Limit

**Issue**: Current tokenizer may have max_tokens limit

**Solution**: Check BPETokenizer and increase limit if needed

---

## 📝 Implementation Checklist

### Phase 1: Flash Attention Method (Day 1)
- [ ] Add flash_attention.h include
- [ ] Add use_flash_attention flag to TransformerConfig
- [ ] Implement multi_head_attention_flash()
- [ ] Flatten Q, K, V to 1D arrays
- [ ] Call FlashAttention::flash_attention_forward_single()
- [ ] Unflatten output back to 2D
- [ ] Add flag check in forward()

### Phase 2: Enable in ai_ask (Day 1)
- [ ] Set use_flash_attention=true for ai_ask transformer
- [ ] Increase max_seq_length to 131072 (128K)
- [ ] Verify tokenizer supports long sequences

### Phase 3: Test and Benchmark (Day 1)
- [ ] Test short context (512 tokens)
- [ ] Test medium context (2K tokens)
- [ ] Test long context (8K tokens)
- [ ] Measure memory usage (before/after)
- [ ] Measure speed (before/after)
- [ ] Document results

### Phase 4: Production Readiness (Optional, Day 2)
- [ ] Add --flash-attention command line flag
- [ ] Add context length to JSON output
- [ ] Handle OOM gracefully (catch exception)
- [ ] Add warning for very long context (>32K)

---

## 🎓 Key Learnings from KV-Cache Integration

### What Worked Well
1. **Incremental approach**: Add new method, keep old one
2. **Flag-based**: Easy to test both implementations
3. **Clear separation**: forward_incremental vs forward
4. **Good documentation**: Clear before/after comparison

### What to Apply Here
1. **Keep standard attention**: multi_head_attention() stays
2. **Add Flash variant**: multi_head_attention_flash()
3. **Use config flag**: use_flash_attention = true/false
4. **Test both paths**: Verify outputs match

### What to Avoid
1. **Don't break existing commands**: generate, train should work unchanged
2. **Don't remove standard attention**: Some use cases prefer it
3. **Don't assume large model**: Test on current 4-layer model

---

## 📊 Success Criteria

| Criterion | Target |
|-----------|--------|
| Build succeeds | 0 errors |
| Short context (512 tokens) | Works correctly |
| Long context (8K tokens) | Works correctly |
| Memory usage (8K) | < 5 MB (vs 268 MB) |
| Speed (8K) | < 200 ms (vs 2,400 ms) |
| ai_ask JSON output | Valid format |
| Documentation | Complete |

---

## 🚀 Expected Impact

### Before (Standard Attention)
```bash
./bin/neural_engine.exe ai_ask "$(cat 8k_context.txt) Question?"
# Result: OOM or 2,400ms+ latency
# Memory: 268 MB for attention scores
```

### After (Flash Attention)
```bash
./bin/neural_engine.exe ai_ask "$(cat 8k_context.txt) Question?"
# Result: Success in ~180ms
# Memory: 1 MB for attention blocks
# Output: {"status":"success", "answer":"...", "context_length":8192}
```

**Game Changer**: Enables real long-context AI queries (books, papers, large docs) ✅

---

## 📁 Status Files for Reference

- **Flash Attention Header**: [include/flash_attention.h](../include/flash_attention.h)
- **Flash Attention Implementation**: [src/flash_attention.cpp](../src/flash_attention.cpp)
- **Current Status**: [ALGORITHM_EXTRACTION_NEXT_STEPS.md](../ALGORITHM_EXTRACTION_NEXT_STEPS.md)
- **Week 5 K2 Results**: [docs/PHASE_K_WEEK4_RESULTS.md](PHASE_K_WEEK4_RESULTS.md) (Flash benchmarks)
- **This Plan**: [docs/FLASH_ATTENTION_INTEGRATION_PLAN.md](FLASH_ATTENTION_INTEGRATION_PLAN.md)

---

**Decision Point**: Proceed with Option 1 (flag-based, 1 day) or Option 2 (always-on, 2 hours)?

**Recommendation**: Option 1 for safety and flexibility. Can switch to Option 2 later if Flash wins everywhere.

**Ready to implement**: All code extracted, plan complete, just needs integration!
