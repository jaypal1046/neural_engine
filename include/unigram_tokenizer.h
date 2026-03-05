#ifndef UNIGRAM_TOKENIZER_H
#define UNIGRAM_TOKENIZER_H

#include <vector>
#include <string>
#include <unordered_map>
#include <cstdint>
#include <cmath>

// Unigram Language Model Tokenizer
// Algorithm adapted from: SentencePiece (Apache 2.0 License)
// Original: https://github.com/google/sentencepiece
// Paper: "SentencePiece: A simple and language independent approach to subword tokenization" (Kudo & Richardson, 2018)
// Modifications: CPU implementation, zero dependencies

namespace UnigramTokenizer {

// ============================================================================
// Unigram Tokenizer Overview
// ============================================================================

// Unigram vs BPE/WordPiece:
//
// BPE (Byte-Pair Encoding):
// - Greedy: merges most frequent pairs
// - Deterministic but not globally optimal
//
// WordPiece:
// - Similar to BPE, likelihood-based merging
// - Used in BERT
//
// Unigram Language Model:
// - Probabilistic: models P(sequence | vocabulary)
// - Uses Viterbi algorithm (dynamic programming)
// - Globally optimal segmentation
// - Better for morphologically rich languages (Chinese, Japanese, Arabic)
//
// Why Unigram?
// 1. Better multilingual support (80+ languages)
// 2. More flexible than BPE (multiple segmentations possible)
// 3. Compression: 30% better than word-level for Asian languages
// 4. Used in: T5, ALBERT, XLM-R, mBART

// ============================================================================
// Token with Probability
// ============================================================================

struct Token {
    std::string piece;       // Token string (e.g., "▁the", "ing", "世界")
    float log_prob;          // Log probability: log P(piece)
    int id;                  // Token ID (0-vocab_size)

    Token() : piece(""), log_prob(0.0f), id(-1) {}
    Token(const std::string& p, float lp, int i)
        : piece(p), log_prob(lp), id(i) {}
};

// ============================================================================
// Lattice Node (for Viterbi algorithm)
// ============================================================================

struct LatticeNode {
    int pos;                 // Position in input string
    float score;             // Cumulative log probability
    int backtrack_pos;       // Previous position (for reconstruction)
    int token_id;            // Token ID used to reach this node

    LatticeNode()
        : pos(0), score(-std::numeric_limits<float>::infinity()),
          backtrack_pos(-1), token_id(-1) {}
};

// ============================================================================
// Unigram Language Model
// ============================================================================

class UnigramModel {
public:
    UnigramModel();

    // Build vocabulary from training corpus
    // - corpus: Training text (one sentence per line)
    // - vocab_size: Target vocabulary size (e.g., 32000)
    // - character_coverage: Coverage ratio (0.9995 = 99.95% of chars)
    void train(
        const std::vector<std::string>& corpus,
        int vocab_size = 32000,
        float character_coverage = 0.9995f
    );

    // Load pre-trained model from file
    void load(const std::string& model_file);

    // Save model to file
    void save(const std::string& model_file) const;

    // Encode text to token IDs
    std::vector<int> encode(const std::string& text) const;

    // Decode token IDs to text
    std::string decode(const std::vector<int>& ids) const;

    // Get vocabulary
    const std::vector<Token>& get_vocab() const { return vocab_; }

    // Get vocabulary size
    int vocab_size() const { return vocab_.size(); }

private:
    std::vector<Token> vocab_;                           // Vocabulary with probabilities
    std::unordered_map<std::string, int> piece_to_id_;  // Fast lookup: piece → ID

    // Viterbi algorithm: find best tokenization
    std::vector<int> viterbi(const std::string& text) const;

    // Build lattice for Viterbi
    void build_lattice(
        const std::string& text,
        std::vector<LatticeNode>& lattice
    ) const;

    // Initialize vocabulary (character-level + special tokens)
    void initialize_vocab(const std::vector<std::string>& corpus, float character_coverage);

    // EM algorithm: Expectation-Maximization for vocabulary pruning
    void em_algorithm(const std::vector<std::string>& corpus, int target_vocab_size);

