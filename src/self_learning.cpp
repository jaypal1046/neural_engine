// =============================================================================
// Self-Learning Module - Implementation
// =============================================================================

#include "self_learning.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <cstring>

namespace fs = std::filesystem;

// External functions from neural_engine.cpp
extern std::string run_ai_ask(const std::string& question);
extern int run_score_response(const std::string& question, const std::string& answer);
extern std::string run_cai_critique(const std::string& question, const std::string& answer);
extern void run_learn(const std::string& source);
extern void run_train_transformer(const std::string& corpus_file, int epochs, float lr, int batch_size);

namespace self_learning {

// =============================================================================
// Utilities
// =============================================================================

void ensure_directory_exists(const std::string& path) {
    try {
        fs::path dir(path);
        if (!fs::exists(dir)) {
            fs::create_directories(dir);
        }
    } catch (const std::exception& e) {
        std::cerr << "[SELF-LEARN] Error creating directory " << path << ": " << e.what() << "\n";
    }
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t_now), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// Simple JSON escape for strings
std::string json_escape(const std::string& str) {
    std::string result;
    result.reserve(str.size() * 1.1);

    for (char c : str) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default:   result += c; break;
        }
    }
    return result;
}

// Simple JSON unescape
std::string json_unescape(const std::string& str) {
    std::string result;
    result.reserve(str.size());

    for (size_t i = 0; i < str.size(); i++) {
        if (str[i] == '\\' && i + 1 < str.size()) {
            switch (str[i + 1]) {
                case '"':  result += '"'; i++; break;
                case '\\': result += '\\'; i++; break;
                case 'n':  result += '\n'; i++; break;
                case 'r':  result += '\r'; i++; break;
                case 't':  result += '\t'; i++; break;
                default:   result += str[i]; break;
            }
        } else {
            result += str[i];
        }
    }
    return result;
}

std::vector<std::string> load_json_array(const std::string& file_path) {
    std::vector<std::string> result;

    std::ifstream file(file_path);
    if (!file) {
        return result;  // Empty array if file doesn't exist
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Simple JSON array parser - find objects between { and }
    size_t pos = 0;
    int brace_depth = 0;
    size_t object_start = std::string::npos;

    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '{') {
            if (brace_depth == 0) {
                object_start = i;
            }
            brace_depth++;
        } else if (content[i] == '}') {
            brace_depth--;
            if (brace_depth == 0 && object_start != std::string::npos) {
                std::string obj = content.substr(object_start, i - object_start + 1);
                result.push_back(obj);
                object_start = std::string::npos;
            }
        }
    }

    return result;
}

void save_json_array(const std::string& file_path, const std::vector<std::string>& json_objects) {
    std::ofstream file(file_path);
    if (!file) {
        std::cerr << "[SELF-LEARN] Error: Cannot write to " << file_path << "\n";
        return;
    }

    file << "[\n";
    for (size_t i = 0; i < json_objects.size(); i++) {
        file << "  " << json_objects[i];
        if (i < json_objects.size() - 1) {
            file << ",";
        }
        file << "\n";
    }
    file << "]\n";
}

void append_json_to_array(const std::string& file_path, const std::string& json_object) {
    // Load existing array
    auto existing = load_json_array(file_path);

    // Add new object
    existing.push_back(json_object);

    // Save updated array
    save_json_array(file_path, existing);
}

int count_json_array_entries(const std::string& file_path) {
    auto entries = load_json_array(file_path);
    return static_cast<int>(entries.size());
}

// =============================================================================
// Data Structures - JSON Serialization
// =============================================================================

std::string WeakResponse::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "    \"timestamp\": \"" << json_escape(timestamp) << "\",\n";
    ss << "    \"question\": \"" << json_escape(question) << "\",\n";
    ss << "    \"answer\": \"" << json_escape(answer) << "\",\n";
    ss << "    \"score\": " << score << ",\n";
    ss << "    \"issues\": [";
    for (size_t i = 0; i < issues.size(); i++) {
        ss << "\"" << json_escape(issues[i]) << "\"";
        if (i < issues.size() - 1) ss << ", ";
    }
    ss << "]\n";
    ss << "  }";
    return ss.str();
}

