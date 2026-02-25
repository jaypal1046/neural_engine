// =============================================================================
// Neural Engine - Unified Main Entry Point
// Routes commands to appropriate subsystems
// =============================================================================

#include <iostream>
#include <string>
#include <cstring>

// Forward declarations
extern int main_compress(int argc, char** argv);
extern int main_neural_engine(int argc, char** argv);
extern int main_test_block_access(int argc, char** argv);

void print_unified_help() {
    std::cout << R"(
Neural Engine - Unified AI & Compression System
================================================

Usage: neural_engine <command> [options]

COMPRESSION COMMANDS:
  compress <input> [options]         Compress a file to .aiz format
  decompress <input> [options]       Decompress an .aiz file
  benchmark <input>                  Benchmark compression

AI COMMANDS:
  knowledge_load <module>            Load knowledge module
  knowledge_query <module> <q>       Query knowledge
  ai_ask <question>                  Ask AI a question
  train_transformer <file> <e> <lr> <batch>   Train model

TESTING:
  test_block <file>                  Test block access

EXAMPLES:
  neural_engine compress file.txt --best
  neural_engine knowledge_query capabilities "What can you do?"
  neural_engine ai_ask "Explain BWT compression"

For detailed help on a command, use: neural_engine <command> --help
)" << std::endl;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        // No command - show help
        print_unified_help();
        return 0;
    }

    std::string cmd = argv[1];

    // Help
    if (cmd == "help" || cmd == "--help" || cmd == "-h") {
        print_unified_help();
        return 0;
    }

    // Version
    if (cmd == "version" || cmd == "--version") {
        std::cout << "Neural Engine v8.0 - Unified AI & Compression\n";
        std::cout << "Format: .aiz (AI Zip)\n";
        return 0;
    }

    // Compression commands
    if (cmd == "compress" || cmd == "decompress" || cmd == "benchmark") {
        // Create new argv: ["neural_engine", "compress", "file", ...]
        // Shift everything left by 1, keeping program name
        return main_compress(argc, argv);
    }

    // Test commands
    if (cmd == "test_block" || cmd == "test") {
        // Shift: ["neural_engine", "test_block", "file"] -> ["neural_engine", "file"]
        if (argc > 2) {
            argv[1] = argv[2];  // Move file path forward
        }
        return main_test_block_access(argc - 1, argv);
    }

    // Everything else goes to neural engine
    // (knowledge_load, ai_ask, train_transformer, etc.)
    return main_neural_engine(argc, argv);
}
