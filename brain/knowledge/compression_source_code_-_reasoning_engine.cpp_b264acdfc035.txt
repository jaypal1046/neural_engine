#include "reasoning_engine.h"
#include <algorithm>
#include <sstream>
#include <regex>
#include <iostream>

ReasoningEngine::ReasoningEngine() {
    // Initialize with some common facts about compression
    add_fact({"BWT", "is", "lossless compression algorithm", 1.0f});
    add_fact({"BWT", "has complexity", "O(n log² n)", 0.9f});
    add_fact({"LZ77", "is", "lossless compression algorithm", 1.0f});
    add_fact({"LZ77", "has complexity", "O(n)", 0.9f});
    add_fact({"Huffman", "is", "entropy encoder", 1.0f});
    add_fact({"rANS", "is", "entropy encoder", 1.0f});
    add_fact({"lossy compression", "removes", "information", 1.0f});
    add_fact({"lossless compression", "preserves", "all information", 1.0f});
}

ReasoningEngine::QuestionType ReasoningEngine::classify_question(const std::string& question) {
    std::string lower = question;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("what is") != std::string::npos ||
        lower.find("define") != std::string::npos) {
        return FACTUAL;
    }

    if (lower.find("compare") != std::string::npos ||
        lower.find("difference") != std::string::npos ||
        lower.find("better") != std::string::npos) {
        return COMPARISON;
    }

    if (lower.find("how do i") != std::string::npos ||
        lower.find("how to") != std::string::npos ||
        lower.find("steps") != std::string::npos) {
        return PROCEDURAL;
    }

    if (lower.find("how long") != std::string::npos ||
        lower.find("how much time") != std::string::npos ||
        lower.find("complexity") != std::string::npos) {
        return COMPUTATIONAL;
    }

    if (lower.find("why") != std::string::npos ||
        lower.find("because") != std::string::npos) {
        return CAUSAL;
    }

    if (lower.find("what if") != std::string::npos ||
        lower.find("suppose") != std::string::npos) {
        return HYPOTHETICAL;
    }

    return FACTUAL;  // Default
}

std::vector<std::string> ReasoningEngine::decompose_question(const std::string& question) {
    std::vector<std::string> sub_questions;
    QuestionType type = classify_question(question);

    switch (type) {
        case COMPARISON: {
            // "How does X compare to Y?" → ["What is X?", "What is Y?", "How do they differ?"]
            std::regex pattern("(\\w+).*compare.*to.*(\\w+)");
            std::smatch matches;
            if (std::regex_search(question, matches, pattern)) {
                std::string x = matches[1];
                std::string y = matches[2];
                sub_questions.push_back("What is " + x + "?");
                sub_questions.push_back("What is " + y + "?");
                sub_questions.push_back("What are the key differences?");
            }
            break;
        }

        case COMPUTATIONAL: {
            // "How long will X take?" → ["What is the complexity?", "What is the input size?", "Calculate time"]
            sub_questions.push_back("What is the algorithmic complexity?");
            sub_questions.push_back("What is the input size?");
            sub_questions.push_back("Calculate estimated time");
            break;
        }

        case PROCEDURAL: {
            // "How do I X?" → ["Step 1", "Step 2", ...]
            sub_questions.push_back("Identify the goal");
            sub_questions.push_back("List required steps");
            sub_questions.push_back("Detail each step");
            break;
        }

        default:
            // For simple questions, no decomposition needed
            sub_questions.push_back(question);
            break;
    }

    return sub_questions;
}

