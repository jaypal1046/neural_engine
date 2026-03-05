// Unigram Language Model Tokenizer Implementation
// Algorithm adapted from: SentencePiece (Apache 2.0 License)
// Original: https://github.com/google/sentencepiece
// Modifications: CPU implementation, zero dependencies

#include "unigram_tokenizer.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cctype>

namespace UnigramTokenizer {

// ============================================================================
// Special Tokens
// ============================================================================

std::string get_special_token(SpecialTokens token) {
    switch (token) {
        case UNK_ID: return "<unk>";
        case BOS_ID: return "<s>";
        case EOS_ID: return "</s>";
        case PAD_ID: return "<pad>";
        default: return "";
    }
}

// ============================================================================
// Text Normalization
// ============================================================================

std::string normalize_text(const std::string& text, bool do_lower_case) {
    std::string result = text;

    // Lowercase if requested
    if (do_lower_case) {
        std::transform(result.begin(), result.end(), result.begin(),
                      [](unsigned char c) { return std::tolower(c); });
    }

    // TODO: Full NFKC normalization (complex Unicode)
    // For now, just basic cleaning

    return result;
}

std::string add_space_prefix(const std::string& text) {
    return "▁" + text;  // U+2581 (lower one eighth block)
}

std::string remove_space_prefix(const std::string& text) {
    if (text.size() >= 3 && text.substr(0, 3) == "▁") {
        return text.substr(3);
    }
    return text;
}

// ============================================================================
// Byte-Level Preprocessing
// ============================================================================

std::vector<uint8_t> text_to_bytes(const std::string& text) {
    return std::vector<uint8_t>(text.begin(), text.end());
}

std::string bytes_to_text(const std::vector<uint8_t>& bytes) {
    return std::string(bytes.begin(), bytes.end());
}

// ============================================================================
// Character Counting
// ============================================================================

std::unordered_map<std::string, int> count_characters(
    const std::vector<std::string>& corpus
) {
    std::unordered_map<std::string, int> counts;

    for (const auto& text : corpus) {
        // Simple: treat each byte as a character
        // TODO: Proper UTF-8 character iteration
        for (char c : text) {
            std::string char_str(1, c);
            counts[char_str]++;
        }
    }

    return counts;
}

std::vector<std::string> get_character_coverage(
    const std::unordered_map<std::string, int>& char_counts,
    float coverage_ratio
) {
    // Sort by frequency
    std::vector<std::pair<std::string, int>> sorted_chars(
        char_counts.begin(), char_counts.end()
    );
    std::sort(sorted_chars.begin(), sorted_chars.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Calculate total count
    int total_count = 0;
    for (const auto& p : sorted_chars) {
        total_count += p.second;
    }

    // Select characters until coverage reached
    std::vector<std::string> result;
    int cumulative_count = 0;
    for (const auto& p : sorted_chars) {
        result.push_back(p.first);
        cumulative_count += p.second;

        float coverage = (float)cumulative_count / total_count;
        if (coverage >= coverage_ratio) {
            break;
        }
    }

    return result;
}

// ============================================================================
// Unigram Model
// ============================================================================

UnigramModel::UnigramModel() {
    // Initialize with special tokens
    vocab_.emplace_back(get_special_token(UNK_ID), 0.0f, UNK_ID);
    vocab_.emplace_back(get_special_token(BOS_ID), 0.0f, BOS_ID);
    vocab_.emplace_back(get_special_token(EOS_ID), 0.0f, EOS_ID);
    vocab_.emplace_back(get_special_token(PAD_ID), 0.0f, PAD_ID);

    for (const auto& token : vocab_) {
        piece_to_id_[token.piece] = token.id;
    }
}

void UnigramModel::train(
    const std::vector<std::string>& corpus,
    int vocab_size,
    float character_coverage
) {
    std::cout << "[TOKENIZER] Training Unigram model..." << std::endl;
    std::cout << "  Corpus size: " << corpus.size() << " sentences" << std::endl;
    std::cout << "  Target vocab size: " << vocab_size << std::endl;

    // 1. Initialize vocabulary (characters + special tokens)
    initialize_vocab(corpus, character_coverage);
    std::cout << "  Initial vocab size: " << vocab_.size() << std::endl;

    // 2. EM algorithm to prune vocabulary
    em_algorithm(corpus, vocab_size);
    std::cout << "  Final vocab size: " << vocab_.size() << std::endl;

    std::cout << "[TOKENIZER] Training complete!" << std::endl;
}

void UnigramModel::initialize_vocab(
    const std::vector<std::string>& corpus,
    float character_coverage
) {
    // Count characters
    auto char_counts = count_characters(corpus);

    // Get characters with coverage
    auto covered_chars = get_character_coverage(char_counts, character_coverage);

    // Add characters to vocabulary
    int next_id = vocab_.size();
    for (const auto& c : covered_chars) {
        if (piece_to_id_.find(c) == piece_to_id_.end()) {
            vocab_.emplace_back(c, -10.0f, next_id);  // Initial log prob
            piece_to_id_[c] = next_id;
            next_id++;
        }
    }

    // TODO: Add common substrings (bigrams, trigrams, etc.)
    // For simplicity, starting with character-level vocabulary
}

void UnigramModel::em_algorithm(
    const std::vector<std::string>& corpus,
    int target_vocab_size
) {
    // Simplified EM: Just keep top-k tokens by frequency
    // Full EM would iteratively prune and re-estimate probabilities

    // For now, vocabulary is already at character level
    // In production, this would:
    // 1. Compute expected counts using current vocabulary
    // 2. Prune lowest probability tokens
    // 3. Re-estimate probabilities
    // 4. Repeat until target vocab size

    // Placeholder: already at reasonable size for character-level
    if ((int)vocab_.size() > target_vocab_size) {
        vocab_.resize(target_vocab_size);

        // Rebuild piece_to_id map
        piece_to_id_.clear();
        for (const auto& token : vocab_) {
            piece_to_id_[token.piece] = token.id;
        }
    }
}

void UnigramModel::compute_expected_counts(
    const std::vector<std::string>& corpus,
    std::vector<float>& expected_counts
) const {
    expected_counts.assign(vocab_.size(), 0.0f);

    // For each sentence, run Viterbi and count tokens
    for (const auto& text : corpus) {
        auto token_ids = viterbi(text);
        for (int id : token_ids) {
            if (id >= 0 && id < (int)expected_counts.size()) {
                expected_counts[id] += 1.0f;
            }
        }
    }
}

std::vector<int> UnigramModel::encode(const std::string& text) const {
    if (text.empty()) return {};
    return viterbi(text);
}

std::string UnigramModel::decode(const std::vector<int>& ids) const {
    std::string result;
    for (int id : ids) {
        if (id >= 0 && id < (int)vocab_.size()) {
            result += vocab_[id].piece;
        }
    }
    return result;
}

std::vector<int> UnigramModel::viterbi(const std::string& text) const {
    if (text.empty()) return {};

    // Build lattice
    std::vector<LatticeNode> lattice(text.size() + 1);
    build_lattice(text, lattice);

    // Backtrack to get best path
    std::vector<int> result;
    int pos = text.size();
    while (pos > 0) {
        if (lattice[pos].backtrack_pos < 0) break;
        result.push_back(lattice[pos].token_id);
        pos = lattice[pos].backtrack_pos;
    }

    std::reverse(result.begin(), result.end());
    return result;
}

void UnigramModel::build_lattice(
    const std::string& text,
    std::vector<LatticeNode>& lattice
) const {
    int n = text.size();

    // Initialize
    lattice[0].pos = 0;
    lattice[0].score = 0.0f;
    lattice[0].backtrack_pos = -1;

    // Forward pass (Viterbi dynamic programming)
    for (int i = 0; i < n; i++) {
        if (std::isinf(lattice[i].score) && lattice[i].score < 0) {
            continue;  // Unreachable state
        }

        // Try all possible tokens starting at position i
        for (const auto& token : vocab_) {
            const std::string& piece = token.piece;
            int piece_len = piece.size();

            // Check if this piece matches at position i
            if (i + piece_len <= n && text.substr(i, piece_len) == piece) {
                int next_pos = i + piece_len;
                float new_score = lattice[i].score + token.log_prob;

                // Update if better path found
                if (new_score > lattice[next_pos].score) {
                    lattice[next_pos].pos = next_pos;
                    lattice[next_pos].score = new_score;
                    lattice[next_pos].backtrack_pos = i;
                    lattice[next_pos].token_id = token.id;
                }
            }
        }
    }
}

void UnigramModel::save(const std::string& model_file) const {
    std::ofstream out(model_file);
    if (!out) {
        std::cerr << "[ERROR] Cannot write model file: " << model_file << std::endl;
        return;
    }

    // Write vocab size
    out << vocab_.size() << "\n";

    // Write each token
    for (const auto& token : vocab_) {
        out << token.id << "\t" << token.log_prob << "\t" << token.piece << "\n";
    }

    out.close();
    std::cout << "[TOKENIZER] Model saved to: " << model_file << std::endl;
}

void UnigramModel::load(const std::string& model_file) {
    std::ifstream in(model_file);
    if (!in) {
        std::cerr << "[ERROR] Cannot read model file: " << model_file << std::endl;
        return;
    }

    vocab_.clear();
    piece_to_id_.clear();

    int vocab_size;
    in >> vocab_size;

    for (int i = 0; i < vocab_size; i++) {
        int id;
        float log_prob;
        std::string piece;
        in >> id >> log_prob;
        in.ignore();  // Skip tab
        std::getline(in, piece);

        vocab_.emplace_back(piece, log_prob, id);
        piece_to_id_[piece] = id;
    }

    in.close();
    std::cout << "[TOKENIZER] Model loaded from: " << model_file << std::endl;
    std::cout << "  Vocab size: " << vocab_.size() << std::endl;
}

// ============================================================================
// Tokenizer (High-Level API)
// ============================================================================

Tokenizer::Tokenizer() : do_lower_case_(false) {}

void Tokenizer::train(
    const std::vector<std::string>& corpus,
    int vocab_size,
    bool do_lower_case,
    float character_coverage
) {
    do_lower_case_ = do_lower_case;

    // Normalize corpus
    std::vector<std::string> normalized_corpus;
    for (const auto& text : corpus) {
        normalized_corpus.push_back(normalize_text(text, do_lower_case_));
    }

    // Train model
    model_.train(normalized_corpus, vocab_size, character_coverage);
}

void Tokenizer::load(const std::string& model_file) {
    model_.load(model_file);
}

void Tokenizer::save(const std::string& model_file) const {
    model_.save(model_file);
}

std::vector<int> Tokenizer::encode(
    const std::string& text,
    bool add_bos,
    bool add_eos
) const {
    // Normalize
    std::string normalized = normalize_text(text, do_lower_case_);

    // Encode
    std::vector<int> ids = model_.encode(normalized);

    // Add special tokens
    if (add_bos) {
        ids.insert(ids.begin(), BOS_ID);
    }
    if (add_eos) {
        ids.push_back(EOS_ID);
    }

    return ids;
}

std::string Tokenizer::decode(const std::vector<int>& ids) const {
    // Filter out special tokens
    std::vector<int> filtered_ids;
    for (int id : ids) {
        if (id != BOS_ID && id != EOS_ID && id != PAD_ID) {
            filtered_ids.push_back(id);
        }
    }

    return model_.decode(filtered_ids);
}

std::vector<std::vector<int>> Tokenizer::encode_batch(
    const std::vector<std::string>& texts,
    bool add_bos,
    bool add_eos
) const {
    std::vector<std::vector<int>> results;
    for (const auto& text : texts) {
        results.push_back(encode(text, add_bos, add_eos));
    }
    return results;
}

std::vector<std::string> Tokenizer::decode_batch(
    const std::vector<std::vector<int>>& batch_ids
) const {
    std::vector<std::string> results;
    for (const auto& ids : batch_ids) {
        results.push_back(decode(ids));
    }
    return results;
}

// ============================================================================
// Utilities
// ============================================================================

void print_tokenizer_stats(const Tokenizer& tokenizer) {
    std::cout << "\n=== Tokenizer Statistics ===" << std::endl;
    std::cout << "Vocabulary size: " << tokenizer.vocab_size() << std::endl;
    std::cout << "============================" << std::endl;
}

} // namespace UnigramTokenizer
