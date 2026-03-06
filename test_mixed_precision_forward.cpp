/*
 * Test Mixed Precision Forward Pass
 * Week 9 K10 Day 2 - Validate FP16/BF16/FP32 modes
 */

#include "mini_transformer.h"
#include "bpe_tokenizer.h"
#include "mixed_precision.h"
#include <iostream>
#include <iomanip>
#include <cmath>
#include <chrono>

using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

int main() {
    std::cout << "========================================\n";
    std::cout << "Testing Mixed Precision Forward Pass\n";
    std::cout << "========================================\n\n";

    // Create small transformer (fast testing)
    TransformerConfig config;
    config.vocab_size = 1000;
    config.embedding_dim = 128;
    config.num_layers = 2;
    config.num_heads = 4;
    config.ff_dim = 512;
    config.max_seq_length = 64;

    std::cout << "[Config] " << config.num_layers << " layers, "
              << config.embedding_dim << "D embeddings, "
              << config.num_heads << " heads\n\n";

    MiniTransformer transformer(config);

    // Test tokens (simple sequence)
    std::vector<int> tokens = {10, 20, 30, 40, 50};

    std::cout << "[Test Input] " << tokens.size() << " tokens: [";
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << tokens[i];
        if (i < tokens.size() - 1) std::cout << ", ";
    }
    std::cout << "]\n\n";

    // Test 1: FP32 baseline
    std::cout << "[Test 1] FP32 (Full Precision)\n";
    std::cout << "----------------------------------------\n";

    auto start_fp32 = std::chrono::high_resolution_clock::now();
    auto output_fp32 = transformer.test_forward(tokens, PMode::FP32);
    auto end_fp32 = std::chrono::high_resolution_clock::now();
    auto duration_fp32 = std::chrono::duration_cast<std::chrono::microseconds>(end_fp32 - start_fp32).count();

    std::cout << "  Output shape: [" << output_fp32.size() << ", " << output_fp32[0].size() << "]\n";
    std::cout << "  Time: " << duration_fp32 << " μs\n";
    std::cout << "  Sample values (last token, first 5 dims):\n    ";
    for (int i = 0; i < 5 && i < (int)output_fp32.back().size(); i++) {
        std::cout << std::fixed << std::setprecision(6) << output_fp32.back()[i] << " ";
    }
    std::cout << "\n\n";

    // Test 2: FP16 mode
    std::cout << "[Test 2] FP16 (Half Precision)\n";
    std::cout << "----------------------------------------\n";

    auto start_fp16 = std::chrono::high_resolution_clock::now();
    auto output_fp16 = transformer.test_forward(tokens, PMode::FP16);
    auto end_fp16 = std::chrono::high_resolution_clock::now();
    auto duration_fp16 = std::chrono::duration_cast<std::chrono::microseconds>(end_fp16 - start_fp16).count();

    std::cout << "  Output shape: [" << output_fp16.size() << ", " << output_fp16[0].size() << "]\n";
    std::cout << "  Time: " << duration_fp16 << " μs\n";
    std::cout << "  Sample values (last token, first 5 dims):\n    ";
    for (int i = 0; i < 5 && i < (int)output_fp16.back().size(); i++) {
        std::cout << std::fixed << std::setprecision(6) << output_fp16.back()[i] << " ";
    }
    std::cout << "\n";

    // Compare FP32 vs FP16
    float max_error_fp16 = 0.0f;
    float avg_error_fp16 = 0.0f;
    int count = 0;
    for (size_t i = 0; i < output_fp32.back().size(); i++) {
        float error = std::abs(output_fp32.back()[i] - output_fp16.back()[i]);
        max_error_fp16 = std::max(max_error_fp16, error);
        avg_error_fp16 += error;
        count++;
    }
    avg_error_fp16 /= count;

    std::cout << "  Max error vs FP32: " << std::scientific << max_error_fp16 << std::defaultfloat << "\n";
    std::cout << "  Avg error vs FP32: " << std::scientific << avg_error_fp16 << std::defaultfloat << "\n";
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2)
              << (float)duration_fp32 / duration_fp16 << "x\n\n";

    // Test 3: BF16 mode
    std::cout << "[Test 3] BF16 (Brain Float 16)\n";
    std::cout << "----------------------------------------\n";

    auto start_bf16 = std::chrono::high_resolution_clock::now();
    auto output_bf16 = transformer.test_forward(tokens, PMode::BF16);
    auto end_bf16 = std::chrono::high_resolution_clock::now();
    auto duration_bf16 = std::chrono::duration_cast<std::chrono::microseconds>(end_bf16 - start_bf16).count();

    std::cout << "  Output shape: [" << output_bf16.size() << ", " << output_bf16[0].size() << "]\n";
    std::cout << "  Time: " << duration_bf16 << " μs\n";
    std::cout << "  Sample values (last token, first 5 dims):\n    ";
    for (int i = 0; i < 5 && i < (int)output_bf16.back().size(); i++) {
        std::cout << std::fixed << std::setprecision(6) << output_bf16.back()[i] << " ";
    }
    std::cout << "\n";

    // Compare FP32 vs BF16
    float max_error_bf16 = 0.0f;
    float avg_error_bf16 = 0.0f;
    count = 0;
    for (size_t i = 0; i < output_fp32.back().size(); i++) {
        float error = std::abs(output_fp32.back()[i] - output_bf16.back()[i]);
        max_error_bf16 = std::max(max_error_bf16, error);
        avg_error_bf16 += error;
        count++;
    }
    avg_error_bf16 /= count;

    std::cout << "  Max error vs FP32: " << std::scientific << max_error_bf16 << std::defaultfloat << "\n";
    std::cout << "  Avg error vs FP32: " << std::scientific << avg_error_bf16 << std::defaultfloat << "\n";
    std::cout << "  Speedup: " << std::fixed << std::setprecision(2)
              << (float)duration_fp32 / duration_bf16 << "x\n\n";

    // Results summary
    std::cout << "========================================\n";
    std::cout << "Test Results\n";
    std::cout << "========================================\n";

    bool fp16_ok = avg_error_fp16 < 0.01f;  // < 1% average error
    bool bf16_ok = avg_error_bf16 < 0.01f;  // < 1% average error

    std::cout << (fp16_ok ? "✅" : "❌") << " FP16 mode: "
              << (fp16_ok ? "PASS" : "FAIL")
              << " (avg error: " << std::scientific << avg_error_fp16 << std::defaultfloat << ")\n";
    std::cout << (bf16_ok ? "✅" : "❌") << " BF16 mode: "
              << (bf16_ok ? "PASS" : "FAIL")
              << " (avg error: " << std::scientific << avg_error_bf16 << std::defaultfloat << ")\n";

    std::cout << "\nPrecision Characteristics:\n";
    std::cout << "  FP32: Baseline (full precision)\n";
    std::cout << "  FP16: ~3 decimal digits, needs loss scaling for training\n";
    std::cout << "  BF16: ~2 decimal digits, same range as FP32\n\n";

    std::cout << "Expected Speedup:\n";
    std::cout << "  Current (CPU): ~1.0x (memory bandwidth limited)\n";
    std::cout << "  With SIMD: ~1.5-2x (vectorized FP16 ops)\n";
    std::cout << "  GPU/TPU: ~2-3x (tensor cores)\n\n";

    return (fp16_ok && bf16_ok) ? 0 : 1;
}
