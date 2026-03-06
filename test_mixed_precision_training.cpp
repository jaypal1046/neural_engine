#include "mini_transformer.h"
#include <iostream>
#include <chrono>
#include <iomanip>

using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

int main() {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║       Mixed Precision Training Test (Week 9 Day 5)          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Small model for testing
    TransformerConfig config;
    config.vocab_size = 100;
    config.embedding_dim = 64;
    config.num_layers = 2;
    config.num_heads = 4;
    config.ff_dim = 128;
    config.max_seq_length = 16;

    std::cout << "Model Configuration:\n";
    std::cout << "  Vocab Size:     " << config.vocab_size << "\n";
    std::cout << "  Embedding Dim:  " << config.embedding_dim << "\n";
    std::cout << "  Layers:         " << config.num_layers << "\n";
    std::cout << "  Heads:          " << config.num_heads << "\n";
    std::cout << "  FF Dim:         " << config.ff_dim << "\n";
    std::cout << "\n";

    // Training data: simple sequence prediction task
    std::vector<int> tokens = {10, 20, 30, 40, 50};
    std::vector<int> targets = {20, 30, 40, 50, 60};

    std::cout << "Training Data:\n";
    std::cout << "  Input:  [";
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << tokens[i];
        if (i < tokens.size() - 1) std::cout << ", ";
    }
    std::cout << "]\n";
    std::cout << "  Target: [";
    for (size_t i = 0; i < targets.size(); i++) {
        std::cout << targets[i];
        if (i < targets.size() - 1) std::cout << ", ";
    }
    std::cout << "]\n\n";

    float learning_rate = 0.01f;
    int num_epochs = 10;

    // ========================================================================
    // Test 1: FP32 Training (Baseline)
    // ========================================================================

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Test 1: FP32 Training (Baseline)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    MiniTransformer transformer_fp32(config);

    auto start_fp32 = std::chrono::high_resolution_clock::now();

    float initial_loss_fp32 = 0.0f;
    float final_loss_fp32 = 0.0f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float loss = transformer_fp32.training_step(tokens, targets, learning_rate, PMode::FP32);

        if (epoch == 0) initial_loss_fp32 = loss;
        if (epoch == num_epochs - 1) final_loss_fp32 = loss;

        if (epoch % 2 == 0 || epoch == num_epochs - 1) {
            std::cout << "  Epoch " << std::setw(2) << epoch << ": Loss = " << std::fixed << std::setprecision(4) << loss << "\n";
        }
    }

    auto end_fp32 = std::chrono::high_resolution_clock::now();
    auto duration_fp32 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_fp32 - start_fp32).count();

    std::cout << "\nFP32 Results:\n";
    std::cout << "  Initial Loss:  " << std::fixed << std::setprecision(4) << initial_loss_fp32 << "\n";
    std::cout << "  Final Loss:    " << std::fixed << std::setprecision(4) << final_loss_fp32 << "\n";
    std::cout << "  Time:          " << duration_fp32 << " μs\n";
    std::cout << "  Convergence:   " << ((initial_loss_fp32 > final_loss_fp32) ? "✅ YES" : "❌ NO") << "\n";
    std::cout << "\n";

    // ========================================================================
    // Test 2: FP16 Training (with loss scaling)
    // ========================================================================

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Test 2: FP16 Training (with loss scaling)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    MiniTransformer transformer_fp16(config);

    auto start_fp16 = std::chrono::high_resolution_clock::now();

    float initial_loss_fp16 = 0.0f;
    float final_loss_fp16 = 0.0f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float loss = transformer_fp16.training_step(tokens, targets, learning_rate, PMode::FP16);

        if (epoch == 0) initial_loss_fp16 = loss;
        if (epoch == num_epochs - 1) final_loss_fp16 = loss;

        if (epoch % 2 == 0 || epoch == num_epochs - 1) {
            std::cout << "  Epoch " << std::setw(2) << epoch << ": Loss = " << std::fixed << std::setprecision(4) << loss << "\n";
        }
    }

    auto end_fp16 = std::chrono::high_resolution_clock::now();
    auto duration_fp16 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_fp16 - start_fp16).count();

    std::cout << "\nFP16 Results:\n";
    std::cout << "  Initial Loss:  " << std::fixed << std::setprecision(4) << initial_loss_fp16 << "\n";
    std::cout << "  Final Loss:    " << std::fixed << std::setprecision(4) << final_loss_fp16 << "\n";
    std::cout << "  Time:          " << duration_fp16 << " μs\n";
    std::cout << "  Speedup:       " << std::fixed << std::setprecision(2) << (float)duration_fp32 / duration_fp16 << "x\n";
    std::cout << "  Convergence:   " << ((initial_loss_fp16 > final_loss_fp16) ? "✅ YES" : "❌ NO") << "\n";
    std::cout << "\n";

    // ========================================================================
    // Test 3: BF16 Training (no loss scaling needed)
    // ========================================================================

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Test 3: BF16 Training (no loss scaling)\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    MiniTransformer transformer_bf16(config);

    auto start_bf16 = std::chrono::high_resolution_clock::now();

    float initial_loss_bf16 = 0.0f;
    float final_loss_bf16 = 0.0f;

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        float loss = transformer_bf16.training_step(tokens, targets, learning_rate, PMode::BF16);

        if (epoch == 0) initial_loss_bf16 = loss;
        if (epoch == num_epochs - 1) final_loss_bf16 = loss;

        if (epoch % 2 == 0 || epoch == num_epochs - 1) {
            std::cout << "  Epoch " << std::setw(2) << epoch << ": Loss = " << std::fixed << std::setprecision(4) << loss << "\n";
        }
    }

    auto end_bf16 = std::chrono::high_resolution_clock::now();
    auto duration_bf16 = std::chrono::duration_cast<std::chrono::microseconds>(
        end_bf16 - start_bf16).count();

    std::cout << "\nBF16 Results:\n";
    std::cout << "  Initial Loss:  " << std::fixed << std::setprecision(4) << initial_loss_bf16 << "\n";
    std::cout << "  Final Loss:    " << std::fixed << std::setprecision(4) << final_loss_bf16 << "\n";
    std::cout << "  Time:          " << duration_bf16 << " μs\n";
    std::cout << "  Speedup:       " << std::fixed << std::setprecision(2) << (float)duration_fp32 / duration_bf16 << "x\n";
    std::cout << "  Convergence:   " << ((initial_loss_bf16 > final_loss_bf16) ? "✅ YES" : "❌ NO") << "\n";
    std::cout << "\n";

    // ========================================================================
    // Test 4: Compare Final Outputs (Accuracy)
    // ========================================================================

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Test 4: Accuracy Comparison\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    auto output_fp32 = transformer_fp32.test_forward(tokens, PMode::FP32);
    auto output_fp16 = transformer_fp16.test_forward(tokens, PMode::FP16);
    auto output_bf16 = transformer_bf16.test_forward(tokens, PMode::BF16);

    // Compute error vs FP32 baseline
    float max_error_fp16 = 0.0f;
    float avg_error_fp16 = 0.0f;
    float max_error_bf16 = 0.0f;
    float avg_error_bf16 = 0.0f;
    int count = 0;

    for (size_t i = 0; i < output_fp32.back().size() && i < 10; i++) {
        float error_fp16 = std::abs(output_fp32.back()[i] - output_fp16.back()[i]);
        float error_bf16 = std::abs(output_fp32.back()[i] - output_bf16.back()[i]);

        max_error_fp16 = std::max(max_error_fp16, error_fp16);
        avg_error_fp16 += error_fp16;

        max_error_bf16 = std::max(max_error_bf16, error_bf16);
        avg_error_bf16 += error_bf16;

        count++;
    }

    avg_error_fp16 /= count;
    avg_error_bf16 /= count;

    std::cout << "FP16 vs FP32:\n";
    std::cout << "  Max Error:  " << std::scientific << std::setprecision(4) << max_error_fp16 << "\n";
    std::cout << "  Avg Error:  " << std::scientific << std::setprecision(4) << avg_error_fp16 << "\n";
    std::cout << "  Relative:   " << std::fixed << std::setprecision(3) << (avg_error_fp16 * 100) << "%\n";
    std::cout << "\n";

    std::cout << "BF16 vs FP32:\n";
    std::cout << "  Max Error:  " << std::scientific << std::setprecision(4) << max_error_bf16 << "\n";
    std::cout << "  Avg Error:  " << std::scientific << std::setprecision(4) << avg_error_bf16 << "\n";
    std::cout << "  Relative:   " << std::fixed << std::setprecision(3) << (avg_error_bf16 * 100) << "%\n";
    std::cout << "\n";

    // ========================================================================
    // Summary and Pass/Fail
    // ========================================================================

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Summary\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n";

    std::cout << "\n";
    std::cout << "Performance:\n";
    std::cout << "┌─────────┬───────────┬───────────┬─────────────┐\n";
    std::cout << "│ Mode    │ Time (μs) │ Speedup   │ Convergence │\n";
    std::cout << "├─────────┼───────────┼───────────┼─────────────┤\n";
    std::cout << "│ FP32    │ " << std::setw(9) << duration_fp32 << " │ 1.00x     │ "
              << ((initial_loss_fp32 > final_loss_fp32) ? "✅ YES      " : "❌ NO       ") << "│\n";
    std::cout << "│ FP16    │ " << std::setw(9) << duration_fp16 << " │ "
              << std::fixed << std::setprecision(2) << std::setw(5) << (float)duration_fp32 / duration_fp16 << "x    │ "
              << ((initial_loss_fp16 > final_loss_fp16) ? "✅ YES      " : "❌ NO       ") << "│\n";
    std::cout << "│ BF16    │ " << std::setw(9) << duration_bf16 << " │ "
              << std::fixed << std::setprecision(2) << std::setw(5) << (float)duration_fp32 / duration_bf16 << "x    │ "
              << ((initial_loss_bf16 > final_loss_bf16) ? "✅ YES      " : "❌ NO       ") << "│\n";
    std::cout << "└─────────┴───────────┴───────────┴─────────────┘\n";
    std::cout << "\n";

    std::cout << "Accuracy:\n";
    std::cout << "┌─────────┬─────────────┬──────────────┐\n";
    std::cout << "│ Mode    │ Avg Error   │ < 1% Error?  │\n";
    std::cout << "├─────────┼─────────────┼──────────────┤\n";
    std::cout << "│ FP16    │ " << std::fixed << std::setprecision(3) << std::setw(10) << (avg_error_fp16 * 100) << "%  │ "
              << ((avg_error_fp16 < 0.01f) ? "✅ YES       " : "❌ NO        ") << "│\n";
    std::cout << "│ BF16    │ " << std::fixed << std::setprecision(3) << std::setw(10) << (avg_error_bf16 * 100) << "%  │ "
              << ((avg_error_bf16 < 0.01f) ? "✅ YES       " : "❌ NO        ") << "│\n";
    std::cout << "└─────────┴─────────────┴──────────────┘\n";
    std::cout << "\n";

    // Final test result
    bool all_converged = (initial_loss_fp32 > final_loss_fp32) &&
                         (initial_loss_fp16 > final_loss_fp16) &&
                         (initial_loss_bf16 > final_loss_bf16);

    bool accuracy_ok = (avg_error_fp16 < 0.01f) && (avg_error_bf16 < 0.01f);

    bool test_passed = all_converged && accuracy_ok;

    std::cout << "═══════════════════════════════════════════════════════════════\n";
    if (test_passed) {
        std::cout << "✅ TEST PASSED - Mixed Precision Training Working!\n";
    } else {
        std::cout << "❌ TEST FAILED - Issues Detected:\n";
        if (!all_converged) {
            std::cout << "   - Loss did not converge for all modes\n";
        }
        if (!accuracy_ok) {
            std::cout << "   - Accuracy error > 1% threshold\n";
        }
    }
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "\n";

    return test_passed ? 0 : 1;
}