    // Compute expected count of each token
    void compute_expected_counts(
        const std::vector<std::string>& corpus,
        std::vector<float>& expected_counts
    ) const;
};

// ============================================================================
// Byte-Level Preprocessing (like GPT-2)
// ============================================================================

// Convert text to UTF-8 bytes
std::vector<uint8_t> text_to_bytes(const std::string& text);

// Convert UTF-8 bytes back to text
std::string bytes_to_text(const std::vector<uint8_t>& bytes);

// ============================================================================
// SentencePiece Normalization
// ============================================================================

// Normalize text (lowercase, NFKC, etc.)
std::string normalize_text(const std::string& text, bool do_lower_case = false);

// Add space prefix (SentencePiece convention: "hello" → "▁hello")
std::string add_space_prefix(const std::string& text);

// Remove space prefix
std::string remove_space_prefix(const std::string& text);

// ============================================================================
// Special Tokens
// ============================================================================

enum SpecialTokens {
    UNK_ID = 0,      // <unk> - unknown token
    BOS_ID = 1,      // <s>   - beginning of sequence
    EOS_ID = 2,      // </s>  - end of sequence
    PAD_ID = 3       // <pad> - padding token
};

// Get special token string
std::string get_special_token(SpecialTokens token);

// ============================================================================
// Tokenizer (High-Level API)
// ============================================================================

class Tokenizer {
public:
    Tokenizer();

    // Train tokenizer on corpus
    void train(
        const std::vector<std::string>& corpus,
        int vocab_size = 32000,
        bool do_lower_case = false,
        float character_coverage = 0.9995f
    );

    // Load from file
    void load(const std::string& model_file);

    // Save to file
    void save(const std::string& model_file) const;

    // Encode text → token IDs
    std::vector<int> encode(
        const std::string& text,
        bool add_bos = false,
        bool add_eos = false
    ) const;

    // Decode token IDs → text
    std::string decode(const std::vector<int>& ids) const;

    // Batch encode (multiple texts)
    std::vector<std::vector<int>> encode_batch(
        const std::vector<std::string>& texts,
        bool add_bos = false,
        bool add_eos = false
    ) const;

    // Batch decode (multiple sequences)
    std::vector<std::string> decode_batch(
        const std::vector<std::vector<int>>& batch_ids
    ) const;

    // Get vocabulary size
    int vocab_size() const { return model_.vocab_size(); }

private:
    UnigramModel model_;
    bool do_lower_case_;
};

// ============================================================================
// Utilities
// ============================================================================

// Count characters in corpus (for character coverage)
std::unordered_map<std::string, int> count_characters(
    const std::vector<std::string>& corpus
);

// Get character coverage (sorted by frequency)
std::vector<std::string> get_character_coverage(
    const std::unordered_map<std::string, int>& char_counts,
    float coverage_ratio
);

// Print tokenizer statistics
void print_tokenizer_stats(const Tokenizer& tokenizer);

// ============================================================================
// Benefits of Unigram Tokenizer
// ============================================================================

// 1. Multilingual Support:
//    - Works well with 80+ languages (Latin, CJK, Arabic, etc.)
//    - Character coverage ensures rare scripts are handled
//
// 2. Better Compression:
//    - English: Similar to BPE (~30% compression)
//    - Chinese: 30% better than word-level (no spaces!)
//    - Japanese: 40% better (Kanji/Hiragana/Katakana mix)
//
// 3. Globally Optimal:
//    - Viterbi algorithm finds best segmentation
//    - BPE is greedy (locally optimal)
//
// 4. Flexible:
//    - Multiple segmentations possible
//    - Probabilistic (can sample different tokenizations)
//
// 5. Used in Production:
//    - T5 (Google): 32k vocab, 100+ languages
//    - ALBERT: 30k vocab, multilingual
//    - XLM-R: 250k vocab, 100 languages
//    - mBART: 250k vocab, 50 languages
//
// Example:
//   Text: "Hello world 世界"
//   BPE:     ["Hello", " world", " ", "世", "界"]  (5 tokens, script mixing)
//   Unigram: ["▁Hello", "▁world", "▁世界"]       (3 tokens, cleaner!)

} // namespace UnigramTokenizer

#endif // UNIGRAM_TOKENIZER_H
