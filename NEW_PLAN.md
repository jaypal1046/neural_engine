# NEW_PLAN.md — The Unified Compression Roadmap
## Updated After Phases 1-18 Complete

---

## THE LAW THAT NEVER CHANGES

Shannon (1948): every file has a minimum possible compressed size.
It equals the file's entropy measured in bits per byte.

```
Entropy 0.0  =  all same byte     =  compress to almost nothing
Entropy 4.0  =  half predictable  =  can compress ~50%
Entropy 7.5  =  mostly random     =  almost nothing left
Entropy 7.95+  =  encrypted / already compressed  =  STORE RAW, always
```

This bound is not a target. It is a wall. No algorithm crosses it.
Every technique below works by exploiting structure BEFORE entropy coding.
Once the data is fully random-looking, no lossless algorithm can help.

---

## WHERE WE STAND — VERIFIED BENCHMARKS

### Default mode (v7): LZ77 + delta pre-filter + rANS order-0

| File | Size | Saved | Notes |
|------|------|-------|-------|
| raw_frames.yuv     | 2.20 MB | **99.2%** | YUV video — near-zero after delta |
| photo_800x600.bmp  | 1.37 MB | **96.8%** | BMP image |
| api_users_2k.json  | 0.72 MB | **89.4%** | JSON API data |
| server_log.txt     | 0.38 MB | **87.8%** | Server log |
| genome.txt         | 0.20 MB | **71.4%** | DNA sequence |
| program_mixed.bin  | 0.03 MB | **24.9%** | Mixed binary |
| pcm_audio.raw      | 0.25 MB | ~0% (stored) | Near-random PCM |
| enwik9 (1GB)       | 1000 MB | **57.5%** | Wikipedia XML — SHA-256 verified |

### Best mode (v8): BWT + MTF + RLE + rANS (order-0 or order-1)

| File | Size | Saved | vs v7 |
|------|------|-------|-------|
| server_log.txt  | 0.38 MB | **92.7%** | +4.9% |
| synthetic_log   | 0.04 MB | **88.1%** | +8.4% |
| source_code.c   | 0.02 MB | **91.7%** | +2.3% |
| 4.6MB log (10x) | 4.60 MB | **97.8%** | +10.0% |

All SHA-256 verified lossless.

---

## WHAT HAS BEEN BUILT (PHASES 1-18)

| Phase | What | Result |
|-------|------|--------|
| 1-5 | Python prototype: RLE → Huffman → LZ77 | .myzip v4, baseline |
| 6 | C++ engine: LZ77 + Huffman | .myzip v5, **383x faster** than Python |
| 7 | Lazy LZ77 + content detector | auto greedy/lazy per block |
| 8 | rANS order-0 entropy coder (ANS_M=16384) | .myzip v6, +2-4% ratio |
| 9 | Order-1 on LZ77 tokens — FAILED | 131KB header > 512KB block gain |
| 10 | Delta pre-filter strides 1-4, compare-and-pick | .myzip v7, +0.4-1.1% |
| 11 | BWT+MTF+RLE+rANS --best mode | .myzip v8, +5-8% on text |
| 12 | 4MB BWT blocks + rANS order-1, compare-and-pick | +10% on large text |
| 13 | SIMD SSE2 match length in LZ77 | **~2x faster v7 compression** |
| 14 | Radix-sort prefix-doubling BWT (O(N log N)) | **~2x faster --best mode** |
| 15 | Order-2 rANS on BWT output (block type 0x03) | +2-4% on large text blocks |
| 16 | PPM order-4 + arithmetic coding (--ultra / v9) | best ratio on text |
| 18 | Streaming / memory-mapped I/O | RAM = 1 block, not whole file |

### enwik9 benchmarks (1GB Wikipedia XML)
| Mode | Saved | Time (compress) | Time (decompress) |
|------|-------|-----------------|-------------------|
| v7 default | 57.5% | ~3-5s | fast |
| v8 --best  | 74.6% | ~800s | ~77s |
| v9 --ultra | **~76-78%** (est) | very slow | slow |

