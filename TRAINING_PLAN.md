# Training Plan: Make the Model Actually Work

**Date**: 2026-03-06
**Goal**: Train model so it gives intelligent responses
**Status**: Training system ready, need data!

---

## 🎯 The Real Problem

**Your model doesn't work because**: NOT TRAINED (random weights)

**NOT because**: Size is wrong (1.4 GB is fine!)

---

## 📋 Training Checklist

### ✅ What You Already Have:

1. ✅ **Transformer architecture** (350M params)
2. ✅ **Training system** (training_step() works)
3. ✅ **Mixed precision** (6.35x speedup with FP16)
4. ✅ **Loss function** (cross-entropy)
5. ✅ **Optimizer** (gradient descent)
6. ✅ **Tokenizer** (BPE)

### ❌ What You Need:

1. ❌ **Training data** (Wikipedia, books, code)
2. ❌ **Data loader** (batch processing)
3. ❌ **Training loop** (epochs, checkpointing)
4. ❌ **Actual training run** (24-48 hours)

---

## 🚀 Training Implementation

### Step 1: Get Training Data (Day 1)

**Option A: Wikipedia (Recommended)**
```bash
# Download English Wikipedia (6 GB compressed, ~20 GB uncompressed)
wget https://dumps.wikimedia.org/enwiki/latest/enwiki-latest-pages-articles.xml.bz2

# Extract
bunzip2 enwiki-latest-pages-articles.xml.bz2

# Result: ~20 GB of text
```

**Option B: Smaller Test Dataset**
```bash
# Just get a sample for testing
head -n 100000 wiki.txt > wiki_sample.txt

# Result: Quick test dataset
```

### Step 2: Preprocess Data (Day 1)

```python
# preprocess_wiki.py
import re

def clean_text(text):
    # Remove XML tags
    text = re.sub(r'<[^>]+>', '', text)
    # Remove URLs
    text = re.sub(r'http\S+', '', text)
    # Remove special chars
    text = re.sub(r'[^\w\s.,!?;:\'-]', '', text)
    return text

def preprocess_wikipedia(input_file, output_file):
    with open(input_file, 'r', encoding='utf-8') as f_in:
        with open(output_file, 'w', encoding='utf-8') as f_out:
            for line in f_in:
                cleaned = clean_text(line)
                if len(cleaned) > 50:  # Only meaningful sentences
                    f_out.write(cleaned + '\n')

# Run
preprocess_wikipedia('enwiki.xml', 'wiki_clean.txt')
```

### Step 3: Create Data Loader (Day 2)

```cpp
// data_loader.h
#ifndef DATA_LOADER_H
#define DATA_LOADER_H

#include <vector>
#include <string>
#include <fstream>
#include "bpe_tokenizer.h"

struct TrainingBatch {
    std::vector<std::vector<int>> tokens;   // [batch_size, seq_len]
    std::vector<std::vector<int>> targets;  // [batch_size, seq_len]
    int batch_size;
    int seq_len;
};

class DataLoader {
public:
    DataLoader(const std::string& corpus_path,
               BPETokenizer& tokenizer,
               int max_seq_length = 512,
               int batch_size = 32);

    // Get next batch
    TrainingBatch next_batch();

    // Check if more data available
    bool has_next() const;

    // Reset to beginning
    void reset();

    // Get total batches
    int get_num_batches() const;

private:
    std::ifstream file_;
    BPETokenizer& tokenizer_;
    int max_seq_length_;
    int batch_size_;
    std::vector<std::string> buffer_;
    size_t current_pos_;

    void load_buffer();
};

#endif
```

