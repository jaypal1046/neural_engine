# Algorithm Extraction Plan: Steal the Best, Remove the Rest

**Goal**: Extract pure algorithms from open-source LLMs (llama.cpp, Mistral, Phi, Qwen) WITHOUT:
- Their models (we train our own)
- Their tracking/telemetry
- Their behavioral constraints
- Their dependencies
- Their frameworks

**Strategy**: Copy code snippets → Adapt to our C++ brain → Optimize → Own it

---

## Phase 1: Performance Optimizations (FROM llama.cpp)

### 1.1 GGML Tensor Operations (MIT License)
**File**: `ggml.c` → Extract to `src/tensor_ops.cpp`

**What to Steal**:
```cpp
// SIMD-optimized matrix multiplication (AVX2/AVX512/NEON)
- ggml_vec_dot_f32() → Our matmul_simd()
- ggml_mul_mat() → Our optimized_matmul()
- ggml_rope() → Rotary Position Embeddings (RoPE)
- ggml_rms_norm() → RMSNorm (better than LayerNorm)

// Quantization (4-bit, 5-bit, 8-bit)
- Q4_0, Q4_1, Q5_0, Q8_0 formats
- Fast dequantization with lookup tables
- Mixed-precision inference (int4 → fp16 → fp32)
```

**Remove**:
- GGML allocator (use our own memory pool)
- GGML compute graph (use our forward() directly)
- Metal/CUDA backends (we do CPU-first)

**Files to Create**:
```
include/tensor_ops.h      // SIMD matmul, RoPE, RMSNorm
include/quantization.h    // 4-bit/8-bit quantization
src/tensor_ops_avx2.cpp   // AVX2 optimized
src/tensor_ops_neon.cpp   // ARM NEON (future mobile)
```

---

### 1.2 KV-Cache Optimization (FROM llama.cpp)
**File**: `llama.cpp` → Extract to `src/kv_cache.cpp`

**What to Steal**:
```cpp
// Grouped-Query Attention (GQA) - 3x faster than MHA
- llama_kv_cache_seq_*() → Our kv_cache_update()
- Multi-query attention (MQA) for 1-head KV
- Flash Attention v2 logic (fused softmax)

// Ring buffer KV cache (constant memory)
- llama_kv_cache_defrag() → Our cache_compact()
- Sliding window attention (8K context)
```

**Remove**:
- llama_batch system (use our own batching)
- llama_context (too coupled to their model)

**Files to Create**:
```
include/kv_cache.h        // Ring buffer, GQA support
src/kv_cache.cpp          // Efficient cache management
```

---

## Phase 2: Tokenization (FROM HuggingFace tokenizers)

### 2.1 SentencePiece/Unigram (FROM Mistral/Qwen)
**Repository**: `sentencepiece` (Apache 2.0) → Extract to `src/unigram_tokenizer.cpp`

**What to Steal**:
```cpp
// Unigram Language Model (better than BPE for multilingual)
- Viterbi algorithm for optimal segmentation
- Subword regularization (dropout during training)
- Byte-fallback (handles any Unicode)

// Normalization
- NFKC Unicode normalization
- Precompiled regex for punctuation
```

**Remove**:
- Protobuf dependency (use JSON instead)
- SentencePiece training (we pre-train offline)

**Files to Create**:
```
include/unigram_tokenizer.h   // Better than BPE
src/unigram_tokenizer.cpp     // Viterbi decode
```

---

### 2.2 Byte-Level BPE (FROM GPT-2)
**Repository**: `tiktoken` (MIT) → Enhance our existing `bpe_tokenizer.cpp`

**What to Steal**:
```cpp
// Byte-level fallback (no unknown tokens)
- Base256 encoding for any bytes
- Regex pre-tokenization (split on punctuation)
- Special token handling (<|im_start|>, <|im_end|>)
```

**Integrate Into**:
```
src/bpe_tokenizer.cpp  // Already exists, enhance it
```

---

## Phase 3: Attention Mechanisms (FROM Multiple Sources)

### 3.1 Flash Attention v2 (FROM Tri Dao's paper)
**Repository**: `flash-attention` (BSD-3) → Extract to `src/flash_attention.cpp`

**What to Steal**:
```cpp
// Memory-efficient attention (O(N) instead of O(N²))
- Tiling algorithm (64x64 blocks)
- Online softmax (no materialized attention matrix)
- Fused backward pass (2x faster training)

// Grouped-Query Attention (GQA)
- Share K/V across heads (8 Q heads → 2 KV heads)
- 3-4x faster inference with minimal quality loss
```

**Remove**:
- CUDA kernels (we need CPU version first)
- Triton code (Python dependency)

**Files to Create**:
```
include/flash_attention.h     // O(N) attention
src/flash_attention_cpu.cpp   // CPU implementation
```

