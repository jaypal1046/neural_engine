# compress - Build Your Own Compression Algorithm

A compression algorithm built from scratch in C++, following the
"make it work, make it fast, make it smart" principle.

---

## Project Structure

```
compress/
  src_cpp/                      <- C++ production engine (current)
    main.cpp                    <- CLI entry point
    compressor.cpp/h            <- Top-level compress/decompress pipeline
    lz77.cpp/h                  <- LZ77 engine (lazy matching, hash table)
    huffman.cpp/h               <- Huffman coder (v5 legacy, still decodes)
    ans.cpp/h                   <- rANS order-0 + order-1 entropy coder
    bwt.cpp/h                   <- BWT + MTF + RLE pipeline (--best mode)
    bit_io.h                    <- Bit-level I/O for Huffman
    myzip.exe                   <- Compiled binary (Windows)

  samples/                      <- Test data (all file types)
    download_samples.py         <- Downloads + generates sample files
    generate_hard_samples.py    <- Generates adversarial test files
    text/                       <- .txt files (logs, books)
    json/                       <- .json files (APIs, configs)
    csv/                        <- .csv files (data exports)
    image/                      <- .bmp files (uncompressed images)
    binary/                     <- .bin files (executables, random data)

  README.md                     <- This file
  PROJECT_PLAN.md               <- Phase-by-phase design notes
  NEW_PLAN.md                   <- BWT phase design document
```

---

## Quick Start

```bash
# Build
cd src_cpp
g++ -O3 -std=c++17 -Wall -march=native -o myzip.exe main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp

# Compress (default: LZ77 + delta pre-filter + rANS, .myzip v7)
src_cpp/myzip.exe compress   myfile.txt
src_cpp/myzip.exe compress   myfile.txt -o out.myzip

# Compress (best: BWT + MTF + RLE + rANS order-1, .myzip v8)
src_cpp/myzip.exe compress   myfile.txt --best
src_cpp/myzip.exe compress   myfile.txt --best -o out.myzip

# Decompress (auto-detects v5/v6/v7/v8)
src_cpp/myzip.exe decompress myfile.myzip
src_cpp/myzip.exe decompress myfile.myzip -o original.txt

# Benchmark
src_cpp/myzip.exe benchmark  myfile.txt
```

---

## Two Compression Modes

### Default mode (`v7`): LZ77 + Delta pre-filter + rANS

Fast. Best for binary, images, mixed data, anything where speed matters.

```
For each 512KB block:
  1. Profile: measure entropy + delta entropy at strides 1-4 (8KB probe)
  2. If medium-entropy block: try BOTH raw and best-delta-filter path
  3. LZ77 encode (32KB window, lazy matching for text, greedy for DNA)
  4. rANS order-0 encode (ANS_M=16384)
  5. Pick smaller of filtered vs unfiltered output
```

### Best mode (`--best`, v8): BWT + MTF + RLE + rANS

Slower. Best for text, logs, source code — anything with repeated phrases.

```
For each 4MB block:
  1. BWT (Burrows-Wheeler Transform) — suffix array, O(N log²N)
     Rearranges data so bytes with same context cluster together
  2. MTF (Move-To-Front) — converts BWT clusters into runs of zeros
  3. RLE — encodes zero runs as (0x00, count-1) pairs
  4. Try THREE candidates, pick smallest:
     a. STORED_RAW       (block_type=0x01): 5 bytes overhead
     b. rANS order-0     (block_type=0x00): 512B freq table
     c. rANS order-1     (block_type=0x02): 131KB context tables
  5. Write block_type byte + chosen encoding
```

Order-1 activates automatically on blocks > ~1MB of text (131KB header amortized).

---

## File Format

### Global Header (54 bytes, all versions)

| Offset | Size     | Field                                         |
|--------|----------|-----------------------------------------------|
| 0      | 4 bytes  | Magic: `MZIP`                                 |
| 4      | 1 byte   | Version: `5`/`6`/`7`/`8`                     |
| 5      | 1 byte   | Mode: `0x00` COMPRESSED / `0x01` STORED       |
| 6      | 8 bytes  | Original size (uint64 LE)                     |
| 14     | 32 bytes | SHA-256 of entire original file               |
| 46     | 4 bytes  | Block count (uint32 LE)                       |
| 50     | 4 bytes  | Block size (uint32 LE) — 512KB (v7), 4MB (v8) |
| 54     | N×8 bytes| Block index: (comp_size, orig_size) per block |

### v7 Block Format (default)

