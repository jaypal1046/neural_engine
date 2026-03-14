// =============================================================================
// Internet Learning Pipeline - Implementation
// 
// Makes the AI self-independent by learning from the internet automatically.
// Uses Wikipedia API to fetch knowledge, processes it into clean training data,
// and incrementally retrains the transformer.
// =============================================================================

#include "internet_learning.h"
#include "web_fetcher.h"
#include "html_parser.h"
#include "self_learning.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <filesystem>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <set>
#include <thread>

namespace fs = std::filesystem;

// External functions
extern void run_learn(const std::string& source);
extern void run_train_transformer(const std::string& corpus_file, int epochs, float lr, int batch_size);

namespace internet_learning {

// =============================================================================
// Utilities
// =============================================================================

static std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

static void ensure_dirs() {
    fs::create_directories("brain/self_learning");
    fs::create_directories("brain/self_learning/sessions");
    fs::create_directories("brain/knowledge");
    fs::create_directories("brain/training");
}

// URL encode a string for use in URLs
static std::string url_encode(const std::string& str) {
    std::string result;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            result += c;
        } else if (c == ' ') {
            result += "%20";
        } else {
            char hex[4];
            snprintf(hex, sizeof(hex), "%%%02X", (unsigned char)c);
            result += hex;
        }
    }
    return result;
}

// Simple JSON string extraction
static std::string json_extract_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\":\"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) {
        // Try with space after colon
        search = "\"" + key + "\": \"";
        pos = json.find(search);
        if (pos == std::string::npos) return "";
    }
    
    pos += search.size();
    
    // Find end quote (handling escaped quotes)
    std::string result;
    for (size_t i = pos; i < json.size(); i++) {
        if (json[i] == '\\' && i + 1 < json.size()) {
            if (json[i+1] == '"') { result += '"'; i++; }
            else if (json[i+1] == 'n') { result += '\n'; i++; }
            else if (json[i+1] == '\\') { result += '\\'; i++; }
            else { result += json[i]; }
        } else if (json[i] == '"') {
            break;
        } else {
            result += json[i];
        }
    }
    
    return result;
}

// =============================================================================
// Wikipedia API
// =============================================================================

std::string build_wikipedia_api_url(const std::string& title, const std::string& lang) {
    // Use Wikipedia REST API to get article extract
    // This returns plain text content (no HTML parsing needed)
    std::string encoded_title = url_encode(title);
    return "https://" + lang + ".wikipedia.org/api/rest_v1/page/summary/" + encoded_title;
}

std::string build_simple_wikipedia_url(const std::string& title) {
    // Simple English Wikipedia - easier to learn from
    std::string encoded_title = url_encode(title);
    return "https://simple.wikipedia.org/api/rest_v1/page/summary/" + encoded_title;
}

std::string extract_wikipedia_text(const std::string& json_response) {
    // The REST API returns JSON with "extract" containing clean text
    std::string extract = json_extract_string(json_response, "extract");
    
    if (extract.empty()) {
        // Try "extract_html" and strip HTML
        std::string html_extract = json_extract_string(json_response, "extract_html");
        if (!html_extract.empty()) {
            extract = extract_text_from_html(html_extract);
        }
    }
    
    return extract;
}

