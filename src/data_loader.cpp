#include "data_loader.h"
#include <iostream>
#include <algorithm>

DataLoader::DataLoader(const std::string& corpus_path,
                       BPETokenizer& tokenizer,
                       int max_seq_length,
                       int batch_size)
    : tokenizer_(tokenizer)
    , max_seq_length_(max_seq_length)
    , batch_size_(batch_size)
    , current_pos_(0)
    , corpus_size_(0)
    , num_batches_(0)
    , current_batch_(0)
    , eof_reached_(false)
{
    file_.open(corpus_path);
    if (!file_.is_open()) {
        std::cerr << "[ERROR] Cannot open corpus: " << corpus_path << "\n";
        return;
    }

    // Get corpus size
    file_.seekg(0, std::ios::end);
    corpus_size_ = file_.tellg();
    file_.seekg(0, std::ios::beg);

    std::cout << "[DATA LOADER] Initialized\n";
    std::cout << "  Corpus: " << corpus_path << "\n";
    std::cout << "  Size: " << (corpus_size_ / 1024 / 1024) << " MB\n";
    std::cout << "  Max sequence length: " << max_seq_length_ << "\n";
    std::cout << "  Batch size: " << batch_size_ << "\n";

    load_buffer();
}

DataLoader::~DataLoader() {
    if (file_.is_open()) {
        file_.close();
    }
}

void DataLoader::load_buffer() {
    buffer_.clear();
    std::string line;

    // Load batch_size * 10 lines into buffer (for efficiency)
    int lines_to_load = batch_size_ * 10;
    int loaded = 0;

    while (loaded < lines_to_load && std::getline(file_, line)) {
        // Skip empty or very short lines
        if (line.length() > 10) {
            buffer_.push_back(line);
            loaded++;
        }
    }

    if (buffer_.empty()) {
        eof_reached_ = true;
    }

    current_pos_ = 0;
}

TrainingBatch DataLoader::next_batch() {
    TrainingBatch batch;
    batch.batch_size = 0;
    batch.seq_len = max_seq_length_;

    // Collect batch_size examples
    while (batch.batch_size < batch_size_ && has_next()) {
        // Get next text
        if (current_pos_ >= buffer_.size()) {
            load_buffer();
            if (buffer_.empty()) break;
        }

        std::string text = buffer_[current_pos_++];

        // Tokenize
        auto tokens = tokenizer_.encode(text);

        // Skip if too short
        if (tokens.size() < 10) {
            continue;
        }

        // Truncate or pad to max_seq_length
        if (tokens.size() > (size_t)max_seq_length_) {
            tokens.resize(max_seq_length_);
        } else {
            // Pad with PAD token (0)
            while (tokens.size() < (size_t)max_seq_length_) {
                tokens.push_back(0);
            }
        }

        // Create targets (shifted by 1 position)
        std::vector<int> targets = tokens;
        targets.erase(targets.begin());
        targets.push_back(3);  // EOS token

        batch.tokens.push_back(tokens);
        batch.targets.push_back(targets);
        batch.batch_size++;
    }

    current_batch_++;
    return batch;
}

bool DataLoader::has_next() const {
    return !eof_reached_ || current_pos_ < buffer_.size();
}

void DataLoader::reset() {
    file_.clear();
    file_.seekg(0);
    current_pos_ = 0;
    current_batch_ = 0;
    eof_reached_ = false;
    load_buffer();

    std::cout << "[DATA LOADER] Reset to beginning\n";
}
