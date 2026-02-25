#include "word_tokenizer.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <queue>

// Define static constants
const int WordTokenizer::PAD_ID;
const int WordTokenizer::UNK_ID;
const int WordTokenizer::BOS_ID;
const int WordTokenizer::EOS_ID;

WordTokenizer::WordTokenizer() : total_token_count(0), bpe_enabled(false) {
    initialize();
}

void WordTokenizer::initialize() {
    // Add special tokens
    idx2word = {"<pad>", "<unk>", "<s>", "</s>"};
    word2idx["<pad>"] = PAD_ID;
    word2idx["<unk>"] = UNK_ID;
    word2idx["<s>"] = BOS_ID;
    word2idx["</s>"] = EOS_ID;

    // Common English stop words
    const char* sw[] = {
        "a", "about", "above", "after", "again", "against", "all", "am", "an", "and",
        "any", "are", "aren't", "as", "at", "be", "because", "been", "before", "being",
        "below", "between", "both", "but", "by", "can", "can't", "cannot", "could",
        "couldn't", "did", "didn't", "do", "does", "doesn't", "doing", "don't", "down",
        "during", "each", "few", "for", "from", "further", "had", "hadn't", "has",
        "hasn't", "have", "haven't", "having", "he", "he'd", "he'll", "he's", "her",
        "here", "here's", "hers", "herself", "him", "himself", "his", "how", "how's",
        "i", "i'd", "i'll", "i'm", "i've", "if", "in", "into", "is", "isn't", "it",
        "it's", "its", "itself", "let's", "me", "more", "most", "mustn't", "my",
        "myself", "no", "nor", "not", "of", "off", "on", "once", "only", "or", "other",
        "ought", "our", "ours", "ourselves", "out", "over", "own", "same", "shan't",
        "she", "she'd", "she'll", "she's", "should", "shouldn't", "so", "some", "such",
        "than", "that", "that's", "the", "their", "theirs", "them", "themselves",
        "then", "there", "there's", "these", "they", "they'd", "they'll", "they're",
        "they've", "this", "those", "through", "to", "too", "under", "until", "up",
        "very", "was", "wasn't", "we", "we'd", "we'll", "we're", "we've", "were",
        "weren't", "what", "what's", "when", "when's", "where", "where's", "which",
        "while", "who", "who's", "whom", "why", "why's", "with", "won't", "would",
        "wouldn't", "you", "you'd", "you'll", "you're", "you've", "your", "yours",
        "yourself", "yourselves", nullptr
    };

    for (int i = 0; sw[i]; i++) {
        stop_words.insert(sw[i]);
    }
}

std::string WordTokenizer::normalize_text(const std::string& text) const {
    std::string result;
    result.reserve(text.size());

    for (char c : text) {
        // Convert to lowercase
        if (c >= 'A' && c <= 'Z') {
            result += (c + 32);
        }
        // Keep letters, numbers, spaces, and apostrophes
        else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ' || c == '\'') {
            result += c;
        }
        // Replace other characters with spaces
        else {
            result += ' ';
        }
    }

    return result;
}

std::vector<std::string> WordTokenizer::tokenize(const std::string& text) const {
    std::string normalized = normalize_text(text);
    std::vector<std::string> tokens;
    std::istringstream iss(normalized);
    std::string word;

    while (iss >> word) {
        // Skip very short words and pure numbers
        if (word.length() > 1 || (word.length() == 1 && isalpha(word[0]))) {
            tokens.push_back(word);
        }
    }

    return tokens;
}

std::vector<std::string> WordTokenizer::tokenize_no_stopwords(const std::string& text) const {
    auto tokens = tokenize(text);
    std::vector<std::string> filtered;

    for (const auto& token : tokens) {
        if (!is_stopword(token)) {
            filtered.push_back(token);
        }
    }

    return filtered;
}

bool WordTokenizer::is_stopword(const std::string& word) const {
    return stop_words.count(word) > 0;
}

int WordTokenizer::add_word(const std::string& word) {
    auto it = word2idx.find(word);
    if (it != word2idx.end()) {
        return it->second;
    }

    int id = static_cast<int>(idx2word.size());
    word2idx[word] = id;
    idx2word.push_back(word);
    return id;
}

