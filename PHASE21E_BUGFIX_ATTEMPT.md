# Phase 21E Bug Fix Attempt

**Date**: 2026-02-23
**Status**: ✅ Bug identified and partially fixed, but skipping to Phase 21F per user request

---

## Bug Found

**Location**: `src/mini_transformer.cpp:435`

**The Bug**:
```cpp
// WRONG: Caching input BEFORE attention
cache.input = x;

// Then attention happens...
auto attn_out = multi_head_attention(x, layer, true);
attn_out = layer_norm(attn_out, ...);

// FF forward uses attn_out (NOT x!)
sum += attn_out[i][k] * layer.ff1_weight[k][j];

// But backward uses cache.input (which is x, not attn_out!)
backward_feedforward_simple(cache.input, ...)  // MISMATCH!
```

**The Fix**:
```cpp
// CORRECT: Cache input AFTER attention (what FF actually uses)
auto attn_out = multi_head_attention(x, layer, true);
attn_out = layer_norm(attn_out, ...);

cache.input = attn_out;  // NOW matches what FF uses!
```

---

## Results After Fix

| Metric | Before Fix | After Fix | Embeddings-Only |
|--------|-----------|-----------|-----------------|
| Time (3 epochs) | 48 min | **10.8 min** | 4 min |
| Perplexity | 66 | **72** | **30** |
| Quality | Bad | Still Bad | Good! |

**Conclusion**:
- ✅ Fix improved speed (48 min → 11 min)
- ❌ Quality still worse than embeddings-only (72 vs 30)
- ❌ Still takes >10 minutes (user limit)
- 🎯 **Decision**: Move to Phase 21F (Attention) per user request

---

## Likely Remaining Issues

Even with the cache fix, FF backward is still:
1. **Slow**: 11 min vs 4 min embeddings-only
2. **Hurts quality**: Perp 72 vs 30

**Possible reasons**:
- GELU derivative might still have issues
- FF training might need different learning rate
- Small corpus (129 lines) might not benefit from FF
- Attention is needed first before FF can help

---

## Decision

**Per user**: "if it take more then 10 min then stop and start phase 21F"

✅ **Moving to Phase 21F - Attention Backward**

Attention is:
- More important for transformers (the "secret sauce")
- Bigger impact (16 matrices vs FF's 8)
- Expected to give larger quality gains
- Can return to optimize FF later if needed

---

**Next**: Phase 21F Implementation