std::vector<std::string> search_wikipedia(const std::string& query, int max_results) {
    std::vector<std::string> results;
    
    // Use Wikipedia search API
    std::string encoded_query = url_encode(query);
    std::string url = "https://en.wikipedia.org/w/api.php?action=opensearch&search=" 
                      + encoded_query 
                      + "&limit=" + std::to_string(max_results)
                      + "&namespace=0&format=json";
    
    HttpResponse resp = fetch_url(url);
    if (resp.status_code != 200) {
        std::cerr << "[INTERNET-LEARN] Wikipedia search failed (status " << resp.status_code << ")\n";
        return results;
    }
    
    // Parse opensearch JSON response
    // Format: ["query", ["result1", "result2", ...], ...]
    // Find the second array (titles)
    size_t first_bracket = resp.body.find("[", 1);
    if (first_bracket == std::string::npos) return results;
    
    size_t second_bracket = resp.body.find("[", first_bracket + 1);
    if (second_bracket == std::string::npos) return results;
    
    size_t end_bracket = resp.body.find("]", second_bracket);
    if (end_bracket == std::string::npos) return results;
    
    std::string titles_str = resp.body.substr(second_bracket + 1, end_bracket - second_bracket - 1);
    
    // Extract quoted strings
    size_t pos = 0;
    while (pos < titles_str.size()) {
        size_t q1 = titles_str.find("\"", pos);
        if (q1 == std::string::npos) break;
        size_t q2 = titles_str.find("\"", q1 + 1);
        if (q2 == std::string::npos) break;
        
        std::string title = titles_str.substr(q1 + 1, q2 - q1 - 1);
        if (!title.empty()) {
            results.push_back(title);
        }
        pos = q2 + 1;
    }
    
    return results;
}

// =============================================================================
// Content Fetching
// =============================================================================

FetchedContent fetch_wikipedia(const std::string& topic, const InternetLearningConfig& config) {
    FetchedContent content;
    content.topic = topic;
    content.quality_score = 0.0f;
    
    // Build URL
    content.url = build_wikipedia_api_url(topic, config.wikipedia_lang);
    
    std::cout << "[INTERNET-LEARN] Fetching Wikipedia: " << topic << "\n";
    
    // Fetch
    HttpResponse resp = fetch_url(content.url);
    
    if (resp.status_code != 200) {
        std::cerr << "[INTERNET-LEARN] Failed to fetch '" << topic << "' (status " << resp.status_code << ")\n";
        return content;
    }
    
    content.raw_html = resp.body;
    content.title = json_extract_string(resp.body, "title");
    content.clean_text = extract_wikipedia_text(resp.body);
    
    // Count words
    int word_count = 0;
    bool in_word = false;
    for (char c : content.clean_text) {
        if (isspace(c)) {
            if (in_word) word_count++;
            in_word = false;
        } else {
            in_word = true;
        }
    }
    if (in_word) word_count++;
    content.word_count = word_count;
    
    // Score quality
    content.quality_score = score_content_quality(content.clean_text);
    
    std::cout << "[INTERNET-LEARN] Got '" << content.title 
              << "' (" << content.word_count << " words, quality=" 
              << (int)(content.quality_score * 100) << "%)\n";
    
    return content;
}

FetchedContent fetch_simple_wikipedia(const std::string& topic, const InternetLearningConfig& config) {
    FetchedContent content;
    content.topic = topic;
    content.quality_score = 0.0f;
    
    content.url = build_simple_wikipedia_url(topic);
    
    std::cout << "[INTERNET-LEARN] Fetching Simple Wikipedia: " << topic << "\n";
    
    HttpResponse resp = fetch_url(content.url);
    
    if (resp.status_code != 200) {
        // Fall back to regular Wikipedia
        std::cout << "[INTERNET-LEARN] Simple Wikipedia doesn't have '" << topic << "', trying regular...\n";
        return fetch_wikipedia(topic, config);
    }
    
    content.raw_html = resp.body;
    content.title = json_extract_string(resp.body, "title");
    content.clean_text = extract_wikipedia_text(resp.body);
    
    // Count words
    int word_count = 0;
    bool in_word = false;
    for (char c : content.clean_text) {
        if (isspace(c)) {
            if (in_word) word_count++;
            in_word = false;
        } else {
            in_word = true;
        }
    }
    if (in_word) word_count++;
    content.word_count = word_count;
    
    content.quality_score = score_content_quality(content.clean_text);
    
    std::cout << "[INTERNET-LEARN] Got '" << content.title 
              << "' (" << content.word_count << " words)\n";
    
    return content;
}

// =============================================================================
// Full Article Fetch (get more than just summary)
// =============================================================================

