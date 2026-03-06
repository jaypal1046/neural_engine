# Advanced Attention Modes Integration Plan (Option C)
**Date**: 2026-03-05
**Goal**: Integrate Mistral sliding window and Qwen dual attention for advanced long-context capabilities
**Status**: Planning phase

---

## 🎯 Overview

We have **4 attention implementations** extracted:
1. ✅ **Standard Attention** - O(N²) memory, works everywhere
2. ✅ **Flash Attention v2** - O(N) memory, long context (128K+)
3. 📋 **Mistral Sliding Window** - Constant memory, infinite context
4. 📋 **Qwen Dual Attention** - Local + Global hybrid

**Goal**: Integrate Mistral and Qwen into MiniTransformer, add mode selection.

---

## 📊 Attention Comparison

| Mode | Memory | Context Limit | Speed | Use Case |
|------|--------|--------------|-------|----------|
| **Standard** | O(N²) | ~2K tokens | Baseline | Short context |
| **Flash** | O(N) | 128K+ tokens | 2-8x faster | Long context |
| **Mistral Sliding** | O(W) | Infinite | Fast | Very long streaming |
| **Qwen Dual** | O(N) | 32K+ tokens | Fast | Structured docs |

**Where**:
- N = sequence length
- W = window size (e.g., 4096 for Mistral)

---

## 🔧 What We Have (Extracted)

### Mistral Sliding Window

From [include/mistral_attention.h](../include/mistral_attention.h):

```cpp
class SlidingWindowAttention {
public:
    void forward(
        const float* input,
        float* output,
        KVCache::CacheManager& kv_cache,
        int layer_idx,
        int seq_len,
        const int* positions
    );
};

struct MistralConfig {
    int sliding_window;  // Window size (e.g., 4096)
    // ... other params
};
```

**Key Innovation**:
- Only attends to last W tokens (sliding window)
- Memory: O(W) instead of O(N)
- Allows **infinite context** with bounded memory

**Example**: 100K token document
- Standard: 100K² = 10B scores (40 GB!) - OOM
- Flash: 100K scores (400 KB) - works but slow
- Mistral: 4K scores (16 KB) - **fast and constant memory!**

### Qwen Dual Attention

From [include/qwen_attention.h](../include/qwen_attention.h):

```cpp
class DualAttention {
public:
    void forward(
        const float* input,
        float* output,
        int seq_len,
        int local_window,     // Local attention window (e.g., 512)
        int global_stride     // Global attention stride (e.g., 64)
    );
};
```

**Key Innovation**:
- **Local attention**: Attends to nearby tokens (±512)
- **Global attention**: Attends to distant tokens (every 64th)
- **Hybrid**: Best of both worlds

**Use Case**: Long structured documents (papers, legal docs)
- Local: Captures sentence/paragraph context
- Global: Captures document structure

---

## 🚀 Integration Approach

### Phase 1: Add Attention Mode Enum

**Add to TransformerConfig**:
```cpp
enum class AttentionMode {
    STANDARD,          // O(N²) - baseline
    FLASH,             // O(N) - long context
    MISTRAL_SLIDING,   // O(W) - infinite context
    QWEN_DUAL          // O(N) - structured docs
};

struct TransformerConfig {
    // ... existing fields ...
    AttentionMode attention_mode = AttentionMode::STANDARD;
    int sliding_window = 4096;     // For Mistral mode
    int local_window = 512;        // For Qwen mode
    int global_stride = 64;        // For Qwen mode
};
```

### Phase 2: Implement Mistral Attention Wrapper

**Add to mini_transformer.cpp**:
```cpp
std::vector<std::vector<float>> MiniTransformer::multi_head_attention_mistral(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    KVCache::CacheManager& cache,
    int layer_idx,
    bool causal_mask
) {
    int seq_len = input.size();
    int d_model = config_.embedding_dim;

    // Compute Q, K, V (same as other modes)
    // ... matmul operations ...

    // Use Mistral sliding window attention
    MistralAttention::MistralConfig mistral_config;
    mistral_config.sliding_window = config_.sliding_window;
    mistral_config.hidden_dim = d_model;
    mistral_config.n_heads = config_.num_heads;

    MistralAttention::SlidingWindowAttention mistral_attn(mistral_config);

    // Call Mistral forward
    std::vector<float> output_flat(seq_len * d_model);
    mistral_attn.forward(
        input_flat.data(),
        output_flat.data(),
        cache,
        layer_idx,
        seq_len,
        positions.data()
    );

    // Output projection (same as other modes)
    // ...
}
```

### Phase 3: Implement Qwen Attention Wrapper

