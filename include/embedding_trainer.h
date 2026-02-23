#pragma once

#include "word_tokenizer.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <random>

/**
 * Embedding Trainer - Train word2vec-style semantic embeddings
 *
 * Implements Skip-Gram with Negative Sampling (Mikolov et al. 2013)
 *
 * How it works:
 * 1. For each word, predict its context (nearby words)
 * 2. Positive samples: actual context words → pull embeddings closer
 * 3. Negative samples: random words → push embeddings apart
 * 4. Update embeddings via gradient descent
 *
 * Result: Words with similar meanings have similar embeddings
 *
 * Example:
 *   EmbeddingTrainer trainer(64);  // 64-dim embeddings
 *   trainer.train_from_file("corpus.txt", epochs=3);
 *   trainer.save("embeddings.bin");
 *
 *   float sim = trainer.similarity("compress", "shrink");
 *   // sim ≈ 0.87 (high similarity!)
 */

class EmbeddingTrainer {
public:
    /**
     * Create embedding trainer
     * @param dim Embedding dimension (default 64)
     * @param window Context window size (default 5)
     * @param neg_samples Number of negative samples per positive (default 5)
     * @param min_count Minimum word frequency to include (default 5)
     */
    EmbeddingTrainer(
        int dim = 64,
        int window = 5,
        int neg_samples = 5,
        int min_count = 5
    );

    /**
     * Train on sentences
     * @param sentences Vector of sentences
     * @param epochs Number of training epochs (default 3)
     * @param learning_rate Initial learning rate (default 0.025)
     */
    void train(
        const std::vector<std::string>& sentences,
        int epochs = 3,
        float learning_rate = 0.025f
    );

    /**
     * Train from text file
     * Automatically handles tokenization and batching
     */
    void train_from_file(
        const std::string& file_path,
        int epochs = 3,
        float learning_rate = 0.025f
    );

    /**
     * Get embedding for a word
     * @return 64-dim vector, or zeros if word not in vocabulary
     */
    std::vector<float> get_embedding(const std::string& word) const;

    /**
     * Compute cosine similarity between two words
     * @return Similarity in [0, 1] (1 = identical, 0 = orthogonal)
     */
    float similarity(const std::string& word1, const std::string& word2) const;

    /**
     * Find most similar words to a given word
     * @param word Query word
     * @param top_k Number of results (default 10)
     * @return Vector of (word, similarity) pairs, sorted by similarity
     */
    std::vector<std::pair<std::string, float>> most_similar(
        const std::string& word,
        int top_k = 10
    ) const;

    /**
     * Word analogy: a is to b as c is to ?
     * Example: king - man + woman ≈ queen
     * @return Top candidates with similarity scores
     */
    std::vector<std::pair<std::string, float>> analogy(
        const std::string& a,
        const std::string& b,
        const std::string& c,
        int top_k = 5
    ) const;

    /**
     * Find nearest neighbors to a given vector
     * Used for analogy and custom queries
     */
    std::vector<std::pair<std::string, float>> find_nearest(
        const std::vector<float>& query_vec,
        int top_k = 10,
        const std::unordered_set<std::string>& exclude = {}
    ) const;

    /**
     * Get sentence embedding (average of word embeddings)
     */
    std::vector<float> sentence_embedding(const std::string& sentence) const;

    /**
     * Save trained embeddings to binary file
     * Format: [vocab_size][dim][word_len word_str embedding_vec]...
     */
    void save(const std::string& file_path) const;

    /**
     * Load trained embeddings from binary file
     */
    void load(const std::string& file_path);

    /**
     * Get vocabulary size
     */
    int vocab_size() const { return tokenizer.vocab_size(); }

    /**
     * Get embedding dimension
     */
    int embedding_dim() const { return dim; }

    /**
     * Training statistics
     */
    struct TrainingStats {
        int total_words_trained = 0;
        int total_sentences = 0;
        int epochs_completed = 0;
        double training_time_sec = 0.0;
        float final_learning_rate = 0.0f;
    };

    TrainingStats get_stats() const { return stats; }

private:
    // Hyperparameters
    int dim;
    int window;
    int neg_samples;
    int min_count;

    // Model data
    WordTokenizer tokenizer;
    std::unordered_map<int, std::vector<float>> embeddings;  // word_id → embedding
    std::unordered_map<int, std::vector<float>> context_embeddings;  // separate for skip-gram

    // Negative sampling table (for fast sampling)
    std::vector<int> neg_table;
    const int NEG_TABLE_SIZE = 100000000;  // 100M entries

    // Training stats
    TrainingStats stats;

    // Random number generator
    std::mt19937 rng;

    /**
     * Initialize embeddings with random values
     */
    void init_embeddings();

    /**
     * Build negative sampling table
     * Words sampled with probability ~ freq^0.75
     */
    void build_neg_table();

    /**
     * Sample a negative word
     */
    int sample_negative();

    /**
     * Train on a single word (skip-gram)
     * @param target_word Center word
     * @param context_words Nearby words
     * @param lr Learning rate
     */
    void train_word(
        int target_word,
        const std::vector<int>& context_words,
        float lr
    );

    /**
     * Update embeddings for one positive sample
     */
    void update_positive(int target, int context, float lr);

    /**
     * Update embeddings for one negative sample
     */
    void update_negative(int target, int negative, float lr);

    /**
     * Sigmoid function
     */
    static float sigmoid(float x) {
        return 1.0f / (1.0f + std::exp(-x));
    }

    /**
     * Dot product of two vectors
     */
    static float dot_product(const std::vector<float>& a, const std::vector<float>& b);

    /**
     * Cosine similarity of two vectors
     */
    static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b);

    /**
     * Normalize vector (L2 normalization)
     */
    static void normalize(std::vector<float>& vec);
};
