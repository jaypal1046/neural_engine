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
extern int handle_quantize_command(int argc, char** argv);
extern int handle_mixed_precision_command(int argc, char** argv);

// Self-learning system
namespace self_learning {
    extern int main_auto_learn(int argc, char** argv);
}

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
  ai_ask <question>                  Ask AI (RAG + reasoning + memory)
  learn <url_or_file>                Learn from URL or local file (CMIX compress + index)
  ask <question>                     Direct knowledge query
  score_response <file>              Score response quality (RewardModel in C++)
  cai_critique <file>                Constitutional AI critique (C++)
  train_transformer <file> <e> <lr> <batch>  Train transformer model
  reason <problem>                   Chain-of-thought reasoning
  verify <claim>                     Verify a claim against knowledge

QUANTIZATION COMMANDS (Week 6 - Algorithm Extraction):
  quantize_model <in.bin> <out.q4_0> [--format Q4_0|Q8_0]  Compress model 8x
  dequantize_model <in.q4_0> <out.bin>                     Restore to FP32
  train_transformer_quantized <corpus> [options]           Train with Q8_0 (4x memory)

MIXED PRECISION COMMANDS (Week 7 - Algorithm Extraction):
  train_transformer_mixed <corpus> [options]               FP16/BF16 training (2x faster, 2x memory)

KV-CACHE COMMANDS (Option B2 - Full Integration):
  generate_cached <prompt>                                 50x faster generation (KV-Cache)

FLASH ATTENTION COMMANDS (Option B1 - Full Integration):
  generate_flash <prompt>                                  Long context O(N) memory (Flash Attn)

SELF-LEARNING COMMANDS:
  auto_learn [options]               Start self-learning daemon (auto-improves AI)
    --daemon                         Run as background daemon (4 threads)
    --monitor-interval=N             Monitor every N seconds (default: 60)
    --threshold=N                    Weak score threshold (default: 65)
    --train-threshold=N              Train after N corrections (default: 10)
    --no-web                         Disable auto-learning from web
    --no-cai                         Disable CAI critique

RLHF TRAINING COMMANDS:
  sft <training_file> [epochs] [lr] [batch]  Supervised Fine-Tuning
  train_reward_model <comparisons> [output]  Train reward model on preferences
  ppo <prompts> <reward_model> [iterations]  PPO alignment training
  create_sample_rlhf_data                    Create sample training data

ADVANCED REASONING COMMANDS:
  tree_of_thought <problem> [depth] [branches]  Tree-of-Thought search
  debate <question> [agents] [rounds]           Multi-agent debate
  self_reflect <question> [target_score] [iters]  Self-reflection loop
  combined_reasoning <question>                   All 3 techniques combined

KNOWLEDGE COMMANDS:
  knowledge_load <module>            Load .aiz knowledge module
  knowledge_query <module> <q>       Query specific knowledge module
  knowledge_list                     List loaded modules

TESTING:
  test_block <file>                  Test block access

EXAMPLES:
  neural_engine ai_ask "How does CMIX work?"
  neural_engine learn https://en.wikipedia.org/wiki/BWT
  neural_engine score_response qa_pair.txt
  neural_engine cai_critique qa_pair.txt
  neural_engine train_transformer corpus.txt 7 0.002 16
  neural_engine compress file.txt --best
  neural_engine auto_learn --daemon               # Start self-learning AI
  neural_engine auto_learn --train-threshold=5    # One-shot correction (5 corrections)
  neural_engine sft brain/training/sft_pairs.json 5 0.0005 4  # SFT training
  neural_engine train_reward_model brain/training/comparisons.json  # Train reward model
  neural_engine ppo brain/training/prompts.txt models/reward_model.bin 100  # PPO training
  neural_engine tree_of_thought "How can we solve climate change?" 4 3  # ToT search
  neural_engine debate "What is consciousness?" 3 3  # 3 agents, 3 rounds
  neural_engine self_reflect "Explain quantum entanglement" 85 5  # Target 85%, max 5 iterations
  neural_engine combined_reasoning "How should AI be regulated?"  # Use all techniques

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

    // Self-learning command
    if (cmd == "auto_learn") {
        return self_learning::main_auto_learn(argc, argv);
    }

    // Quantization commands (Week 6)
    if (cmd == "quantize_model" || cmd == "dequantize_model" || cmd == "train_transformer_quantized") {
        return handle_quantize_command(argc, argv);
    }

    // Mixed Precision commands (Week 7)
    if (cmd == "train_transformer_mixed") {
        return handle_mixed_precision_command(argc, argv);
    }

    // Everything else goes to neural engine
    // (knowledge_load, ai_ask, train_transformer, etc.)
    return main_neural_engine(argc, argv);
}