```
1 byte   filter_type  (0=none, 1=delta1, 2=delta2, 3=delta3/RGB, 4=delta4/RGBA)
512 bytes ANS freq table (order-0, 256 × uint16 LE)
4 bytes  symbol_count
4 bytes  encoded_byte_count
N bytes  rANS encoded data
```

### v8 Block Format (--best)

```
1 byte   block_type:
  0x00 = BWT + rANS order-0:  4B primary_index + 512B freqs + 4B sym_count + 4B enc_size + N bytes
  0x01 = STORED_RAW:          4B raw_size + raw_size bytes
  0x02 = BWT + rANS order-1:  4B primary_index + 131072B ctx1_freqs + 4B sym_count + 4B enc_size + N bytes
```

**Backwards compatible:** decompressor handles v5 (Huffman), v6 (rANS order-0),
v7 (rANS + delta filter), v8 (BWT + rANS).
SHA-256 always verified on decompress.

---

## Performance

### Default mode (v7) vs Best mode (v8)

All results SHA-256 verified lossless:

| File | Size | v7 saved | v8 saved | v8 gain |
|------|------|----------|----------|---------|
| server_log_500kb.txt | 0.38 MB | 87.8% | 92.7% | **+4.9%** |
| synthetic_log.txt    | 0.04 MB | 79.7% | 88.1% | **+8.4%** |
| source_code.c        | 0.02 MB | 89.4% | 91.7% | **+2.3%** |
| 4.6MB log (10x)      | 4.60 MB | 87.8% | 97.8% | **+10.0%** |

### Default mode (v7) benchmarks

| File | Size | Saved | Notes |
|------|------|-------|-------|
| raw_frames.yuv     | 2.20 MB | **99.2%** | YUV video frames |
| photo_800x600.bmp  | 1.37 MB | **96.8%** | Photo bitmap |
| synthetic_grad.bmp | 0.03 MB | **97.2%** | Gradient image |
| api_users_2k.json  | 0.72 MB | **89.4%** | JSON API data |
| server_log.txt     | 0.38 MB | **87.8%** | Server log |
| genome.txt         | 0.20 MB | **71.4%** | DNA sequence |
| program_mixed.bin  | 0.03 MB | **24.9%** | Mixed binary |
| pcm_audio.raw      | 0.25 MB | ~0% (stored) | Random-like PCM |

**enwik9** (1 GB Wikipedia XML): **57.5% saved**, SHA-256 verified.

### Speed

On a 2.2MB YUV video file:
- **v7 (default):** ~45 MB/s
- **v8 (--best):** ~2 MB/s (BWT O(N log²N) suffix array, 4MB blocks)
- **Python prototype:** 0.2 MB/s — **383x slower than C++**

---

## Phase History

| Phase | Status | What was built |
|-------|--------|----------------|
| 1–5   | Done   | Python prototype: analyze → RLE → Huffman → LZ77 → .myzip v4 |
| 6     | Done   | C++ engine: LZ77 + Huffman, .myzip v5, **383x speedup** |
| 7     | Done   | Lazy LZ77 + content detector (auto lazy/greedy per block) |
| 8     | Done   | rANS order-0 replaces Huffman, .myzip v6 |
| 9     | Done (reverted) | Order-1 on LZ77 tokens: 131KB header too large for 512KB blocks |
| 10    | Done   | Delta pre-filter strides 1-4, compare-and-pick, .myzip v7 |
| 11    | Done   | BWT+MTF+RLE+rANS --best mode, .myzip v8 |
| 12    | Done   | 4MB BWT blocks + rANS order-1, compare-and-pick all three |
| 13    | Done   | SIMD SSE2 match length in LZ77 — **~2x faster v7** |
| 14    | Done   | Radix-sort prefix-doubling BWT (O(N log N)) — **~2x faster --best** |

---

## Key Engineering Rules

- **Never** move to the next phase before `decode(encode(x)) == x`
- **Never** optimize before measuring where the slowness is
- **Never** assume compression made the file smaller — always check
- **Compare-and-pick**: always try multiple paths and keep the smallest
- **ANS order-1**: heap-allocate AnsCtx1 (12.5MB) — stack overflow otherwise
- **BWT decode**: L→F traversal gives bytes in reverse — write `out[N-1-i]`
- **SIMD match length**: uses `__SSE2__` guard — falls back to scalar if unavailable
- **BWT suffix array**: uses CIRCULAR rotation order (via `% N`) — SA-IS is linear only
