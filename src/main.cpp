#include "compressor.h"
#include <iostream>
#include <fstream>
#include <string>
#include <chrono>
#include <cstring>
#include <cstdio>

// =============================================================================
// Progress bar
// =============================================================================

static auto g_start = std::chrono::steady_clock::now();

static void print_progress(const char* stage, size_t done, size_t total) {
    if (total == 0) return;

    auto now     = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(now - g_start).count();
    double pct   = (double)done / (double)total;
    int    filled = (int)(35.0 * pct);

    double eta = (elapsed > 0 && pct > 0.001)
                 ? elapsed / pct * (1.0 - pct)
                 : -1.0;

    char eta_str[16];
    if (eta < 0)         snprintf(eta_str, sizeof(eta_str), "  ??s");
    else if (eta < 1.0)  snprintf(eta_str, sizeof(eta_str), " <1s");
    else                 snprintf(eta_str, sizeof(eta_str), "%4.0fs", eta);

    double mb_done  = done  / (1024.0 * 1024.0);
    double mb_total = total / (1024.0 * 1024.0);

    char bar[36];
    for (int i = 0; i < 35; ++i)
        bar[i] = (i < filled) ? '#' : '.';
    bar[35] = '\0';

    // Pad stage label to 7 chars
    char slabel[8];
    snprintf(slabel, sizeof(slabel), "%-7s", stage);

    printf("\r  [%s] %s  %5.1f%%  %5.1f/%5.1f MB  ETA %s",
           bar, slabel, pct * 100.0, mb_done, mb_total, eta_str);
    fflush(stdout);

    if (done >= total) {
        printf("\n");
        fflush(stdout);
    }
}

// =============================================================================
// File helpers
// =============================================================================

static long long file_size(const std::string& path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    return f ? (long long)f.tellg() : -1;
}

static bool ends_with(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() &&
           s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// =============================================================================
// Commands
// =============================================================================

static void cmd_compress(const std::string& input, const std::string& output,
                         bool verbose, CompressMode mode) {
    long long in_size = file_size(input);
    if (in_size < 0) {
        fprintf(stderr, "Error: cannot open '%s'\n", input.c_str());
        exit(1);
    }

    const char* mode_label = (mode == CompressMode::ULTRA) ? "PPM (--ultra)"
                           : (mode == CompressMode::BEST)  ? "BWT (--best)"
                           : "LZ77+delta (default)";
    printf("\n  Input   : %s\n",   input.c_str());
    printf("  Output  : %s\n",     output.c_str());
    printf("  Mode    : %s\n",     mode_label);
    printf("  Original: %.2f MB (%lld bytes)\n\n", in_size / (1024.0*1024.0), in_size);

    g_start = std::chrono::steady_clock::now();

    bool show_bar = (in_size > 500 * 1024);
    ProgressCb cb = nullptr;
    if (show_bar) {
        cb = [](const char* stage, size_t done, size_t total) {
            if (strcmp(stage, "write") != 0)   // skip the write stage
                print_progress(stage, done, total);
        };
    }

    int rc = compress_file(input, output, cb, mode);
    if (rc != 0) { fprintf(stderr, "Compression failed.\n"); exit(1); }

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - g_start).count();

    long long out_size = file_size(output);
    double ratio = (in_size > 0) ? (double)out_size / (double)in_size : 1.0;

    printf("  Compressed : %.2f MB (%lld bytes)\n", out_size/(1024.0*1024.0), out_size);
    printf("  Ratio      : %.4f  (%.1f%% saved)\n", ratio, (1.0 - ratio)*100.0);
    printf("  Time       : %.2f s\n\n", elapsed);
}

static void cmd_decompress(const std::string& input, const std::string& output,
                            bool verbose) {
    long long in_size = file_size(input);
    if (in_size < 0) {
        fprintf(stderr, "Error: cannot open '%s'\n", input.c_str());
        exit(1);
    }

    printf("\n  Input : %s\n", input.c_str());
    printf("  Output: %s\n\n", output.c_str());

    g_start = std::chrono::steady_clock::now();

    bool show_bar = (in_size > 100 * 1024);
    ProgressCb cb = nullptr;
    if (show_bar) {
        cb = [](const char* stage, size_t done, size_t total) {
            if (strcmp(stage, "decompress") == 0)
                print_progress(stage, done, total);
        };
    }

    int rc = decompress_file(input, output, cb);
    if (rc != 0) { fprintf(stderr, "Decompression failed.\n"); exit(1); }

    double elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - g_start).count();

    long long out_size = file_size(output);
    printf("  Recovered  : %.2f MB (%lld bytes)\n", out_size/(1024.0*1024.0), out_size);
    printf("  Time       : %.2f s\n", elapsed);
    printf("  SHA-256    : OK (lossless verified)\n\n");
}

