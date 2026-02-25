#include "bpe_tokenizer.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <sstream>

BPETokenizer::BPETokenizer(int vocab_size)
    : vocab_size_(vocab_size), pad_token_id_(0), unk_token_id_(1),
      bos_token_id_(2), eos_token_id_(3) {

    // Initialize special tokens
    add_token("<PAD>");
    add_token("<UNK>");
    add_token("<BOS>");
    add_token("<EOS>");

    // Add all single bytes as base tokens (256 tokens)
    for (int i = 0; i < 256; i++) {
        std::string byte_token = std::string(1, (char)i);
        add_token(byte_token);
    }
}

void BPETokenizer::add_token(const std::string& token) {
    int id = token_to_id_.size();
    token_to_id_[token] = id;
    id_to_token_[id] = token;
}

std::vector<std::string> BPETokenizer::byte_encode(const std::string& text) const {
    std::vector<std::string> result;
    for (char c : text) {
        result.push_back(std::string(1, c));
    }
    return result;
}

void BPETokenizer::train(const std::vector<std::string>& texts, int num_merges) {
    std::cerr << "[BPE] Training tokenizer on " << texts.size() << " texts...\n";

    // Start with byte-level encoding
    std::vector<std::vector<std::string>> token_sequences;
    for (const auto& text : texts) {
        token_sequences.push_back(byte_encode(text));
    }

    // Learn BPE merges
    for (int merge_idx = 0; merge_idx < num_merges && token_to_id_.size() < vocab_size_; merge_idx++) {
        // Count all adjacent pairs
        auto pair_counts = count_pairs(token_sequences);

        if (pair_counts.empty()) break;

        // Find most frequent pair
        std::string best_pair;
        int max_count = 0;
        for (const auto& [pair, count] : pair_counts) {
            if (count > max_count) {
                max_count = count;
                best_pair = pair;
            }
        }

        if (max_count < 2) break;  // Stop if no pair occurs more than once

        // Split pair into tokens
        size_t space_pos = best_pair.find(' ');
        std::string token1 = best_pair.substr(0, space_pos);
        std::string token2 = best_pair.substr(space_pos + 1);
        std::string merged = token1 + token2;

        // Add merged token to vocabulary
        add_token(merged);

        // Record merge rule
        Merge merge;
        merge.pair = best_pair;
        merge.merged = merged;
        merge.priority = merge_idx;
        merges_.push_back(merge);

        // Apply merge to all sequences
        for (auto& seq : token_sequences) {
            std::vector<std::string> new_seq;
            for (size_t i = 0; i < seq.size(); i++) {
                if (i + 1 < seq.size() && seq[i] == token1 && seq[i+1] == token2) {
                    new_seq.push_back(merged);
                    i++;  // Skip next token
                } else {
                    new_seq.push_back(seq[i]);
                }
            }
            seq = new_seq;
        }

        if ((merge_idx + 1) % 1000 == 0) {
            std::cerr << "[BPE] Merge " << (merge_idx + 1) << "/" << num_merges
                      << " | Vocab: " << token_to_id_.size() << "\n";
        }
    }

    std::cerr << "[BPE] Training complete. Vocabulary size: " << token_to_id_.size() << "\n";
}

std::unordered_map<std::string, int> BPETokenizer::count_pairs(
    const std::vector<std::vector<std::string>>& token_sequences
) const {
    std::unordered_map<std::string, int> pair_counts;

    for (const auto& seq : token_sequences) {
        for (size_t i = 0; i + 1 < seq.size(); i++) {
            std::string pair = seq[i] + " " + seq[i+1];
            pair_counts[pair]++;
        }
    }

    return pair_counts;
}

std::vector<std::string> BPETokenizer::apply_merges(const std::vector<std::string>& tokens) const {
    std::vector<std::string> result = tokens;

    // Apply merges in order of priority
    for (const auto& merge : merges_) {
        size_t space_pos = merge.pair.find(' ');
        std::string token1 = merge.pair.substr(0, space_pos);
        std::string token2 = merge.pair.substr(space_pos + 1);

        std::vector<std::string> new_result;
        for (size_t i = 0; i < result.size(); i++) {
            if (i + 1 < result.size() && result[i] == token1 && result[i+1] == token2) {
                new_result.push_back(merge.merged);
                i++;  // Skip next token
            } else {
                new_result.push_back(result[i]);
            }
        }
        result = new_result;
    }

    return result;
}

