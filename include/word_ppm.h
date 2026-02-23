#pragma once

#include "word_tokenizer.h"
#include <map>
#include <vector>
#include <string>
#include <memory>

/**
 * Word-Level PPM (Prediction by Partial Matching) Language Model
 *
 * Unlike character-level PPM (used in compression), this predicts WORDS.
 *
 * How it works:
 * - Builds N-gram contexts (order 1 to 5)
 * - Predicts next word based on previous words
 * - Uses backoff for unseen contexts
 * - Combines predictions from multiple orders
 *
 * Example:
 *   WordPPM model(order=3);
 *   model.train("the cat sat on the mat");
 *   auto predictions = model.predict({"the", "cat"});
 *   // predictions = [("sat", 0.7), ("is", 0.2), ...]
 *
 * This is MUCH smarter than character-level for language generation:
 * - Character: "the cat s**a**t" (predicts next letter)
 * - Word: "the cat **sat**" (predicts next word)
 */

class WordPPM {
public:
    /**
     * Create word-level PPM model
     * @param order Maximum context length (default 5 = 5-gram)
     * @param alpha Smoothing parameter (Laplace smoothing)
     */
    explicit WordPPM(int order = 5, float alpha = 0.1f);

    /**
     * Train on text corpus
     */
    void train_from_text(const std::string& text, WordTokenizer& tokenizer);
    void train_from_file(const std::string& file_path, WordTokenizer& tokenizer);
    void train_from_tokens(const std::vector<int>& token_ids);

    /**
     * Predict next word given context
     * @param context Previous words (e.g., {"the", "cat", "sat"})
     * @param tokenizer Tokenizer to convert words to IDs
     * @param top_k Return top-k predictions
     * @return Vector of (word, probability) pairs
     */
    std::vector<std::pair<std::string, float>> predict(
        const std::vector<std::string>& context,
        const WordTokenizer& tokenizer,
        int top_k = 10
    ) const;

    /**
     * Predict next word ID given context IDs
     * @param context_ids Previous word IDs
     * @param top_k Return top-k predictions
     * @return Vector of (word_id, probability) pairs
     */
    std::vector<std::pair<int, float>> predict_id(
        const std::vector<int>& context_ids,
        int top_k = 10
    ) const;

    /**
     * Generate text continuation
     * @param prompt Starting text
     * @param tokenizer Tokenizer
     * @param num_words Number of words to generate
     * @param temperature Sampling temperature (0.0 = greedy, 1.0 = random)
     * @return Generated text
     */
    std::string generate(
        const std::string& prompt,
        WordTokenizer& tokenizer,
        int num_words = 30,
        float temperature = 0.7f
    );

    /**
     * Compute perplexity on test text (lower = better)
     */
    float compute_perplexity(
        const std::string& text,
        WordTokenizer& tokenizer
    ) const;

    /**
     * Model statistics
     */
    int get_order() const { return order; }
    size_t get_context_count() const;
    size_t get_total_ngrams() const { return total_ngrams; }

    /**
     * Save/Load model
     */
    void save(const std::string& file_path) const;
    void load(const std::string& file_path);

private:
    int order;          // Maximum N-gram order
    float alpha;        // Smoothing parameter
    size_t total_ngrams;

    /**
     * N-gram context storage
     * contexts[order][context_key] = {word_id: count}
     *
     * Example for order=3, context="the cat":
     * contexts[2]["the cat"] = {sat: 5, is: 2, was: 1}
     */
    std::vector<std::map<std::string, std::map<int, int>>> contexts;

    /**
     * Total counts for each context (for normalization)
     */
    std::vector<std::map<std::string, int>> context_totals;

    /**
     * Build context key from word IDs
     */
    std::string build_context_key(const std::vector<int>& ids, int start, int len) const;

    /**
     * Get probability distribution for a context at given order
     * Returns empty map if context not found
     */
    std::map<int, float> get_distribution(const std::vector<int>& context_ids, int ctx_order) const;

    /**
     * Combine distributions from multiple orders using backoff
     */
    std::map<int, float> combine_distributions(const std::vector<int>& context_ids) const;

    /**
     * Sample word ID from probability distribution
     */
    int sample_from_distribution(const std::map<int, float>& dist, float temperature) const;
};
