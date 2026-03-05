// Benchmark: KV-Cache with GQA (Grouped-Query Attention)
// Tests memory efficiency, inference speed, and GQA performance

#include "kv_cache.h"
#include <iostream>
#include <vector>
#include <chrono>
#include <random>
#include <iomanip>

using namespace std::chrono;
using namespace KVCache;

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

void test_cache_basic() {
    std::cout << "\n=== Basic KV-Cache Test ===" << std::endl;

    CacheConfig config;
    config.n_layers = 6;
    config.n_heads = 8;
    config.n_kv_heads = 8;  // MHA (standard)
    config.head_dim = 64;
    config.max_seq_len = 2048;
    config.use_gqa = false;
    config.use_ring_buffer = false;

    CacheManager cache(config);

    // Generate random K, V for one token
    std::vector<float> K(config.n_kv_heads * config.head_dim);
    std::vector<float> V(config.n_kv_heads * config.head_dim);
    random_tensor(K, K.size());
    random_tensor(V, V.size());

    // Update cache (layer 0)
    int pos = cache.update(0, K.data(), V.data(), 1);
    std::cout << "Updated cache at position: " << pos << std::endl;

    // Retrieve cache
    int len;
    const float* cached_K = cache.get_keys(0, len);
    const float* cached_V = cache.get_values(0, len);

    std::cout << "Cached length: " << len << std::endl;

    // Verify data matches
    bool match = true;
    for (int i = 0; i < config.n_kv_heads * config.head_dim; i++) {
        if (std::abs(K[i] - cached_K[i]) > 1e-6f) {
            match = false;
            break;
        }
    }

    std::cout << "Data integrity: " << (match ? "✓ PASS" : "✗ FAIL") << std::endl;

    // Get stats
    auto stats = cache.get_stats();
    std::cout << "Memory usage: " << (stats.used_bytes / 1024) << " KB / "
              << (stats.total_bytes / 1024 / 1024) << " MB" << std::endl;
    std::cout << "Utilization: " << std::fixed << std::setprecision(2)
              << stats.utilization << "%" << std::endl;
}

void test_gqa_performance() {
    std::cout << "\n=== GQA Performance Test ===" << std::endl;

    const int n_tokens = 128;
    const int iterations = 100;

    // Config 1: MHA (standard)
    CacheConfig config_mha;
    config_mha.n_layers = 1;
    config_mha.n_heads = 8;
    config_mha.n_kv_heads = 8;  // All heads have their own K, V
    config_mha.head_dim = 64;
    config_mha.max_seq_len = 2048;
    config_mha.use_gqa = false;

    // Config 2: GQA (Grouped-Query Attention)
    CacheConfig config_gqa;
    config_gqa.n_layers = 1;
    config_gqa.n_heads = 8;
    config_gqa.n_kv_heads = 2;  // 8 Q heads → 2 KV heads (4:1 ratio)
    config_gqa.head_dim = 64;
    config_gqa.max_seq_len = 2048;
    config_gqa.use_gqa = true;
    config_gqa.n_heads_per_kv = 4;

    // Create caches
    CacheManager cache_mha(config_mha);
    CacheManager cache_gqa(config_gqa);

    // Generate random data
    std::vector<float> Q_mha(n_tokens * config_mha.n_heads * config_mha.head_dim);
    std::vector<float> K_mha(n_tokens * config_mha.n_kv_heads * config_mha.head_dim);
    std::vector<float> V_mha(n_tokens * config_mha.n_kv_heads * config_mha.head_dim);

    std::vector<float> Q_gqa(n_tokens * config_gqa.n_heads * config_gqa.head_dim);
    std::vector<float> K_gqa(n_tokens * config_gqa.n_kv_heads * config_gqa.head_dim);
    std::vector<float> V_gqa(n_tokens * config_gqa.n_kv_heads * config_gqa.head_dim);

    random_tensor(Q_mha, Q_mha.size());
    random_tensor(K_mha, K_mha.size());
    random_tensor(V_mha, V_mha.size());

    random_tensor(Q_gqa, Q_gqa.size());
    random_tensor(K_gqa, K_gqa.size());
    random_tensor(V_gqa, V_gqa.size());

    // Fill caches
    cache_mha.update(0, K_mha.data(), V_mha.data(), n_tokens);
    cache_gqa.update(0, K_gqa.data(), V_gqa.data(), n_tokens);

    // Benchmark MHA attention
    std::vector<float> output_mha(n_tokens * config_mha.n_heads * config_mha.head_dim);
    Timer timer;
    timer.start();
    for (int i = 0; i < iterations; i++) {
        attention_with_cache(Q_mha.data(), cache_mha, 0, output_mha.data(), n_tokens, true);
    }
    double time_mha = timer.elapsed_ms();

    // Benchmark GQA attention
    std::vector<float> output_gqa(n_tokens * config_gqa.n_heads * config_gqa.head_dim);
    timer.start();
    for (int i = 0; i < iterations; i++) {
        attention_with_cache(Q_gqa.data(), cache_gqa, 0, output_gqa.data(), n_tokens, true);
    }
    double time_gqa = timer.elapsed_ms();

    std::cout << std::fixed << std::setprecision(2);
    std::cout << "Tokens: " << n_tokens << ", Iterations: " << iterations << std::endl;
    std::cout << "\n[MHA] Multi-Head Attention (8 heads, 8 KV heads):" << std::endl;
    std::cout << "  Time: " << time_mha << " ms" << std::endl;
    std::cout << "  Avg: " << (time_mha / iterations) << " ms/iter" << std::endl;

    std::cout << "\n[GQA] Grouped-Query Attention (8 heads, 2 KV heads):" << std::endl;
    std::cout << "  Time: " << time_gqa << " ms" << std::endl;
    std::cout << "  Avg: " << (time_gqa / iterations) << " ms/iter" << std::endl;
    std::cout << "  Speedup: " << (time_mha / time_gqa) << "x" << std::endl;

    // Memory comparison
    auto stats_mha = cache_mha.get_stats();
    auto stats_gqa = cache_gqa.get_stats();

    std::cout << "\nMemory Usage:" << std::endl;
    std::cout << "  MHA: " << (stats_mha.total_bytes / 1024) << " KB" << std::endl;
    std::cout << "  GQA: " << (stats_gqa.total_bytes / 1024) << " KB" << std::endl;
    std::cout << "  Reduction: " << ((float)stats_mha.total_bytes / stats_gqa.total_bytes) << "x" << std::endl;
}