static FetchedContent fetch_full_article(const std::string& topic, const InternetLearningConfig& config) {
    FetchedContent content;
    content.topic = topic;
    content.quality_score = 0.0f;
    
    // Use the mobile HTML endpoint for cleaner content
    std::string encoded = url_encode(topic);
    content.url = "https://" + config.wikipedia_lang + ".wikipedia.org/api/rest_v1/page/mobile-html/" + encoded;
    
    std::cout << "[INTERNET-LEARN] Fetching full article: " << topic << "\n";
    
    HttpResponse resp = fetch_url(content.url);
    
    if (resp.status_code != 200) {
        // Fall back to summary
        return config.use_simple_wikipedia ? 
               fetch_simple_wikipedia(topic, config) : 
               fetch_wikipedia(topic, config);
    }
    
    content.raw_html = resp.body;
    content.title = topic;
    
    // Extract text from full HTML
    content.clean_text = extract_text_from_html(resp.body);
    
    // Truncate if too long
    if ((int)content.clean_text.size() > config.max_content_length) {
        content.clean_text = content.clean_text.substr(0, config.max_content_length);
    }
    
    // Count words
    int word_count = 0;
    bool in_word = false;
    for (char c : content.clean_text) {
        if (isspace(c)) {
            if (in_word) word_count++;
            in_word = false;
        } else {
            in_word = true;
        }
    }
    if (in_word) word_count++;
    content.word_count = word_count;
    
    content.quality_score = score_content_quality(content.clean_text);
    
    std::cout << "[INTERNET-LEARN] Full article: " << content.word_count << " words\n";
    
    return content;
}

// =============================================================================
// Content Processing
// =============================================================================

float score_content_quality(const std::string& text) {
    if (text.empty()) return 0.0f;
    
    float score = 0.5f; // Base score
    
    // Length bonus (longer = more information)
    if (text.size() > 100) score += 0.1f;
    if (text.size() > 500) score += 0.1f;
    if (text.size() > 2000) score += 0.1f;
    
    // Check for complete sentences (periods)
    int period_count = 0;
    for (char c : text) {
        if (c == '.') period_count++;
    }
    if (period_count > 3) score += 0.1f;
    
    // Penalize if too many special characters (likely noise)
    int special_count = 0;
    for (char c : text) {
        if (!isalnum(c) && !isspace(c) && c != '.' && c != ',' && c != ';' && c != ':' && c != '!' && c != '?') {
            special_count++;
        }
    }
    float special_ratio = (float)special_count / text.size();
    if (special_ratio > 0.1f) score -= 0.2f;
    
    return std::max(0.0f, std::min(1.0f, score));
}