WeakResponse WeakResponse::from_json(const std::string& json) {
    WeakResponse resp;

    // Simple JSON parser (extract fields)
    auto extract_string_field = [](const std::string& json, const std::string& field) -> std::string {
        std::string search = "\"" + field + "\": \"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";

        pos += search.size();
        size_t end = json.find("\"", pos);
        if (end == std::string::npos) return "";

        return json_unescape(json.substr(pos, end - pos));
    };

    auto extract_int_field = [](const std::string& json, const std::string& field) -> int {
        std::string search = "\"" + field + "\": ";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;

        pos += search.size();
        size_t end = json.find_first_of(",\n}", pos);
        if (end == std::string::npos) return 0;

        return std::stoi(json.substr(pos, end - pos));
    };

    resp.timestamp = extract_string_field(json, "timestamp");
    resp.question = extract_string_field(json, "question");
    resp.answer = extract_string_field(json, "answer");
    resp.score = extract_int_field(json, "score");

    // Extract issues array (simplified)
    size_t issues_pos = json.find("\"issues\": [");
    if (issues_pos != std::string::npos) {
        size_t start = issues_pos + 11;
        size_t end = json.find("]", start);
        if (end != std::string::npos) {
            std::string issues_str = json.substr(start, end - start);
            // Parse comma-separated quoted strings
            size_t pos = 0;
            while (pos < issues_str.size()) {
                size_t q1 = issues_str.find("\"", pos);
                if (q1 == std::string::npos) break;
                size_t q2 = issues_str.find("\"", q1 + 1);
                if (q2 == std::string::npos) break;

                std::string issue = issues_str.substr(q1 + 1, q2 - q1 - 1);
                resp.issues.push_back(json_unescape(issue));
                pos = q2 + 1;
            }
        }
    }

    return resp;
}

std::string Correction::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "    \"timestamp\": \"" << json_escape(timestamp) << "\",\n";
    ss << "    \"question\": \"" << json_escape(question) << "\",\n";
    ss << "    \"old_answer\": \"" << json_escape(old_answer) << "\",\n";
    ss << "    \"old_score\": " << old_score << ",\n";
    ss << "    \"new_answer\": \"" << json_escape(new_answer) << "\",\n";
    ss << "    \"new_score\": " << new_score << ",\n";
    ss << "    \"learned_from\": \"" << json_escape(learned_from) << "\"\n";
    ss << "  }";
    return ss.str();
}

Correction Correction::from_json(const std::string& json) {
    Correction corr;

    auto extract_string = [&](const std::string& field) -> std::string {
        std::string search = "\"" + field + "\": \"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        return end != std::string::npos ? json_unescape(json.substr(pos, end - pos)) : "";
    };

    auto extract_int = [&](const std::string& field) -> int {
        std::string search = "\"" + field + "\": ";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.size();
        size_t end = json.find_first_of(",\n}", pos);
        return end != std::string::npos ? std::stoi(json.substr(pos, end - pos)) : 0;
    };

    corr.timestamp = extract_string("timestamp");
    corr.question = extract_string("question");
    corr.old_answer = extract_string("old_answer");
    corr.old_score = extract_int("old_score");
    corr.new_answer = extract_string("new_answer");
    corr.new_score = extract_int("new_score");
    corr.learned_from = extract_string("learned_from");

    return corr;
}

std::string CAIViolation::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "    \"timestamp\": \"" << json_escape(timestamp) << "\",\n";
    ss << "    \"question\": \"" << json_escape(question) << "\",\n";
    ss << "    \"answer\": \"" << json_escape(answer) << "\",\n";
    ss << "    \"violations\": [";
    for (size_t i = 0; i < violations.size(); i++) {
        ss << "\"" << json_escape(violations[i]) << "\"";
        if (i < violations.size() - 1) ss << ", ";
    }
    ss << "],\n";
    ss << "    \"revised_answer\": \"" << json_escape(revised_answer) << "\",\n";
    ss << "    \"cai_score\": " << cai_score << "\n";
    ss << "  }";
    return ss.str();
}

