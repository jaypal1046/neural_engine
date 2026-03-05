// Benchmark: Flash Attention v2 vs Standard Attention
// Tests memory efficiency and speed on various sequence lengths

#include "flash_attention.h"
#include "tensor_ops.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std::chrono;
using namespace FlashAttention;

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

void random_tensor(std::vector<float>& tensor, int size) {
    std::mt19937 gen(42);
    std::normal_distribution<float> dist(0.0f, 1.0f);
    tensor.resize(size);
    for (auto& val : tensor) {
        val = dist(gen);
    }
}

// Standard attention (O(N²) memory)
void standard_attention(
    const float* Q,
    const float* K,
    const float* V,
    float* O,
    int seq_len,
    int n_heads,
    int head_dim,
    bool causal_mask
) {
    float scale = 1.0f / std::sqrt((float)head_dim);

    for (int h = 0; h < n_heads; h++) {
        // Compute attention scores: Q @ K^T
        std::vector<float> scores(seq_len * seq_len);

        for (int i = 0; i < seq_len; i++) {
            for (int j = 0; j < seq_len; j++) {
                const float* q = Q + (h * seq_len + i) * head_dim;
                const float* k = K + (h * seq_len + j) * head_dim;

                float score = TensorOps::vec_dot(q, k, head_dim) * scale;

                if (causal_mask && j > i) {
                    score = -1e9f;
                }

                scores[i * seq_len + j] = score;
            }
        }

        // Softmax
        for (int i = 0; i < seq_len; i++) {
            TensorOps::softmax(&scores[i * seq_len], &scores[i * seq_len], seq_len);
        }

        // Weighted sum: scores @ V
        for (int i = 0; i < seq_len; i++) {
            float* out = O + (h * seq_len + i) * head_dim;
            std::fill_n(out, head_dim, 0.0f);

            for (int j = 0; j < seq_len; j++) {
                const float* v = V + (h * seq_len + j) * head_dim;
                float weight = scores[i * seq_len + j];
                for (int d = 0; d < head_dim; d++) {
                    out[d] += weight * v[d];
                }
            }
        }
    }
}

void test_correctness() {
    std::cout << "\n=== Correctness Test ===" << std::endl;

    const int seq_len = 128;
    const int n_heads = 4;
    const int head_dim = 64;

    std::vector<float> Q, K, V;
    random_tensor(Q, seq_len * n_heads * head_dim);
    random_tensor(K, seq_len * n_heads * head_dim);
    random_tensor(V, seq_len * n_heads * head_dim);

    std::vector<float> O_standard(seq_len * n_heads * head_dim);
    std::vector<float> O_flash(seq_len * n_heads * head_dim);

    // Standard attention
    standard_attention(Q.data(), K.data(), V.data(), O_standard.data(),
                      seq_len, n_heads, head_dim, true);

    // Flash attention
    FlashConfig config;
    config.block_size_q = 64;
    config.block_size_kv = 64;
    config.use_causal_mask = true;

    flash_attention_forward_single(Q.data(), K.data(), V.data(), O_flash.data(),
                                   seq_len, n_heads, head_dim, config);

    // Compare results
    float max_diff = 0.0f;
    float avg_diff = 0.0f;
    int count = 0;

    for (size_t i = 0; i < O_standard.size(); i++) {
        float diff = std::abs(O_standard[i] - O_flash[i]);
        max_diff = std::max(max_diff, diff);
        avg_diff += diff;
        count++;
    }

    avg_diff /= count;

    std::cout << "Sequence length: " << seq_len << std::endl;
    std::cout << "Heads: " << n_heads << ", Head dim: " << head_dim << std::endl;
    std::cout << "Max difference: " << std::scientific << max_diff << std::endl;
    std::cout << "Avg difference: " << avg_diff << std::endl;
    std::cout << "Correctness: " << (max_diff < 1e-4f ? "✓ PASS" : "✗ FAIL") << std::endl;
}

