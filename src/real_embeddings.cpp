#include "real_embeddings.h"
#include <fstream>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <numeric>

RealEmbeddings::RealEmbeddings(int embedding_dim, int vocab_size)
    : embedding_dim_(embedding_dim), vocab_size_(vocab_size), rng_(std::random_device{}()) {
    initialize_embeddings();
}

void RealEmbeddings::initialize_embeddings() {
    // Xavier initialization
    float scale = std::sqrt(6.0f / (vocab_size_ + embedding_dim_));

    embeddings_.resize(vocab_size_);
    context_embeddings_.resize(vocab_size_);

    std::uniform_real_distribution<float> dist(-scale, scale);

    for (int i = 0; i < vocab_size_; i++) {
        embeddings_[i].resize(embedding_dim_);
        context_embeddings_[i].resize(embedding_dim_);

        for (int j = 0; j < embedding_dim_; j++) {
            embeddings_[i][j] = dist(rng_);
            context_embeddings_[i][j] = dist(rng_);
        }
    }
}

void RealEmbeddings::build_unigram_table(const std::vector<int>& token_counts) {
    // Build sampling table with smoothed distribution (power 0.75)
    const int table_size = 100000000;  // 100M entries for smooth sampling
    unigram_table_.clear();
    unigram_table_.reserve(table_size);

    // Compute smoothed frequencies
    std::vector<float> smoothed_freq(vocab_size_);
    float total = 0.0f;
    for (int i = 0; i < vocab_size_; i++) {
        smoothed_freq[i] = std::pow(token_counts[i], 0.75f);
        total += smoothed_freq[i];
    }

    // Normalize
    for (int i = 0; i < vocab_size_; i++) {
        smoothed_freq[i] /= total;
    }

    // Fill table
    int token_id = 0;
    float cumulative = smoothed_freq[0];

    for (int i = 0; i < table_size; i++) {
        float pos = (float)i / table_size;

        while (pos > cumulative && token_id < vocab_size_ - 1) {
            token_id++;
            cumulative += smoothed_freq[token_id];
        }

        unigram_table_.push_back(token_id);
    }

    std::cerr << "[EMBED] Built unigram sampling table (" << table_size << " entries)\n";
}

int RealEmbeddings::sample_negative() const {
    std::uniform_int_distribution<int> dist(0, unigram_table_.size() - 1);
    return unigram_table_[dist(const_cast<std::mt19937&>(rng_))];
}

float RealEmbeddings::sigmoid(float x) const {
    if (x > 6.0f) return 1.0f;
    if (x < -6.0f) return 0.0f;
    return 1.0f / (1.0f + std::exp(-x));
}

void RealEmbeddings::train(
    const std::vector<std::string>& texts,
    BPETokenizer& tokenizer,
    int epochs,
    float learning_rate,
    int window_size,
    int negative_samples
) {
    std::cerr << "[EMBED] Training Word2Vec embeddings...\n";
    std::cerr << "[EMBED] Params: dim=" << embedding_dim_ << ", epochs=" << epochs
              << ", lr=" << learning_rate << ", window=" << window_size
              << ", neg_samples=" << negative_samples << "\n";

    // Tokenize all texts
    std::vector<std::vector<int>> tokenized_texts;
    std::vector<int> token_counts(vocab_size_, 0);

    for (const auto& text : texts) {
        auto tokens = tokenizer.encode(text);
        tokenized_texts.push_back(tokens);

        for (int token_id : tokens) {
            if (token_id < vocab_size_) {
                token_counts[token_id]++;
            }
        }
    }

    // Build negative sampling table
    build_unigram_table(token_counts);

    // Training loop
    int total_samples = 0;
    for (const auto& tokens : tokenized_texts) {
        total_samples += tokens.size() * window_size * 2;  // Approximate
    }

    std::cerr << "[EMBED] Total training samples: ~" << total_samples << "\n";

    for (int epoch = 0; epoch < epochs; epoch++) {
        float current_lr = learning_rate * (1.0f - (float)epoch / epochs);  // Learning rate decay
        int samples_processed = 0;

        for (const auto& tokens : tokenized_texts) {
            for (size_t center_pos = 0; center_pos < tokens.size(); center_pos++) {
                int center_token = tokens[center_pos];
                if (center_token >= vocab_size_) continue;

                // Skip-Gram: predict context from center word
                int window_start = std::max(0, (int)center_pos - window_size);
                int window_end = std::min((int)tokens.size(), (int)center_pos + window_size + 1);

                for (int context_pos = window_start; context_pos < window_end; context_pos++) {
                    if (context_pos == (int)center_pos) continue;

                    int context_token = tokens[context_pos];
                    if (context_token >= vocab_size_) continue;

                    // Positive sample: real context word
                    float score = 0.0f;
                    for (int d = 0; d < embedding_dim_; d++) {
                        score += embeddings_[center_token][d] * context_embeddings_[context_token][d];
                    }

                    float pred = sigmoid(score);
                    float error = (1.0f - pred);  // Target = 1 for positive

                    // Update embeddings
                    for (int d = 0; d < embedding_dim_; d++) {
                        float grad_center = error * context_embeddings_[context_token][d];
                        float grad_context = error * embeddings_[center_token][d];

                        embeddings_[center_token][d] += current_lr * grad_center;
                        context_embeddings_[context_token][d] += current_lr * grad_context;
                    }

                    // Negative samples: random words
                    for (int neg = 0; neg < negative_samples; neg++) {
                        int neg_token = sample_negative();
                        if (neg_token == context_token) continue;

                        float neg_score = 0.0f;
                        for (int d = 0; d < embedding_dim_; d++) {
                            neg_score += embeddings_[center_token][d] * context_embeddings_[neg_token][d];
                        }

                        float neg_pred = sigmoid(neg_score);
                        float neg_error = (0.0f - neg_pred);  // Target = 0 for negative

                        // Update embeddings
                        for (int d = 0; d < embedding_dim_; d++) {
                            float grad_center = neg_error * context_embeddings_[neg_token][d];
                            float grad_context = neg_error * embeddings_[center_token][d];

                            embeddings_[center_token][d] += current_lr * grad_center;
                            context_embeddings_[neg_token][d] += current_lr * grad_context;
                        }
                    }

                    samples_processed++;
                    if (samples_processed % 100000 == 0) {
                        std::cerr << "[EMBED] Epoch " << (epoch + 1) << "/" << epochs
                                  << " | Samples: " << samples_processed << "/" << total_samples
                                  << " | LR: " << current_lr << "\r" << std::flush;
                    }
                }
            }
        }

        std::cerr << "\n[EMBED] Epoch " << (epoch + 1) << " complete.\n";
    }

    std::cerr << "[EMBED] Training complete!\n";
}

