// Benchmark: 4-bit and 8-bit Quantization
// Tests compression ratio, accuracy, and inference speed

#include "quantization.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std::chrono;
using namespace Quantization;

class Timer {
public:
    void start() { start_ = high_resolution_clock::now(); }
    double elapsed_ms() {
        auto end = high_resolution_clock::now();
        return duration_cast<microseconds>(end - start_).count() / 1000.0;
    }
private:
    high_resolution_clock::time_point start_;
};

void random_weights(std::vector<float>& weights, int n) {
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 0.5f);  // Typical weight distribution

    weights.resize(n);
    for (auto& w : weights) {
        w = dist(gen);
    }
}

void benchmark_quantization_accuracy() {
    std::cout << "\n=== Quantization Accuracy Benchmark ===" << std::endl;

    const int n = 1024;  // 1K weights
    std::vector<float> weights;
    random_weights(weights, n);

    // Test Q4_0
    std::cout << "\n[Q4_0] 4-bit Symmetric Quantization:" << std::endl;
    auto stats_q4 = analyze_quantization(weights.data(), n, 0);
    std::cout << "  Compression Ratio: " << std::fixed << std::setprecision(2) << stats_q4.compression_ratio << "x" << std::endl;
    std::cout << "  Mean Error: " << std::scientific << std::setprecision(4) << stats_q4.mean_error << std::endl;
    std::cout << "  Max Error: " << stats_q4.max_error << std::endl;
    std::cout << "  SNR: " << std::fixed << std::setprecision(2) << stats_q4.snr_db << " dB" << std::endl;

    // Test Q8_0
    std::cout << "\n[Q8_0] 8-bit Quantization:" << std::endl;
    auto stats_q8 = analyze_quantization(weights.data(), n, 3);
    std::cout << "  Compression Ratio: " << stats_q8.compression_ratio << "x" << std::endl;
    std::cout << "  Mean Error: " << std::scientific << stats_q8.mean_error << std::endl;
    std::cout << "  Max Error: " << stats_q8.max_error << std::endl;
    std::cout << "  SNR: " << std::fixed << stats_q8.snr_db << " dB" << std::endl;
}

void benchmark_quantization_speed() {
    std::cout << "\n=== Quantization Speed Benchmark ===" << std::endl;

    const int n = 32768;  // 32K weights (typical transformer layer)
    const int iterations = 1000;

    std::vector<float> weights;
    random_weights(weights, n);

    Timer timer;

    // Benchmark Q4_0 quantization
    std::vector<BlockQ4_0> q4_blocks(n / QK4_0);
    timer.start();
    for (int i = 0; i < iterations; i++) {
        quantize_q4_0(weights.data(), q4_blocks.data(), n);
    }
    double time_q4_quant = timer.elapsed_ms();

    // Benchmark Q4_0 dequantization
    std::vector<float> dequantized(n);
    timer.start();
    for (int i = 0; i < iterations; i++) {
        dequantize_q4_0(q4_blocks.data(), dequantized.data(), n);
    }
    double time_q4_dequant = timer.elapsed_ms();

    // Benchmark Q8_0 quantization
    std::vector<BlockQ8_0> q8_blocks(n / QK8_0);
    timer.start();
    for (int i = 0; i < iterations; i++) {
        quantize_q8_0(weights.data(), q8_blocks.data(), n);
    }
    double time_q8_quant = timer.elapsed_ms();

    // Benchmark Q8_0 dequantization
    timer.start();
    for (int i = 0; i < iterations; i++) {
        dequantize_q8_0(q8_blocks.data(), dequantized.data(), n);
    }
    double time_q8_dequant = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "\nWeights: " << n << " (32K)" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "\n[Q4_0]" << std::endl;
    std::cout << "  Quantize:   " << time_q4_quant << " ms (" << (time_q4_quant / iterations) << " ms/iter)" << std::endl;
    std::cout << "  Dequantize: " << time_q4_dequant << " ms (" << (time_q4_dequant / iterations) << " ms/iter)" << std::endl;
    std::cout << "\n[Q8_0]" << std::endl;
    std::cout << "  Quantize:   " << time_q8_quant << " ms (" << (time_q8_quant / iterations) << " ms/iter)" << std::endl;
    std::cout << "  Dequantize: " << time_q8_dequant << " ms (" << (time_q8_dequant / iterations) << " ms/iter)" << std::endl;
}

void benchmark_quantized_matvec() {
    std::cout << "\n=== Quantized Matrix-Vector Benchmark ===" << std::endl;

    const int m = 512;   // Rows (output dim)
    const int k = 2048;  // Cols (input dim) - typical feed-forward layer
    const int iterations = 100;

    std::vector<float> weights(m * k);
    std::vector<float> input(k);
    std::vector<float> output_fp32(m);
    std::vector<float> output_q4(m);

    random_weights(weights, m * k);
    random_weights(input, k);

    // Quantize matrix to Q4_0
    std::vector<BlockQ4_0> q4_matrix(m * k / QK4_0);
    for (int row = 0; row < m; row++) {
        quantize_q4_0(&weights[row * k], &q4_matrix[row * (k / QK4_0)], k);
    }

    Timer timer;

    // Benchmark FP32 matrix-vector (naive)
    timer.start();
    for (int iter = 0; iter < iterations; iter++) {
        for (int i = 0; i < m; i++) {
            float sum = 0.0f;
            for (int j = 0; j < k; j++) {
                sum += weights[i * k + j] * input[j];
            }
            output_fp32[i] = sum;
        }
    }
    double time_fp32 = timer.elapsed_ms();

    // Benchmark Q4_0 matrix-vector
    timer.start();
    for (int iter = 0; iter < iterations; iter++) {
        matvec_q4_0(q4_matrix.data(), input.data(), output_q4.data(), m, k);
    }
    double time_q4 = timer.elapsed_ms();

    // Compute accuracy
    float max_diff = 0.0f;
    for (int i = 0; i < m; i++) {
        max_diff = std::max(max_diff, std::abs(output_fp32[i] - output_q4[i]));
    }

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Matrix: [" << m << " x " << k << "]" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "\nFP32 MatVec:  " << time_fp32 << " ms" << std::endl;
    std::cout << "Q4_0 MatVec:  " << time_q4 << " ms" << std::endl;
    std::cout << "Speedup:      " << (time_fp32 / time_q4) << "x" << std::endl;
    std::cout << "Max Error:    " << std::scientific << max_diff << std::endl;
    std::cout << std::fixed;

    // Memory usage
    size_t fp32_bytes = m * k * sizeof(float);
    size_t q4_bytes = m * (k / QK4_0) * sizeof(BlockQ4_0);
    std::cout << "\nMemory Usage:" << std::endl;
    std::cout << "  FP32: " << (fp32_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Q4_0: " << (q4_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Compression: " << std::setprecision(2) << ((float)fp32_bytes / q4_bytes) << "x" << std::endl;
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "4-bit/8-bit Quantization Benchmark" << std::endl;
    std::cout << "Testing GGML-extracted quantization" << std::endl;
    std::cout << "=====================================" << std::endl;

    benchmark_quantization_accuracy();
    benchmark_quantization_speed();
    benchmark_quantized_matvec();

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}