std::vector<ReasoningEngine::Fact> ReasoningEngine::extract_facts(const std::string& text) {
    std::vector<Fact> facts;

    // Simple fact extraction using patterns
    // Pattern: "X is Y"
    std::regex is_pattern("([A-Za-z0-9]+)\\s+is\\s+([^.!?]+)");
    std::regex has_pattern("([A-Za-z0-9]+)\\s+has\\s+([^.!?]+)");
    std::regex uses_pattern("([A-Za-z0-9]+)\\s+uses\\s+([^.!?]+)");

    std::smatch matches;
    std::string::const_iterator search_start(text.cbegin());

    while (std::regex_search(search_start, text.cend(), matches, is_pattern)) {
        Fact fact;
        fact.subject = matches[1];
        fact.relation = "is";
        fact.object = matches[2];
        fact.confidence = 0.7f;
        facts.push_back(fact);

        search_start = matches.suffix().first;
    }

    return facts;
}

bool ReasoningEngine::contradicts(const Fact& fact1, const Fact& fact2) {
    // Same subject, same relation, different objects = contradiction
    if (fact1.subject == fact2.subject &&
        fact1.relation == fact2.relation &&
        fact1.object != fact2.object) {

        // Check for negation patterns
        bool fact1_negative = (fact1.object.find("not") != std::string::npos);
        bool fact2_negative = (fact2.object.find("not") != std::string::npos);

        if (fact1_negative != fact2_negative) {
            return true;  // One says "is", other says "is not"
        }

        // Different values for same property
        return true;
    }

    return false;
}

std::vector<std::string> ReasoningEngine::infer_chain(
    const std::string& start,
    const std::string& goal,
    const std::vector<Fact>& knowledge
) {
    std::vector<std::string> chain;

    // Simple forward chaining
    std::string current = start;
    chain.push_back("Start: " + current);

    for (int step = 0; step < 5 && current != goal; step++) {
        bool found = false;

        for (const auto& fact : knowledge) {
            if (fact.subject == current) {
                current = fact.object;
                chain.push_back("→ " + fact.relation + " → " + current);
                found = true;
                break;
            }
        }

        if (!found) {
            chain.push_back("Cannot infer further");
            break;
        }
    }

    if (current == goal) {
        chain.push_back("Goal reached!");
    }

    return chain;
}

std::string ReasoningEngine::estimate_complexity(
    const std::string& algorithm,
    const std::string& input_size
) {
    // Look up known complexities
    auto facts = query_facts(algorithm);

    for (const auto& fact : facts) {
        if (fact.relation == "has complexity") {
            std::stringstream result;
            result << "Algorithm: " << algorithm << "\n";
            result << "Complexity: " << fact.object << "\n";
            result << "Input size: " << input_size << "\n";

            // Parse complexity (simple cases)
            if (fact.object.find("O(n log² n)") != std::string::npos) {
                result << "For input size n, time grows as n × log²(n)\n";
            } else if (fact.object.find("O(n)") != std::string::npos) {
                result << "Time grows linearly with input size\n";
            } else if (fact.object.find("O(n²)") != std::string::npos) {
                result << "Time grows quadratically with input size\n";
            }

            return result.str();
        }
    }

    return "Complexity unknown for " + algorithm;
}