std::vector<int> BPETokenizer::encode(const std::string& text) const {
    // Start with byte-level encoding
    auto tokens = byte_encode(text);

    // Apply learned merges
    tokens = apply_merges(tokens);

    // Convert tokens to IDs
    std::vector<int> token_ids;
    for (const auto& token : tokens) {
        auto it = token_to_id_.find(token);
        if (it != token_to_id_.end()) {
            token_ids.push_back(it->second);
        } else {
            token_ids.push_back(unk_token_id_);  // Unknown token
        }
    }

    return token_ids;
}

std::string BPETokenizer::decode(const std::vector<int>& tokens) const {
    std::string result;
    for (int token_id : tokens) {
        // Skip special tokens
        if (token_id == pad_token_id_ || token_id == bos_token_id_ || token_id == eos_token_id_) {
            continue;
        }

        auto it = id_to_token_.find(token_id);
        if (it != id_to_token_.end()) {
            result += it->second;
        } else {
            result += "<UNK>";
        }
    }
    return result;
}

std::string BPETokenizer::token_to_string(int token_id) const {
    auto it = id_to_token_.find(token_id);
    return (it != id_to_token_.end()) ? it->second : "<UNK>";
}

int BPETokenizer::string_to_token(const std::string& token) const {
    auto it = token_to_id_.find(token);
    return (it != token_to_id_.end()) ? it->second : unk_token_id_;
}

void BPETokenizer::save(const std::string& path) const {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[BPE] Error: Cannot save to " << path << "\n";
        return;
    }

    // Write vocab size
    int vocab_size = token_to_id_.size();
    file.write(reinterpret_cast<const char*>(&vocab_size), sizeof(vocab_size));

    // Write tokens
    for (const auto& [token, id] : token_to_id_) {
        int token_len = token.size();
        file.write(reinterpret_cast<const char*>(&token_len), sizeof(token_len));
        file.write(token.data(), token_len);
        file.write(reinterpret_cast<const char*>(&id), sizeof(id));
    }

    // Write merges
    int num_merges = merges_.size();
    file.write(reinterpret_cast<const char*>(&num_merges), sizeof(num_merges));
    for (const auto& merge : merges_) {
        int pair_len = merge.pair.size();
        file.write(reinterpret_cast<const char*>(&pair_len), sizeof(pair_len));
        file.write(merge.pair.data(), pair_len);

        int merged_len = merge.merged.size();
        file.write(reinterpret_cast<const char*>(&merged_len), sizeof(merged_len));
        file.write(merge.merged.data(), merged_len);

        file.write(reinterpret_cast<const char*>(&merge.priority), sizeof(merge.priority));
    }

    std::cerr << "[BPE] Saved tokenizer to " << path << "\n";
}

void BPETokenizer::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[BPE] Error: Cannot load from " << path << "\n";
        return;
    }

    token_to_id_.clear();
    id_to_token_.clear();
    merges_.clear();

    // Read vocab size
    int vocab_size;
    file.read(reinterpret_cast<char*>(&vocab_size), sizeof(vocab_size));

    // Read tokens
    for (int i = 0; i < vocab_size; i++) {
        int token_len;
        file.read(reinterpret_cast<char*>(&token_len), sizeof(token_len));

        std::string token(token_len, '\0');
        file.read(&token[0], token_len);

        int id;
        file.read(reinterpret_cast<char*>(&id), sizeof(id));

        token_to_id_[token] = id;
        id_to_token_[id] = token;
    }

    // Read merges
    int num_merges;
    file.read(reinterpret_cast<char*>(&num_merges), sizeof(num_merges));
    for (int i = 0; i < num_merges; i++) {
        Merge merge;

        int pair_len;
        file.read(reinterpret_cast<char*>(&pair_len), sizeof(pair_len));
        merge.pair.resize(pair_len);
        file.read(&merge.pair[0], pair_len);

        int merged_len;
        file.read(reinterpret_cast<char*>(&merged_len), sizeof(merged_len));
        merge.merged.resize(merged_len);
        file.read(&merge.merged[0], merged_len);

        file.read(reinterpret_cast<char*>(&merge.priority), sizeof(merge.priority));

        merges_.push_back(merge);
    }

    std::cerr << "[BPE] Loaded tokenizer from " << path
              << " (vocab: " << token_to_id_.size() << ", merges: " << merges_.size() << ")\n";
}