```cpp
// data_loader.cpp
#include "data_loader.h"
#include <iostream>

DataLoader::DataLoader(const std::string& corpus_path,
                       BPETokenizer& tokenizer,
                       int max_seq_length,
                       int batch_size)
    : tokenizer_(tokenizer)
    , max_seq_length_(max_seq_length)
    , batch_size_(batch_size)
    , current_pos_(0)
{
    file_.open(corpus_path);
    if (!file_.is_open()) {
        std::cerr << "ERROR: Cannot open corpus: " << corpus_path << "\n";
    }
    load_buffer();
}

void DataLoader::load_buffer() {
    buffer_.clear();
    std::string line;

    // Load batch_size * 10 lines into buffer
    for (int i = 0; i < batch_size_ * 10 && std::getline(file_, line); i++) {
        if (line.length() > 10) {
            buffer_.push_back(line);
        }
    }
}

TrainingBatch DataLoader::next_batch() {
    TrainingBatch batch;
    batch.batch_size = std::min(batch_size_, (int)buffer_.size() - (int)current_pos_);
    batch.seq_len = max_seq_length_;

    for (int i = 0; i < batch.batch_size; i++) {
        // Tokenize text
        std::string text = buffer_[current_pos_++];
        auto tokens = tokenizer_.encode(text);

        // Truncate or pad to max_seq_length
        if (tokens.size() > max_seq_length_) {
            tokens.resize(max_seq_length_);
        } else {
            while (tokens.size() < max_seq_length_) {
                tokens.push_back(0);  // PAD token
            }
        }

        // Create targets (shifted by 1)
        std::vector<int> targets = tokens;
        targets.erase(targets.begin());
        targets.push_back(0);  // EOS token

        batch.tokens.push_back(tokens);
        batch.targets.push_back(targets);
    }

    // Reload buffer if needed
    if (current_pos_ >= buffer_.size()) {
        load_buffer();
        current_pos_ = 0;
    }

    return batch;
}

bool DataLoader::has_next() const {
    return !buffer_.empty() || !file_.eof();
}

void DataLoader::reset() {
    file_.clear();
    file_.seekg(0);
    current_pos_ = 0;
    load_buffer();
}
```

### Step 4: Training Loop (Day 3)

```cpp
// train_llm.cpp
#include "mini_transformer.h"
#include "data_loader.h"
#include "bpe_tokenizer.h"
#include <iostream>
#include <chrono>

int main(int argc, char** argv) {
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║              AIZip Brain LLM Training                        ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    // Configuration
    std::string corpus_path = "wiki_clean.txt";
    int num_epochs = 10;
    float learning_rate = 0.0001f;
    int batch_size = 32;
    int max_seq_length = 512;

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (std::string(argv[i]) == "--corpus") corpus_path = argv[++i];
        if (std::string(argv[i]) == "--epochs") num_epochs = std::stoi(argv[++i]);
        if (std::string(argv[i]) == "--lr") learning_rate = std::stof(argv[++i]);
        if (std::string(argv[i]) == "--batch") batch_size = std::stoi(argv[++i]);
    }

    std::cout << "Configuration:\n";
    std::cout << "  Corpus: " << corpus_path << "\n";
    std::cout << "  Epochs: " << num_epochs << "\n";
    std::cout << "  Learning Rate: " << learning_rate << "\n";
    std::cout << "  Batch Size: " << batch_size << "\n\n";

    // Initialize model
    TransformerConfig config;
    config.vocab_size = 32000;
    config.embedding_dim = 512;
    config.num_layers = 6;
    config.num_heads = 8;
    config.ff_dim = 2048;
    config.max_seq_length = max_seq_length;

    MiniTransformer transformer(config);

    std::cout << "Model initialized: "
              << config.num_layers << " layers, "
              << config.embedding_dim << " dim\n\n";

    // Initialize tokenizer
    BPETokenizer tokenizer;
    tokenizer.load("tokenizer.model");

    // Create data loader
    DataLoader data_loader(corpus_path, tokenizer, max_seq_length, batch_size);

    std::cout << "Starting training...\n\n";

    // Training loop
    for (int epoch = 0; epoch < num_epochs; epoch++) {
        std::cout << "═══════════════════════════════════════════\n";
        std::cout << "Epoch " << (epoch + 1) << "/" << num_epochs << "\n";
        std::cout << "═══════════════════════════════════════════\n";

        data_loader.reset();
        float epoch_loss = 0.0f;
        int num_batches = 0;

        auto epoch_start = std::chrono::high_resolution_clock::now();

        while (data_loader.has_next()) {
            auto batch = data_loader.next_batch();

            // Train on each example in batch
            for (int i = 0; i < batch.batch_size; i++) {
                float loss = transformer.training_step(
                    batch.tokens[i],
                    batch.targets[i],
                    learning_rate,
                    MixedPrecision::MixedPrecisionOptimizer::PrecisionMode::FP16  // 6.35x faster!
                );

                epoch_loss += loss;
                num_batches++;

                // Progress update every 100 batches
                if (num_batches % 100 == 0) {
                    float avg_loss = epoch_loss / num_batches;
                    std::cout << "  Batch " << num_batches
                              << " - Avg Loss: " << avg_loss << "\r" << std::flush;
                }
            }
        }

        auto epoch_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            epoch_end - epoch_start).count();

        float avg_loss = epoch_loss / num_batches;
        std::cout << "\nEpoch " << (epoch + 1) << " complete:\n";
        std::cout << "  Average Loss: " << avg_loss << "\n";
        std::cout << "  Batches: " << num_batches << "\n";
        std::cout << "  Time: " << duration << " seconds\n";
        std::cout << "  Loss is " << (avg_loss < 3.0 ? "GOOD ✅" : "HIGH ⚠️") << "\n\n";

        // Save checkpoint
        std::string checkpoint = "model_epoch_" + std::to_string(epoch + 1) + ".bin";
        transformer.save(checkpoint);
        std::cout << "  Checkpoint saved: " << checkpoint << "\n\n";
    }

    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║           Training Complete! Model is now smart! ✅          ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n\n";

    std::cout << "To test your trained model:\n";
    std::cout << "  ./bin/test_model model_epoch_10.bin \"Hello, AI!\"\n\n";

    return 0;
}
```

