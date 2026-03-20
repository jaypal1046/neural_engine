#include "cloud_teacher.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <array>
#include <memory>
#include <regex>

namespace CloudTeacher {

    std::string exec_python_cloud(const char* cmd) {
        std::array<char, 256> buffer;
        std::string result;
#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
#endif
        if (!pipe) {
            return "Error: Could not execute Python API call.";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    std::string escape_shell(const std::string& str) {
        std::string escaped = "\"";
        for (char c : str) {
            if (c == '"') escaped += "\\\"";
            else escaped += c;
        }
        escaped += "\"";
        return escaped;
    }

    std::string ask_claude(const std::string& prompt) {
        std::string cmd = "python ../query_claude.py " + escape_shell(prompt);
#ifdef _WIN32
        cmd = "python query_claude.py " + escape_shell(prompt);
#endif
        std::string output = exec_python_cloud(cmd.c_str());
        
        size_t start = output.find("CLAUDE_ANSWER_START");
        size_t end = output.find("CLAUDE_ANSWER_END");
        
        if (start != std::string::npos && end != std::string::npos) {
            start += 19; // length of "CLAUDE_ANSWER_START" usually has a newline next to it
            // Strip leading newline if present
            if (output[start] == '\n') start++;
            if (output[start] == '\r' && output[start+1] == '\n') start += 2;
            
            return output.substr(start, end - start - 1);
        }
        
        return output; // Returned error line or whatever was printed
    }

    bool bootstrap_from_claude() {
        std::cout << "\n========================================================" << std::endl;
        std::cout << " CLOUD TEACHER - CLAUDE 3.5 SONNET API INTEGRATION" << std::endl;
        std::cout << "========================================================" << std::endl;

        std::string prompt = "Explain the fundamental laws of thermodynamics plainly, one per line. Do not use formatting or markdown, just pure text sentences.";
        std::cout << "[CLOUD] Asking Claude: \"" << prompt << "\"...\n";
        
        std::string answer = ask_claude(prompt);
        
        if (answer.find("Error:") != std::string::npos || answer.find("Exception:") != std::string::npos) {
             std::cout << answer << std::endl;
             std::cout << "[CLOUD] Request failed! Ensure you set ANTHROPIC_API_KEY.\n";
             return false;
        }
        
        std::cout << "\n--- CLAUDE'S RESPONSE ---\n" << answer << "\n--------------------------\n";

        std::string corpus_path = "brain/self_learning/internet_corpus.txt";
        std::ofstream corpus_file(corpus_path, std::ios::app);
        
        if (!corpus_file.is_open()) return false;
        
        // Basic cleanup of newlines to make sure they're valid sentences
        std::string clean = std::regex_replace(answer, std::regex("\r\n|\n\r"), "\n");
        corpus_file << clean << "\n";
        
        std::cout << "[CLOUD] Appended high-quality data to " << corpus_path << ".\n";
        
        // Trigger generic mini-transformer update
        std::cout << "[CLOUD] Starting self-improvement phase via MiniTransformer...\n";
#ifdef _WIN32
        std::system("bin\\neural_engine.exe train_transformer brain\\self_learning\\internet_corpus.txt 3");
#else
        std::system("./bin/neural_engine train_transformer brain/self_learning/internet_corpus.txt 3");
#endif
        return true;
    }
}
