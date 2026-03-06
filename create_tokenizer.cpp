#include "bpe_tokenizer.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>

int main(int argc, char** argv) {
    std::cout << "\n";
    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "     BPE Tokenizer Training Tool\n";
    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "\n";

    std::string corpus_path = "wiki_training.txt";
    std::string output_path = "tokenizer.model";
    int vocab_size = 32000;
    int num_merges = 10000;

    // Parse arguments
    if (argc > 1) corpus_path = argv[1];
    if (argc > 2) output_path = argv[2];
    if (argc > 3) vocab_size = std::stoi(argv[3]);
    if (argc > 4) num_merges = std::stoi(argv[4]);

    std::cout << "Configuration:\n";
    std::cout << "  Corpus:      " << corpus_path << "\n";
    std::cout << "  Output:      " << output_path << "\n";
    std::cout << "  Vocab Size:  " << vocab_size << "\n";
    std::cout << "  BPE Merges:  " << num_merges << "\n";
    std::cout << "\n";

    // Read training corpus
    std::cout << "Loading training corpus...\n";
    std::ifstream file(corpus_path);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open " << corpus_path << "\n";
        return 1;
    }

    std::vector<std::string> texts;
    std::string line;
    int line_count = 0;
    int max_lines = 50000;  // Limit for faster training

    while (std::getline(file, line) && line_count < max_lines) {
        if (!line.empty() && line.length() > 10) {
            texts.push_back(line);
            line_count++;

            if (line_count % 5000 == 0) {
                std::cout << "  Loaded " << line_count << " lines...\r" << std::flush;
            }
        }
    }

    file.close();

    std::cout << "\n";
    std::cout << "  Total lines loaded: " << texts.size() << "\n";
    std::cout << "\n";

    // Train tokenizer
    std::cout << "Training BPE tokenizer...\n";
    std::cout << "  This may take a few minutes...\n";

    BPETokenizer tokenizer(vocab_size);
    tokenizer.train(texts, num_merges);

    std::cout << "  Training complete!\n";
    std::cout << "  Final vocab size: " << tokenizer.vocab_size() << "\n";
    std::cout << "\n";

    // Save tokenizer
    std::cout << "Saving tokenizer to: " << output_path << "\n";
    tokenizer.save(output_path);
    std::cout << "  Saved successfully!\n";
    std::cout << "\n";

    // Test tokenizer
    std::cout << "Testing tokenizer...\n";
    std::string test_text = "Hello world! This is a test of the BPE tokenizer.";
    std::cout << "  Input:  \"" << test_text << "\"\n";

    auto tokens = tokenizer.encode(test_text);
    std::cout << "  Tokens: [";
    for (size_t i = 0; i < tokens.size(); i++) {
        std::cout << tokens[i];
        if (i < tokens.size() - 1) std::cout << ", ";
    }
    std::cout << "] (" << tokens.size() << " tokens)\n";

    auto decoded = tokenizer.decode(tokens);
    std::cout << "  Decoded: \"" << decoded << "\"\n";
    std::cout << "\n";

    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "  Tokenizer created successfully!\n";
    std::cout << "════════════════════════════════════════════════\n";
    std::cout << "\n";

    return 0;
}
