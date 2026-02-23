#pragma once

#include <string>
#include <vector>
#include <deque>
#include <ctime>

/**
 * Conversation Memory - Learn from every interaction
 *
 * Features:
 * 1. Remember conversation history
 * 2. Track user satisfaction (thumbs up/down)
 * 3. Learn from corrections
 * 4. Replay high-quality conversations for training
 * 5. Active learning (ask for help when uncertain)
 *
 * This makes the AI self-improving - it gets smarter with every use.
 *
 * Example:
 *   ConversationMemory memory;
 *   memory.record_turn("What is BWT?", "BWT is a transform...", 0.9);
 *   memory.set_user_feedback(0, 1.0);  // User liked it!
 *
 *   // Later: replay for training
 *   auto high_quality = memory.get_high_quality_turns(0.8);
 *   word_model.train(high_quality);
 */

class ConversationMemory {
public:
    /**
     * Single conversation turn
     */
    struct Turn {
        int turn_id;
        std::string user_message;
        std::string ai_response;
        float confidence;              // AI's confidence (0-1)
        float user_satisfaction;       // User feedback (0-1, -1 = not set)
        std::vector<std::string> sources;  // Knowledge sources used
        time_t timestamp;
        bool was_corrected;           // Did user provide correction?
        std::string correction;       // If corrected, the right answer
    };

    ConversationMemory(int max_history = 1000);

    /**
     * Record a conversation turn
     * @return Turn ID
     */
    int record_turn(
        const std::string& user_msg,
        const std::string& ai_response,
        float confidence,
        const std::vector<std::string>& sources = {}
    );

    /**
     * Set user feedback for a turn
     * @param turn_id Turn to update
     * @param satisfaction 0.0 (bad) to 1.0 (excellent)
     */
    void set_user_feedback(int turn_id, float satisfaction);

    /**
     * Record a user correction
     * @param turn_id Turn that was wrong
     * @param correct_answer The right answer
     */
    void record_correction(int turn_id, const std::string& correct_answer);

    /**
     * Get high-quality turns for training
     * @param min_satisfaction Minimum satisfaction score (default 0.8)
     * @return Turns suitable for training
     */
    std::vector<Turn> get_high_quality_turns(float min_satisfaction = 0.8f) const;

    /**
     * Get recent conversation context
     * @param num_turns Number of recent turns to return
     */
    std::vector<Turn> get_recent_context(int num_turns = 5) const;

    /**
     * Check if question is similar to recent conversations
     * Helps avoid repetition and can use cached answers
     */
    int find_similar_question(const std::string& question, float threshold = 0.8f) const;

    /**
     * Active learning: should we ask user for help?
     * Returns true if:
     * - Confidence is low (<0.5)
     * - Similar questions got negative feedback before
     */
    bool should_ask_for_help(const std::string& question, float confidence) const;

    /**
     * Get statistics
     */
    struct Stats {
        int total_turns = 0;
        int turns_with_feedback = 0;
        int corrections = 0;
        float average_confidence = 0.0f;
        float average_satisfaction = 0.0f;
        int high_quality_turns = 0;  // satisfaction >= 0.8
    };

    Stats get_stats() const;

    /**
     * Save/load conversation history
     */
    void save(const std::string& file_path) const;
    void load(const std::string& file_path);

    /**
     * Clear old conversations (keep only recent N)
     */
    void prune(int keep_recent = 1000);

private:
    std::deque<Turn> history;
    int max_history;
    int next_turn_id;

    /**
     * Compute similarity between questions (simple)
     */
    float question_similarity(const std::string& q1, const std::string& q2) const;
};
