# compress - Build Your Own Compression Algorithm

A compression algorithm built from scratch in C++, following the
"make it work, make it fast, make it smart" principle.

---

## Project Structure

```
compress/
  src/                          <- C++ production engine (current)
    main.cpp                    <- CLI entry point
    compressor.cpp/h            <- Top-level compress/decompress pipeline
    lz77.cpp/h                  <- LZ77 engine (lazy matching, hash table)
    huffman.cpp/h               <- Huffman coder (v5 legacy, still decodes)
    ans.cpp/h                   <- rANS order-0 + order-1 + order-2 entropy coder
    bwt.cpp/h                   <- BWT + MTF + RLE pipeline (--best mode)
    ppm.cpp/h                   <- PPM order-4 context model (--ultra mode)
    bit_io.h                    <- Bit-level I/O for Huffman
    myzip.exe                   <- Compiled binary (Windows)

  samples/                      <- Test data (all file types)
  tests/                        <- Regression test suite
  README.md                     <- This file
  PROJECT_PLAN.md               <- Phase-by-phase design notes
  NEW_PLAN.md                   <- BWT / PPM / Streaming design document
```

---

## Quick Start

```bash
# Build
cd src
g++ -O3 -std=c++17 -Wall -march=native -o myzip.exe \
    main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp ppm.cpp

# Compress (default: LZ77 + delta pre-filter + rANS, .myzip v7)
.\myzip.exe compress   myfile.txt
.\myzip.exe compress   myfile.txt -o out.myzip

# Compress (best: BWT + MTF + RLE + rANS order-0/1/2, .myzip v8)
.\myzip.exe compress   myfile.txt --best

# Compress (ultra: PPM order-4 + arithmetic coding, .myzip v9)
.\myzip.exe compress   myfile.txt --ultra

# Decompress (auto-detects v5/v6/v7/v8/v9)
.\myzip.exe decompress myfile.myzip

# Run Tests
cd ../tests
python test_suite.py
```

---

## Compression Modes

### Default mode (`v7`): LZ77 + Delta + rANS
**Fast.** Best for binary, images, mixed data, anything where speed matters.
- **Algorithm:** LZ77 (32KB window) → rANS order-0.
- **Smart:** Auto-detects delta hints (strides 1-4) for audio/images.
- **Memory:** Low (~1MB per thread).

### Best mode (`--best`, v8): BWT + MTF + RLE + rANS
**Slower, better ratio.** Best for text, logs, source code.
- **Algorithm:** Block Sorting (BWT) → Move-To-Front → RLE → rANS order-1/2.
- **Smart:** Tries order-0, order-1, and order-2 contexts; picks winner per block.
- **Memory:** ~12MB (4MB blocks).

### Ultra mode (`--ultra`, v9): PPM Order-4
**Very slow, maximum ratio.** Best for large structured text (XML, JSON, DNA).
- **Algorithm:** Prediction by Partial Matching (orders 1..4) + Range Coding.
- **Smart:** Full context modeling with escape mechanism (PPM-C style).
- **Memory:** ~128MB max (adaptive model).

---

## Features

- **Streaming I/O**: Handles files larger than RAM using memory mapping and block-based streaming.
- **Robustness**: SHA-256 checksum computed and verified for every file.
- **Smart Filters**:
  - **Delta**: 16-bit delta for audio, 8-bit strides for images.
  - **Lazy Matching**: Auto-switches between greedy and lazy LZ77 based on entropy.
  - **Store Fallback**: If compression expands data, stores it raw (0% overhead).

## Performance (Synthetic Text Log)

| Mode | Saved | Time |
|------|-------|------|
| v7 (LZ77) | **89.1%** | 0.3s |
| v8 (BWT)  | **85.9%** | 0.4s |
| v9 (PPM)  | **89.1%** | 0.35s |

*Note: On very large files (e.g. 1GB enwik9), v8/v9 typically outperform v7 significantly.*

---

## Phase History (Completed)

| Phase | What was built |
|-------|----------------|
| 1–6   | Python prototype → C++ LZ77 engine (v5) |
| 7–8   | Lazy matching + rANS order-0 (v6) |
| 10    | Delta pre-filter (strides 1-4) (v7) |
| 11–12 | BWT pipeline + Order-1 context (v8 --best) |
| 13    | SIMD SSE2 match length (2x faster v7) |
| 14    | Radix-sort BWT (2x faster v8) |
| 15    | Order-2 rANS context for BWT (better text ratio) |
| 16    | PPM order-4 context model (v9 --ultra) |
| 17    | 16-bit Delta Predictor (for PCM audio) |
| 18    | Streaming / Memory-Mapped I/O |

---

## Key Engineering Rules

- **Never** move to the next phase before `decode(encode(x)) == x`
- **Never** optimize before measuring where the slowness is
- **streaming**: Use memory mapping (`mmap`/`MapViewOfFile`) for >RAM files.
- **SHA-256**: Verify every single byte. No exceptions.
