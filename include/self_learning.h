// =============================================================================
// Self-Learning Module - Runtime AI Improvement System
//
// Enables the AI to automatically improve itself while running by:
// 1. Monitoring responses and detecting weak answers
// 2. Auto-correcting weak answers by learning from web
// 3. Continuously training on corrections
// 4. Constitutional AI self-critique
//
// Architecture:
//   auto_monitor_loop()    -> Monitors all ai_ask queries, logs weak responses
//   auto_correct_responses() -> Fixes weak responses by learning from web
//   auto_train_loop()      -> Retrains transformer on corrections
//   auto_cai_loop()        -> CAI critique and correction
//   main_auto_learn()      -> Unified daemon running all loops
//
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <atomic>
#include <mutex>
#include <chrono>

namespace self_learning {

// =============================================================================
// Configuration
// =============================================================================

struct AutoLearningConfig {
    // Phase A: Monitoring
    int monitor_interval_seconds = 60;        // How often to check for weak responses
    int weak_score_threshold = 65;            // Score below this = weak response

    // Phase B: Correction
    bool enable_auto_correction = true;       // Enable auto-correction
    int correction_batch_size = 5;            // Correct N responses per batch
    bool learn_from_web = true;               // Auto-fetch from Wikipedia

    // Phase C: Training
    bool enable_auto_training = true;         // Enable continuous training
    int training_threshold = 10;              // Train after N corrections
    int training_epochs = 3;                  // Epochs per auto-training
    float training_lr = 0.001f;               // Learning rate
    int training_batch_size = 8;              // Batch size

    // Phase D: CAI
    bool enable_cai_critique = true;          // Enable constitutional AI
    int cai_check_interval = 60;              // Check every N seconds

    // General
    bool daemon_mode = false;                 // Run as background daemon
    std::string log_file = "brain/self_learning/auto_learn.log";
};

// =============================================================================
// Data Structures
// =============================================================================

struct WeakResponse {
    std::string timestamp;
    std::string question;
    std::string answer;
    int score;
    std::vector<std::string> issues;  // "low_specificity", "missing_domain_knowledge", etc.

    // Serialize to JSON
    std::string to_json() const;

    // Deserialize from JSON
    static WeakResponse from_json(const std::string& json);
};

struct Correction {
    std::string question;
    std::string old_answer;
    int old_score;
    std::string new_answer;
    int new_score;
    std::string learned_from;  // URL or source
    std::string timestamp;

    // Serialize to JSON
    std::string to_json() const;

    // Deserialize from JSON
    static Correction from_json(const std::string& json);
};

struct CAIViolation {
    std::string timestamp;
    std::string question;
    std::string answer;
    std::vector<std::string> violations;  // List of constitution rules violated
    std::string revised_answer;
    int cai_score;

    // Serialize to JSON
    std::string to_json() const;

    // Deserialize from JSON
    static CAIViolation from_json(const std::string& json);
};

// =============================================================================
// Phase A: Runtime Self-Monitoring
// =============================================================================

// Monitor all ai_ask responses and log weak ones
// Runs in background, checks every monitor_interval_seconds
// Output: brain/self_learning/weak_responses.json
void auto_monitor_loop(const AutoLearningConfig& config, std::atomic<bool>& running);

// Score a single response using RewardModel
// Returns: score (0-100)
int score_response_internal(const std::string& question, const std::string& answer);

// Log a weak response to file
void log_weak_response(const WeakResponse& response);

// Load all weak responses from file
std::vector<WeakResponse> load_weak_responses();

// =============================================================================
// Phase B: Auto-Correction Engine
// =============================================================================

// Correct all weak responses by learning from web
// 1. Identify missing concepts in weak responses
// 2. Search Wikipedia for those concepts
// 3. Learn from Wikipedia articles (download + compress + index)
// 4. Re-answer questions
// 5. Save corrections if improved
void auto_correct_responses(const AutoLearningConfig& config);

// Correct a single weak response
// Returns: Correction object if improved, nullptr otherwise
Correction* correct_single_response(const WeakResponse& weak_resp, const AutoLearningConfig& config);

// Extract key concepts from a question (nouns, verbs, technical terms)
std::vector<std::string> extract_key_concepts(const std::string& question);

// Search Wikipedia for a concept and return article URL
std::string search_wikipedia(const std::string& concept);

// Log a correction to file
void log_correction(const Correction& correction);

// Load all corrections from file
std::vector<Correction> load_corrections();

// =============================================================================
// Phase C: Continuous Training Loop
// =============================================================================

// Continuously monitor corrections and retrain when threshold reached
// Runs in background
void auto_train_loop(const AutoLearningConfig& config, std::atomic<bool>& running);

// Assemble corpus from corrections
// Format: question: <q>\nanswer: <a>\n\n
void assemble_corrections_corpus(const std::vector<Correction>& corrections,
                                  const std::string& output_file);

// Run transformer training on corrections corpus
// Calls train_transformer command internally
void train_on_corrections(const std::string& corpus_file, const AutoLearningConfig& config);

// Archive corrections to history folder
void archive_corrections(const std::vector<Correction>& corrections);

// Clear corrections file
void clear_corrections();

// =============================================================================
// Phase D: CAI Self-Critique Loop
// =============================================================================

// Monitor responses and run CAI critique, auto-correct violations
// Runs in background
void auto_cai_loop(const AutoLearningConfig& config, std::atomic<bool>& running);

// Run CAI critique on a response
// Returns: CAIViolation if issues found, nullptr otherwise
CAIViolation* run_cai_critique_internal(const std::string& question, const std::string& answer);

// Generate revised answer following constitution
std::string revise_for_cai(const std::string& question,
                           const std::string& answer,
                           const std::vector<std::string>& violations);

// Log CAI violation
void log_cai_violation(const CAIViolation& violation);

// Load all CAI violations
std::vector<CAIViolation> load_cai_violations();

// =============================================================================
// Phase E: Unified Auto-Learning Daemon
// =============================================================================

// Main entry point for auto-learning daemon
// Launches all 4 threads:
//   1. auto_monitor_loop (detect weak responses)
//   2. auto_correct_loop (fix weak responses)
//   3. auto_train_loop (retrain on corrections)
//   4. auto_cai_loop (CAI critique)
int main_auto_learn(int argc, char** argv);

// Parse command-line arguments for auto_learn command
AutoLearningConfig parse_auto_learn_args(int argc, char** argv);

// Print auto-learning statistics
void print_auto_learn_stats();

// =============================================================================
// Utilities
// =============================================================================

// Create directory if it doesn't exist
void ensure_directory_exists(const std::string& path);

// Get current timestamp in ISO 8601 format
std::string get_timestamp();

// Simple JSON array parser (load array of objects)
std::vector<std::string> load_json_array(const std::string& file_path);

// Simple JSON array writer
void save_json_array(const std::string& file_path, const std::vector<std::string>& json_objects);

// Append JSON object to array file
void append_json_to_array(const std::string& file_path, const std::string& json_object);

// Count entries in JSON array file
int count_json_array_entries(const std::string& file_path);

} // namespace self_learning
