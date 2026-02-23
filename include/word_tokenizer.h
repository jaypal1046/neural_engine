#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <map>

/**
 * Word-Level Tokenizer with BPE (Byte-Pair Encoding) Support
 *
 * Upgrades the character-level tokenizer to word-level for:
 * - Better language modeling (predict whole words, not characters)
 * - Semantic understanding (word embeddings)
 * - Subword fallback for unknown words (BPE)
 *
 * Example:
 *   WordTokenizer tok;
 *   tok.train_from_file("corpus.txt");
 *   auto ids = tok.encode("the cat sat on the mat");
 *   // ids = [4, 23, 156, 8, 4, 782]
 *
 *   auto text = tok.decode(ids);
 *   // text = "the cat sat on the mat"
 */

class WordTokenizer {
public:
    WordTokenizer();

    // Special token IDs
    static const int PAD_ID = 0;
    static const int UNK_ID = 1;
    static const int BOS_ID = 2;  // Beginning of sequence
    static const int EOS_ID = 3;  // End of sequence

    /**
     * Train tokenizer on a corpus
     * Builds vocabulary from words, optionally learns BPE merges
     */
    void train_from_text(const std::string& text, int max_vocab = 50000, bool use_bpe = false);
    void train_from_file(const std::string& file_path, int max_vocab = 50000, bool use_bpe = false);

    /**
     * Encode text to word IDs
     * Unknown words are split into subwords if BPE is available
     */
    std::vector<int> encode(const std::string& text) const;

    /**
     * Decode word IDs back to text
     */
    std::string decode(const std::vector<int>& ids) const;

    /**
     * Tokenize text into words (before encoding)
     */
    std::vector<std::string> tokenize(const std::string& text) const;

    /**
     * Get word ID for a word (returns UNK_ID if not in vocab)
     */
    int get_id(const std::string& word) const;

    /**
     * Get word for an ID
     */
    std::string get_word(int id) const;

    /**
     * Add word to vocabulary
     */
    int add_word(const std::string& word);

    /**
     * Vocabulary statistics
     */
    int vocab_size() const { return static_cast<int>(idx2word.size()); }
    int get_word_freq(const std::string& word) const;
    int total_tokens() const { return total_token_count; }

    /**
     * Save/Load vocabulary and BPE merges
     */
    void save(const std::string& file_path) const;
    void load(const std::string& file_path);

    /**
     * Filter stop words for TF-IDF
     */
    std::vector<std::string> tokenize_no_stopwords(const std::string& text) const;
    bool is_stopword(const std::string& word) const;

private:
    // Vocabulary
    std::unordered_map<std::string, int> word2idx;
    std::vector<std::string> idx2word;
    std::unordered_map<std::string, int> word_freq;
    int total_token_count;

    // BPE (Byte-Pair Encoding) for subword tokenization
    bool bpe_enabled;
    std::vector<std::pair<std::string, std::string>> bpe_merges;  // Ordered merge rules
    std::map<std::pair<std::string, std::string>, int> merge_priorities;

    // Stop words for filtering
    std::unordered_set<std::string> stop_words;

    /**
     * Initialize special tokens and stop words
     */
    void initialize();

    /**
     * Clean and normalize text before tokenization
     */
    std::string normalize_text(const std::string& text) const;

    /**
     * Train BPE merges from word frequencies
     */
    void train_bpe(const std::unordered_map<std::string, int>& word_freqs, int num_merges);

    /**
     * Apply BPE merges to encode an unknown word into subwords
     */
    std::vector<std::string> apply_bpe(const std::string& word) const;

    /**
     * Count bigram frequencies for BPE training
     */
    std::map<std::pair<std::string, std::string>, int> count_bigram_freqs(
        const std::map<std::string, std::vector<std::string>>& word_splits,
        const std::unordered_map<std::string, int>& word_freqs
    ) const;
};
