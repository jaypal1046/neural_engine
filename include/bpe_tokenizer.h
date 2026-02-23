#ifndef BPE_TOKENIZER_H
#define BPE_TOKENIZER_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// Byte Pair Encoding Tokenizer (like GPT-2/GPT-3)
// Learns merge rules from training data to build optimal vocabulary
// Handles subword tokens for better generalization

class BPETokenizer {
public:
    BPETokenizer(int vocab_size = 32000);

    // Training: Learn BPE merges from corpus
    void train(const std::vector<std::string>& texts, int num_merges);

    // Encoding: Text -> Token IDs
    std::vector<int> encode(const std::string& text) const;

    // Decoding: Token IDs -> Text
    std::string decode(const std::vector<int>& tokens) const;

    // Vocabulary
    int vocab_size() const { return token_to_id_.size(); }
    std::string token_to_string(int token_id) const;
    int string_to_token(const std::string& token) const;

    // Persistence
    void save(const std::string& path) const;
    void load(const std::string& path);

private:
    struct Merge {
        std::string pair;
        std::string merged;
        int priority;
    };

    int vocab_size_;
    std::unordered_map<std::string, int> token_to_id_;
    std::unordered_map<int, std::string> id_to_token_;
    std::vector<Merge> merges_;

    // Special tokens
    int pad_token_id_;
    int unk_token_id_;
    int bos_token_id_;  // Begin of sequence
    int eos_token_id_;  // End of sequence

    // Helper functions
    std::vector<std::string> byte_encode(const std::string& text) const;
    std::vector<std::string> apply_merges(const std::vector<std::string>& tokens) const;
    void add_token(const std::string& token);

    // Statistics for BPE training
    std::unordered_map<std::string, int> count_pairs(
        const std::vector<std::vector<std::string>>& token_sequences
    ) const;
};

#endif // BPE_TOKENIZER_H