### 5MB Synthetic Text Benchmark
| Mode | Size | Ratio | Saved |
|------|------|-------|-------|
| v7 (LZ77) | 1.72 MB | 0.288 | 71.1% |
| v8 (BWT)  | 0.84 MB | 0.141 | 85.9% |
| v9 (PPM)  | **0.65 MB** | **0.109** | **89.1%** |


---

## CURRENT PIPELINE ARCHITECTURE

### Default (v7) — 512KB blocks

```
Block (512KB)
  → Profile 8KB: measure entropy H + delta entropy at strides 1-4
  → If 2.0 < H < 7.5 and delta reduces H by ≥ 0.5 bits:
      try BOTH: raw path and delta-filtered path
  → LZ77 (32KB window, lazy matching for text, greedy for DNA)
  → rANS order-0 (ANS_M=16384)
  → Pick smaller output (compare-and-pick)
  → Write: 1B filter_type + 512B ANS freqs + 4B sym_count + 4B enc_size + N bytes
```

### Best (v8) — 4MB blocks

```
Block (4MB)
  → BWT: suffix array (prefix-doubling O(N log²N)), yields last column
  → MTF: move-to-front, converts BWT clusters → runs of zeros (88%+ zeros on text)
  → RLE: (0x00, count-1) pairs for zero runs
  → Try THREE candidates:
      0x00: rANS order-0 (512B freq table)
      0x01: STORED_RAW
      0x02: rANS order-1 (131KB context tables — wins on blocks > ~1MB text)
  → Write block_type byte + smallest result
```

---

## WHAT IS STILL LEFT ON THE TABLE

After Phases 1-12, here is the remaining gap and how to close it:

---

### PHASE 13: SIMD MATCH LENGTH IN LZ77 — DONE ✓
**Effort:** 30 lines. **Gain:** ~2x compression speed on v7. **Ratio change:** None.

Implemented: `match_length()` static inline function in lz77.cpp using SSE2.
Compares 16 bytes at a time via `_mm_cmpeq_epi8` + `_mm_movemask_epi8`.
Falls back to scalar loop for tail bytes (<16) and on non-SSE2 targets.
Verified: all SHA-256 tests pass. No format change.

---

### PHASE 14: FASTER BWT (RADIX-SORT PREFIX-DOUBLING, O(N log N)) — DONE ✓
**Effort:** ~60 lines. **Gain:** ~2x faster --best mode. **Ratio change:** None.

Replaced `std::sort` (O(N log N) per doubling step) with counting sort (O(N) per step).
Result: total O(N log N) instead of O(N log²N).
4MB block: ~0.1s per block (down from ~0.2s with old std::sort).
enwik9 --best: estimated ~800s (down from ~1613s).

Note: SA-IS (O(N)) was attempted but requires a circular suffix array for BWT
correctness. The radix-sort approach maintains the proven circular rotation logic
while eliminating the O(log N) factor from each doubling step.
Verified: all SHA-256 tests pass. No format change.

---

### PHASE 15: ORDER-2 CONTEXT MODEL ON BWT OUTPUT — DONE ✓
**Effort:** Medium (110 lines). **Gain:** +2-4% ratio on large text blocks. **Cost:** 131KB header same as order-1.

After MTF+RLE, the symbol stream has strong 2-byte correlations.
Order-1 captures (prev_byte → current_byte).
Order-2 captures (prev_two_bytes → current_byte).

Threshold: blocks ≥ 3MB so 131KB header stays < ~4% of block size.

Compare-and-pick: try order-0, order-1, order-2, stored — keep smallest.
Block type 0x03 used for order-2 (same 131KB wire format as order-1).
Context key: `(prev2 * 5 + prev1) & 0xFF` — folds 2-byte context into 256 buckets.

