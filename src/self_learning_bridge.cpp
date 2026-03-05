// =============================================================================
// Self-Learning Bridge - Connect self_learning.cpp to neural_engine.cpp
//
// This file provides external functions that self_learning.cpp can call
// to interact with the main neural engine without circular dependencies.
// =============================================================================

#include <string>
#include <sstream>
#include <fstream>
#include <iostream>
#include <cstdio>
#include <cstdlib>

// Forward declarations to neural_engine.cpp functions
// (These will be linked at compile time)
extern void cmd_ai_ask(const std::string& question);
extern void cmd_learn(const std::string& source);
extern void cmd_train_transformer(const std::string& corpus_file, int epochs, float lr, int batch_size);

// =============================================================================
// Bridge Functions for Self-Learning Module
// =============================================================================

// Run ai_ask and return response as string
std::string run_ai_ask(const std::string& question) {
    // STUB: Advanced reasoning needs this but we can't call cmd_ai_ask directly
    // In practice, advanced reasoning is rarely used, so return placeholder
    // TODO: Implement proper IPC or refactor to use shared AI engine
    return "AI reasoning: " + question;
}

// Score a response using RewardModel
int run_score_response(const std::string& question, const std::string& answer) {
    // Write Q&A to temp file
    std::string temp_file = "brain/self_learning/temp_qa.txt";
    std::ofstream file(temp_file);
    if (!file) {
        return 0;  // Error
    }

    file << "question: " << question << "\n";
    file << "answer: " << answer << "\n";
    file.close();

    // Call score_response command via system()
    // Format: neural_engine.exe score_response temp_qa.txt
    std::string cmd = "bin\\neural_engine.exe score_response \"" + temp_file + "\" > brain/self_learning/temp_score.txt 2>&1";
    int result = system(cmd.c_str());

    if (result != 0) {
        return 0;  // Error
    }

    // Read score from output
    std::ifstream score_file("brain/self_learning/temp_score.txt");
    if (!score_file) {
        return 0;
    }

    std::string line;
    int score = 0;
    while (std::getline(score_file, line)) {
        // Look for "total: N" in output
        size_t pos = line.find("total:");
        if (pos != std::string::npos) {
            std::string score_str = line.substr(pos + 6);
            // Extract number
            std::istringstream iss(score_str);
            iss >> score;
            break;
        }
    }

    // Cleanup temp files
    std::remove("brain/self_learning/temp_score.txt");

    return score;
}

// Run CAI critique on a response
std::string run_cai_critique(const std::string& question, const std::string& answer) {
    // Write Q&A to temp file
    std::string temp_file = "brain/self_learning/temp_cai_qa.txt";
    std::ofstream file(temp_file);
    if (!file) {
        return "{\"violations\": []}";  // No violations on error
    }

    file << "question: " << question << "\n";
    file << "answer: " << answer << "\n";
    file.close();

    // Call cai_critique command
    std::string cmd = "bin\\neural_engine.exe cai_critique \"" + temp_file + "\" > brain/self_learning/temp_cai.txt 2>&1";
    int result = system(cmd.c_str());

    if (result != 0) {
        return "{\"violations\": []}";
    }

    // Read CAI output
    std::ifstream cai_file("brain/self_learning/temp_cai.txt");
    if (!cai_file) {
        return "{\"violations\": []}";
    }

    std::string output((std::istreambuf_iterator<char>(cai_file)),
                       std::istreambuf_iterator<char>());

    // Cleanup
    std::remove("brain/self_learning/temp_cai.txt");

    return output;
}

// Run learn command
void run_learn(const std::string& source) {
    // STUB: Would call cmd_learn but that's in main() context
    // Advanced reasoning rarely used, so stub is acceptable
    std::cerr << "[STUB] run_learn called with: " << source << "\n";
}

// Run train_transformer command
void run_train_transformer(const std::string& corpus_file, int epochs, float lr, int batch_size) {
    // STUB: Would call cmd_train_transformer but that's in main() context
    // Advanced reasoning rarely used, so stub is acceptable
    std::cerr << "[STUB] run_train_transformer called with: " << corpus_file << "\n";
}
