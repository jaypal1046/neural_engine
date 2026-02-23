// Bootstrap tool for training BPE tokenizer + Word2Vec embeddings
// Usage: train_language_model <corpus.txt> <output_dir>

#include "../include/bpe_tokenizer.h"
#include "../include/real_embeddings.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

std::vector<std::string> load_corpus(const std::string& path) {
    std::vector<std::string> texts;
    std::ifstream file(path);

    if (!file) {
        std::cerr << "Error: Cannot open " << path << "\n";
        return texts;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (line.length() > 10) {  // Skip very short lines
            texts.push_back(line);
        }
    }

    std::cerr << "[CORPUS] Loaded " << texts.size() << " lines from " << path << "\n";
    return texts;
}

int main(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: " << argv[0] << " <corpus.txt> <output_dir>\n\n";
        std::cerr << "Example:\n";
        std::cerr << "  " << argv[0] << " wikipedia.txt models/\n\n";
        std::cerr << "This will train:\n";
        std::cerr << "  - BPE tokenizer (32K vocabulary)\n";
        std::cerr << "  - Word2Vec embeddings (128-dimensional)\n";
        std::cerr << "  - Output: tokenizer.bin, embeddings.bin\n";
        return 1;
    }

    std::string corpus_path = argv[1];
    std::string output_dir = argv[2];

    // Create output directory
    fs::create_directories(output_dir);

    std::cerr << "\n========================================\n";
    std::cerr << "Language Model Training Pipeline\n";
    std::cerr << "========================================\n\n";

    // Step 1: Load corpus
    std::cerr << "[1/3] Loading corpus...\n";
    auto texts = load_corpus(corpus_path);

    if (texts.empty()) {
        std::cerr << "Error: No texts loaded!\n";
        return 1;
    }

    // Limit to first 100K texts for reasonable training time
    if (texts.size() > 100000) {
        std::cerr << "[CORPUS] Using first 100,000 texts (for speed)\n";
        texts.resize(100000);
    }

    // Step 2: Train BPE tokenizer
    std::cerr << "\n[2/3] Training BPE tokenizer...\n";
    BPETokenizer tokenizer(32000);
    tokenizer.train(texts, 30000);  // Learn 30K merges

    std::string tokenizer_path = output_dir + "/tokenizer.bin";
    tokenizer.save(tokenizer_path);

    // Step 3: Train Word2Vec embeddings
    std::cerr << "\n[3/3] Training Word2Vec embeddings...\n";
    RealEmbeddings embeddings(128, tokenizer.vocab_size());

    embeddings.train(
        texts,
        tokenizer,
        5,      // epochs
        0.025f, // learning rate
        5,      // window size
        5       // negative samples
    );

    std::string embeddings_path = output_dir + "/embeddings.bin";
    embeddings.save(embeddings_path);

    // Summary
    std::cerr << "\n========================================\n";
    std::cerr << "Training Complete!\n";
    std::cerr << "========================================\n\n";
    std::cerr << "Tokenizer:\n";
    std::cerr << "  Vocabulary size: " << tokenizer.vocab_size() << "\n";
    std::cerr << "  Saved to: " << tokenizer_path << "\n\n";

    std::cerr << "Embeddings:\n";
    std::cerr << "  Dimension: " << embeddings.embedding_dim() << "\n";
    std::cerr << "  Vocab coverage: " << embeddings.vocab_size() << "\n";
    std::cerr << "  Saved to: " << embeddings_path << "\n\n";

    // Test: encode and decode
    std::string test_text = "The quick brown fox jumps over the lazy dog.";
    std::cerr << "Test encoding:\n";
    std::cerr << "  Input: \"" << test_text << "\"\n";

    auto tokens = tokenizer.encode(test_text);
    std::cerr << "  Tokens (" << tokens.size() << "): ";
    for (size_t i = 0; i < std::min(tokens.size(), (size_t)10); i++) {
        std::cerr << tokens[i] << " ";
    }
    if (tokens.size() > 10) std::cerr << "...";
    std::cerr << "\n";

    std::string decoded = tokenizer.decode(tokens);
    std::cerr << "  Decoded: \"" << decoded << "\"\n";

    // Test: semantic similarity
    int dog_token = tokenizer.string_to_token("dog");
    if (dog_token > 3) {  // Not a special token
        std::cerr << "\nSemantic similarity test (token 'dog'):\n";
        auto similar = embeddings.find_similar(dog_token, 5);
        for (const auto& [token_id, sim] : similar) {
            std::string token_str = tokenizer.token_to_string(token_id);
            std::cerr << "  " << token_str << ": " << sim << "\n";
        }
    }

    std::cerr << "\nReady to use! Load these files in neural_engine.exe\n\n";

    return 0;
}