**Add to mini_transformer.cpp**:
```cpp
std::vector<std::vector<float>> MiniTransformer::multi_head_attention_qwen(
    const std::vector<std::vector<float>>& input,
    const Weights::Layer& layer,
    bool causal_mask
) {
    // Compute Q, K, V
    // ...

    // Use Qwen dual attention
    QwenAttention::DualAttention qwen_attn(config_);

    std::vector<float> output_flat(seq_len * d_model);
    qwen_attn.forward(
        input_flat.data(),
        output_flat.data(),
        seq_len,
        config_.local_window,
        config_.global_stride
    );

    // Output projection
    // ...
}
```

### Phase 4: Update forward() with Mode Selection

**Modify forward()**:
```cpp
std::vector<std::vector<float>> MiniTransformer::forward(const std::vector<int>& tokens) {
    // ... embeddings ...

    for (int l = 0; l < config_.num_layers; l++) {
        auto& layer = weights_.layers[l];

        // Select attention mode
        std::vector<std::vector<float>> attn_out;

        switch (config_.attention_mode) {
            case AttentionMode::STANDARD:
                attn_out = multi_head_attention(x, layer, true);
                break;

            case AttentionMode::FLASH:
                attn_out = multi_head_attention_flash(x, layer, true);
                break;

            case AttentionMode::MISTRAL_SLIDING:
                attn_out = multi_head_attention_mistral(x, layer, cache, l, true);
                break;

            case AttentionMode::QWEN_DUAL:
                attn_out = multi_head_attention_qwen(x, layer, true);
                break;
        }

        // ... rest of layer (same for all modes)
    }
}
```

### Phase 5: Add Commands

**generate_mistral command**:
```cpp
else if (cmd == "generate_mistral" && argc >= 3) {
    TransformerConfig config;
    config.attention_mode = AttentionMode::MISTRAL_SLIDING;
    config.sliding_window = 4096;  // 4K sliding window
    config.max_seq_length = 100000;  // Can handle 100K!

    MiniTransformer transformer(config);
    transformer.load("models/transformer.bin");

    std::string generated = transformer.generate(prompt, tokenizer, 1000, 0.8f, 40);
    // ... output ...
}
```

**generate_qwen command**:
```cpp
else if (cmd == "generate_qwen" && argc >= 3) {
    TransformerConfig config;
    config.attention_mode = AttentionMode::QWEN_DUAL;
    config.local_window = 512;      // Local context
    config.global_stride = 64;      // Global sampling
    config.max_seq_length = 32768;  // 32K context

    MiniTransformer transformer(config);
    // ... generate ...
}
```

---

## 📈 Expected Performance

### Mistral Sliding Window

| Context Length | Standard | Flash | **Mistral** |
|----------------|----------|-------|------------|
| 8K tokens | 268 MB (OOM) | 1 MB | **16 KB** ✅ |
| 32K tokens | 4.2 GB (OOM) | 4 MB | **16 KB** ✅ |
| 100K tokens | 40 GB (OOM) | 13 MB | **16 KB** ✅ |

**Key**: Memory is **constant** regardless of sequence length!

**Trade-off**: Can only attend to last 4K tokens (older context forgotten)

### Qwen Dual Attention

| Context Length | Standard | Flash | **Qwen Dual** |
|----------------|----------|-------|--------------|
| 8K tokens | 268 MB | 1 MB | **5 MB** |
| 32K tokens | 4.2 GB | 4 MB | **20 MB** |

**Key**: Combines local (fine-grained) + global (coarse) attention

**Use Case**: Structured documents where distant context matters

---

## 📝 Implementation Checklist

### Phase C1: Mistral Sliding Window (3-4 days)

**Day 1: Add Includes and Config**
- [ ] Add `#include "mistral_attention.h"` to mini_transformer.cpp
- [ ] Add `AttentionMode` enum to TransformerConfig
- [ ] Add `sliding_window` parameter
- [ ] Test build

**Day 2: Implement Wrapper**
- [ ] Implement `multi_head_attention_mistral()`
- [ ] Handle Q/K/V computation
- [ ] Call Mistral sliding window forward
- [ ] Handle output projection
- [ ] Test method in isolation

**Day 3: Integrate and Test**
- [ ] Update `forward()` with mode selection
- [ ] Create `generate_mistral` command
- [ ] Test with short sequence (verify works)
- [ ] Test with long sequence (8K+ tokens)
- [ ] Benchmark memory usage

**Day 4: Document**
- [ ] Measure performance vs Flash/Standard
- [ ] Document memory savings
- [ ] Create usage guide
- [ ] Update help text

### Phase C2: Qwen Dual Attention (3-4 days)

**Day 1-2: Implement Wrapper**
- [ ] Add `#include "qwen_attention.h"`
- [ ] Implement `multi_head_attention_qwen()`
- [ ] Test in isolation

**Day 3: Integrate**
- [ ] Add to mode selection in `forward()`
- [ ] Create `generate_qwen` command
- [ ] Test on structured documents