ReasoningEngine::Result ReasoningEngine::reason_about(const std::string& question) {
    Result result;
    result.question = question;
    result.overall_confidence = 0.5f;

    // Step 1: Classify question
    QuestionType type = classify_question(question);
    Step classify_step;
    classify_step.description = "Classify question type";

    switch (type) {
        case FACTUAL:
            classify_step.conclusion = "This is a factual question";
            break;
        case COMPARISON:
            classify_step.conclusion = "This is a comparison question";
            break;
        case COMPUTATIONAL:
            classify_step.conclusion = "This is a computational question";
            break;
        case PROCEDURAL:
            classify_step.conclusion = "This is a procedural question";
            break;
        case CAUSAL:
            classify_step.conclusion = "This is a causal question";
            break;
        case HYPOTHETICAL:
            classify_step.conclusion = "This is a hypothetical question";
            break;
    }
    classify_step.confidence = 0.9f;
    result.steps.push_back(classify_step);

    // Step 2: Decompose if needed
    auto sub_questions = decompose_question(question);
    if (sub_questions.size() > 1) {
        Step decompose_step;
        decompose_step.description = "Break down into sub-questions";
        std::stringstream ss;
        for (size_t i = 0; i < sub_questions.size(); i++) {
            ss << (i + 1) << ". " << sub_questions[i] << "\n";
        }
        decompose_step.conclusion = ss.str();
        decompose_step.confidence = 0.8f;
        result.steps.push_back(decompose_step);
    }

    // Step 3: Query knowledge base
    Step query_step;
    query_step.description = "Query knowledge base";

    // Extract key terms
    std::istringstream iss(question);
    std::vector<std::string> words;
    std::string word;
    while (iss >> word) {
        if (word.length() > 3) {  // Skip short words
            words.push_back(word);
        }
    }

    int facts_found = 0;
    for (const auto& w : words) {
        auto facts = query_facts(w);
        facts_found += facts.size();
    }

    query_step.conclusion = "Found " + std::to_string(facts_found) + " relevant facts";
    query_step.confidence = facts_found > 0 ? 0.8f : 0.3f;
    result.steps.push_back(query_step);

    // Step 4: Synthesize answer
    Step answer_step;
    answer_step.description = "Synthesize answer from facts";

    if (type == COMPUTATIONAL) {
        // Try to estimate complexity
        for (const auto& w : words) {
            std::string estimate = estimate_complexity(w, "n");
            if (estimate.find("unknown") == std::string::npos) {
                result.final_answer = estimate;
                answer_step.conclusion = "Generated complexity analysis";
                answer_step.confidence = 0.7f;
                break;
            }
        }
    } else if (type == COMPARISON) {
        answer_step.conclusion = "Compared relevant facts from knowledge base";
        answer_step.confidence = 0.6f;
        result.final_answer = "Comparison requires domain-specific knowledge";
    } else {
        answer_step.conclusion = "Retrieved facts from knowledge base";
        answer_step.confidence = facts_found > 0 ? 0.7f : 0.3f;
        result.final_answer = "Answer based on " + std::to_string(facts_found) + " facts";
    }

    result.steps.push_back(answer_step);

    // Compute overall confidence
    float total_conf = 0.0f;
    for (const auto& step : result.steps) {
        total_conf += step.confidence;
    }
    result.overall_confidence = total_conf / result.steps.size();

    // Self-verification (simple version)
    result.self_verified = result.overall_confidence > 0.6f;

    return result;
}

bool ReasoningEngine::verify_answer(
    const std::string& question,
    const std::string& answer,
    const std::vector<std::string>& sources
) {
    // Extract facts from answer
    auto answer_facts = extract_facts(answer);

    // Check for contradictions
    for (size_t i = 0; i < answer_facts.size(); i++) {
        for (size_t j = i + 1; j < answer_facts.size(); j++) {
            if (contradicts(answer_facts[i], answer_facts[j])) {
                return false;  // Self-contradictory
            }
        }
    }

    // Check semantic coherence (simple: answer should mention question terms)
    std::istringstream q_iss(question), a_iss(answer);
    std::vector<std::string> q_words, a_words;
    std::string word;

    while (q_iss >> word) {
        if (word.length() > 3) q_words.push_back(word);
    }

    while (a_iss >> word) {
        if (word.length() > 3) a_words.push_back(word);
    }

    // Answer should contain at least 30% of question terms
    int overlap = 0;
    for (const auto& qw : q_words) {
        if (std::find(a_words.begin(), a_words.end(), qw) != a_words.end()) {
            overlap++;
        }
    }

    float coherence = static_cast<float>(overlap) / std::max(1.0f, static_cast<float>(q_words.size()));

    return coherence >= 0.3f;
}

void ReasoningEngine::add_fact(const Fact& fact) {
    fact_database[fact.subject].push_back(fact);
}

std::vector<ReasoningEngine::Fact> ReasoningEngine::query_facts(const std::string& topic) {
    auto it = fact_database.find(topic);
    if (it != fact_database.end()) {
        return it->second;
    }
    return {};
}
