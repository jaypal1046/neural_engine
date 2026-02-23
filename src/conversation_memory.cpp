#include "conversation_memory.h"
#include <fstream>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <cstring>

ConversationMemory::ConversationMemory(int max_history)
    : max_history(max_history), next_turn_id(0) {
}

int ConversationMemory::record_turn(
    const std::string& user_msg,
    const std::string& ai_response,
    float confidence,
    const std::vector<std::string>& sources
) {
    Turn turn;
    turn.turn_id = next_turn_id++;
    turn.user_message = user_msg;
    turn.ai_response = ai_response;
    turn.confidence = confidence;
    turn.user_satisfaction = -1.0f;  // Not set yet
    turn.sources = sources;
    turn.timestamp = std::time(nullptr);
    turn.was_corrected = false;

    history.push_back(turn);

    // Prune if too large
    if (static_cast<int>(history.size()) > max_history) {
        history.pop_front();
    }

    return turn.turn_id;
}

void ConversationMemory::set_user_feedback(int turn_id, float satisfaction) {
    for (auto& turn : history) {
        if (turn.turn_id == turn_id) {
            turn.user_satisfaction = satisfaction;
            return;
        }
    }
}

void ConversationMemory::record_correction(int turn_id, const std::string& correct_answer) {
    for (auto& turn : history) {
        if (turn.turn_id == turn_id) {
            turn.was_corrected = true;
            turn.correction = correct_answer;
            turn.user_satisfaction = 0.0f;  // Wrong answer = 0 satisfaction
            return;
        }
    }
}

std::vector<ConversationMemory::Turn> ConversationMemory::get_high_quality_turns(
    float min_satisfaction
) const {
    std::vector<Turn> high_quality;

    for (const auto& turn : history) {
        // Include if:
        // 1. User gave positive feedback
        // 2. OR confidence was high and no negative feedback
        bool is_high_quality = (turn.user_satisfaction >= min_satisfaction) ||
                              (turn.user_satisfaction < 0 && turn.confidence >= 0.8f && !turn.was_corrected);

        if (is_high_quality) {
            high_quality.push_back(turn);
        }
    }

    return high_quality;
}

std::vector<ConversationMemory::Turn> ConversationMemory::get_recent_context(
    int num_turns
) const {
    std::vector<Turn> recent;

    int start = std::max(0, static_cast<int>(history.size()) - num_turns);
    for (int i = start; i < static_cast<int>(history.size()); i++) {
        recent.push_back(history[i]);
    }

    return recent;
}

float ConversationMemory::question_similarity(const std::string& q1, const std::string& q2) const {
    // Simple word overlap similarity
    std::istringstream iss1(q1), iss2(q2);
    std::vector<std::string> words1, words2;
    std::string word;

    while (iss1 >> word) {
        // Convert to lowercase
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words1.push_back(word);
    }

    while (iss2 >> word) {
        std::transform(word.begin(), word.end(), word.begin(), ::tolower);
        words2.push_back(word);
    }

    if (words1.empty() || words2.empty()) return 0.0f;

    // Count overlap
    int overlap = 0;
    for (const auto& w1 : words1) {
        if (std::find(words2.begin(), words2.end(), w1) != words2.end()) {
            overlap++;
        }
    }

    return static_cast<float>(overlap) / std::max(words1.size(), words2.size());
}

int ConversationMemory::find_similar_question(const std::string& question, float threshold) const {
    float max_sim = 0.0f;
    int best_turn_id = -1;

    for (const auto& turn : history) {
        float sim = question_similarity(question, turn.user_message);
        if (sim > max_sim && sim >= threshold) {
            max_sim = sim;
            best_turn_id = turn.turn_id;
        }
    }

    return best_turn_id;
}

bool ConversationMemory::should_ask_for_help(const std::string& question, float confidence) const {
    // Low confidence -> ask for help
    if (confidence < 0.5f) {
        return true;
    }

    // Check if similar questions got negative feedback
    for (const auto& turn : history) {
        float sim = question_similarity(question, turn.user_message);
        if (sim > 0.7f && (turn.user_satisfaction < 0.3f || turn.was_corrected)) {
            // Similar question was answered poorly before
            return true;
        }
    }

    return false;
}