void WordTokenizer::train_from_text(const std::string& text, int max_vocab, bool use_bpe) {
    auto tokens = tokenize(text);

    // Count word frequencies
    std::unordered_map<std::string, int> word_freqs;
    for (const auto& token : tokens) {
        word_freqs[token]++;
        total_token_count++;
    }

    // Sort by frequency
    std::vector<std::pair<std::string, int>> sorted_words(word_freqs.begin(), word_freqs.end());
    std::sort(sorted_words.begin(), sorted_words.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Add top words to vocabulary
    int vocab_limit = std::min(max_vocab - 4, static_cast<int>(sorted_words.size()));  // -4 for special tokens
    for (int i = 0; i < vocab_limit; i++) {
        add_word(sorted_words[i].first);
        word_freq[sorted_words[i].first] = sorted_words[i].second;
    }

    // Train BPE for remaining words if requested
    if (use_bpe && static_cast<int>(sorted_words.size()) > vocab_limit) {
        std::unordered_map<std::string, int> remaining_words;
        for (int i = vocab_limit; i < static_cast<int>(sorted_words.size()); i++) {
            remaining_words[sorted_words[i].first] = sorted_words[i].second;
        }
        train_bpe(remaining_words, 5000);  // Learn 5000 merge rules
        bpe_enabled = true;
    }

    std::cout << "[WordTokenizer] Trained on " << total_token_count << " tokens\n";
    std::cout << "[WordTokenizer] Vocabulary size: " << vocab_size() << "\n";
    if (bpe_enabled) {
        std::cout << "[WordTokenizer] BPE merges: " << bpe_merges.size() << "\n";
    }
}

void WordTokenizer::train_from_file(const std::string& file_path, int max_vocab, bool use_bpe) {
    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "[WordTokenizer] Error: Cannot open file " << file_path << "\n";
        return;
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    train_from_text(text, max_vocab, use_bpe);
}

void WordTokenizer::train_bpe(const std::unordered_map<std::string, int>& word_freqs, int num_merges) {
    // Initialize: split each word into characters
    std::map<std::string, std::vector<std::string>> word_splits;
    for (const auto& [word, freq] : word_freqs) {
        std::vector<std::string> chars;
        for (char c : word) {
            chars.push_back(std::string(1, c));
        }
        word_splits[word] = chars;
    }

    // Learn merges
    for (int merge_num = 0; merge_num < num_merges; merge_num++) {
        // Count bigram frequencies
        auto bigram_freqs = count_bigram_freqs(word_splits, word_freqs);
        if (bigram_freqs.empty()) break;

        // Find most frequent bigram
        auto max_bigram = std::max_element(
            bigram_freqs.begin(), bigram_freqs.end(),
            [](const auto& a, const auto& b) { return a.second < b.second; }
        );

        auto [left, right] = max_bigram->first;
        std::string merged = left + right;

        // Record merge rule
        bpe_merges.push_back({left, right});
        merge_priorities[{left, right}] = merge_num;

        // Apply merge to all word splits
        for (auto& [word, splits] : word_splits) {
            std::vector<std::string> new_splits;
            for (size_t i = 0; i < splits.size(); i++) {
                if (i + 1 < splits.size() && splits[i] == left && splits[i + 1] == right) {
                    new_splits.push_back(merged);
                    i++;  // Skip next token
                } else {
                    new_splits.push_back(splits[i]);
                }
            }
            splits = new_splits;
        }
    }
}

std::map<std::pair<std::string, std::string>, int> WordTokenizer::count_bigram_freqs(
    const std::map<std::string, std::vector<std::string>>& word_splits,
    const std::unordered_map<std::string, int>& word_freqs
) const {
    std::map<std::pair<std::string, std::string>, int> bigram_freqs;

    for (const auto& [word, splits] : word_splits) {
        int freq = word_freqs.at(word);
        for (size_t i = 0; i + 1 < splits.size(); i++) {
            bigram_freqs[{splits[i], splits[i + 1]}] += freq;
        }
    }

    return bigram_freqs;
}

std::vector<std::string> WordTokenizer::apply_bpe(const std::string& word) const {
    // Start with character-level splits
    std::vector<std::string> splits;
    for (char c : word) {
        splits.push_back(std::string(1, c));
    }

    // Apply merge rules in order
    for (const auto& [left, right] : bpe_merges) {
        std::vector<std::string> new_splits;
        for (size_t i = 0; i < splits.size(); i++) {
            if (i + 1 < splits.size() && splits[i] == left && splits[i + 1] == right) {
                new_splits.push_back(left + right);
                i++;  // Skip next
            } else {
                new_splits.push_back(splits[i]);
            }
        }
        splits = new_splits;
    }

    return splits;
}

std::vector<int> WordTokenizer::encode(const std::string& text) const {
    auto tokens = tokenize(text);
    std::vector<int> ids;

    for (const auto& token : tokens) {
        auto it = word2idx.find(token);
        if (it != word2idx.end()) {
            // Known word
            ids.push_back(it->second);
        } else if (bpe_enabled) {
            // Unknown word → apply BPE
            auto subwords = apply_bpe(token);
            for (const auto& subword : subwords) {
                auto sub_it = word2idx.find(subword);
                ids.push_back(sub_it != word2idx.end() ? sub_it->second : UNK_ID);
            }
        } else {
            // No BPE → just use UNK
            ids.push_back(UNK_ID);
        }
    }

    return ids;
}

std::string WordTokenizer::decode(const std::vector<int>& ids) const {
    std::string result;
    for (int id : ids) {
        if (id >= 0 && id < static_cast<int>(idx2word.size())) {
            if (!result.empty() && idx2word[id] != "<s>" && idx2word[id] != "</s>") {
                result += " ";
            }
            if (idx2word[id] != "<pad>" && idx2word[id] != "<s>" && idx2word[id] != "</s>") {
                result += idx2word[id];
            }
        }
    }
    return result;
}

int WordTokenizer::get_id(const std::string& word) const {
    auto it = word2idx.find(word);
    return it != word2idx.end() ? it->second : UNK_ID;
}

std::string WordTokenizer::get_word(int id) const {
    if (id >= 0 && id < static_cast<int>(idx2word.size())) {
        return idx2word[id];
    }
    return "<unk>";
}

int WordTokenizer::get_word_freq(const std::string& word) const {
    auto it = word_freq.find(word);
    return it != word_freq.end() ? it->second : 0;
}

void WordTokenizer::save(const std::string& file_path) const {
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[WordTokenizer] Error: Cannot save to " << file_path << "\n";
        return;
    }

    // Save vocabulary size
    int vocab_sz = vocab_size();
    file.write(reinterpret_cast<const char*>(&vocab_sz), sizeof(vocab_sz));

    // Save words
    for (const auto& word : idx2word) {
        int len = static_cast<int>(word.size());
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(word.data(), len);
    }

    // Save word frequencies
    int freq_count = static_cast<int>(word_freq.size());
    file.write(reinterpret_cast<const char*>(&freq_count), sizeof(freq_count));
    for (const auto& [word, freq] : word_freq) {
        int len = static_cast<int>(word.size());
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(word.data(), len);
        file.write(reinterpret_cast<const char*>(&freq), sizeof(freq));
    }

    // Save BPE merges
    int bpe_count = static_cast<int>(bpe_merges.size());
    file.write(reinterpret_cast<const char*>(&bpe_count), sizeof(bpe_count));
    for (const auto& [left, right] : bpe_merges) {
        int len1 = static_cast<int>(left.size());
        int len2 = static_cast<int>(right.size());
        file.write(reinterpret_cast<const char*>(&len1), sizeof(len1));
        file.write(left.data(), len1);
        file.write(reinterpret_cast<const char*>(&len2), sizeof(len2));
        file.write(right.data(), len2);
    }

    file.write(reinterpret_cast<const char*>(&total_token_count), sizeof(total_token_count));

    std::cout << "[WordTokenizer] Saved to " << file_path << "\n";
}