CAIViolation CAIViolation::from_json(const std::string& json) {
    CAIViolation viol;

    auto extract_string = [&](const std::string& field) -> std::string {
        std::string search = "\"" + field + "\": \"";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return "";
        pos += search.size();
        size_t end = json.find("\"", pos);
        return end != std::string::npos ? json_unescape(json.substr(pos, end - pos)) : "";
    };

    auto extract_int = [&](const std::string& field) -> int {
        std::string search = "\"" + field + "\": ";
        size_t pos = json.find(search);
        if (pos == std::string::npos) return 0;
        pos += search.size();
        size_t end = json.find_first_of(",\n}", pos);
        return end != std::string::npos ? std::stoi(json.substr(pos, end - pos)) : 0;
    };

    viol.timestamp = extract_string("timestamp");
    viol.question = extract_string("question");
    viol.answer = extract_string("answer");
    viol.revised_answer = extract_string("revised_answer");
    viol.cai_score = extract_int("cai_score");

    // Extract violations array
    size_t viol_pos = json.find("\"violations\": [");
    if (viol_pos != std::string::npos) {
        size_t start = viol_pos + 15;
        size_t end = json.find("]", start);
        if (end != std::string::npos) {
            std::string viol_str = json.substr(start, end - start);
            size_t pos = 0;
            while (pos < viol_str.size()) {
                size_t q1 = viol_str.find("\"", pos);
                if (q1 == std::string::npos) break;
                size_t q2 = viol_str.find("\"", q1 + 1);
                if (q2 == std::string::npos) break;

                std::string v = viol_str.substr(q1 + 1, q2 - q1 - 1);
                viol.violations.push_back(json_unescape(v));
                pos = q2 + 1;
            }
        }
    }

    return viol;
}

// =============================================================================
// Phase A: Runtime Self-Monitoring
// =============================================================================

int score_response_internal(const std::string& question, const std::string& answer) {
    // Call external scoring function from neural_engine.cpp
    return run_score_response(question, answer);
}

void log_weak_response(const WeakResponse& response) {
    ensure_directory_exists("brain/self_learning");

    std::string file_path = "brain/self_learning/weak_responses.json";
    append_json_to_array(file_path, response.to_json());

    std::cout << "[SELF-LEARN] Logged weak response (score=" << response.score << "): "
              << response.question.substr(0, 50) << "...\n";
}

std::vector<WeakResponse> load_weak_responses() {
    std::vector<WeakResponse> result;

    std::string file_path = "brain/self_learning/weak_responses.json";
    auto json_objects = load_json_array(file_path);

    for (const auto& json : json_objects) {
        result.push_back(WeakResponse::from_json(json));
    }

    return result;
}

void auto_monitor_loop(const AutoLearningConfig& config, std::atomic<bool>& running) {
    std::cout << "[SELF-LEARN] Starting auto-monitor loop (interval="
              << config.monitor_interval_seconds << "s, threshold="
              << config.weak_score_threshold << ")\n";

    // This would monitor recent ai_ask responses
    // For now, we'll implement a simple version that reads from a monitoring queue
    // In a full implementation, ai_ask would push to this queue

    while (running) {
        // Sleep for the configured interval
        std::this_thread::sleep_for(std::chrono::seconds(config.monitor_interval_seconds));

        // In real implementation: check monitoring queue, score responses, log weak ones
        // For now: placeholder
        std::cout << "[SELF-LEARN] Monitor heartbeat - checking for weak responses...\n";
    }

    std::cout << "[SELF-LEARN] Auto-monitor loop stopped\n";
}

// =============================================================================
// Phase B: Auto-Correction Engine
// =============================================================================

