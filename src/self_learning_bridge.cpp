// =============================================================================
// Self-Learning Bridge - Connect self_learning.cpp to neural_engine.cpp
//
// This file provides REAL implementations that self_learning.cpp can call
// to interact with the main neural engine.
// 
// KEY: These functions call neural_engine.exe via subprocess so they work
// independently of the main process (no circular dependency issues).
// =============================================================================

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <vector>
#include <algorithm>

// Web fetcher for internet learning
#include "web_fetcher.h"
#include "html_parser.h"

namespace fs = std::filesystem;

// =============================================================================
// Helper: Run neural_engine.exe and capture output
// =============================================================================
static std::string run_neural_engine_cmd(const std::string& args) {
    std::string cmd = "bin\\neural_engine.exe " + args + " 2>NUL";
    
    FILE* pipe = _popen(cmd.c_str(), "r");
    if (!pipe) {
        return "{\"error\": \"Failed to execute neural_engine.exe\"}";
    }
    
    std::string result;
    char buffer[4096];
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    _pclose(pipe);
    
    return result;
}

// =============================================================================
// Bridge Functions for Self-Learning Module
// =============================================================================

// Run ai_ask and return response as string
std::string run_ai_ask(const std::string& question) {
    // Escape quotes in question for command line
    std::string escaped = question;
    // Replace " with ' to avoid command line issues
    std::replace(escaped.begin(), escaped.end(), '"', '\'');
    
    std::string output = run_neural_engine_cmd("ai_ask \"" + escaped + "\"");
    
    // Extract answer from JSON output
    // Look for "answer":"..." in the output
    size_t pos = output.find("\"answer\":\"");
    if (pos != std::string::npos) {
        pos += 10; // Skip "answer":"
        size_t end = output.find("\"", pos);
        if (end != std::string::npos) {
            return output.substr(pos, end - pos);
        }
    }
    
    // Fallback: return the raw output
    if (!output.empty()) {
        return output;
    }
    
    return "I don't have enough knowledge to answer that yet.";
}

// Score a response using RewardModel
int run_score_response(const std::string& question, const std::string& answer) {
    // Write Q&A to temp file
    std::string temp_file = "brain/self_learning/temp_qa.txt";
    
    // Ensure directory exists
    fs::create_directories("brain/self_learning");
    
    std::ofstream file(temp_file);
    if (!file) {
        return 0;
    }

    file << "question: " << question << "\n";
    file << "answer: " << answer << "\n";
    file.close();

    // Call score_response command
    std::string output = run_neural_engine_cmd("score_response \"" + temp_file + "\"");

    // Parse score from output
    int score = 50; // Default middle score
    
    // Look for "total:" in output
    size_t pos = output.find("total:");
    if (pos != std::string::npos) {
        std::string score_str = output.substr(pos + 6);
        std::istringstream iss(score_str);
        iss >> score;
    }
    
    // Also try JSON format: "score":N
    pos = output.find("\"score\":");
    if (pos != std::string::npos) {
        std::string score_str = output.substr(pos + 8);
        std::istringstream iss(score_str);
        iss >> score;
    }

    // Cleanup
    std::remove(temp_file.c_str());

    return score;
}

// Run CAI critique on a response
std::string run_cai_critique(const std::string& question, const std::string& answer) {
    // Write Q&A to temp file
    std::string temp_file = "brain/self_learning/temp_cai_qa.txt";
    
    fs::create_directories("brain/self_learning");
    
    std::ofstream file(temp_file);
    if (!file) {
        return "{\"violations\": []}";
    }

    file << "question: " << question << "\n";
    file << "answer: " << answer << "\n";
    file.close();

    // Call cai_critique command
    std::string output = run_neural_engine_cmd("cai_critique \"" + temp_file + "\"");

    // Cleanup
    std::remove(temp_file.c_str());

    if (output.empty()) {
        return "{\"violations\": []}";
    }
    
    return output;
}