std::string filter_noise(const std::string& text) {
    std::string result;
    std::istringstream iss(text);
    std::string line;
    
    while (std::getline(iss, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Skip very short lines (likely headers or noise)
        if (line.size() < 10) continue;
        
        // Skip lines that look like references
        if (line.find("[edit]") != std::string::npos) continue;
        if (line.find("Retrieved") == 0) continue;
        if (line.find("External links") == 0) continue;
        if (line.find("See also") == 0) continue;
        if (line.find("References") == 0) continue;
        if (line.find("Further reading") == 0) continue;
        if (line.find("Bibliography") == 0) continue;
        if (line.find("Categories:") != std::string::npos) continue;
        
        // Skip lines that are mostly numbers (likely tables)
        int digit_count = 0;
        for (char c : line) {
            if (isdigit(c)) digit_count++;
        }
        if (line.size() > 0 && (float)digit_count / line.size() > 0.5f) continue;
        
        result += line + "\n";
    }
    
    return result;
}

std::vector<std::string> split_into_sentences(const std::string& text) {
    std::vector<std::string> sentences;
    std::string current;
    
    for (size_t i = 0; i < text.size(); i++) {
        current += text[i];
        
        // Sentence boundary detection
        if (text[i] == '.' || text[i] == '!' || text[i] == '?') {
            // Check it's not an abbreviation (e.g., "Dr.", "Mr.", "etc.")
            bool is_sentence_end = true;
            
            if (i + 1 < text.size() && !isspace(text[i+1]) && text[i+1] != '\n') {
                is_sentence_end = false;
            }
            
            // Check for common abbreviations
            if (current.size() < 5) {
                is_sentence_end = false;
            }
            
            if (is_sentence_end) {
                // Trim whitespace
                size_t start = current.find_first_not_of(" \t\n\r");
                if (start != std::string::npos) {
                    std::string sentence = current.substr(start);
                    if (sentence.size() >= 20) { // Min sentence length
                        sentences.push_back(sentence);
                    }
                }
                current.clear();
            }
        }
        
        // Also split on newlines
        if (text[i] == '\n') {
            size_t start = current.find_first_not_of(" \t\n\r");
            if (start != std::string::npos) {
                std::string segment = current.substr(start);
                if (segment.size() >= 20) {
                    sentences.push_back(segment);
                }
            }
            current.clear();
        }
    }
    
    // Don't forget the last segment
    if (!current.empty()) {
        size_t start = current.find_first_not_of(" \t\n\r");
        if (start != std::string::npos) {
            std::string segment = current.substr(start);
            if (segment.size() >= 20) {
                sentences.push_back(segment);
            }
        }
    }
    
    return sentences;
}

std::string process_content(const FetchedContent& content, const InternetLearningConfig& config) {
    if (content.clean_text.empty()) return "";
    
    // Step 1: Filter noise
    std::string filtered = filter_noise(content.clean_text);
    
    // Step 2: Split into sentences
    auto sentences = split_into_sentences(filtered);
    
    // Step 3: Build training text
    std::string result;
    for (const auto& sent : sentences) {
        if ((int)sent.size() >= config.min_sentence_length && 
            (int)sent.size() <= config.max_sentence_length) {
            result += sent + "\n";
        }
    }
    
    return result;
}

int add_to_corpus(const std::string& content, const std::string& corpus_path) {
    if (content.empty()) return 0;
    
    std::ofstream file(corpus_path, std::ios::app);
    if (!file) {
        std::cerr << "[INTERNET-LEARN] Cannot write to corpus: " << corpus_path << "\n";
        return 0;
    }
    
    file << content;
    file.close();
    
    // Count lines added
    int lines = 0;
    for (char c : content) {
        if (c == '\n') lines++;
    }
    
    return lines;
}

// =============================================================================
// Topic Discovery
// =============================================================================

std::vector<LearningTopic> discover_topics(const InternetLearningConfig& config) {
    std::vector<LearningTopic> topics;
    
    std::cout << "[INTERNET-LEARN] Discovering topics to learn...\n";
    
    // Source 1: Weak responses from self-learning module
    auto weak_responses = self_learning::load_weak_responses();
    
    std::set<std::string> seen_topics;
    
    for (const auto& weak : weak_responses) {
        // Extract key concepts from the question
        auto concepts = self_learning::extract_key_concepts(weak.question);
        
        for (const auto& concept : concepts) {
            if (seen_topics.count(concept)) continue;
            if (is_topic_learned(concept)) continue;
            
            seen_topics.insert(concept);
            
            LearningTopic topic;
            topic.topic = concept;
            topic.reason = "Weak response (score=" + std::to_string(weak.score) + ") to: " + weak.question.substr(0, 50);
            topic.priority = std::max(1, 10 - weak.score / 10);
            topic.learned = false;
            topic.source_question = weak.question;
            
            topics.push_back(topic);
        }
    }
    
    // Source 2: Essential knowledge areas (if no weak responses)
    if (topics.empty()) {
        std::vector<std::string> essential_topics = {
            "artificial intelligence", "machine learning", "neural network",
            "computer science", "mathematics", "physics", "chemistry",
            "biology", "history", "geography", "programming", "algorithm",
            "data structure", "database", "internet", "software engineering",
            "operating system", "computer network", "cybersecurity",
            "natural language processing", "deep learning", "robotics",
            "cloud computing", "web development", "python programming",
            "javascript", "statistics", "linear algebra", "calculus",
            "probability", "graph theory", "compiler", "encryption"
        };
        
        for (const auto& t : essential_topics) {
            if (is_topic_learned(t)) continue;
            
            LearningTopic topic;
            topic.topic = t;
            topic.reason = "Essential knowledge area";
            topic.priority = 5;
            topic.learned = false;
            
            topics.push_back(topic);
            
            if ((int)topics.size() >= config.max_topics_per_cycle) break;
        }
    }
    
    // Sort by priority (highest first)
    std::sort(topics.begin(), topics.end(), 
              [](const LearningTopic& a, const LearningTopic& b) {
                  return a.priority > b.priority;
              });
    
    // Limit to max topics per cycle
    if ((int)topics.size() > config.max_topics_per_cycle) {
        topics.resize(config.max_topics_per_cycle);
    }
    
    std::cout << "[INTERNET-LEARN] Discovered " << topics.size() << " topics to learn\n";
    for (const auto& t : topics) {
        std::cout << "  [" << t.priority << "] " << t.topic << " (" << t.reason << ")\n";
    }
    
    return topics;
}

// =============================================================================
// Learning History
// =============================================================================

static const std::string LEARNED_TOPICS_FILE = "brain/self_learning/learned_topics.txt";
static const std::string SESSIONS_DIR = "brain/self_learning/sessions";

std::vector<std::string> get_learned_topics() {
    std::vector<std::string> topics;
    
    std::ifstream file(LEARNED_TOPICS_FILE);
    if (!file) return topics;
    
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty()) {
            topics.push_back(line);
        }
    }
    
    return topics;
}