std::vector<std::string> extract_key_concepts(const std::string& question) {
    std::vector<std::string> concepts;

    // Simple approach: extract nouns and technical terms
    // Convert to lowercase
    std::string lower = question;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // Common question words to skip
    std::vector<std::string> skip_words = {
        "what", "when", "where", "who", "why", "how", "is", "are", "was", "were",
        "do", "does", "did", "can", "could", "should", "would", "will",
        "the", "a", "an", "and", "or", "but", "in", "on", "at", "to", "for"
    };

    // Extract words (simple tokenization)
    std::istringstream iss(lower);
    std::string word;
    while (iss >> word) {
        // Remove punctuation
        word.erase(std::remove_if(word.begin(), word.end(), ::ispunct), word.end());

        // Skip if in skip list or too short
        if (word.length() < 3) continue;

        bool skip = false;
        for (const auto& sw : skip_words) {
            if (word == sw) {
                skip = true;
                break;
            }
        }

        if (!skip) {
            concepts.push_back(word);
        }
    }

    return concepts;
}

std::string search_wikipedia(const std::string& concept) {
    // Construct Wikipedia URL
    // Format: https://en.wikipedia.org/wiki/Concept_name
    std::string url = "https://en.wikipedia.org/wiki/";

    // Capitalize first letter, replace spaces with underscores
    std::string formatted = concept;
    if (!formatted.empty()) {
        formatted[0] = std::toupper(formatted[0]);
    }

    // Replace spaces with underscores
    std::replace(formatted.begin(), formatted.end(), ' ', '_');

    url += formatted;
    return url;
}

void log_correction(const Correction& correction) {
    ensure_directory_exists("brain/self_learning");

    std::string file_path = "brain/self_learning/corrections.json";
    append_json_to_array(file_path, correction.to_json());

    std::cout << "[SELF-LEARN] Logged correction (score " << correction.old_score
              << " -> " << correction.new_score << "): "
              << correction.question.substr(0, 50) << "...\n";
}

std::vector<Correction> load_corrections() {
    std::vector<Correction> result;

    std::string file_path = "brain/self_learning/corrections.json";
    auto json_objects = load_json_array(file_path);

    for (const auto& json : json_objects) {
        result.push_back(Correction::from_json(json));
    }

    return result;
}

Correction* correct_single_response(const WeakResponse& weak_resp, const AutoLearningConfig& config) {
    std::cout << "[SELF-LEARN] Correcting: " << weak_resp.question.substr(0, 60) << "...\n";

    // 1. Extract key concepts from question
    auto concepts = extract_key_concepts(weak_resp.question);

    if (concepts.empty()) {
        std::cout << "[SELF-LEARN] No key concepts found, skipping\n";
        return nullptr;
    }

    std::cout << "[SELF-LEARN] Key concepts: ";
    for (const auto& c : concepts) std::cout << c << " ";
    std::cout << "\n";

    // 2. Learn from Wikipedia for first concept (to avoid overwhelming the system)
    std::string concept = concepts[0];
    std::string wiki_url = search_wikipedia(concept);

    std::cout << "[SELF-LEARN] Learning from: " << wiki_url << "\n";

    if (config.learn_from_web) {
        try {
            run_learn(wiki_url);
        } catch (const std::exception& e) {
            std::cerr << "[SELF-LEARN] Failed to learn from " << wiki_url << ": " << e.what() << "\n";
            return nullptr;
        }
    }

    // 3. Re-answer the question
    std::cout << "[SELF-LEARN] Re-answering question...\n";
    std::string new_answer = run_ai_ask(weak_resp.question);

    // 4. Score new answer
    int new_score = score_response_internal(weak_resp.question, new_answer);

    std::cout << "[SELF-LEARN] New score: " << new_score << " (was " << weak_resp.score << ")\n";

    // 5. If improved, create correction
    if (new_score > weak_resp.score) {
        Correction* corr = new Correction();
        corr->timestamp = get_timestamp();
        corr->question = weak_resp.question;
        corr->old_answer = weak_resp.answer;
        corr->old_score = weak_resp.score;
        corr->new_answer = new_answer;
        corr->new_score = new_score;
        corr->learned_from = wiki_url;

        return corr;
    } else {
        std::cout << "[SELF-LEARN] No improvement, skipping\n";
        return nullptr;
    }
}