Implemented in `ans.cpp` (`ans_ctx2_build`, `ans_ctx2_encode`, `ans_ctx2_decode`).
Decompressor `0x03` case added to `compressor.cpp` decompress path.
Verified: SHA-256 passes on 6MB synthetic log.

---

### PHASE 16: PPM CONTEXT MODEL — DONE ✓
**Effort:** High (~400 lines). **Gain:** Best ratio on text. **Cost:** Slow, moderate RAM.

PPM (Prediction by Partial Match) with order-1..4 contexts.
For each byte: try highest order context first, escape to lower if unseen.
Blend via escape mechanism (PPM-C style). Encode with 32-bit range coder.

Implemented as `ppm.cpp`/`ppm.h`:
- `ppm_encode(data, len)` → range-coded stream
- `ppm_decode(coded, coded_len, sym_count)` → original bytes
- Bounded hash table (256K entries, ~128 MB max) — model freezes when full
- Orders 1..4, escape cumulated as `total+1`

Wired into `compressor.cpp` as v9 (`--ultra` mode):
- `compress_block_ppm()` — compare PPM vs STORED_RAW, pick smaller
- Block type 0x00 = PPM_CODED, 0x01 = STORED_RAW
- `decompress_file()` v9 branch calls `ppm_decode()`

Results on server_log_500kb.txt (392 KB):
- v7 default: 87.8% saved
- v8 --best:  92.7% saved
- v9 --ultra: 91.1% saved  (PPM favors larger training data)

All SHA-256 verified. Regression tests pass across all 3 modes.

---

### PHASE 17: DELTA PREDICTOR FOR AUDIO/SENSOR DATA
**Effort:** Low (100 lines). **Gain:** 20-50% on PCM audio. **Cost:** None for other types.

Currently `pcm_audio.raw` stores raw (entropy ~7.8 bits/byte).
But adjacent 16-bit PCM samples are strongly correlated — their DIFFERENCE is small.

```
Raw 16-bit PCM:   32100, 32200, 32150, 32300 ...  (big numbers, high entropy)
Delta encoded:        0,   100,   -50,   150 ...  (small numbers, low entropy)
```

After delta encoding, entropy drops from ~7.8 to ~4.0 bits/byte → 50% compression.
FLAC achieves 30-60% on PCM audio using exactly this principle.

Implementation:
1. Detect PCM-like data in content detector (entropy 7.0-7.9 + strong autocorrelation)
2. Apply 16-bit delta predictor (already have 8-bit at strides 1-4 in v7)
3. Encode residuals with rANS
4. Write as new v7 filter_type=5 (DELTA16) or extend delta filter range

This closes the `pcm_audio.raw` gap from 0% to ~40% savings.

---

### PHASE 18: STREAMING / MEMORY-MAPPED I/O — DONE ✓
**Effort:** Medium. **Gain:** Handle files > available RAM. **Cost:** Complexity.

**compress_file** now:
1. Memory-maps the input (`CreateFileMapping`/`MapViewOfFile` on Win32, `mmap()` on POSIX). OS handles paging — no `read()` into RAM.
2. Computes SHA-256 by streaming `Sha256Ctx::update()` over mapped pages in one pass.
3. Writes the 54-byte header + zeroed block index to the output file immediately.
4. Compresses each block one at a time, `fwrite()`s it directly to the output, then `fseek()`s back to patch that block's index entry.
5. If all-blocks-compressed is larger than raw, rewrites the file as STORED (streams via mmap).

**Peak RAM during compress**: 1 uncompressed block + 1 compressed block.
- v7: ~512 KB + ~512 KB ≈ 1 MB
- v8: ~4 MB + ~4 MB + BWT working buffers ≈ 12 MB
- v9: ~512 KB + ~512 KB PPM ≈ 1 MB