ConversationMemory::Stats ConversationMemory::get_stats() const {
    Stats stats;
    stats.total_turns = history.size();

    float sum_confidence = 0.0f;
    float sum_satisfaction = 0.0f;
    int satisfaction_count = 0;

    for (const auto& turn : history) {
        sum_confidence += turn.confidence;

        if (turn.user_satisfaction >= 0.0f) {
            sum_satisfaction += turn.user_satisfaction;
            satisfaction_count++;
            stats.turns_with_feedback++;

            if (turn.user_satisfaction >= 0.8f) {
                stats.high_quality_turns++;
            }
        }

        if (turn.was_corrected) {
            stats.corrections++;
        }
    }

    if (stats.total_turns > 0) {
        stats.average_confidence = sum_confidence / stats.total_turns;
    }

    if (satisfaction_count > 0) {
        stats.average_satisfaction = sum_satisfaction / satisfaction_count;
    }

    return stats;
}

void ConversationMemory::save(const std::string& file_path) const {
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[Memory] Error: Cannot save to " << file_path << "\n";
        return;
    }

    // Write header
    int history_size = history.size();
    file.write(reinterpret_cast<const char*>(&history_size), sizeof(history_size));
    file.write(reinterpret_cast<const char*>(&next_turn_id), sizeof(next_turn_id));

    // Write turns
    for (const auto& turn : history) {
        file.write(reinterpret_cast<const char*>(&turn.turn_id), sizeof(turn.turn_id));

        // Write strings
        int len = turn.user_message.length();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(turn.user_message.data(), len);

        len = turn.ai_response.length();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(turn.ai_response.data(), len);

        file.write(reinterpret_cast<const char*>(&turn.confidence), sizeof(turn.confidence));
        file.write(reinterpret_cast<const char*>(&turn.user_satisfaction), sizeof(turn.user_satisfaction));

        // Write sources
        int source_count = turn.sources.size();
        file.write(reinterpret_cast<const char*>(&source_count), sizeof(source_count));
        for (const auto& source : turn.sources) {
            len = source.length();
            file.write(reinterpret_cast<const char*>(&len), sizeof(len));
            file.write(source.data(), len);
        }

        file.write(reinterpret_cast<const char*>(&turn.timestamp), sizeof(turn.timestamp));
        file.write(reinterpret_cast<const char*>(&turn.was_corrected), sizeof(turn.was_corrected));

        len = turn.correction.length();
        file.write(reinterpret_cast<const char*>(&len), sizeof(len));
        file.write(turn.correction.data(), len);
    }

    std::cout << "[Memory] Saved " << history_size << " conversation turns\n";
}

void ConversationMemory::load(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[Memory] Error: Cannot load from " << file_path << "\n";
        return;
    }

    history.clear();

    // Read header
    int history_size;
    file.read(reinterpret_cast<char*>(&history_size), sizeof(history_size));
    file.read(reinterpret_cast<char*>(&next_turn_id), sizeof(next_turn_id));

    // Read turns
    for (int i = 0; i < history_size; i++) {
        Turn turn;

        file.read(reinterpret_cast<char*>(&turn.turn_id), sizeof(turn.turn_id));

        // Read strings
        int len;
        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        turn.user_message.resize(len);
        file.read(&turn.user_message[0], len);

        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        turn.ai_response.resize(len);
        file.read(&turn.ai_response[0], len);

        file.read(reinterpret_cast<char*>(&turn.confidence), sizeof(turn.confidence));
        file.read(reinterpret_cast<char*>(&turn.user_satisfaction), sizeof(turn.user_satisfaction));

        // Read sources
        int source_count;
        file.read(reinterpret_cast<char*>(&source_count), sizeof(source_count));
        for (int j = 0; j < source_count; j++) {
            file.read(reinterpret_cast<char*>(&len), sizeof(len));
            std::string source(len, '\0');
            file.read(&source[0], len);
            turn.sources.push_back(source);
        }

        file.read(reinterpret_cast<char*>(&turn.timestamp), sizeof(turn.timestamp));
        file.read(reinterpret_cast<char*>(&turn.was_corrected), sizeof(turn.was_corrected));

        file.read(reinterpret_cast<char*>(&len), sizeof(len));
        turn.correction.resize(len);
        file.read(&turn.correction[0], len);

        history.push_back(turn);
    }

    std::cout << "[Memory] Loaded " << history_size << " conversation turns\n";
}

void ConversationMemory::prune(int keep_recent) {
    while (static_cast<int>(history.size()) > keep_recent) {
        history.pop_front();
    }
}