bool is_topic_learned(const std::string& topic) {
    auto topics = get_learned_topics();
    
    std::string lower_topic = topic;
    std::transform(lower_topic.begin(), lower_topic.end(), lower_topic.begin(), ::tolower);
    
    for (const auto& t : topics) {
        std::string lower_t = t;
        std::transform(lower_t.begin(), lower_t.end(), lower_t.begin(), ::tolower);
        if (lower_t == lower_topic) return true;
    }
    
    return false;
}

static void mark_topic_learned(const std::string& topic) {
    std::ofstream file(LEARNED_TOPICS_FILE, std::ios::app);
    if (file) {
        file << topic << "\n";
    }
}

void save_session(const LearningSession& session) {
    ensure_dirs();
    
    std::string filename = SESSIONS_DIR + "/session_" + session.timestamp + ".txt";
    // Replace : with - for safe filename
    std::replace(filename.begin(), filename.end(), ':', '-');
    
    std::ofstream file(filename);
    if (!file) return;
    
    file << "timestamp: " << session.timestamp << "\n";
    file << "topics_discovered: " << session.topics_discovered << "\n";
    file << "articles_fetched: " << session.articles_fetched << "\n";
    file << "total_words_learned: " << session.total_words_learned << "\n";
    file << "corpus_lines_added: " << session.corpus_lines_added << "\n";
    file << "training_triggered: " << (session.training_triggered ? "yes" : "no") << "\n";
    file << "training_loss: " << session.training_loss << "\n";
    
    file << "\ntopics_learned:\n";
    for (const auto& t : session.topics_learned) {
        file << "  - " << t << "\n";
    }
    
    file << "\nfailed_topics:\n";
    for (const auto& t : session.failed_topics) {
        file << "  - " << t << "\n";
    }
}

// =============================================================================
// Main Learning Session
// =============================================================================