**Day 4: Document**
- [ ] Benchmark vs other modes
- [ ] Document use cases
- [ ] Create examples

### Phase C3: Mode Selection CLI (1 day)

- [ ] Add `--attention` flag to all generate commands:
  ```bash
  neural_engine generate "prompt" --attention flash
  neural_engine generate "prompt" --attention mistral --window 4096
  neural_engine generate "prompt" --attention qwen --local 512 --global 64
  ```

- [ ] Update unified_main.cpp help text
- [ ] Test all modes via CLI
- [ ] Document flag usage

**Total Estimate**: 8-10 days (1.5-2 weeks)

---

## 🚧 Challenges

### Challenge 1: Mistral Requires RoPE (Rotary Position Embeddings)

**Issue**: Mistral uses RoPE instead of learned positional embeddings

**Current**: We have `weights_.position_embeddings` (learned)

**Solution**: Add RoPE calculation or skip position encoding for Mistral mode

**Quick Fix**: Use zero positions (works but suboptimal)

### Challenge 2: Different Tensor Layouts

**Issue**: Mistral/Qwen expect `[batch, seq_len, hidden]` layout

**Current**: Our code uses `vector<vector<float>>` (flexible but not contiguous)

**Solution**: Flatten to 1D arrays (already doing this for Flash Attention)

### Challenge 3: Weight Compatibility

**Issue**: Mistral/Qwen have different weight shapes

**Current**: Our weights are trained for standard attention

**Solution**: Mode selection only works if model trained with that mode

**For Now**: Focus on inference interface, document training requirement

---

## 💡 Simplified Implementation Strategy

**Pragmatic Approach**: Don't implement everything perfectly, focus on **interface demonstration**

### Minimal Working Demo (1-2 days)

Instead of full integration:

1. **Add mode enum** - Show the architecture
2. **Add stub methods** - Demonstrate how it would work
3. **Log when called** - Show mode selection happening
4. **Document** - Explain what full implementation needs

**Example stub**:
```cpp
std::vector<std::vector<float>> MiniTransformer::multi_head_attention_mistral(...) {
    std::cerr << "⚡ Mistral Sliding Window Attention (4K window)\n";
    std::cerr << "   Memory: Constant 16 KB regardless of context length\n";
    std::cerr << "   Note: Full implementation requires RoPE and retraining\n\n";

    // Fall back to Flash Attention (closest approximation)
    return multi_head_attention_flash(input, layer, causal_mask);
}
```

**Value**:
- ✅ Shows architecture is extensible
- ✅ Documents what's needed for full implementation
- ✅ Provides educational output
- ✅ Takes 1-2 days instead of 2 weeks

---

## 🎯 Recommendation

### Option C-Minimal: Stub Implementation (Recommended, 1-2 days)

**Why**:
- We already have working Flash Attention (covers long context)
- Mistral/Qwen require model retraining (not quick to demonstrate)
- Stub shows architecture + educates users
- Can return for full implementation when training larger models

**Deliverables**:
1. AttentionMode enum in config ✅
2. Mode selection in forward() ✅
3. Stub methods with educational output ✅
4. Documentation of full implementation plan ✅

### Option C-Full: Complete Implementation (2 weeks)

**Why**:
- Production-quality feature
- Actually works (not just demo)
- Enables research on different attention patterns

**When**: After training infrastructure scales up (larger models, GPUs)

---

## 🚦 Decision Point

**Question**: Implement stubs (1-2 days) or full integration (2 weeks)?

**Factors**:
- Current model: 3M params, 512 max context - Flash Attention sufficient
- RoPE requirement: Major refactoring of positional encoding
- Training: Would need to retrain model with new attention mode

**Verdict**: **Stub implementation now**, full implementation when scaling up.

This follows the pattern we used with B3 (quantized training) - document the path forward without spending weeks on features that current model size doesn't need yet.

---

## 📁 Files for Reference

- **Mistral API**: [include/mistral_attention.h](../include/mistral_attention.h)
- **Qwen API**: [include/qwen_attention.h](../include/qwen_attention.h)
- **Flash Attention**: [include/flash_attention.h](../include/flash_attention.h) (current long-context solution)
- **This Plan**: [docs/ADVANCED_ATTENTION_INTEGRATION_PLAN.md](ADVANCED_ATTENTION_INTEGRATION_PLAN.md)

---

**Summary**: Advanced attention modes (Mistral, Qwen) are extracted and ready. For current 3M model, Flash Attention is sufficient. Can implement stubs (1-2 days) to show architecture, or defer full implementation until training larger models (2 weeks).

**Recommendation**: Move to **Option A (Week 7 K10-K12)** or **Option D (continue extraction)** instead, as Flash Attention already solves long-context needs.