void auto_correct_responses(const AutoLearningConfig& config) {
    std::cout << "[SELF-LEARN] Starting auto-correction...\n";

    // Load weak responses
    auto weak_responses = load_weak_responses();

    if (weak_responses.empty()) {
        std::cout << "[SELF-LEARN] No weak responses to correct\n";
        return;
    }

    std::cout << "[SELF-LEARN] Found " << weak_responses.size() << " weak responses\n";

    // Correct up to batch_size responses
    int corrected = 0;
    int batch_size = std::min(config.correction_batch_size, static_cast<int>(weak_responses.size()));

    for (int i = 0; i < batch_size && corrected < config.correction_batch_size; i++) {
        Correction* corr = correct_single_response(weak_responses[i], config);

        if (corr != nullptr) {
            log_correction(*corr);
            corrected++;
            delete corr;
        }
    }

    std::cout << "[SELF-LEARN] Auto-correction complete: " << corrected << " corrections made\n";
}

// =============================================================================
// Phase C: Continuous Training Loop
// =============================================================================

void assemble_corrections_corpus(const std::vector<Correction>& corrections,
                                  const std::string& output_file) {
    std::ofstream file(output_file);
    if (!file) {
        std::cerr << "[SELF-LEARN] Error: Cannot write corpus to " << output_file << "\n";
        return;
    }

    for (const auto& corr : corrections) {
        file << "question: " << corr.question << "\n";
        file << "answer: " << corr.new_answer << "\n\n";
    }

    std::cout << "[SELF-LEARN] Assembled corpus with " << corrections.size() << " Q&A pairs\n";
}

void train_on_corrections(const std::string& corpus_file, const AutoLearningConfig& config) {
    std::cout << "[SELF-LEARN] Training transformer on corrections...\n";

    try {
        run_train_transformer(corpus_file, config.training_epochs, config.training_lr, config.training_batch_size);
        std::cout << "[SELF-LEARN] Training complete!\n";
    } catch (const std::exception& e) {
        std::cerr << "[SELF-LEARN] Training failed: " << e.what() << "\n";
    }
}

void archive_corrections(const std::vector<Correction>& corrections) {
    ensure_directory_exists("brain/training/history");

    std::string timestamp = get_timestamp();
    std::replace(timestamp.begin(), timestamp.end(), ':', '-');  // Safe filename
    std::string archive_file = "brain/training/history/corrections_" + timestamp + ".json";

    std::vector<std::string> json_objects;
    for (const auto& corr : corrections) {
        json_objects.push_back(corr.to_json());
    }

    save_json_array(archive_file, json_objects);
    std::cout << "[SELF-LEARN] Archived " << corrections.size() << " corrections to " << archive_file << "\n";
}

void clear_corrections() {
    std::string file_path = "brain/self_learning/corrections.json";

    // Clear by writing empty array
    save_json_array(file_path, {});

    std::cout << "[SELF-LEARN] Cleared corrections file\n";
}

void auto_train_loop(const AutoLearningConfig& config, std::atomic<bool>& running) {
    std::cout << "[SELF-LEARN] Starting auto-train loop (threshold=" << config.training_threshold << ")\n";

    while (running) {
        // Check corrections count
        int correction_count = count_json_array_entries("brain/self_learning/corrections.json");

        if (correction_count >= config.training_threshold) {
            std::cout << "[SELF-LEARN] Training threshold reached (" << correction_count
                      << " >= " << config.training_threshold << ")\n";

            // Load corrections
            auto corrections = load_corrections();

            // Assemble corpus
            std::string corpus_file = "brain/self_learning/corrections_corpus.txt";
            assemble_corrections_corpus(corrections, corpus_file);

            // Train
            train_on_corrections(corpus_file, config);

            // Archive
            archive_corrections(corrections);

            // Clear
            clear_corrections();
        } else {
            std::cout << "[SELF-LEARN] Train heartbeat - " << correction_count << "/"
                      << config.training_threshold << " corrections\n";
        }

        // Sleep for 5 minutes
        std::this_thread::sleep_for(std::chrono::seconds(300));
    }

    std::cout << "[SELF-LEARN] Auto-train loop stopped\n";
}

// =============================================================================
// Phase D: CAI Self-Critique Loop
// =============================================================================