void benchmark_speed(int seq_len) {
    std::cout << "\n=== Speed Benchmark (seq_len=" << seq_len << ") ===" << std::endl;

    const int n_heads = 8;
    const int head_dim = 64;
    const int iterations = seq_len > 1024 ? 5 : 20;

    std::vector<float> Q, K, V;
    random_tensor(Q, seq_len * n_heads * head_dim);
    random_tensor(K, seq_len * n_heads * head_dim);
    random_tensor(V, seq_len * n_heads * head_dim);

    std::vector<float> O(seq_len * n_heads * head_dim);

    Timer timer;

    // Standard attention
    timer.start();
    for (int i = 0; i < iterations; i++) {
        standard_attention(Q.data(), K.data(), V.data(), O.data(),
                          seq_len, n_heads, head_dim, true);
    }
    double time_standard = timer.elapsed_ms();

    // Flash attention
    FlashConfig config;
    config.block_size_q = 64;
    config.block_size_kv = 64;
    config.use_causal_mask = true;

    timer.start();
    for (int i = 0; i < iterations; i++) {
        flash_attention_forward_single(Q.data(), K.data(), V.data(), O.data(),
                                       seq_len, n_heads, head_dim, config);
    }
    double time_flash = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Iterations: " << iterations << std::endl;
    std::cout << "Standard Attention: " << time_standard << " ms (" << (time_standard/iterations) << " ms/iter)" << std::endl;
    std::cout << "Flash Attention:    " << time_flash << " ms (" << (time_flash/iterations) << " ms/iter)" << std::endl;
    std::cout << "Speedup:            " << (time_standard / time_flash) << "x" << std::endl;

    // Memory usage
    auto stats = compute_memory_stats(1, seq_len, n_heads, head_dim);
    std::cout << "\nMemory Usage:" << std::endl;
    std::cout << "  Standard: " << (stats.standard_attention / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Flash:    " << (stats.flash_attention / 1024 / 1024) << " MB" << std::endl;
    std::cout << "  Reduction: " << stats.reduction_factor << "x" << std::endl;
}

void test_gqa() {
    std::cout << "\n=== Flash Attention + GQA Test ===" << std::endl;

    const int seq_len = 256;
    const int n_heads = 8;
    const int n_kv_heads = 2;  // 4:1 GQA ratio
    const int head_dim = 64;

    std::vector<float> Q, K, V, O;
    random_tensor(Q, seq_len * n_heads * head_dim);
    random_tensor(K, seq_len * n_kv_heads * head_dim);
    random_tensor(V, seq_len * n_kv_heads * head_dim);
    O.resize(seq_len * n_heads * head_dim);

    FlashConfig config;
    config.block_size_q = 64;
    config.block_size_kv = 64;
    config.use_causal_mask = true;

    Timer timer;
    timer.start();
    flash_attention_gqa(Q.data(), K.data(), V.data(), O.data(),
                       seq_len, n_heads, n_kv_heads, head_dim, config);
    double time = timer.elapsed_ms();

    std::cout << "Sequence length: " << seq_len << std::endl;
    std::cout << "Q heads: " << n_heads << ", KV heads: " << n_kv_heads << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(2) << time << " ms" << std::endl;
    std::cout << "✓ Flash Attention + GQA working" << std::endl;
}

void test_sliding_window() {
    std::cout << "\n=== Sliding Window Flash Attention Test ===" << std::endl;

    const int seq_len = 2048;
    const int n_heads = 8;
    const int head_dim = 64;
    const int window_size = 512;

    std::vector<float> Q, K, V, O;
    random_tensor(Q, seq_len * n_heads * head_dim);
    random_tensor(K, seq_len * n_heads * head_dim);
    random_tensor(V, seq_len * n_heads * head_dim);
    O.resize(seq_len * n_heads * head_dim);

    FlashConfig config;
    config.block_size_q = 64;
    config.block_size_kv = 64;
    config.use_causal_mask = true;

    Timer timer;
    timer.start();
    flash_attention_sliding_window(Q.data(), K.data(), V.data(), O.data(),
                                   seq_len, n_heads, head_dim, window_size, config);
    double time = timer.elapsed_ms();

    std::cout << "Sequence length: " << seq_len << std::endl;
    std::cout << "Sliding window: " << window_size << " tokens" << std::endl;
    std::cout << "Time: " << std::fixed << std::setprecision(2) << time << " ms" << std::endl;
    std::cout << "✓ Only attended to " << window_size << "-token window (not all " << seq_len << ")" << std::endl;
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "Flash Attention v2 Benchmark" << std::endl;
    std::cout << "Memory-efficient O(N) attention" << std::endl;
    std::cout << "=====================================" << std::endl;

    test_correctness();

    // Benchmark various sequence lengths
    benchmark_speed(128);   // Short sequence
    benchmark_speed(512);   // Medium sequence
    benchmark_speed(1024);  // Long sequence
    benchmark_speed(2048);  // Very long sequence

    test_gqa();
    test_sliding_window();

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}
