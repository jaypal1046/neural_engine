// Benchmark: Compare old vs new tensor operations
// Tests SIMD optimizations from GGML extraction

#include "tensor_ops.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std::chrono;

// Simple timer class
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

// Reference scalar implementation (old)
void matmul_old(const float* A, const float* B, float* C, int m, int k, int n) {
    for (int i = 0; i < m; i++) {
        for (int j = 0; j < n; j++) {
            float sum = 0.0f;
            for (int p = 0; p < k; p++) {
                sum += A[i * k + p] * B[p * n + j];
            }
            C[i * n + j] = sum;
        }
    }
}

// Generate random matrix
void random_matrix(std::vector<float>& mat, int rows, int cols) {
    std::mt19937 gen(42);
    std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

    mat.resize(rows * cols);
    for (auto& val : mat) {
        val = dist(gen);
    }
}

// Verify results match (within floating point tolerance)
bool verify_results(const std::vector<float>& a, const std::vector<float>& b, float tolerance = 1e-4f) {
    if (a.size() != b.size()) return false;

    for (size_t i = 0; i < a.size(); i++) {
        float diff = std::abs(a[i] - b[i]);
        if (diff > tolerance) {
            std::cout << "Mismatch at index " << i << ": " << a[i] << " vs " << b[i]
                      << " (diff: " << diff << ")" << std::endl;
            return false;
        }
    }
    return true;
}

void benchmark_matmul(int m, int k, int n, int iterations) {
    std::cout << "\n=== Matrix Multiplication Benchmark ===" << std::endl;
    std::cout << "Size: [" << m << " x " << k << "] @ [" << k << " x " << n << "] = [" << m << " x " << n << "]" << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;

    // Generate random matrices
    std::vector<float> A, B, C_old, C_new;
    random_matrix(A, m, k);
    random_matrix(B, k, n);
    C_old.resize(m * n);
    C_new.resize(m * n);

    Timer timer;

    // Benchmark old implementation
    timer.start();
    for (int i = 0; i < iterations; i++) {
        matmul_old(A.data(), B.data(), C_old.data(), m, k, n);
    }
    double time_old = timer.elapsed_ms();

    // Benchmark new implementation (auto-dispatches to best SIMD)
    timer.start();
    for (int i = 0; i < iterations; i++) {
        TensorOps::matmul(A.data(), B.data(), C_new.data(), m, k, n);
    }
    double time_new = timer.elapsed_ms();

    // Verify correctness
    bool correct = verify_results(C_old, C_new);

    // Print results
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Old (scalar):  " << time_old << " ms" << std::endl;
    std::cout << "New (SIMD):    " << time_new << " ms" << std::endl;
    std::cout << "Speedup:       " << (time_old / time_new) << "x" << std::endl;
    std::cout << "Correctness:   " << (correct ? "✓ PASS" : "✗ FAIL") << std::endl;

    // Calculate GFLOPS
    double flops = 2.0 * m * k * n * iterations;  // 2 ops per multiply-add
    double gflops_old = flops / (time_old * 1e6);
    double gflops_new = flops / (time_new * 1e6);
    std::cout << "GFLOPS (old):  " << gflops_old << std::endl;
    std::cout << "GFLOPS (new):  " << gflops_new << std::endl;
}

void benchmark_vec_dot(int n, int iterations) {
    std::cout << "\n=== Vector Dot Product Benchmark ===" << std::endl;
    std::cout << "Vector size: " << n << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;

    std::vector<float> a, b;
    random_matrix(a, 1, n);
    random_matrix(b, 1, n);

    Timer timer;

    // Old implementation
    timer.start();
    float sum_old = 0.0f;
    for (int i = 0; i < iterations; i++) {
        float sum = 0.0f;
        for (int j = 0; j < n; j++) {
            sum += a[j] * b[j];
        }
        sum_old += sum;
    }
    double time_old = timer.elapsed_ms();

    // New implementation
    timer.start();
    float sum_new = 0.0f;
    for (int i = 0; i < iterations; i++) {
        sum_new += TensorOps::vec_dot(a.data(), b.data(), n);
    }
    double time_new = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Old (scalar):  " << time_old << " ms" << std::endl;
    std::cout << "New (SIMD):    " << time_new << " ms" << std::endl;
    std::cout << "Speedup:       " << (time_old / time_new) << "x" << std::endl;
    std::cout << "Results match: " << (std::abs(sum_old - sum_new) < 1e-3f ? "✓ PASS" : "✗ FAIL") << std::endl;
}

