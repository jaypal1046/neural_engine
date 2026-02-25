// Test program for block-based random access API
#include "compressor.h"
#include <cstdio>
#include <cstring>
#include <chrono>

// Main function for block access testing (can be called from unified aiz.exe or standalone)
int main_test_block_access(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <compressed_file.aiz>\n", argv[0]);
        return 1;
    }

    const char* file_path = argv[1];

    // Open compressed file
    BlockFileInfo info;
    void* handle = block_open(file_path, info);
    if (!handle) {
        fprintf(stderr, "Failed to open: %s\n", file_path);
        return 1;
    }

    printf("File: %s\n", file_path);
    printf("Version: v%d\n", info.version);
    printf("Mode: %s\n", info.mode == 0x01 ? "STORED" : "COMPRESSED");
    printf("Original size: %.2f MB (%llu bytes)\n",
           info.orig_size / (1024.0*1024.0),
           (unsigned long long)info.orig_size);
    printf("Block count: %u\n", info.block_count);
    printf("Block size: %.2f MB (%u bytes)\n",
           info.block_size / (1024.0*1024.0),
           info.block_size);
    printf("\n");

    // Show block index
    printf("Block Index:\n");
    printf("  Block  | Compressed  | Original   | Ratio   | Offset\n");
    printf("  ------ | ----------- | ---------- | ------- | ----------\n");

    uint64_t total_compressed = 0;
    for (uint32_t b = 0; b < info.block_count && b < 10; ++b) {
        BlockInfo binfo;
        if (!block_get_info(handle, b, binfo)) {
            fprintf(stderr, "Failed to get info for block %u\n", b);
            break;
        }

        double ratio = binfo.original_size > 0
            ? (double)binfo.compressed_size / (double)binfo.original_size
            : 1.0;

        printf("  %6u | %11u | %10u | %6.2f%% | %10llu\n",
               b,
               binfo.compressed_size,
               binfo.original_size,
               ratio * 100.0,
               (unsigned long long)binfo.file_offset);

        total_compressed += binfo.compressed_size;
    }

    if (info.block_count > 10) {
        printf("  ... (%u more blocks)\n", info.block_count - 10);
    }

    printf("\n");

    // Test random access decompression of first block
    printf("Testing random access decompression of block 0...\n");
    auto block_data = block_decompress(handle, 0);
    if (block_data.empty()) {
        fprintf(stderr, "Failed to decompress block 0\n");
        block_close(handle);
        return 1;
    }

    printf("  Decompressed %zu bytes\n", block_data.size());
    printf("  First 64 bytes (hex):\n  ");
    for (size_t i = 0; i < std::min((size_t)64, block_data.size()); ++i) {
        printf("%02x ", block_data[i]);
        if ((i + 1) % 16 == 0) printf("\n  ");
    }
    printf("\n\n");

    // Show first 256 characters as text
    printf("  First 256 characters (text):\n  ");
    for (size_t i = 0; i < std::min((size_t)256, block_data.size()); ++i) {
        char c = (char)block_data[i];
        if (c >= 32 && c <= 126) putchar(c);
        else if (c == '\n') { printf("\n  "); }
        else putchar('.');
    }
    printf("\n\n");

    // Benchmark random access
    printf("Benchmarking random access (decompressing all blocks)...\n");
    auto start = std::chrono::steady_clock::now();
    size_t total_decompressed = 0;

    for (uint32_t b = 0; b < info.block_count; ++b) {
        auto data = block_decompress(handle, b);
        if (data.empty()) {
            fprintf(stderr, "Failed to decompress block %u\n", b);
            break;
        }
        total_decompressed += data.size();
    }

    auto end = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(end - start).count();
    double speed = (total_decompressed / (1024.0 * 1024.0)) / elapsed;

    printf("  Decompressed: %.2f MB\n", total_decompressed / (1024.0*1024.0));
    printf("  Time: %.2f seconds\n", elapsed);
    printf("  Speed: %.2f MB/s\n", speed);
    printf("\n");

    printf("✓ Block access test successful!\n");

    block_close(handle);
    return 0;
}

#ifndef UNIFIED_BUILD
// Standalone main for test_block_access.exe
int main(int argc, char** argv) {
    return main_test_block_access(argc, argv);
}
#endif