std::vector<float> RealEmbeddings::get_embedding(int token_id) const {
    if (token_id < 0 || token_id >= vocab_size_) {
        return std::vector<float>(embedding_dim_, 0.0f);
    }
    return embeddings_[token_id];
}

std::vector<float> RealEmbeddings::encode_text(const std::string& text, const BPETokenizer& tokenizer) const {
    auto tokens = tokenizer.encode(text);

    std::vector<float> avg_embedding(embedding_dim_, 0.0f);
    int count = 0;

    for (int token_id : tokens) {
        if (token_id < vocab_size_) {
            for (int d = 0; d < embedding_dim_; d++) {
                avg_embedding[d] += embeddings_[token_id][d];
            }
            count++;
        }
    }

    // Average
    if (count > 0) {
        for (int d = 0; d < embedding_dim_; d++) {
            avg_embedding[d] /= count;
        }
    }

    // Normalize
    float norm = 0.0f;
    for (float val : avg_embedding) {
        norm += val * val;
    }
    norm = std::sqrt(norm);

    if (norm > 0.0001f) {
        for (float& val : avg_embedding) {
            val /= norm;
        }
    }

    return avg_embedding;
}

float RealEmbeddings::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size()) return 0.0f;

    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a * norm_b);
    return (denom > 0.0001f) ? (dot / denom) : 0.0f;
}

std::vector<std::pair<int, float>> RealEmbeddings::find_similar(int token_id, int top_k) const {
    if (token_id < 0 || token_id >= vocab_size_) {
        return {};
    }

    auto query_emb = embeddings_[token_id];
    std::vector<std::pair<int, float>> similarities;

    for (int i = 0; i < vocab_size_; i++) {
        if (i == token_id) continue;
        float sim = cosine_similarity(query_emb, embeddings_[i]);
        similarities.push_back({i, sim});
    }

    // Partial sort to get top-k
    std::partial_sort(
        similarities.begin(),
        similarities.begin() + std::min(top_k, (int)similarities.size()),
        similarities.end(),
        [](const auto& a, const auto& b) { return a.second > b.second; }
    );

    similarities.resize(std::min(top_k, (int)similarities.size()));
    return similarities;
}

void RealEmbeddings::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[EMBED] Error: Cannot save to " << path << "\n";
        return;
    }

    // Write dimensions
    file.write(reinterpret_cast<const char*>(&embedding_dim_), sizeof(embedding_dim_));
    file.write(reinterpret_cast<const char*>(&vocab_size_), sizeof(vocab_size_));

    // Write embeddings
    for (const auto& emb : embeddings_) {
        file.write(reinterpret_cast<const char*>(emb.data()), emb.size() * sizeof(float));
    }

    std::cerr << "[EMBED] Saved embeddings to " << path << " ("
              << (vocab_size_ * embedding_dim_ * sizeof(float) / (1024 * 1024)) << " MB)\n";
}

void RealEmbeddings::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[EMBED] Error: Cannot load from " << path << "\n";
        return;
    }

    // Read dimensions
    file.read(reinterpret_cast<char*>(&embedding_dim_), sizeof(embedding_dim_));
    file.read(reinterpret_cast<char*>(&vocab_size_), sizeof(vocab_size_));

    // Read embeddings
    embeddings_.resize(vocab_size_);
    for (int i = 0; i < vocab_size_; i++) {
        embeddings_[i].resize(embedding_dim_);
        file.read(reinterpret_cast<char*>(embeddings_[i].data()), embedding_dim_ * sizeof(float));
    }

    std::cerr << "[EMBED] Loaded embeddings from " << path
              << " (vocab: " << vocab_size_ << ", dim: " << embedding_dim_ << ")\n";
}