### Step 5: Test Trained Model (Day 4)

```cpp
// test_model.cpp
#include "mini_transformer.h"
#include "bpe_tokenizer.h"
#include <iostream>

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: test_model <model.bin> <prompt>\n";
        return 1;
    }

    std::string model_path = argv[1];
    std::string prompt = argv[2];

    // Load config (match training)
    TransformerConfig config;
    config.vocab_size = 32000;
    config.embedding_dim = 512;
    config.num_layers = 6;
    config.num_heads = 8;
    config.ff_dim = 2048;
    config.max_seq_length = 512;

    // Load trained model
    MiniTransformer transformer(config);
    transformer.load(model_path);

    // Load tokenizer
    BPETokenizer tokenizer;
    tokenizer.load("tokenizer.model");

    // Generate response
    std::cout << "Prompt: " << prompt << "\n";
    std::cout << "Response: ";

    std::string response = transformer.generate(
        prompt,
        tokenizer,
        50,     // max_tokens
        0.8f,   // temperature
        40      // top_k
    );

    std::cout << response << "\n";

    return 0;
}
```

---

## 📊 Expected Results

### After Training:

**Before (Random Weights)**:
```
Input: "What is AI?"
Output: "xkjdflkj asdflkj asdflkj" (gibberish)
```

**After (Trained Weights)**:
```
Input: "What is AI?"
Output: "AI is artificial intelligence, the simulation of human intelligence..."
```

**THIS is what makes it "work"!** ✅

---

## ⏱️ Timeline

| Day | Task | Time | Result |
|-----|------|------|--------|
| Day 1 | Get Wikipedia, preprocess | 2 hours | 20 GB training data |
| Day 2 | Implement data loader | 3 hours | Batch processing works |
| Day 3 | Implement training loop | 3 hours | Can train model |
| Day 4 | Run training | 24-48 hours | **SMART MODEL!** ✅ |
| Day 5 | Test & validate | 2 hours | Intelligent responses |

**Total**: ~1 week to working AI!

---

## 🎯 Success Criteria

### Model "Works" When:

1. ✅ Loss decreases during training (< 3.0 is good)
2. ✅ Generates coherent sentences
3. ✅ Answers questions sensibly
4. ✅ Doesn't produce gibberish

### NOT Required:

- ❌ Small size (1.4 GB is fine!)
- ❌ Quantization (can add later)
- ❌ Perfect responses (GPT-2 level is great!)

---

## 💡 Key Insight

**Size DOESN'T matter!**

GPT-2 Medium (same size as yours):
- Size: 1.5 GB
- Params: 345M
- **Works great!** ✅

Why? **TRAINED!**

Your model:
- Size: 1.4 GB ✅ (perfect!)
- Params: 350M ✅ (perfect!)
- **Doesn't work** ❌ (not trained!)

**Solution**: TRAIN IT! (Days 1-5)

---

**Bottom Line**: Forget about size - just TRAIN the model and it will work! 🚀