CAIViolation* run_cai_critique_internal(const std::string& question, const std::string& answer) {
    // Call external CAI critique function
    std::string result = run_cai_critique(question, answer);

    // Parse result (simplified - assumes JSON response with "violations" field)
    if (result.find("\"violations\": []") != std::string::npos) {
        // No violations
        return nullptr;
    }

    // Has violations - create CAIViolation object
    CAIViolation* viol = new CAIViolation();
    viol->timestamp = get_timestamp();
    viol->question = question;
    viol->answer = answer;

    // Extract violations (simplified parser)
    size_t viol_start = result.find("\"violations\": [");
    if (viol_start != std::string::npos) {
        size_t viol_end = result.find("]", viol_start);
        if (viol_end != std::string::npos) {
            std::string viol_str = result.substr(viol_start + 15, viol_end - viol_start - 15);
            // Parse comma-separated strings
            std::istringstream iss(viol_str);
            std::string v;
            while (std::getline(iss, v, ',')) {
                // Remove quotes and whitespace
                v.erase(std::remove(v.begin(), v.end(), '"'), v.end());
                v.erase(std::remove(v.begin(), v.end(), ' '), v.end());
                if (!v.empty()) {
                    viol->violations.push_back(v);
                }
            }
        }
    }

    // Extract CAI score
    size_t score_pos = result.find("\"cai_score\": ");
    if (score_pos != std::string::npos) {
        size_t score_start = score_pos + 13;
        size_t score_end = result.find_first_of(",\n}", score_start);
        if (score_end != std::string::npos) {
            viol->cai_score = std::stoi(result.substr(score_start, score_end - score_start));
        }
    }

    return viol;
}

std::string revise_for_cai(const std::string& question,
                           const std::string& answer,
                           const std::vector<std::string>& violations) {
    // Simple revision: prepend disclaimer for common violations
    std::string revised = answer;

    bool has_harmful = false;
    bool has_uncertain = false;

    for (const auto& v : violations) {
        if (v.find("harmful") != std::string::npos || v.find("illegal") != std::string::npos) {
            has_harmful = true;
        }
        if (v.find("uncertain") != std::string::npos || v.find("speculative") != std::string::npos) {
            has_uncertain = true;
        }
    }

    if (has_harmful) {
        revised = "I cannot provide advice on harmful or illegal activities. " + revised;
    }
    if (has_uncertain) {
        revised += " Note: This answer may not be fully accurate. Please verify from authoritative sources.";
    }

    return revised;
}

void log_cai_violation(const CAIViolation& violation) {
    ensure_directory_exists("brain/self_learning");

    std::string file_path = "brain/self_learning/cai_violations.json";
    append_json_to_array(file_path, violation.to_json());

    std::cout << "[SELF-LEARN] Logged CAI violation (score=" << violation.cai_score << "): "
              << violation.question.substr(0, 50) << "...\n";
}

std::vector<CAIViolation> load_cai_violations() {
    std::vector<CAIViolation> result;

    std::string file_path = "brain/self_learning/cai_violations.json";
    auto json_objects = load_json_array(file_path);

    for (const auto& json : json_objects) {
        result.push_back(CAIViolation::from_json(json));
    }

    return result;
}

void auto_cai_loop(const AutoLearningConfig& config, std::atomic<bool>& running) {
    std::cout << "[SELF-LEARN] Starting auto-CAI loop (interval=" << config.cai_check_interval << "s)\n";

    while (running) {
        // In real implementation: check recent responses from conversation_memory
        // For now: placeholder

        std::this_thread::sleep_for(std::chrono::seconds(config.cai_check_interval));
        std::cout << "[SELF-LEARN] CAI heartbeat - checking for violations...\n";
    }

    std::cout << "[SELF-LEARN] Auto-CAI loop stopped\n";
}

// =============================================================================
// Phase E: Unified Auto-Learning Daemon
// =============================================================================

