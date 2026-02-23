#include "embedding_trainer.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <set>

EmbeddingTrainer::EmbeddingTrainer(int dim, int window, int neg_samples, int min_count)
    : dim(dim), window(window), neg_samples(neg_samples), min_count(min_count),
      rng(std::random_device{}())
{
}

void EmbeddingTrainer::init_embeddings() {
    std::uniform_real_distribution<float> dist(-0.5f / dim, 0.5f / dim);

    for (int i = 0; i < tokenizer.vocab_size(); i++) {
        // Target embeddings
        embeddings[i].resize(dim);
        for (int j = 0; j < dim; j++) {
            embeddings[i][j] = dist(rng);
        }

        // Context embeddings (separate for skip-gram)
        context_embeddings[i].resize(dim, 0.0f);
    }

    std::cout << "[Embeddings] Initialized " << tokenizer.vocab_size()
              << " embeddings of dimension " << dim << "\n";
}

void EmbeddingTrainer::build_neg_table() {
    std::cout << "[Embeddings] Building negative sampling table...\n";

    // Build frequency table (word -> count^0.75)
    std::vector<double> power_freqs;
    double total = 0.0;

    for (int i = 0; i < tokenizer.vocab_size(); i++) {
        std::string word = tokenizer.get_word(i);
        int freq = tokenizer.get_word_freq(word);
        if (freq == 0) freq = 1;

        double power = std::pow(freq, 0.75);
        power_freqs.push_back(power);
        total += power;
    }

    // Populate neg_table proportionally
    neg_table.clear();
    neg_table.reserve(NEG_TABLE_SIZE);

    int word_idx = 0;
    double cumulative = power_freqs[0] / total;

    for (int i = 0; i < NEG_TABLE_SIZE; i++) {
        double target = static_cast<double>(i) / NEG_TABLE_SIZE;

        while (cumulative < target && word_idx < static_cast<int>(power_freqs.size()) - 1) {
            word_idx++;
            cumulative += power_freqs[word_idx] / total;
        }

        neg_table.push_back(word_idx);
    }

    std::cout << "[Embeddings] Negative sampling table built ("
              << (NEG_TABLE_SIZE / 1000000) << "M entries)\n";
}

int EmbeddingTrainer::sample_negative() {
    std::uniform_int_distribution<int> dist(0, NEG_TABLE_SIZE - 1);
    return neg_table[dist(rng)];
}

float EmbeddingTrainer::dot_product(const std::vector<float>& a, const std::vector<float>& b) {
    float sum = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        sum += a[i] * b[i];
    }
    return sum;
}

float EmbeddingTrainer::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;

    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }

    mag_a = std::sqrt(mag_a);
    mag_b = std::sqrt(mag_b);

    if (mag_a < 1e-10f || mag_b < 1e-10f) return 0.0f;

    return dot / (mag_a * mag_b);
}

void EmbeddingTrainer::normalize(std::vector<float>& vec) {
    float mag = 0.0f;
    for (float v : vec) mag += v * v;
    mag = std::sqrt(mag);

    if (mag > 1e-10f) {
        for (float& v : vec) v /= mag;
    }
}

void EmbeddingTrainer::update_positive(int target, int context, float lr) {
    auto& target_emb = embeddings[target];
    auto& context_emb = context_embeddings[context];

    // Forward pass
    float score = dot_product(target_emb, context_emb);
    float pred = sigmoid(score);
    float gradient = (1.0f - pred) * lr;  // Positive sample: target = 1

    // Backward pass (gradient descent)
    for (int i = 0; i < dim; i++) {
        float grad_target = gradient * context_emb[i];
        float grad_context = gradient * target_emb[i];

        target_emb[i] += grad_target;
        context_emb[i] += grad_context;
    }
}

void EmbeddingTrainer::update_negative(int target, int negative, float lr) {
    auto& target_emb = embeddings[target];
    auto& neg_emb = context_embeddings[negative];

    // Forward pass
    float score = dot_product(target_emb, neg_emb);
    float pred = sigmoid(score);
    float gradient = (0.0f - pred) * lr;  // Negative sample: target = 0

    // Backward pass
    for (int i = 0; i < dim; i++) {
        float grad_target = gradient * neg_emb[i];
        float grad_neg = gradient * target_emb[i];

        target_emb[i] += grad_target;
        neg_emb[i] += grad_neg;
    }
}

