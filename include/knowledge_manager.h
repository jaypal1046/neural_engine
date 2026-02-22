#pragma once
#include <string>
#include <vector>

// =============================================================================
// Knowledge Manager - Learn, store, and retrieve knowledge
// =============================================================================

// Learn from a URL or file and store compressed
void learn_and_store(const std::string& source);

// Answer question from stored knowledge
void answer_from_knowledge(const std::string& question);

// Smart compress using persistent neural weights
void smart_compress(const std::string& file_path);

// Show brain statistics
void show_brain_status();

// Initialize the brain system (load index, mixers, etc.)
void init_brain();

// Compute embedding for text (calls neural_engine)
std::vector<float> compute_embedding(const std::string& text);

// Generate topic name from URL or filename
std::string generate_topic_name(const std::string& source);
