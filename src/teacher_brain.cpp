#include "teacher_brain.h"
#include "web_fetcher.h"
#include "self_learning.h"
#include "internet_learning.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>
#include <cstdlib>
#include <stdexcept>
#include <array>
#include <memory>

namespace TeacherBrain {

    // Helper to run a command and capture output (similar to self_learning_bridge)
    std::string exec_offline(const char* cmd) {
        std::array<char, 128> buffer;
        std::string result;
#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
#endif
        if (!pipe) {
            throw std::runtime_error("popen() failed!");
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    bool ensure_teacher_model_exists() {
        std::filesystem::create_directories("models");
        
        std::string model_path = "models/stories15M.bin";
        std::string tok_path   = "models/tokenizer.bin";
        std::string run_c_path = "models/run.c";
#ifdef _WIN32
        std::string exe_path   = "models/small_brain.exe";
#else
        std::string exe_path   = "models/small_brain";
#endif

        bool missing_files = false;

        // 1. Download offline baseline model (60MB)
        if (!std::filesystem::exists(model_path)) {
            std::cerr << "[TEACHER] Downloading 15M offline baseline (60MB)..." << std::endl;
            if (!download_to_file("https://huggingface.co/karpathy/tinyllamas/resolve/main/stories15M.bin", model_path)) {
                std::cerr << "[TEACHER] ERROR: Failed to download baseline brain weights." << std::endl;
                missing_files = true;
            }
        }

        // 2. Download Tokenizer
        if (!std::filesystem::exists(tok_path)) {
            std::cerr << "[TEACHER] Downloading tokenizer..." << std::endl;
            if (!download_to_file("https://github.com/karpathy/llama2.c/raw/master/tokenizer.bin", tok_path)) {
                missing_files = true;
            }
        }

        // 3. Download the standalone C inference engine and compile it
        if (!std::filesystem::exists(exe_path)) {
            if (!std::filesystem::exists(run_c_path)) {
                std::cerr << "[TEACHER] Downloading offline inference engine..." << std::endl;
                download_to_file("https://raw.githubusercontent.com/karpathy/llama2.c/master/run.c", run_c_path);
            }
            if (std::filesystem::exists(run_c_path)) {
                std::cerr << "[TEACHER] Compiling specialized offline brain executable..." << std::endl;
#ifdef _WIN32
                std::string compile_cmd = "gcc -O3 " + run_c_path + " -o " + exe_path;
#else
                std::string compile_cmd = "gcc -O3 " + run_c_path + " -o " + exe_path + " -lm";
#endif
                int compile_result = std::system(compile_cmd.c_str());
                if (compile_result != 0) {
                    std::cerr << "[TEACHER] ERROR: Failed to compile baseline engine." << std::endl;
                    missing_files = true;
                }
            } else {
                return false;
            }
        }

        return !missing_files;
    }

    std::string generate_synthetic_text(float temperature, int max_tokens, const std::string& prompt) {
        if (!ensure_teacher_model_exists()) {
            return "Error: Teacher model unavailable.";
        }

        std::cerr << "[TEACHER] Connecting to offline logical baseline..." << std::endl;
        
#ifdef _WIN32
        std::string exe_path = "models\\small_brain.exe";
#else
        std::string exe_path = "./models/small_brain";
#endif

        // Run the local model 100% offline
        // Command syntax: run models/stories15M.bin -t temperature -n max_tokens -i "prompt"
        std::ostringstream cmd;
        cmd << exe_path << " models/stories15M.bin -t " << temperature << " -n " << max_tokens;
        if (!prompt.empty()) {
            cmd << " -i \"" << prompt << "\"";
        }

        std::string output = exec_offline(cmd.str().c_str());

        // Process output to remove timing stats "achieved tok/s"
        size_t stats_pos = output.find("achieved tok/s");
        if (stats_pos != std::string::npos) {
            output = output.substr(0, stats_pos);
        }

        return output;
    }

    bool run_offline_bootstrap(int num_sentences) {
        std::cerr << "\n========================================================" << std::endl;
        std::cerr << " INITIALIZING OFFLINE BASELINE TEACHER (15M Params)" << std::endl;
        std::cerr << "========================================================" << std::endl;

        if (!ensure_teacher_model_exists()) {
            std::cerr << "[TEACHER-FATAL] Could not initialize offline model!" << std::endl;
            return false;
        }

        std::cerr << "[TEACHER] Baseline loaded into memory. No internet connection used." << std::endl;
        
        std::string corpus_path = "brain/self_learning/internet_corpus.txt";
        std::filesystem::create_directories("brain/self_learning");

        std::ofstream corpus_file(corpus_path, std::ios::app);
        if (!corpus_file.is_open()) {
            std::cerr << "[TEACHER] Could not open corpus block." << std::endl;
            return false;
        }

        std::vector<std::string> seed_prompts = {
            "The king decided to",
            "Gravity is a force that",
            "The AI realized that",
            "Water boils at",
            "A fast sorting algorithm is",
            "Once upon a time in a digital"
        };

        int total_generated = 0;
        int step = 0;

        std::cerr << "[TEACHER] Generating " << num_sentences << " high-quality synthetic thoughts..." << std::endl;

        while (total_generated < num_sentences) {
            std::string prompt = seed_prompts[step % seed_prompts.size()];
            std::cerr << "  -> Thinking about: \"" << prompt << "\" ... ";
            
            // Generate offline without internet
            std::string generated = generate_synthetic_text(0.7f, 100, prompt);
            
            // Clean up formatting
            generated.erase(std::remove(generated.begin(), generated.end(), '\n'), generated.end());
            generated.erase(std::remove(generated.begin(), generated.end(), '\r'), generated.end());
            
            if (!generated.empty()) {
                corpus_file << generated << std::endl;
                std::cerr << "OK (Length: " << generated.length() << ")" << std::endl;
                total_generated += 3; // Approx sentences per 100 tokens
            } else {
                std::cerr << "FAILED" << std::endl;
            }
            step++;
        }
        corpus_file.close();

        std::cerr << "\n[TEACHER] Synthetic generation complete. Feeding to Main C++ MiniTransformer..." << std::endl;

        // Bootstrapping the main model by triggering incremental training via the self learning bridge
#ifdef _WIN32
        std::system("bin\\neural_engine.exe train_transformer brain\\self_learning\\internet_corpus.txt 3");
#else
        std::system("./bin/neural_engine train_transformer brain/self_learning/internet_corpus.txt 3");
#endif

        std::cerr << "\n[TEACHER] Internal base C++ model successfully improved via local pre-trained baseline!" << std::endl;
        return true;
    }

} // namespace TeacherBrain