void EmbeddingTrainer::train_word(
    int target_word,
    const std::vector<int>& context_words,
    float lr
) {
    // Positive samples (actual context)
    for (int context : context_words) {
        update_positive(target_word, context, lr);
    }

    // Negative samples (random words)
    for (int n = 0; n < neg_samples; n++) {
        int negative = sample_negative();

        // Don't use target or context as negative
        if (negative == target_word) continue;
        bool skip = false;
        for (int ctx : context_words) {
            if (negative == ctx) {
                skip = true;
                break;
            }
        }
        if (skip) continue;

        update_negative(target_word, negative, lr);
    }
}

void EmbeddingTrainer::train(
    const std::vector<std::string>& sentences,
    int epochs,
    float learning_rate
) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "[Embeddings] Training on " << sentences.size() << " sentences\n";

    // Build vocabulary
    std::cout << "[Embeddings] Building vocabulary...\n";
    for (const auto& sentence : sentences) {
        tokenizer.train_from_text(sentence, 100000, false);
    }

    // Initialize embeddings
    init_embeddings();

    // Build negative sampling table
    build_neg_table();

    // Training loop
    stats.total_sentences = sentences.size();

    for (int epoch = 0; epoch < epochs; epoch++) {
        std::cout << "[Embeddings] Epoch " << (epoch + 1) << "/" << epochs << "\n";

        float lr = learning_rate * (1.0f - static_cast<float>(epoch) / epochs);
        stats.final_learning_rate = lr;

        int words_trained = 0;

        for (const auto& sentence : sentences) {
            auto tokens = tokenizer.tokenize(sentence);
            if (tokens.empty()) continue;

            // Convert to IDs
            std::vector<int> word_ids;
            for (const auto& token : tokens) {
                word_ids.push_back(tokenizer.get_id(token));
            }

            // Train skip-gram
            for (size_t i = 0; i < word_ids.size(); i++) {
                int target = word_ids[i];

                // Skip rare words
                if (tokenizer.get_word_freq(tokenizer.get_word(target)) < min_count) {
                    continue;
                }

                // Get context window
                std::vector<int> context;
                int start = std::max(0, static_cast<int>(i) - window);
                int end = std::min(static_cast<int>(word_ids.size()), static_cast<int>(i) + window + 1);

                for (int j = start; j < end; j++) {
                    if (j != static_cast<int>(i)) {
                        context.push_back(word_ids[j]);
                    }
                }

                // Train
                train_word(target, context, lr);
                words_trained++;
            }
        }

        stats.total_words_trained += words_trained;
        stats.epochs_completed = epoch + 1;

        std::cout << "[Embeddings] Epoch " << (epoch + 1)
                  << " complete, trained " << words_trained << " words\n";
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.training_time_sec = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "[Embeddings] Training complete in " << stats.training_time_sec << " seconds\n";
    std::cout << "[Embeddings] Total words trained: " << stats.total_words_trained << "\n";
}

void EmbeddingTrainer::train_from_file(
    const std::string& file_path,
    int epochs,
    float learning_rate
) {
    std::cout << "[Embeddings] Loading corpus from " << file_path << "\n";

    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "[Embeddings] Error: Cannot open file\n";
        return;
    }

    std::vector<std::string> sentences;
    std::string line;

    while (std::getline(file, line)) {
        if (line.length() >= 10) {  // Skip very short lines
            sentences.push_back(line);
        }
    }

    std::cout << "[Embeddings] Loaded " << sentences.size() << " sentences\n";

    train(sentences, epochs, learning_rate);
}

std::vector<float> EmbeddingTrainer::get_embedding(const std::string& word) const {
    int id = tokenizer.get_id(word);
    auto it = embeddings.find(id);

    if (it != embeddings.end()) {
        return it->second;
    }

    // Return zero vector if not found
    return std::vector<float>(dim, 0.0f);
}

float EmbeddingTrainer::similarity(const std::string& word1, const std::string& word2) const {
    auto emb1 = get_embedding(word1);
    auto emb2 = get_embedding(word2);

    return cosine_similarity(emb1, emb2);
}

std::vector<std::pair<std::string, float>> EmbeddingTrainer::most_similar(
    const std::string& word,
    int top_k
) const {
    auto query_emb = get_embedding(word);

    std::vector<std::pair<std::string, float>> results;

    for (int i = 0; i < tokenizer.vocab_size(); i++) {
        std::string other = tokenizer.get_word(i);
        if (other == word) continue;

        auto it = embeddings.find(i);
        if (it == embeddings.end()) continue;

        float sim = cosine_similarity(query_emb, it->second);
        results.push_back({other, sim});
    }

    // Sort by similarity
    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Return top-k
    if (static_cast<int>(results.size()) > top_k) {
        results.resize(top_k);
    }

    return results;
}