void WordTokenizer::load(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[WordTokenizer] Error: Cannot load from " << file_path << "\n";
        return;
    }

    // Clear current state
    word2idx.clear();
    idx2word.clear();
    word_freq.clear();
    bpe_merges.clear();
    merge_priorities.clear();

    // Load vocabulary size
    int vocab_sz;
    file.read(reinterpret_cast<char*>(&vocab_sz), sizeof(vocab_sz));

    // Load words
    for (int i = 0; i < vocab_sz; i++) {
        int len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string word(len, '\0');
        file.read(&word[0], len);
        idx2word.push_back(word);
        word2idx[word] = i;
    }

    // Load word frequencies
    int freq_count;
    file.read(reinterpret_cast<char*>(&freq_count), sizeof(freq_count));
    for (int i = 0; i < freq_count; i++) {
        int len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        std::string word(len, '\0');
        file.read(&word[0], len);
        int freq;
        file.read(reinterpret_cast<char*>(&freq), sizeof(freq));
        word_freq[word] = freq;
    }

    // Load BPE merges
    int bpe_count;
    file.read(reinterpret_cast<char*>(&bpe_count), sizeof(bpe_count));
    for (int i = 0; i < bpe_count; i++) {
        int len1, len2;
        file.read(reinterpret_cast<char*>(&len1), sizeof(len1));
        std::string left(len1, '\0');
        file.read(&left[0], len1);
        file.read(reinterpret_cast<char*>(&len2), sizeof(len2));
        std::string right(len2, '\0');
        file.read(&right[0], len2);
        bpe_merges.push_back({left, right});
        merge_priorities[{left, right}] = i;
    }

    file.read(reinterpret_cast<char*>(&total_token_count), sizeof(total_token_count));

    bpe_enabled = !bpe_merges.empty();

    std::cout << "[WordTokenizer] Loaded from " << file_path << "\n";
    std::cout << "[WordTokenizer] Vocabulary size: " << vocab_size() << "\n";
    if (bpe_enabled) {
        std::cout << "[WordTokenizer] BPE merges: " << bpe_merges.size() << "\n";
    }
}
