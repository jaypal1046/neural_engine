#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include "bpe_tokenizer.h"

// Training batch containing tokenized sequences
struct TrainingBatch {
    std::vector<std::vector<int>> tokens;   // [batch_size, seq_len]
    std::vector<std::vector<int>> targets;  // [batch_size, seq_len]
    int batch_size;
    int seq_len;
};

// Data loader for training transformer models
// Reads text corpus, tokenizes, and provides batches
class DataLoader {
public:
    DataLoader(const std::string& corpus_path,
               BPETokenizer& tokenizer,
               int max_seq_length = 512,
               int batch_size = 32);

    ~DataLoader();

    // Get next batch of training data
    TrainingBatch next_batch();

    // Check if more data available
    bool has_next() const;

    // Reset to beginning of corpus
    void reset();

    // Get statistics
    int get_num_batches() const { return num_batches_; }
    size_t get_corpus_size() const { return corpus_size_; }
    int get_current_batch() const { return current_batch_; }

private:
    std::ifstream file_;
    BPETokenizer& tokenizer_;
    int max_seq_length_;
    int batch_size_;

    std::vector<std::string> buffer_;  // Text buffer
    size_t current_pos_;                // Position in buffer
    size_t corpus_size_;                // Total corpus size
    int num_batches_;                   // Total batches
    int current_batch_;                 // Current batch number

    void load_buffer();                 // Load next chunk into buffer
    bool eof_reached_;
};

#endif // DATA_LOADER_H