---

### 3.2 Rotary Position Embeddings (RoPE)
**FROM**: llama.cpp/Mistral → Extract to `src/rope.cpp`

**What to Steal**:
```cpp
// Better than absolute position embeddings
- ggml_rope_custom() → Our apply_rope()
- Frequency scaling for long contexts (8K → 128K)
- Cached sin/cos tables (precompute once)
```

**Files to Create**:
```
include/rope.h           // Rotary embeddings
src/rope.cpp             // Precomputed tables
```

---

## Phase 4: Model Architectures (Code ONLY, NOT weights)

### 4.1 Mistral 7B Architecture (Apache 2.0)
**Repository**: `mistralai/mistral-src` → Extract to `include/mistral_arch.h`

**What to Steal**:
```cpp
// Sliding Window Attention (SWA)
- Local attention (4K window) + full attention at intervals
- 10x faster than full attention on long docs

// Grouped-Query Attention (GQA)
- 32 query heads → 8 KV heads
- Config: n_heads=32, n_kv_heads=8

// RMSNorm instead of LayerNorm
- Faster, no bias term
```

**Remove**:
- Model weights (4GB+ files)
- Safetensors loader (use our own)

**Files to Create**:
```
include/mistral_arch.h         // SWA + GQA architecture
src/sliding_window_attn.cpp    // Memory-efficient attention
```

---

### 4.2 Phi-3 Architecture (MIT License)
**Repository**: `microsoft/Phi-3` → Extract to `include/phi_arch.h`

**What to Steal**:
```cpp
// Long RoPE (128K context with freq scaling)
- Original RoPE freq=10000, long freq=1000000
- Linear interpolation between contexts

// Token healing (better prompt handling)
- Rescore first generated token based on prompt suffix
```

**Files to Create**:
```
include/phi_arch.h       // Long context RoPE
src/long_rope.cpp        // Frequency scaling
```

---

### 4.3 Qwen2.5 Architecture (Apache 2.0)
**Repository**: `Qwen/Qwen2.5` → Extract to `include/qwen_arch.h`

**What to Steal**:
```cpp
// Dual attention (local + global)
- Layer 0-5: Local window (512 tokens)
- Layer 6-11: Global attention (full context)
- Best of both worlds (speed + quality)

// Multi-lingual vocabulary (152K tokens)
- Covers 80+ languages efficiently
- Better compression than GPT-2 vocab (50K)
```

**Files to Create**:
```
include/qwen_arch.h          // Dual attention
src/dual_attention.cpp       // Local + global hybrid
```

---

## Phase 5: Training Optimizations (FROM PyTorch/HuggingFace)

### 5.1 Mixed Precision Training (FROM NVIDIA Apex)
**Repository**: `apex` (BSD-3) → Extract to `src/mixed_precision.cpp`

**What to Steal**:
```cpp
// FP16 forward + FP32 master weights
- Dynamic loss scaling (prevent underflow)
- Gradient accumulation in FP32

// BF16 (bfloat16) support
- Better range than FP16, no loss scaling needed
- Use for inference (faster)
```

**Files to Create**:
```
include/mixed_precision.h    // FP16/BF16 training
src/fp16_kernels.cpp         // Half-precision ops
```

---

### 5.2 Gradient Checkpointing (FROM HuggingFace)
**Repository**: `transformers` (Apache 2.0) → Extract to `src/gradient_checkpoint.cpp`

**What to Steal**:
```cpp
// Trade compute for memory (train 2x larger models)
- Recompute activations in backward pass
- Only store checkpoints at layer boundaries
- 40% slower training, 50% less memory
```

**Files to Create**:
```
include/gradient_checkpoint.h   // Activation checkpointing
src/gradient_checkpoint.cpp     // Selective recomputation
```

---

## Phase 6: Multi-Language Integration (WITHOUT Dependencies)

### 6.1 Rust → C++ Bridge (Copy Algorithm Logic)
**What to Steal**:
```rust
// FROM tokenizers (HuggingFace Rust crate)
- src/normalizers/unicode.rs → Our unicode_normalize.cpp
- src/pre_tokenizers/byte_level.rs → Our byte_level.cpp
- src/models/unigram.rs → Our unigram_viterbi.cpp
```

**Strategy**: Read Rust code → Rewrite in C++ → No FFI needed

---

### 6.2 Python → C++ (Algorithm Translation)
**What to Steal**:
```python
# FROM vLLM (PagedAttention)
- vllm/attention/backends/flash_attn.py → Our paged_attention.cpp
- vllm/model_executor/layers/rotary_embedding.py → Our rope.cpp

# FROM transformers (HuggingFace)
- transformers/models/mistral/modeling_mistral.py → Architecture reference
- transformers/models/phi3/modeling_phi3.py → Long RoPE config
```

