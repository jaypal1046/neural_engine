#pragma once
#include <string>
#include <vector>
#include <functional>

// =============================================================================
// Internet Learning Pipeline
// 
// Enables the AI to autonomously learn from the internet:
// 1. Discover topics it needs to learn about (from weak responses)
// 2. Fetch knowledge from Wikipedia and educational sources
// 3. Clean and process the content
// 4. Add to training corpus
// 5. Incrementally retrain the transformer
//
// This is the KEY module that makes the AI self-independent.
// =============================================================================

namespace internet_learning {

// Configuration for internet learning
struct InternetLearningConfig {
    // Wikipedia settings
    std::string wikipedia_lang = "en";          // Language code
    bool use_simple_wikipedia = true;           // Use Simple English Wikipedia first
    int max_articles_per_topic = 3;             // Articles to fetch per topic
    int max_content_length = 50000;             // Max chars per article
    
    // Content filtering
    int min_sentence_length = 20;               // Min chars per sentence
    int max_sentence_length = 500;              // Max chars per sentence
    bool filter_references = true;              // Remove reference sections
    bool filter_see_also = true;                // Remove "See also" sections
    
    // Training settings
    int min_corpus_lines = 100;                 // Min lines before triggering training
    int training_epochs = 3;                    // Quick retraining epochs
    float training_lr = 0.001f;                 // Higher LR for fine-tuning
    int training_batch_size = 8;
    
    // Auto-discovery
    bool enable_auto_discovery = true;          // Auto-discover topics from weak responses
    int max_topics_per_cycle = 5;               // Topics to learn per cycle
};

// Represents a topic the AI wants to learn about
struct LearningTopic {
    std::string topic;              // e.g., "machine learning"
    std::string reason;             // Why it needs to learn this
    int priority;                   // 1-10, higher = more important
    bool learned;                   // Whether we've already fetched content
    std::string source_question;    // The original question that triggered this
};

// Represents fetched content from the internet
struct FetchedContent {
    std::string url;                // Source URL
    std::string title;              // Article title
    std::string raw_html;           // Raw HTML content
    std::string clean_text;         // Cleaned text
    int word_count;                 // Number of words
    float quality_score;            // 0-1, estimated quality
    std::string topic;              // What topic this is about
};

// Learning session result
struct LearningSession {
    std::string timestamp;
    int topics_discovered;
    int articles_fetched;
    int total_words_learned;
    int corpus_lines_added;
    bool training_triggered;
    float training_loss;            // Final loss if training was triggered
    std::vector<std::string> topics_learned;
    std::vector<std::string> failed_topics;
};

// =============================================================================
// Core Functions
// =============================================================================

// Discover topics the AI needs to learn about
// Analyzes weak responses and extracts missing knowledge areas
std::vector<LearningTopic> discover_topics(const InternetLearningConfig& config);

// Fetch Wikipedia article for a topic
// Returns cleaned content ready for training
FetchedContent fetch_wikipedia(const std::string& topic, const InternetLearningConfig& config);

// Fetch from Simple English Wikipedia (easier to learn from)
FetchedContent fetch_simple_wikipedia(const std::string& topic, const InternetLearningConfig& config);

// Clean and process fetched content for training
std::string process_content(const FetchedContent& content, const InternetLearningConfig& config);

// Add processed content to the training corpus
int add_to_corpus(const std::string& content, const std::string& corpus_path);

// Run a complete learning session
// Discovers topics → fetches content → processes → trains
LearningSession run_learning_session(const InternetLearningConfig& config);

// =============================================================================
// Wikipedia API Helpers
// =============================================================================

// Build Wikipedia API URL for fetching article content
std::string build_wikipedia_api_url(const std::string& title, const std::string& lang = "en");

// Build Simple Wikipedia URL
std::string build_simple_wikipedia_url(const std::string& title);

// Extract article text from Wikipedia API JSON response
std::string extract_wikipedia_text(const std::string& json_response);

// Search Wikipedia for related articles
std::vector<std::string> search_wikipedia(const std::string& query, int max_results = 5);

// =============================================================================
// Content Quality Assessment
// =============================================================================

// Score content quality (0-1)
// Higher = more useful for training
float score_content_quality(const std::string& text);

// Filter out noise from content
std::string filter_noise(const std::string& text);

// Split text into clean training sentences
std::vector<std::string> split_into_sentences(const std::string& text);

// =============================================================================
// Learning History
// =============================================================================

// Save learning session to history
void save_session(const LearningSession& session);

// Load all past learning sessions
std::vector<LearningSession> load_session_history();

// Get list of all topics already learned
std::vector<std::string> get_learned_topics();

// Check if a topic has already been learned
bool is_topic_learned(const std::string& topic);

// =============================================================================
// Main Entry Point
// =============================================================================

// Run the internet learning daemon
// This is the main loop that continuously learns from the internet
int main_internet_learn(int argc, char** argv);

} // namespace internet_learning