**decompress_file** now:
1. `fread()`s only the 54-byte header + block index (tiny).
2. For each block: `fread()`s only that block's compressed bytes, decompresses in RAM, then `fwrite()`s the decompressed bytes straight to the output file.
3. Streams SHA-256 via `Sha256Ctx::update()` over each `fwrite()` chunk — no `recovered[]` buffer.

**Peak RAM during decompress**: 1 compressed block + 1 decompressed block.

Verified: SHA-256 passes for v7/v8/v9 on 500KB and 5MB test files.

---

## PRIORITY ORDER — WHAT TO BUILD NEXT

| Priority | Phase | Status | Effort | Gain | Type |
|----------|-------|--------|--------|------|------|
| **DONE** | SIMD match length (Phase 13) | ✓ | 30 lines | ~2x faster v7 | Speed |
| **DONE** | Radix-sort BWT (Phase 14) | ✓ | 60 lines | ~2x faster v8 | Speed |
| **DONE** | 16-bit delta predictor (Phase 17) | ✓ | 80 lines | **14.3% on audio** | Ratio |
| **DONE** | Order-2 on BWT output (Phase 15) | ✓ | ~110 lines | +2-4% on large text | Ratio |
| **DONE** | PPM context model (Phase 16) | ✓ | ~400 lines | best ratio on text | Ratio |
| **DONE** | Streaming / mmap I/O (Phase 18) | ✓ | ~200 lines | RAM = 1 block | Robustness |

**ALL PHASES 1-18 COMPLETE.**

**Phase 19 (Future):** Multithreading? (Parallel block compression).
**Phase 20 (Future):** Solid mode / Context continuity across blocks?


---

## THE STOPPING CONDITION

After each compression layer, measure entropy of the output.
If entropy of compressed data > 7.9 bits/byte → store raw, stop.
If compressed size >= previous size → stop immediately.
Never apply the same algorithm twice to the same data.

The rANS/ANS output approaches true entropy — after it runs,
there is nothing left for any lossless algorithm to find.
The only way to go further is to find MORE STRUCTURE before encoding.
That is what BWT, MTF, delta, and PPM all do — they expose structure
that entropy coding then eliminates.

---

## WHAT IS NOT WORTH DOING

**Re-compressing the rANS output with anything:**
rANS output is near-random (entropy ~7.95 bits/byte).
No lossless algorithm compresses near-random data.
Any second pass will make the file slightly larger.

**Applying BWT to already-BWT-compressed data:**
BWT rearranges bytes to create clustering. The output is already
exploited. Re-applying BWT on it produces garbage clustering.

**Huffman after rANS:**
rANS already achieves fractional-bit coding. Huffman on rANS output
adds overhead with zero compression gain.

**Iterative re-compression with the same technique:**
LZ77 on LZ77 output finds the same (now-absent) patterns.
Each pass must use a fundamentally different technique.

---

## THE GOLDEN RULE — UNCHANGED

```
sha256(decompress(compress(file))) == sha256(file)
```

For every layer added. For every test file. Not sometimes. Every time.
If it fails, the layer is not done.

---

## BUILD AND TEST COMMANDS

```bash
# Build
cd src
g++ -O3 -std=c++17 -Wall -march=native -o myzip.exe \
    main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp ppm.cpp

# Default mode (v7)
.\myzip.exe compress   input.txt
.\myzip.exe decompress input.txt.myzip

# Best mode (v8, BWT+order-1)
.\myzip.exe compress   input.txt --best
.\myzip.exe decompress input.txt.myzip

# Benchmark
.\myzip.exe benchmark  input.txt

# enwik9 full test
.\myzip.exe compress   C:\Users\jaypr\Downloads\enwik9\enwik9
.\myzip.exe decompress C:\Users\jaypr\Downloads\enwik9\enwik9.myzip
```

Debug build with sanitizers (catches memory bugs during development):
```bash
g++ -O0 -g -std=c++17 -fsanitize=address,undefined -o myzip_debug.exe \
    main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp ppm.cpp
```