**Strategy**: Read Python → Extract math → Implement in C++ with SIMD

---

## Phase 7: What NOT to Steal

### ❌ DO NOT Extract:
1. **Pre-trained Weights** (.bin, .safetensors, .gguf files)
   - We train our own models on our own data

2. **Telemetry/Analytics** (Any code with metrics.send(), analytics.track())
   - Privacy violation, remove entirely

3. **Cloud API Clients** (OpenAI API, Anthropic API wrappers)
   - We're building local-first AI

4. **Model Downloaders** (HuggingFace Hub, ModelScope downloaders)
   - We manage our own model storage

5. **License Incompatible Code** (GPL, AGPL, proprietary)
   - Only MIT, Apache 2.0, BSD-3 allowed

6. **Behavioral Guardrails** (Content filtering, refusal training, RLHF alignment)
   - User controls behavior, not pre-baked constraints

---

## Implementation Priority (3 Weeks)

### Week 1: Core Performance (CRITICAL)
- [ ] Extract GGML tensor ops (SIMD matmul, RoPE, RMSNorm)
- [ ] Implement 4-bit quantization (Q4_0 format)
- [ ] Add KV-cache ring buffer with GQA support
- [ ] **Target**: 3x faster inference

### Week 2: Architecture Upgrades
- [ ] Flash Attention v2 CPU implementation
- [ ] Sliding Window Attention (Mistral style)
- [ ] Dual Attention (Qwen style - local + global)
- [ ] **Target**: 128K context support

### Week 3: Training Optimizations
- [ ] Mixed precision training (FP16 forward + FP32 master)
- [ ] Gradient checkpointing (2x larger models)
- [ ] Unigram tokenizer (better multilingual)
- [ ] **Target**: Train 1B param model on single GPU

---

## License Compliance

| Source | License | Compatible | Notes |
|--------|---------|------------|-------|
| llama.cpp | MIT | ✅ Yes | Full code extraction OK |
| GGML | MIT | ✅ Yes | Tensor ops, quantization |
| Mistral-src | Apache 2.0 | ✅ Yes | Architecture only (no weights) |
| Phi-3 | MIT | ✅ Yes | Code + architecture |
| Qwen2.5 | Apache 2.0 | ✅ Yes | Code + architecture |
| tiktoken | MIT | ✅ Yes | BPE tokenizer |
| sentencepiece | Apache 2.0 | ✅ Yes | Unigram tokenizer |
| flash-attention | BSD-3 | ✅ Yes | Algorithm only (CPU port) |
| vLLM | Apache 2.0 | ✅ Yes | Paged attention logic |
| transformers | Apache 2.0 | ✅ Yes | Reference architectures |
| NVIDIA Apex | BSD-3 | ✅ Yes | Mixed precision training |

---

## Code Extraction Checklist (Per Algorithm)

For each algorithm you extract:
1. ✅ Find original source file (GitHub link)
2. ✅ Check license (MIT/Apache 2.0/BSD-3 only)
3. ✅ Read code → Understand algorithm → Rewrite in our style
4. ✅ Remove dependencies (no Protobuf, no PyTorch, no CUDA)
5. ✅ Add attribution comment in header:
   ```cpp
   // Algorithm adapted from: <project_name> (<license>)
   // Original: <github_url>
   // Modifications: Removed dependencies, optimized for AIZip brain
   ```
6. ✅ Test standalone (unit test before integration)
7. ✅ Integrate into neural_engine.cpp
8. ✅ Benchmark (must be faster than current implementation)
9. ✅ Document in ALGORITHM_EXTRACTION_REPORT.md

---

## Success Metrics

**Before Extraction** (Current State):
- Inference: ~50 tokens/sec (mini_transformer)
- Context: 512 tokens max
- Training: Embeddings-only (6.5 min, Perplexity 23.7)
- Model size: 512-dim, 6 layers

**After Extraction** (Target):
- Inference: **200+ tokens/sec** (4x faster with SIMD + quantization)
- Context: **128K tokens** (Flash Attention + RoPE scaling)
- Training: **Full backprop in 10 min** (gradient checkpointing + FP16)
- Model size: **1.5B params** (Mistral/Phi architecture)

**Quality Target**:
- Perplexity: < 15 (from 23.7)
- AI Score: 95%+ (from 74%)
- Multilingual: 80+ languages (Qwen vocab)

---

## Next Steps

Would you like me to:
1. **Start Week 1** → Extract GGML tensor ops (SIMD matmul, quantization)
2. **Create extraction scripts** → Automated code download + license check
3. **Build comparison tests** → Benchmark current vs extracted algorithms
4. **Design hybrid architecture** → How to integrate all extracted components

**Your Choice**? 🚀
