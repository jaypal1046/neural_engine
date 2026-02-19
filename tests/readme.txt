# myzip Verification Suite

This folder contains automated regression tests for the myzip compressor.

## How to Run

1. Build the project first:
   cd ../src
   g++ -O3 -std=c++17 -Wall -march=native -o myzip.exe main.cpp compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp ppm.cpp

2. Run the test suite:
   cd ../tests
   python test_suite.py

## What it Tests

- File generation:
  - Structured text (logs)
  - Random binary (uncompressible)
  - Zeroes (highly compressible)
- Compression modes:
  - v7 Default (LZ77+rANS)
  - v8 Best (BWT+MTF+RLE+rANS)
  - v9 Ultra (PPM+Arithmetic)
- SHA-256 integrity check:
  - Ensures decompressed == original bit-for-bit.
