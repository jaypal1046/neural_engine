#pragma once

#include <string>
#include <vector>
#include <map>

/**
 * Reasoning Engine - Chain-of-Thought for Complex Problems
 *
 * Enables the AI to:
 * 1. Break down complex questions into steps
 * 2. Reason through multi-step problems
 * 3. Show its work (explainable AI)
 * 4. Verify its own answers
 * 5. Detect contradictions
 *
 * This is what separates a search engine from true intelligence.
 *
 * Example:
 *   ReasoningEngine reasoner;
 *   auto steps = reasoner.reason_about("If BWT compresses 1GB in 5 min, how long for 10GB?");
 *
 *   Output:
 *     Step 1: BWT complexity is O(n log² n)
 *     Step 2: 1GB → 5 minutes
 *     Step 3: 10GB is 10× larger
 *     Step 4: Time ≈ 5 × 10 × log²(10) ≈ 166 minutes
 *     Answer: About 2.7 hours
 */

class ReasoningEngine {
public:
    /**
     * Reasoning step
     */
    struct Step {
        std::string description;       // What we're doing
        std::string reasoning;         // Why we're doing it
        std::string conclusion;        // What we learned
        float confidence;              // How sure we are (0-1)
    };

    /**
     * Reasoning result
     */
    struct Result {
        std::string question;
        std::vector<Step> steps;
        std::string final_answer;
        float overall_confidence;
        bool self_verified;            // Did it pass self-check?
        std::vector<std::string> contradictions;  // If any found
    };

    ReasoningEngine();

    /**
     * Reason about a question
     * Returns step-by-step reasoning process
     */
    Result reason_about(const std::string& question);

    /**
     * Decompose complex question into sub-questions
     * Example: "How does BWT compare to LZ77?"
     *   → ["What is BWT?", "What is LZ77?", "How do they differ?"]
     */
    std::vector<std::string> decompose_question(const std::string& question);

    /**
     * Extract facts from text
     * Example: "BWT is a lossless compression algorithm"
     *   → Fact("BWT", "is", "lossless compression algorithm")
     */
    struct Fact {
        std::string subject;
        std::string relation;
        std::string object;
        float confidence;
    };

    std::vector<Fact> extract_facts(const std::string& text);

    /**
     * Check if two facts contradict each other
     */
    bool contradicts(const Fact& fact1, const Fact& fact2);

    /**
     * Build inference chain
     * Example: A → B, B → C implies A → C
     */
    std::vector<std::string> infer_chain(
        const std::string& start,
        const std::string& goal,
        const std::vector<Fact>& knowledge
    );

    /**
     * Verify answer makes sense
     * Checks:
     * - Semantic coherence
     * - Factual consistency
     * - No contradictions
     */
    bool verify_answer(
        const std::string& question,
        const std::string& answer,
        const std::vector<std::string>& sources
    );

    /**
     * Estimate computation complexity
     * For questions like "how long will this take?"
     */
    std::string estimate_complexity(const std::string& algorithm, const std::string& input_size);

private:
    /**
     * Detect question type
     */
    enum QuestionType {
        FACTUAL,         // "What is X?"
        COMPARISON,      // "How does X compare to Y?"
        PROCEDURAL,      // "How do I do X?"
        COMPUTATIONAL,   // "How long will X take?"
        CAUSAL,          // "Why does X happen?"
        HYPOTHETICAL     // "What if X?"
    };

    QuestionType classify_question(const std::string& question);

    /**
     * Knowledge base of common facts
     */
    std::map<std::string, std::vector<Fact>> fact_database;

    /**
     * Add fact to database
     */
    void add_fact(const Fact& fact);

    /**
     * Query facts about a topic
     */
    std::vector<Fact> query_facts(const std::string& topic);
};