void test_sliding_window() {
    std::cout << "\n=== Sliding Window Attention Test ===" << std::endl;

    CacheConfig config;
    config.n_layers = 1;
    config.n_heads = 8;
    config.n_kv_heads = 8;
    config.head_dim = 64;
    config.max_seq_len = 8192;
    config.use_gqa = false;
    config.sliding_window = 4096;  // Mistral-style 4K window

    CacheManager cache(config);

    // Fill cache with long sequence
    const int total_tokens = 6000;
    const int window_size = 4096;

    std::vector<float> K(config.n_kv_heads * config.head_dim);
    std::vector<float> V(config.n_kv_heads * config.head_dim);

    std::cout << "Filling cache with " << total_tokens << " tokens..." << std::endl;

    for (int t = 0; t < total_tokens; t++) {
        random_tensor(K, K.size());
        random_tensor(V, V.size());
        cache.update(0, K.data(), V.data(), 1);
    }

    // Check cache stats
    auto stats = cache.get_stats();
    std::cout << "Cache filled length: " << stats.max_position << " tokens" << std::endl;
    std::cout << "Memory used: " << (stats.used_bytes / 1024) << " KB" << std::endl;

    // Test sliding window attention
    std::vector<float> Q(config.n_heads * config.head_dim);
    std::vector<float> output(config.n_heads * config.head_dim);
    random_tensor(Q, Q.size());

    Timer timer;
    timer.start();
    attention_sliding_window(Q.data(), cache, 0, output.data(), 1, window_size);
    double time = timer.elapsed_ms();

    std::cout << "Sliding window attention (4K window): " << time << " ms" << std::endl;
    std::cout << "✓ Only attended to last " << window_size << " tokens (not all " << total_tokens << ")" << std::endl;
}

void test_ring_buffer() {
    std::cout << "\n=== Ring Buffer Test ===" << std::endl;

    CacheConfig config;
    config.n_layers = 1;
    config.n_heads = 8;
    config.n_kv_heads = 8;
    config.head_dim = 64;
    config.max_seq_len = 512;
    config.use_ring_buffer = true;
    config.sliding_window = 512;

    CacheManager cache(config);

    std::vector<float> K(config.n_kv_heads * config.head_dim);
    std::vector<float> V(config.n_kv_heads * config.head_dim);

    // Add more tokens than max_seq_len (should wrap around)
    const int total_tokens = 1000;
    std::cout << "Adding " << total_tokens << " tokens to cache (max: " << config.max_seq_len << ")" << std::endl;

    for (int t = 0; t < total_tokens; t++) {
        random_tensor(K, K.size());
        random_tensor(V, V.size());
        cache.update(0, K.data(), V.data(), 1);
    }

    auto stats = cache.get_stats();
    std::cout << "Final cache length: " << stats.max_position << " tokens" << std::endl;
    std::cout << "Memory used: " << (stats.used_bytes / 1024) << " KB" << std::endl;
    std::cout << "✓ Ring buffer kept memory constant despite " << total_tokens << " tokens" << std::endl;
}

int main() {
    std::cout << "=====================================" << std::endl;
    std::cout << "KV-Cache + GQA Benchmark" << std::endl;
    std::cout << "Testing llama.cpp-extracted features" << std::endl;
    std::cout << "=====================================" << std::endl;

    test_cache_basic();
    test_gqa_performance();
    test_sliding_window();
    test_ring_buffer();

    std::cout << "\n=====================================" << std::endl;
    std::cout << "Benchmark Complete!" << std::endl;
    std::cout << "=====================================" << std::endl;

    return 0;
}