LearningSession run_learning_session(const InternetLearningConfig& config) {
    LearningSession session;
    session.timestamp = get_timestamp();
    session.topics_discovered = 0;
    session.articles_fetched = 0;
    session.total_words_learned = 0;
    session.corpus_lines_added = 0;
    session.training_triggered = false;
    session.training_loss = 0.0f;
    
    ensure_dirs();
    
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  INTERNET LEARNING SESSION                               ║\n";
    std::cout << "║  Learning from Wikipedia to become smarter!              ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    
    // Step 1: Discover topics
    auto topics = discover_topics(config);
    session.topics_discovered = topics.size();
    
    if (topics.empty()) {
        std::cout << "[INTERNET-LEARN] No new topics to learn! AI is up to date.\n";
        return session;
    }
    
    // Step 2: Fetch and process each topic
    std::string corpus_path = "brain/self_learning/internet_corpus.txt";
    
    for (auto& topic : topics) {
        std::cout << "\n━━━ Learning: " << topic.topic << " ━━━\n";
        
        // Try Simple Wikipedia first, then regular
        FetchedContent content;
        
        if (config.use_simple_wikipedia) {
            content = fetch_simple_wikipedia(topic.topic, config);
        } else {
            content = fetch_wikipedia(topic.topic, config);
        }
        
        // Also try to get full article for more content
        if (content.word_count < 50) {
            std::cout << "[INTERNET-LEARN] Summary too short, trying full article...\n";
            content = fetch_full_article(topic.topic, config);
        }
        
        if (content.word_count < 10) {
            std::cout << "[INTERNET-LEARN] Could not get content for: " << topic.topic << "\n";
            session.failed_topics.push_back(topic.topic);
            continue;
        }
        
        // Also search for related articles
        auto related = search_wikipedia(topic.topic, 2);
        for (const auto& rel : related) {
            if (rel != topic.topic && !is_topic_learned(rel)) {
                FetchedContent rel_content = config.use_simple_wikipedia ?
                    fetch_simple_wikipedia(rel, config) :
                    fetch_wikipedia(rel, config);
                
                if (rel_content.word_count > 20) {
                    std::string processed = process_content(rel_content, config);
                    int lines = add_to_corpus(processed, corpus_path);
                    session.corpus_lines_added += lines;
                    session.total_words_learned += rel_content.word_count;
                    session.articles_fetched++;
                }
            }
        }
        
        // Process main content
        std::string processed = process_content(content, config);
        int lines = add_to_corpus(processed, corpus_path);
        
        session.articles_fetched++;
        session.total_words_learned += content.word_count;
        session.corpus_lines_added += lines;
        session.topics_learned.push_back(topic.topic);
        
        // Mark as learned
        mark_topic_learned(topic.topic);
        
        std::cout << "[INTERNET-LEARN] ✅ Learned: " << topic.topic 
                  << " (" << content.word_count << " words, " << lines << " lines)\n";
        
        // Small delay between requests to be respectful
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // Step 3: Trigger training if enough data
    if (session.corpus_lines_added >= config.min_corpus_lines) {
        std::cout << "\n";
        std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
        std::cout << "║  TRIGGERING INCREMENTAL TRAINING                         ║\n";
        std::cout << "║  Training on " << session.corpus_lines_added << " new lines of knowledge           ║\n";
        std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
        std::cout << "\n";
        
        session.training_triggered = true;
        run_train_transformer(corpus_path, config.training_epochs, config.training_lr, config.training_batch_size);
    } else {
        std::cout << "\n[INTERNET-LEARN] Not enough data for training yet (" 
                  << session.corpus_lines_added << "/" << config.min_corpus_lines << " lines)\n";
        std::cout << "[INTERNET-LEARN] Will train when threshold is reached.\n";
    }
    
    // Step 4: Save session
    save_session(session);
    
    // Print summary
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════╗\n";
    std::cout << "║  LEARNING SESSION COMPLETE                               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";
    std::cout << "  Topics discovered:    " << session.topics_discovered << "\n";
    std::cout << "  Articles fetched:     " << session.articles_fetched << "\n";
    std::cout << "  Words learned:        " << session.total_words_learned << "\n";
    std::cout << "  Corpus lines added:   " << session.corpus_lines_added << "\n";
    std::cout << "  Training triggered:   " << (session.training_triggered ? "YES" : "NO") << "\n";
    
    if (!session.topics_learned.empty()) {
        std::cout << "\n  Topics learned:\n";
        for (const auto& t : session.topics_learned) {
            std::cout << "    ✅ " << t << "\n";
        }
    }
    
    if (!session.failed_topics.empty()) {
        std::cout << "\n  Failed topics:\n";
        for (const auto& t : session.failed_topics) {
            std::cout << "    ❌ " << t << "\n";
        }
    }
    
    std::cout << "\n";
    
    return session;
}

// =============================================================================
// Main Entry Point for Internet Learning
// =============================================================================

int main_internet_learn(int argc, char** argv) {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║  SELF-INDEPENDENT AI — Internet Learning System              ║
║  Learning from the world to become smarter                   ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;

    InternetLearningConfig config;
    
    // Parse arguments
    bool daemon_mode = false;
    int interval_minutes = 30;
    
    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--daemon") {
            daemon_mode = true;
        } else if (arg.find("--interval=") == 0) {
            interval_minutes = std::stoi(arg.substr(11));
        } else if (arg.find("--topics=") == 0) {
            config.max_topics_per_cycle = std::stoi(arg.substr(9));
        } else if (arg.find("--articles=") == 0) {
            config.max_articles_per_topic = std::stoi(arg.substr(11));
        } else if (arg == "--no-simple") {
            config.use_simple_wikipedia = false;
        } else if (arg.find("--epochs=") == 0) {
            config.training_epochs = std::stoi(arg.substr(9));
        } else if (arg.find("--min-corpus=") == 0) {
            config.min_corpus_lines = std::stoi(arg.substr(13));
        } else if (arg == "--help") {
            std::cout << "Usage: neural_engine.exe internet_learn [options]\n\n";
            std::cout << "Options:\n";
            std::cout << "  --daemon            Run continuously in background\n";
            std::cout << "  --interval=N        Minutes between learning cycles (default: 30)\n";
            std::cout << "  --topics=N          Topics per cycle (default: 5)\n";
            std::cout << "  --articles=N        Articles per topic (default: 3)\n";
            std::cout << "  --epochs=N          Training epochs (default: 3)\n";
            std::cout << "  --min-corpus=N      Min lines before training (default: 100)\n";
            std::cout << "  --no-simple         Skip Simple Wikipedia\n";
            std::cout << "  --help              Show this help\n";
            return 0;
        }
    }
    
    // Print configuration
    std::cout << "[CONFIG] Topics per cycle:     " << config.max_topics_per_cycle << "\n";
    std::cout << "[CONFIG] Articles per topic:   " << config.max_articles_per_topic << "\n";
    std::cout << "[CONFIG] Simple Wikipedia:     " << (config.use_simple_wikipedia ? "yes" : "no") << "\n";
    std::cout << "[CONFIG] Training epochs:      " << config.training_epochs << "\n";
    std::cout << "[CONFIG] Min corpus lines:     " << config.min_corpus_lines << "\n";
    std::cout << "[CONFIG] Daemon mode:          " << (daemon_mode ? "yes" : "no") << "\n";
    
    if (daemon_mode) {
        std::cout << "[CONFIG] Interval:             " << interval_minutes << " minutes\n";
    }
    std::cout << "\n";
    
    // Show what's already been learned
    auto learned = get_learned_topics();
    if (!learned.empty()) {
        std::cout << "[HISTORY] Already learned " << learned.size() << " topics:\n";
        for (const auto& t : learned) {
            std::cout << "  ✅ " << t << "\n";
        }
        std::cout << "\n";
    }
    
    if (daemon_mode) {
        // Continuous learning loop
        std::cout << "[DAEMON] Starting continuous learning loop...\n";
        std::cout << "[DAEMON] Learning every " << interval_minutes << " minutes\n";
        std::cout << "[DAEMON] Press Ctrl+C to stop\n\n";
        
        int cycle = 0;
        while (true) {
            cycle++;
            std::cout << "\n═══════════ Learning Cycle " << cycle << " ═══════════\n\n";
            
            auto session = run_learning_session(config);
            
            if (session.topics_discovered == 0 && learned.size() > 30) {
                std::cout << "\n[DAEMON] All essential topics learned! Reducing frequency...\n";
                interval_minutes = std::max(interval_minutes, 60);
            }
            
            std::cout << "\n[DAEMON] Next cycle in " << interval_minutes << " minutes...\n";
            std::this_thread::sleep_for(std::chrono::minutes(interval_minutes));
        }
    } else {
        // One-shot learning
        auto session = run_learning_session(config);
        
        std::cout << "\n[INFO] For continuous learning, use: neural_engine.exe internet_learn --daemon\n";
    }
    
    return 0;
}

} // namespace internet_learning
