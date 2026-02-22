// =============================================================================
// Smart Brain - Main CLI entry point for the unified AI compression system
// =============================================================================

#include "knowledge_manager.h"
#include <iostream>
#include <string>
#include <cstring>

void print_usage() {
    std::cout <<
R"(
Smart Brain v1.0 - AI-Powered Compression & Knowledge System

Usage:
  smart_brain <command> [args...]

Commands:
  learn <url|file>          Learn from web URL or local file
                            - Downloads and extracts clean text
                            - Compresses with CMIX neural engine
                            - Stores in brain/knowledge/*.myzip
                            - Updates vector index for search

  ask <question>            Query knowledge base
                            - Searches indexed knowledge
                            - Returns confidence score
                            - Decompresses relevant .myzip files
                            - Recommends web search if low confidence

  compress <file>           Smart compression using persistent neural weights
                            - Auto-detects file type (text, code, JSON, binary)
                            - Uses specialized mixer for better ratio
                            - Saves improved weights for next time

  status                    Show brain statistics
                            - Total knowledge entries
                            - Storage usage (original vs compressed)
                            - Average compression ratio

Examples:
  smart_brain learn https://en.wikipedia.org/wiki/Data_compression
  smart_brain learn myfile.txt
  smart_brain ask "What is data compression?"
  smart_brain compress large_file.txt
  smart_brain status

Brain Storage:
  brain/knowledge/*.myzip   - Compressed knowledge files
  brain/index.bin           - Vector index (SIMD-optimized search)
  brain/mixer_*.weights     - Persistent neural network weights
)";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string cmd = argv[1];

    try {
        if (cmd == "learn") {
            if (argc < 3) {
                std::cerr << "Error: missing URL or file path\n";
                std::cerr << "Usage: smart_brain learn <url|file>\n";
                return 1;
            }
            std::string source = argv[2];
            learn_and_store(source);

        } else if (cmd == "ask") {
            if (argc < 3) {
                std::cerr << "Error: missing question\n";
                std::cerr << "Usage: smart_brain ask <question>\n";
                return 1;
            }

            // Combine all remaining args into question
            std::string question;
            for (int i = 2; i < argc; i++) {
                if (i > 2) question += " ";
                question += argv[i];
            }

            answer_from_knowledge(question);

        } else if (cmd == "compress") {
            if (argc < 3) {
                std::cerr << "Error: missing file path\n";
                std::cerr << "Usage: smart_brain compress <file>\n";
                return 1;
            }
            std::string file_path = argv[2];
            smart_compress(file_path);

        } else if (cmd == "status") {
            show_brain_status();

        } else if (cmd == "--help" || cmd == "-h") {
            print_usage();

        } else {
            std::cerr << "Error: unknown command '" << cmd << "'\n";
            print_usage();
            return 1;
        }

    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