// =============================================================================
// REAL Internet Learning: Fetch URL → Extract Text → Save to Knowledge Base  
// =============================================================================
void run_learn(const std::string& source) {
    std::cout << "[LEARN] Fetching: " << source << "\n";
    
    // Ensure knowledge directory exists
    fs::create_directories("brain/knowledge");
    
    // Check if source is a URL or file path
    bool is_url = (source.find("http://") == 0 || source.find("https://") == 0);
    
    std::string text_content;
    std::string source_name;
    
    if (is_url) {
        // Fetch from internet
        HttpResponse resp = fetch_url(source);
        
        if (resp.status_code != 200) {
            std::cerr << "[LEARN] Failed to fetch URL (status " << resp.status_code << "): " << source << "\n";
            return;
        }
        
        std::cout << "[LEARN] Downloaded " << resp.body.size() << " bytes\n";
        
        // Extract clean text from HTML
        text_content = extract_text_from_html(resp.body);
        
        // Create a safe filename from URL
        source_name = source;
        // Remove protocol
        size_t proto_end = source_name.find("://");
        if (proto_end != std::string::npos) {
            source_name = source_name.substr(proto_end + 3);
        }
        // Replace unsafe chars
        for (char& c : source_name) {
            if (c == '/' || c == '\\' || c == ':' || c == '?' || c == '&' ||
                c == '=' || c == '#' || c == '%' || c == ' ') {
                c = '_';
            }
        }
        // Limit filename length
        if (source_name.size() > 100) {
            source_name = source_name.substr(0, 100);
        }
        
    } else {
        // Read from local file
        std::ifstream file(source);
        if (!file) {
            std::cerr << "[LEARN] Cannot open file: " << source << "\n";
            return;
        }
        
        text_content = std::string(
            (std::istreambuf_iterator<char>(file)),
            std::istreambuf_iterator<char>()
        );
        
        source_name = fs::path(source).stem().string();
    }
    
    // Filter out very short content
    if (text_content.size() < 50) {
        std::cerr << "[LEARN] Content too short (" << text_content.size() << " chars), skipping\n";
        return;
    }
    
    // Save to knowledge base
    std::string output_file = "brain/knowledge/" + source_name + ".txt";
    std::ofstream out(output_file);
    if (out) {
        out << text_content;
        out.close();
        std::cout << "[LEARN] Saved " << text_content.size() << " chars to " << output_file << "\n";
    }
    
    // Also append to a combined training corpus for future retraining
    std::string corpus_file = "brain/self_learning/learned_corpus.txt";
    std::ofstream corpus(corpus_file, std::ios::app);
    if (corpus) {
        // Write each sentence as a line
        std::istringstream iss(text_content);
        std::string line;
        int lines_added = 0;
        while (std::getline(iss, line)) {
            // Skip very short lines
            if (line.size() > 20) {
                corpus << line << "\n";
                lines_added++;
            }
        }
        corpus.close();
        std::cout << "[LEARN] Added " << lines_added << " lines to training corpus\n";
    }
    
    std::cout << "[LEARN] ✅ Learning complete from: " << source << "\n";
}

// =============================================================================
// REAL Training: Run transformer training on a corpus file
// =============================================================================
void run_train_transformer(const std::string& corpus_file, int epochs, float lr, int batch_size) {
    std::cout << "[TRAIN] Starting transformer training on: " << corpus_file << "\n";
    std::cout << "[TRAIN] Epochs: " << epochs << ", LR: " << lr << ", Batch: " << batch_size << "\n";
    
    // Check if corpus file exists
    if (!fs::exists(corpus_file)) {
        std::cerr << "[TRAIN] Error: Corpus file not found: " << corpus_file << "\n";
        return;
    }
    
    // Check corpus size
    auto file_size = fs::file_size(corpus_file);
    std::cout << "[TRAIN] Corpus size: " << file_size << " bytes\n";
    
    if (file_size < 100) {
        std::cerr << "[TRAIN] Corpus too small, need at least 100 bytes\n";
        return;
    }
    
    // Run training via subprocess 
    std::string cmd = "bin\\neural_engine.exe train_transformer \"" + corpus_file + "\" " 
                      + std::to_string(epochs) + " " 
                      + std::to_string(lr) + " " 
                      + std::to_string(batch_size);
    
    std::cout << "[TRAIN] Executing: " << cmd << "\n";
    
    int result = system(cmd.c_str());
    
    if (result == 0) {
        std::cout << "[TRAIN] ✅ Training complete!\n";
    } else {
        std::cerr << "[TRAIN] ⚠️ Training exited with code: " << result << "\n";
    }
}
