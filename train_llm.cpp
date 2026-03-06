#include "mini_transformer.h"
#include "data_loader.h"
#include "bpe_tokenizer.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstring>

using PMode = MixedPrecision::MixedPrecisionOptimizer::PrecisionMode;

void print_usage() {
    std::cout << "Usage: train_llm [options]\n";
    std::cout << "Options:\n";
    std::cout << "  --corpus <path>     Training corpus file (default: wiki_clean.txt)\n";
    std::cout << "  --epochs <n>        Number of epochs (default: 10)\n";
    std::cout << "  --lr <float>        Learning rate (default: 0.0001)\n";
    std::cout << "  --batch <n>         Batch size (default: 8)\n";
    std::cout << "  --seq-len <n>       Max sequence length (default: 128)\n";
    std::cout << "  --precision <mode>  FP32, FP16, or BF16 (default: FP16)\n";
    std::cout << "  --checkpoint <n>    Save every N epochs (default: 1)\n";
    std::cout << "  --output <path>     Output model path (default: model_trained.bin)\n";
}

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║            AIZip Brain LLM Training System                   ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    // Default configuration
    std::string corpus_path = "wiki_clean.txt";
    std::string output_path = "model_trained.bin";
    std::string tokenizer_path = "tokenizer.model";
    int num_epochs = 10;
    float learning_rate = 0.0001f;
    int batch_size = 8;          // Small for CPU training
    int max_seq_length = 128;    // Reduced for faster training
    int checkpoint_interval = 1;
    PMode precision_mode = PMode::FP16;  // Use FP16 for 6.35x speedup!

    // Parse arguments
    for (int i = 1; i < argc; i++) {
        if (std::strcmp(argv[i], "--help") == 0 || std::strcmp(argv[i], "-h") == 0) {
            print_usage();
            return 0;
        }
        else if (std::strcmp(argv[i], "--corpus") == 0 && i + 1 < argc) {
            corpus_path = argv[++i];
        }
        else if (std::strcmp(argv[i], "--epochs") == 0 && i + 1 < argc) {
            num_epochs = std::stoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--lr") == 0 && i + 1 < argc) {
            learning_rate = std::stof(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--batch") == 0 && i + 1 < argc) {
            batch_size = std::stoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--seq-len") == 0 && i + 1 < argc) {
            max_seq_length = std::stoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--precision") == 0 && i + 1 < argc) {
            std::string mode = argv[++i];
            if (mode == "FP32") precision_mode = PMode::FP32;
            else if (mode == "FP16") precision_mode = PMode::FP16;
            else if (mode == "BF16") precision_mode = PMode::BF16;
        }
        else if (std::strcmp(argv[i], "--checkpoint") == 0 && i + 1 < argc) {
            checkpoint_interval = std::stoi(argv[++i]);
        }
        else if (std::strcmp(argv[i], "--output") == 0 && i + 1 < argc) {
            output_path = argv[++i];
        }
    }

    std::cout << "Configuration:\n";
    std::cout << "  Corpus:         " << corpus_path << "\n";
    std::cout << "  Epochs:         " << num_epochs << "\n";
    std::cout << "  Learning Rate:  " << learning_rate << "\n";
    std::cout << "  Batch Size:     " << batch_size << "\n";
    std::cout << "  Sequence Length:" << max_seq_length << "\n";
    std::cout << "  Precision:      " << (precision_mode == PMode::FP32 ? "FP32" :
                                          precision_mode == PMode::FP16 ? "FP16" : "BF16") << "\n";
    std::cout << "  Checkpoint:     Every " << checkpoint_interval << " epoch(s)\n";
    std::cout << "  Output:         " << output_path << "\n";
    std::cout << "\n";

    // Initialize model (smaller config for faster training)
    TransformerConfig config;
    config.vocab_size = 32000;
    config.embedding_dim = 256;      // Smaller for faster training
    config.num_layers = 4;            // Fewer layers
    config.num_heads = 4;
    config.ff_dim = 1024;
    config.max_seq_length = max_seq_length;

    std::cout << "Model Architecture:\n";
    std::cout << "  Vocab Size:     " << config.vocab_size << "\n";
    std::cout << "  Embedding Dim:  " << config.embedding_dim << "\n";
    std::cout << "  Layers:         " << config.num_layers << "\n";
    std::cout << "  Heads:          " << config.num_heads << "\n";
    std::cout << "  FF Dim:         " << config.ff_dim << "\n";
    std::cout << "\n";

    MiniTransformer transformer(config);

    // Initialize tokenizer
    std::cout << "Loading tokenizer from: " << tokenizer_path << "\n";
    BPETokenizer tokenizer;
    tokenizer.load(tokenizer_path);
    std::cout << "  Tokenizer loaded ✅\n\n";

    // Create data loader
    std::cout << "Initializing data loader...\n";
    DataLoader data_loader(corpus_path, tokenizer, max_seq_length, batch_size);
    std::cout << "\n";

    // Training loop
    std::cout << "═══════════════════════════════════════════════════════════════\n";
    std::cout << "Starting Training...\n";
    std::cout << "═══════════════════════════════════════════════════════════════\n\n";

    for (int epoch = 0; epoch < num_epochs; epoch++) {
        std::cout << "┌─────────────────────────────────────────────────────────────┐\n";
        std::cout << "│ Epoch " << std::setw(2) << (epoch + 1) << "/" << num_epochs << "                                                  │\n";
        std::cout << "└─────────────────────────────────────────────────────────────┘\n";

        data_loader.reset();
        float epoch_loss = 0.0f;
        int num_batches = 0;
        int num_examples = 0;

        auto epoch_start = std::chrono::high_resolution_clock::now();

        while (data_loader.has_next()) {
            auto batch = data_loader.next_batch();

            if (batch.batch_size == 0) break;

            // Train on each example in batch
            float batch_loss = 0.0f;
            for (int i = 0; i < batch.batch_size; i++) {
                float loss = transformer.training_step(
                    batch.tokens[i],
                    batch.targets[i],
                    learning_rate,
                    precision_mode
                );

                batch_loss += loss;
                epoch_loss += loss;
                num_examples++;
            }

            num_batches++;
            batch_loss /= batch.batch_size;

            // Progress update every 10 batches
            if (num_batches % 10 == 0) {
                float avg_loss = epoch_loss / num_examples;
                std::cout << "  Batch " << std::setw(4) << num_batches
                          << " | Loss: " << std::fixed << std::setprecision(4) << batch_loss
                          << " | Avg: " << avg_loss
                          << "                    \r" << std::flush;
            }
        }

        auto epoch_end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            epoch_end - epoch_start).count();

        float avg_loss = epoch_loss / num_examples;

        std::cout << "\n";
        std::cout << "  ✓ Epoch " << (epoch + 1) << " Complete\n";
        std::cout << "    Loss:     " << std::fixed << std::setprecision(4) << avg_loss;

        // Loss quality indicator
        if (avg_loss < 2.0) std::cout << " ✅ EXCELLENT";
        else if (avg_loss < 3.0) std::cout << " ✅ GOOD";
        else if (avg_loss < 4.0) std::cout << " ⚠️  OK";
        else std::cout << " ❌ HIGH";

        std::cout << "\n";
        std::cout << "    Batches:  " << num_batches << "\n";
        std::cout << "    Examples: " << num_examples << "\n";
        std::cout << "    Time:     " << duration << " seconds";
        std::cout << " (" << (duration / 60) << "m " << (duration % 60) << "s)\n";

        // Save checkpoint
        if ((epoch + 1) % checkpoint_interval == 0 || epoch == num_epochs - 1) {
            std::string checkpoint;
            if (epoch == num_epochs - 1) {
                checkpoint = output_path;
            } else {
                checkpoint = "checkpoint_epoch_" + std::to_string(epoch + 1) + ".bin";
            }

            transformer.save(checkpoint);
            std::cout << "    Saved:    " << checkpoint << " ✅\n";
        }

        std::cout << "\n";
    }

    std::cout << "╔══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║             Training Complete! 🎉                            ║\n";
    std::cout << "╚══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    std::cout << "Final model saved to: " << output_path << "\n";
    std::cout << "\n";
    std::cout << "To test your trained model:\n";
    std::cout << "  ./bin/inference_engine " << output_path << " \"Your prompt here\"\n";
    std::cout << "\n";

    return 0;
}