void benchmark_rmsnorm(int size, int iterations) {
    std::cout << "\n=== RMSNorm Benchmark ===" << std::endl;
    std::cout << "Vector size: " << size << std::endl;
    std::cout << "Iterations: " << iterations << std::endl;

    std::vector<float> input, weight, output;
    random_matrix(input, 1, size);
    random_matrix(weight, 1, size);
    output.resize(size);

    Timer timer;
    timer.start();
    for (int i = 0; i < iterations; i++) {
        TensorOps::rmsnorm(output.data(), input.data(), weight.data(), size);
    }
    double time_ms = timer.elapsed_ms();

    std::cout << "Time: " << time_ms << " ms" << std::endl;
    std::cout << "Avg per iteration: " << (time_ms / iterations) << " ms" << std::endl;
}

void benchmark_rope() {
    std::cout << "\n=== RoPE (Rotary Position Embeddings) Benchmark ===" << std::endl;

    const int seq_len = 128;
    const int n_heads = 8;
    const int head_dim = 64;
    const int iterations = 1000;

    TensorOps::RoPEConfig config;
    config.dim = head_dim;
    config.max_seq_len = 2048;
    config.theta = 10000.0f;

    TensorOps::RoPECache rope_cache(config);

    std::vector<float> input, output;
    std::vector<int> positions;
    random_matrix(input, seq_len * n_heads, head_dim);
    output.resize(seq_len * n_heads * head_dim);

    for (int i = 0; i < seq_len; i++) {
        positions.push_back(i);
    }

    Timer timer;
    timer.start();
    for (int i = 0; i < iterations; i++) {
        rope_cache.apply(output.data(), input.data(), positions.data(), seq_len, n_heads, head_dim);
    }
    double time_ms = timer.elapsed_ms();

    std::cout << "Sequence length: " << seq_len << ", Heads: " << n_heads << ", Head dim: " << head_dim << std::endl;
    std::cout << "Total time: " << time_ms << " ms" << std::endl;
    std::cout << "Avg per iteration: " << (time_ms / iterations) << " ms" << std::endl;
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "Tensor Operations Benchmark" << std::endl;
    std::cout << "Testing GGML-extracted optimizations" << std::endl;
    std::cout << "=====================================" << std::endl;

    // Detect CPU features
    auto features = TensorOps::detect_cpu_features();
    std::cout << "\nCPU Features:" << std::endl;
    std::cout << "  SSE2:   " << (features.sse2 ? "✓" : "✗") << std::endl;
    std::cout << "  SSE4.2: " << (features.sse42 ? "✓" : "✗") << std::endl;
    std::cout << "  AVX:    " << (features.avx ? "✓" : "✗") << std::endl;
    std::cout << "  AVX2:   " << (features.avx2 ? "✓" : "✗") << std::endl;
    std::cout << "  AVX512: " << (features.avx512 ? "✓" : "✗") << std::endl;
    std::cout << "  FMA:    " << (features.fma ? "✓" : "✗") << std::endl;

    // Vector dot product (key operation in attention)
    benchmark_vec_dot(512, 10000);
    benchmark_vec_dot(4096, 1000);

    // Matrix multiplication (various sizes)
    // Small (embedding lookup)
    benchmark_matmul(32, 512, 512, 100);

    // Medium (attention computation in mini_transformer)
    benchmark_matmul(128, 512, 512, 50);

    // Large (feed-forward layer)
    benchmark_matmul(128, 512, 2048, 20);

    // Very large (typical transformer block)
    benchmark_matmul(512, 512, 512, 10);

    // RMSNorm (used in LLaMA/Mistral)
    benchmark_rmsnorm(512, 10000);
    benchmark_rmsnorm(2048, 1000);

    // RoPE (rotary position embeddings)
    benchmark_rope();

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}
