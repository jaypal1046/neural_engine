#ifndef REAL_EMBEDDINGS_H
#define REAL_EMBEDDINGS_H

#include "bpe_tokenizer.h"
#include <vector>
#include <string>
#include <random>
#include <memory>

// Real Word2Vec Embeddings with Negative Sampling
// Skip-Gram architecture: predicts context from center word
// Provides TRUE semantic understanding (car ≈ automobile)

class RealEmbeddings {
public:
    RealEmbeddings(int embedding_dim = 128, int vocab_size = 32000);

    // Training
    void train(
        const std::vector<std::string>& texts,
        BPETokenizer& tokenizer,
        int epochs = 5,
        float learning_rate = 0.025f,
        int window_size = 5,
        int negative_samples = 5
    );

    // Get embedding for a token
    std::vector<float> get_embedding(int token_id) const;

    // Get embedding for text (average of token embeddings)
    std::vector<float> encode_text(const std::string& text, const BPETokenizer& tokenizer) const;

    // Semantic similarity
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const;
    std::vector<std::pair<int, float>> find_similar(int token_id, int top_k = 10) const;

    // Persistence
    void save(const std::string& path) const;
    void load(const std::string& path);

    int embedding_dim() const { return embedding_dim_; }
    int vocab_size() const { return vocab_size_; }

private:
    int embedding_dim_;
    int vocab_size_;

    // Embedding matrices
    std::vector<std::vector<float>> embeddings_;  // [vocab_size, embedding_dim]
    std::vector<std::vector<float>> context_embeddings_;  // [vocab_size, embedding_dim] for output

    // Negative sampling
    std::vector<int> unigram_table_;  // Pre-computed sampling table
    std::mt19937 rng_;

    // Helper functions
    void initialize_embeddings();
    void build_unigram_table(const std::vector<int>& token_counts);
    int sample_negative() const;
    float sigmoid(float x) const;
};

#endif // REAL_EMBEDDINGS_H