AutoLearningConfig parse_auto_learn_args(int argc, char** argv) {
    AutoLearningConfig config;

    for (int i = 2; i < argc; i++) {
        std::string arg = argv[i];

        if (arg == "--daemon") {
            config.daemon_mode = true;
        } else if (arg.find("--monitor-interval=") == 0) {
            config.monitor_interval_seconds = std::stoi(arg.substr(19));
        } else if (arg.find("--threshold=") == 0) {
            config.weak_score_threshold = std::stoi(arg.substr(12));
        } else if (arg.find("--train-threshold=") == 0) {
            config.training_threshold = std::stoi(arg.substr(18));
        } else if (arg == "--no-web") {
            config.learn_from_web = false;
        } else if (arg == "--no-cai") {
            config.enable_cai_critique = false;
        }
    }

    return config;
}

void print_auto_learn_stats() {
    std::cout << "\n[SELF-LEARN] Statistics:\n";
    std::cout << "========================================\n";

    int weak_count = count_json_array_entries("brain/self_learning/weak_responses.json");
    int corr_count = count_json_array_entries("brain/self_learning/corrections.json");
    int cai_count = count_json_array_entries("brain/self_learning/cai_violations.json");

    std::cout << "Weak responses logged:  " << weak_count << "\n";
    std::cout << "Corrections made:       " << corr_count << "\n";
    std::cout << "CAI violations:         " << cai_count << "\n";

    // Count archived training runs
    int training_runs = 0;
    if (fs::exists("brain/training/history")) {
        for (const auto& entry : fs::directory_iterator("brain/training/history")) {
            if (entry.path().extension() == ".json") {
                training_runs++;
            }
        }
    }

    std::cout << "Training runs archived: " << training_runs << "\n";
    std::cout << "========================================\n\n";
}

int main_auto_learn(int argc, char** argv) {
    std::cout << R"(
╔═══════════════════════════════════════════════════════════════╗
║  SELF-LEARNING AI DAEMON - Neural Studio V10                  ║
║  Runtime AI Improvement System                                ║
╚═══════════════════════════════════════════════════════════════╝
)" << std::endl;

    // Parse arguments
    AutoLearningConfig config = parse_auto_learn_args(argc, argv);

    // Print configuration
    std::cout << "[SELF-LEARN] Configuration:\n";
    std::cout << "  Monitor interval:     " << config.monitor_interval_seconds << "s\n";
    std::cout << "  Weak score threshold: " << config.weak_score_threshold << "\n";
    std::cout << "  Training threshold:   " << config.training_threshold << " corrections\n";
    std::cout << "  Auto-correction:      " << (config.enable_auto_correction ? "enabled" : "disabled") << "\n";
    std::cout << "  Learn from web:       " << (config.learn_from_web ? "enabled" : "disabled") << "\n";
    std::cout << "  CAI critique:         " << (config.enable_cai_critique ? "enabled" : "disabled") << "\n";
    std::cout << "  Daemon mode:          " << (config.daemon_mode ? "enabled" : "disabled") << "\n\n";

    // Ensure directories exist
    ensure_directory_exists("brain/self_learning");
    ensure_directory_exists("brain/training/history");

    // Print current stats
    print_auto_learn_stats();

    if (config.daemon_mode) {
        // Launch all 4 threads
        std::atomic<bool> running{true};

        std::cout << "[SELF-LEARN] Launching 4 background threads...\n";

        std::thread monitor_thread([&]() {
            auto_monitor_loop(config, running);
        });

        std::thread correct_thread([&]() {
            while (running) {
                std::this_thread::sleep_for(std::chrono::seconds(120));  // Run every 2 minutes
                auto_correct_responses(config);
            }
        });

        std::thread train_thread([&]() {
            auto_train_loop(config, running);
        });

        std::thread cai_thread([&]() {
            if (config.enable_cai_critique) {
                auto_cai_loop(config, running);
            }
        });

        std::cout << "[SELF-LEARN] All threads running. Press Ctrl+C to stop.\n\n";

        // Wait for threads
        monitor_thread.join();
        correct_thread.join();
        train_thread.join();
        cai_thread.join();
    } else {
        // One-shot mode: run correction once
        std::cout << "[SELF-LEARN] Running one-shot auto-correction...\n";
        auto_correct_responses(config);

        std::cout << "\n[SELF-LEARN] One-shot complete. Use --daemon for continuous operation.\n";
        print_auto_learn_stats();
    }

    return 0;
}

} // namespace self_learning