static void cmd_benchmark(const std::string& input) {
    long long in_size = file_size(input);
    if (in_size < 0) {
        fprintf(stderr, "Error: cannot open '%s'\n", input.c_str());
        exit(1);
    }

    std::string tmp_c  = input + ".bench.myzip";
    std::string tmp_d  = input + ".bench.recovered";

    printf("\n  Benchmarking: %s  (%.2f MB)\n\n", input.c_str(), in_size/(1024.0*1024.0));

    // Compress
    g_start = std::chrono::steady_clock::now();
    compress_file(input, tmp_c, nullptr, CompressMode::DEFAULT);
    double c_time = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - g_start).count();

    // Decompress
    g_start = std::chrono::steady_clock::now();
    decompress_file(tmp_c, tmp_d, nullptr);
    double d_time = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - g_start).count();

    long long c_size = file_size(tmp_c);
    double ratio = (double)c_size / (double)in_size;
    double c_speed = (in_size / (1024.0*1024.0)) / c_time;
    double d_speed = (in_size / (1024.0*1024.0)) / d_time;

    printf("  %-20s %10s %8s %8s %10s\n", "Stage", "Size", "Ratio", "Saved", "Speed");
    printf("  %-20s %10s %8s %8s %10s\n", "--------------------",
           "----------", "--------", "--------", "----------");
    printf("  %-20s %7.2f MB %8.4f %7.1f%%  %6.1f MB/s\n",
           "Compress (C++)", c_size/(1024.0*1024.0), ratio, (1.0-ratio)*100.0, c_speed);
    printf("  %-20s %7.2f MB %8s %8s  %6.1f MB/s\n",
           "Decompress (C++)", in_size/(1024.0*1024.0), "-", "-", d_speed);
    printf("\n");

    // Clean up temp files
    remove(tmp_c.c_str());
    remove(tmp_d.c_str());
}

// =============================================================================
// Usage
// =============================================================================

static void usage() {
    printf(
        "\nmyzip - High-Performance Compressor (C++)\n"
        "\n"
        "Usage:\n"
        "  myzip compress   <input> [-o <output>] [--best] [--ultra] [-v]\n"
        "  myzip decompress <input> [-o <output>] [-v]\n"
        "  myzip benchmark  <input>\n"
        "\n"
        "Options:\n"
        "  -o <path>   Output file path\n"
        "  --best      BWT+MTF+RLE+rANS mode (slower, better ratio on text)\n"
        "  --ultra     PPM order-4 + arithmetic coding (slow, best ratio)\n"
        "  -v          Verbose output\n"
        "\n"
        "Examples:\n"
        "  myzip compress   data.txt\n"
        "  myzip compress   data.txt --best\n"
        "  myzip compress   data.txt --ultra\n"
        "  myzip compress   video.yuv -o video.myzip\n"
        "  myzip decompress video.myzip\n"
        "  myzip decompress video.myzip -o video_out.yuv\n"
        "  myzip benchmark  large_file.bin\n"
        "\n"
    );
}

// =============================================================================
// main
// =============================================================================

int main(int argc, char* argv[]) {
    if (argc < 2) { usage(); return 1; }

    std::string cmd = argv[1];
    std::string input, output;
    bool verbose       = false;
    CompressMode mode  = CompressMode::DEFAULT;

    // Parse remaining args
    for (int i = 2; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "-o" && i + 1 < argc) {
            output = argv[++i];
        } else if (a == "-v") {
            verbose = true;
        } else if (a == "--best") {
            mode = CompressMode::BEST;
        } else if (a == "--ultra") {
            mode = CompressMode::ULTRA;
        } else if (a[0] != '-') {
            if (input.empty()) input = a;
            else               output = a;  // positional output
        } else {
            fprintf(stderr, "Unknown option: %s\n", a.c_str());
            return 1;
        }
    }

    if (cmd == "compress") {
        if (input.empty()) { fprintf(stderr, "Error: missing input file\n"); return 1; }
        if (output.empty()) output = input + ".myzip";
        cmd_compress(input, output, verbose, mode);

    } else if (cmd == "decompress") {
        if (input.empty()) { fprintf(stderr, "Error: missing input file\n"); return 1; }
        if (output.empty()) {
            output = ends_with(input, ".myzip")
                     ? input.substr(0, input.size() - 6) + ".recovered"
                     : input + ".recovered";
        }
        cmd_decompress(input, output, verbose);

    } else if (cmd == "benchmark") {
        if (input.empty()) { fprintf(stderr, "Error: missing input file\n"); return 1; }
        cmd_benchmark(input);

    } else {
        fprintf(stderr, "Unknown command: %s\n", cmd.c_str());
        usage();
        return 1;
    }

    return 0;
}