std::vector<std::pair<std::string, float>> EmbeddingTrainer::analogy(
    const std::string& a,
    const std::string& b,
    const std::string& c,
    int top_k
) const {
    // Compute: b - a + c
    auto emb_a = get_embedding(a);
    auto emb_b = get_embedding(b);
    auto emb_c = get_embedding(c);

    std::vector<float> target(dim);
    for (int i = 0; i < dim; i++) {
        target[i] = emb_b[i] - emb_a[i] + emb_c[i];
    }

    std::unordered_set<std::string> exclude = {a, b, c};
    return find_nearest(target, top_k, exclude);
}

std::vector<std::pair<std::string, float>> EmbeddingTrainer::find_nearest(
    const std::vector<float>& query_vec,
    int top_k,
    const std::unordered_set<std::string>& exclude
) const {
    std::vector<std::pair<std::string, float>> results;

    for (int i = 0; i < tokenizer.vocab_size(); i++) {
        std::string word = tokenizer.get_word(i);
        if (exclude.count(word)) continue;

        auto it = embeddings.find(i);
        if (it == embeddings.end()) continue;

        float sim = cosine_similarity(query_vec, it->second);
        results.push_back({word, sim});
    }

    std::sort(results.begin(), results.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    if (static_cast<int>(results.size()) > top_k) {
        results.resize(top_k);
    }

    return results;
}

std::vector<float> EmbeddingTrainer::sentence_embedding(const std::string& sentence) const {
    auto tokens = tokenizer.tokenize(sentence);
    std::vector<float> avg(dim, 0.0f);

    int count = 0;
    for (const auto& token : tokens) {
        auto emb = get_embedding(token);
        bool non_zero = false;
        for (float v : emb) {
            if (std::abs(v) > 1e-10f) {
                non_zero = true;
                break;
            }
        }

        if (non_zero) {
            for (int i = 0; i < dim; i++) {
                avg[i] += emb[i];
            }
            count++;
        }
    }

    if (count > 0) {
        for (int i = 0; i < dim; i++) {
            avg[i] /= count;
        }
    }

    return avg;
}

void EmbeddingTrainer::save(const std::string& file_path) const {
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[Embeddings] Error: Cannot save to " << file_path << "\n";
        return;
    }

    // Write header
    int vocab_sz = tokenizer.vocab_size();
    file.write(reinterpret_cast<const char*>(&vocab_sz), sizeof(vocab_sz));
    file.write(reinterpret_cast<const char*>(&dim), sizeof(dim));

    // Write embeddings
    for (int i = 0; i < vocab_sz; i++) {
        std::string word = tokenizer.get_word(i);
        auto it = embeddings.find(i);

        if (it == embeddings.end()) continue;

        // Write word
        int word_len = word.length();
        file.write(reinterpret_cast<const char*>(&word_len), sizeof(word_len));
        file.write(word.data(), word_len);

        // Write embedding
        file.write(reinterpret_cast<const char*>(it->second.data()), dim * sizeof(float));
    }

    std::cout << "[Embeddings] Saved " << vocab_sz << " embeddings to " << file_path << "\n";
}

void EmbeddingTrainer::load(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[Embeddings] Error: Cannot load from " << file_path << "\n";
        return;
    }

    // Read header
    int vocab_sz;
    file.read(reinterpret_cast<char*>(&vocab_sz), sizeof(vocab_sz));
    file.read(reinterpret_cast<char*>(&dim), sizeof(dim));

    // Read embeddings
    for (int i = 0; i < vocab_sz; i++) {
        // Read word
        int word_len;
        file.read(reinterpret_cast<char*>(&word_len), sizeof(word_len));
        std::string word(word_len, '\0');
        file.read(&word[0], word_len);

        // Read embedding
        std::vector<float> emb(dim);
        file.read(reinterpret_cast<char*>(emb.data()), dim * sizeof(float));

        // Store
        int word_id = tokenizer.add_word(word);
        embeddings[word_id] = emb;
    }

    std::cout << "[Embeddings] Loaded " << vocab_sz << " embeddings from " << file_path << "\n";
}
