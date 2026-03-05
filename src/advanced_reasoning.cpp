// =============================================================================
// Advanced Reasoning Implementation
// =============================================================================

#include "advanced_reasoning.h"
#include <iostream>
#include <sstream>
#include <algorithm>
#include <queue>
#include <cmath>
#include <random>
#include <iomanip>

// External functions from neural_engine (defined in self_learning_bridge.cpp, global namespace)
extern std::string run_ai_ask(const std::string& question);
extern int run_score_response(const std::string& question, const std::string& answer);

namespace advanced_reasoning {
using ::run_ai_ask;
using ::run_score_response;

// =============================================================================
// Phase G1: Tree-of-Thought Implementation
// =============================================================================

std::vector<std::string> generate_next_thoughts(const std::string& problem,
                                                  const std::string& current_thought,
                                                  int depth) {
    std::vector<std::string> next_thoughts;

    // Construct prompt for generating next reasoning steps
    std::stringstream prompt;
    prompt << "Problem: " << problem << "\n\n";
    prompt << "Current reasoning: " << current_thought << "\n\n";
    prompt << "Generate 3 possible next reasoning steps. Be concise.";

    std::string response = run_ai_ask(prompt.str());

    // Parse response (simple: split by newlines)
    std::istringstream iss(response);
    std::string line;
    while (std::getline(iss, line)) {
        // Clean up line
        line.erase(0, line.find_first_not_of(" \t\r\n-123."));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);

        if (line.length() > 10) {  // Skip very short lines
            next_thoughts.push_back(line);
            if (next_thoughts.size() >= 3) break;
        }
    }

    // Fallback if parsing failed
    if (next_thoughts.empty()) {
        next_thoughts.push_back(current_thought + " (continued)");
        next_thoughts.push_back("Alternative: " + current_thought.substr(0, 50));
        next_thoughts.push_back("Let's try another approach");
    }

    return next_thoughts;
}

float score_thought(const std::string& problem, const std::string& thought) {
    // Combine problem and thought for scoring
    std::string combined = "Q: " + problem + "\nA: " + thought;

    // Use external scoring function
    int score = run_score_response(problem, thought);

    return static_cast<float>(score);
}

std::vector<std::string> build_path(const std::shared_ptr<ThoughtNode>& node) {
    std::vector<std::string> path;

    auto current = node;
    while (current) {
        path.insert(path.begin(), current->thought);
        current = current->parent.lock();
    }

    return path;
}

std::shared_ptr<ThoughtNode> find_best_leaf(const std::shared_ptr<ThoughtNode>& root) {
    std::shared_ptr<ThoughtNode> best = root;
    float best_score = root->score;

    // BFS to find all leaves
    std::queue<std::shared_ptr<ThoughtNode>> queue;
    queue.push(root);

    while (!queue.empty()) {
        auto node = queue.front();
        queue.pop();

        // If leaf and better score
        if (node->children.empty() && node->score > best_score) {
            best = node;
            best_score = node->score;
        }

        // Add children to queue
        for (auto& child : node->children) {
            queue.push(child);
        }
    }

    return best;
}

ToTResult tree_of_thought(const std::string& problem, const ToTConfig& config) {
    std::cout << "[ToT] Starting Tree-of-Thought search...\n";
    std::cout << "[ToT] Problem: " << problem.substr(0, 60) << "...\n";
    std::cout << "[ToT] Max depth: " << config.max_depth
              << ", Branches: " << config.branches_per_node << "\n\n";

    ToTResult result;
    result.nodes_explored = 0;
    result.paths_evaluated = 0;

    // Create root node (initial thought)
    auto root = std::make_shared<ThoughtNode>("Let me think about this step by step.", 50.0f, 0);
    result.nodes_explored++;

    // Beam search: keep top-k nodes at each level
    std::vector<std::shared_ptr<ThoughtNode>> current_level = {root};

    for (int depth = 1; depth <= config.max_depth; depth++) {
        std::cout << "[ToT] Depth " << depth << "/" << config.max_depth << "\n";

        std::vector<std::shared_ptr<ThoughtNode>> next_level;

        // Expand each node in current level
        for (auto& node : current_level) {
            // Generate next thoughts
            auto next_thoughts = generate_next_thoughts(problem, node->thought, depth);

            // Create child nodes
            for (const auto& thought : next_thoughts) {
                // Score this thought
                float score = score_thought(problem, thought);
                result.nodes_explored++;

                std::cout << "[ToT]   Node " << result.nodes_explored
                          << " (depth " << depth << "): Score = " << std::fixed
                          << std::setprecision(1) << score << "\n";

                // Prune low-scoring thoughts
                if (score >= config.pruning_threshold) {
                    auto child = std::make_shared<ThoughtNode>(thought, score, depth);
                    child->parent = node;
                    node->children.push_back(child);
                    next_level.push_back(child);
                }
            }
        }

        // If no nodes survived pruning, stop
        if (next_level.empty()) {
            std::cout << "[ToT] No nodes passed pruning threshold. Stopping.\n";
            break;
        }

        // Keep only top-k nodes (beam search)
        if (config.use_beam_search && (int)next_level.size() > config.top_k_paths) {
            std::sort(next_level.begin(), next_level.end(),
                     [](const auto& a, const auto& b) { return a->score > b->score; });
            next_level.resize(config.top_k_paths);

            std::cout << "[ToT] Pruned to top " << config.top_k_paths << " nodes\n";
        }

        current_level = next_level;
        std::cout << "[ToT] " << current_level.size() << " nodes at depth " << depth << "\n\n";
    }

    // Find best leaf node
    auto best_leaf = find_best_leaf(root);
    result.reasoning_path = build_path(best_leaf);
    result.confidence = best_leaf->score;
    result.paths_evaluated = result.nodes_explored / config.branches_per_node;

    // Final answer is the last thought in best path
    result.final_answer = best_leaf->thought;

    std::cout << "[ToT] Search complete!\n";
    std::cout << "[ToT] Nodes explored: " << result.nodes_explored << "\n";
    std::cout << "[ToT] Best path confidence: " << std::fixed << std::setprecision(1)
              << result.confidence << "%\n";
    std::cout << "[ToT] Path length: " << result.reasoning_path.size() << " steps\n\n";

    return result;
}

// =============================================================================
// Phase G2: Multi-Agent Debate Implementation
// =============================================================================

std::string generate_agent_response(const std::string& question,
                                     const AgentConfig& agent,
                                     const std::vector<std::string>& previous_responses) {
    std::stringstream prompt;
    prompt << agent.system_prompt << "\n\n";
    prompt << "Question: " << question << "\n\n";

    if (!previous_responses.empty()) {
        prompt << "Other agents have said:\n";
        for (size_t i = 0; i < previous_responses.size(); i++) {
            prompt << "Agent " << (i + 1) << ": " << previous_responses[i] << "\n\n";
        }
        prompt << "Your response (agree, disagree, or add new insights):";
    } else {
        prompt << "Your initial answer:";
    }

    return run_ai_ask(prompt.str());
}

float score_agent_response(const std::string& question, const std::string& response) {
    return static_cast<float>(run_score_response(question, response));
}

std::string extract_consensus(const std::string& question,
                              const std::vector<std::string>& final_responses) {
    std::stringstream prompt;
    prompt << "Question: " << question << "\n\n";
    prompt << "Multiple experts provided these answers:\n\n";

    for (size_t i = 0; i < final_responses.size(); i++) {
        prompt << "Expert " << (i + 1) << ": " << final_responses[i] << "\n\n";
    }

    prompt << "Synthesize a consensus answer that combines the best insights from all experts:";

    return run_ai_ask(prompt.str());
}

std::vector<std::string> extract_key_insights(const DebateResult& debate) {
    std::vector<std::string> insights;

    // Extract from final round
    if (!debate.rounds.empty()) {
        const auto& final_round = debate.rounds.back();

        for (const auto& response : final_round.agent_responses) {
            // Simple extraction: first sentence of each response
            size_t period = response.find('.');
            if (period != std::string::npos && period < 100) {
                insights.push_back(response.substr(0, period + 1));
            }
        }
    }

    return insights;
}

DebateResult multi_agent_debate(const std::string& question,
                                 int num_agents,
                                 int num_rounds) {
    std::cout << "[DEBATE] Starting multi-agent debate...\n";
    std::cout << "[DEBATE] Question: " << question.substr(0, 60) << "...\n";
    std::cout << "[DEBATE] Agents: " << num_agents << ", Rounds: " << num_rounds << "\n\n";

    DebateResult result;
    result.question = question;

    // Create agent configs with different temperatures/perspectives
    std::vector<AgentConfig> agents;
    std::vector<std::string> perspectives = {
        "You are a careful, methodical thinker who values accuracy and precision.",
        "You are a creative thinker who explores unconventional ideas.",
        "You are a critical thinker who questions assumptions and identifies flaws.",
        "You are a practical thinker who focuses on real-world applications.",
        "You are an analytical thinker who breaks down complex problems."
    };

    for (int i = 0; i < num_agents; i++) {
        AgentConfig agent;
        agent.name = "Agent_" + std::string(1, 'A' + i);
        agent.temperature = 0.7f + (i * 0.2f);  // Vary temperature
        agent.max_response_length = 200;
        agent.system_prompt = perspectives[i % perspectives.size()];
        agents.push_back(agent);
    }

    // Run debate rounds
    for (int round = 0; round < num_rounds; round++) {
        std::cout << "[DEBATE] === Round " << (round + 1) << "/" << num_rounds << " ===\n";

        DebateRound debate_round;
        debate_round.round_number = round + 1;

        std::vector<std::string> previous_responses;
        if (!result.rounds.empty()) {
            previous_responses = result.rounds.back().agent_responses;
        }

        // Each agent responds
        for (int i = 0; i < num_agents; i++) {
            std::cout << "[DEBATE] " << agents[i].name << " thinking...\n";

            std::string response = generate_agent_response(question, agents[i], previous_responses);
            float score = score_agent_response(question, response);

            debate_round.agent_responses.push_back(response);
            debate_round.response_scores.push_back(score);

            std::cout << "[DEBATE] " << agents[i].name << " (score: " << std::fixed
                      << std::setprecision(1) << score << "): "
                      << response.substr(0, 80) << "...\n";

            previous_responses.push_back(response);
        }

        // Find best response in this round
        auto max_it = std::max_element(debate_round.response_scores.begin(),
                                       debate_round.response_scores.end());
        int best_idx = std::distance(debate_round.response_scores.begin(), max_it);
        debate_round.best_response = debate_round.agent_responses[best_idx];

        result.rounds.push_back(debate_round);
        std::cout << "[DEBATE] Best response in round: " << agents[best_idx].name << "\n\n";
    }

    // Extract consensus from final round
    std::cout << "[DEBATE] Extracting consensus...\n";
    result.final_consensus = extract_consensus(question, result.rounds.back().agent_responses);
    result.consensus_confidence = score_agent_response(question, result.final_consensus);

    // Extract key insights
    result.key_insights = extract_key_insights(result);

    std::cout << "[DEBATE] Debate complete!\n";
    std::cout << "[DEBATE] Consensus confidence: " << std::fixed << std::setprecision(1)
              << result.consensus_confidence << "%\n";
    std::cout << "[DEBATE] Key insights: " << result.key_insights.size() << "\n\n";

    return result;
}

// =============================================================================
// Phase G3: Self-Reflection Implementation
// =============================================================================

std::string generate_initial_response(const std::string& question) {
    return run_ai_ask(question);
}

Reflection critique_response(const std::string& question, const std::string& response) {
    Reflection reflection;
    reflection.original_response = response;
    reflection.original_score = static_cast<float>(run_score_response(question, response));

    // Generate critique
    std::stringstream prompt;
    prompt << "Critically analyze this Q&A:\n\n";
    prompt << "Question: " << question << "\n\n";
    prompt << "Answer: " << response << "\n\n";
    prompt << "Identify:\n";
    prompt << "1. Weaknesses (what's missing or wrong)\n";
    prompt << "2. Strengths (what's good)\n";
    prompt << "3. How to improve it\n";

    reflection.critique = run_ai_ask(prompt.str());

    // Parse critique (simple extraction)
    std::istringstream iss(reflection.critique);
    std::string line;
    std::string current_section;

    while (std::getline(iss, line)) {
        if (line.find("Weakness") != std::string::npos || line.find("weakness") != std::string::npos) {
            current_section = "weakness";
        } else if (line.find("Strength") != std::string::npos || line.find("strength") != std::string::npos) {
            current_section = "strength";
        } else if (line.find("Improve") != std::string::npos || line.find("improve") != std::string::npos) {
            current_section = "improve";
        } else if (!line.empty() && line.find_first_not_of(" \t-123.") != std::string::npos) {
            // Clean line
            line.erase(0, line.find_first_not_of(" \t-123."));

            if (current_section == "weakness") {
                reflection.weaknesses.push_back(line);
            } else if (current_section == "strength") {
                reflection.strengths.push_back(line);
            } else if (current_section == "improve") {
                reflection.improvements.push_back(line);
            }
        }
    }

    return reflection;
}

std::string improve_response(const std::string& question,
                             const std::string& original_response,
                             const Reflection& reflection) {
    std::stringstream prompt;
    prompt << "Improve this answer based on the critique:\n\n";
    prompt << "Question: " << question << "\n\n";
    prompt << "Original answer: " << original_response << "\n\n";
    prompt << "Weaknesses to fix:\n";
    for (const auto& w : reflection.weaknesses) {
        prompt << "- " << w << "\n";
    }
    prompt << "\nSuggestions:\n";
    for (const auto& i : reflection.improvements) {
        prompt << "- " << i << "\n";
    }
    prompt << "\nProvide an improved answer:";

    return run_ai_ask(prompt.str());
}

bool has_converged(const ReflectionResult& result, float target_score) {
    return result.improved_score >= target_score ||
           (result.iterations > 1 && result.improved_score <= result.reflection.original_score);
}

ReflectionResult self_reflect(const std::string& question,
                              float target_score,
                              int max_iterations) {
    std::cout << "[REFLECT] Starting self-reflection...\n";
    std::cout << "[REFLECT] Question: " << question.substr(0, 60) << "...\n";
    std::cout << "[REFLECT] Target score: " << target_score << "%\n\n";

    ReflectionResult result;
    result.question = question;
    result.iterations = 0;
    result.converged = false;

    // Generate initial response
    std::cout << "[REFLECT] Iteration 1: Generating initial response...\n";
    result.improved_response = generate_initial_response(question);
    result.improved_score = static_cast<float>(run_score_response(question, result.improved_response));

    std::cout << "[REFLECT] Initial score: " << std::fixed << std::setprecision(1)
              << result.improved_score << "%\n\n";

    // Reflection loop
    for (int iter = 1; iter < max_iterations; iter++) {
        result.iterations = iter;

        // Check convergence
        if (result.improved_score >= target_score) {
            std::cout << "[REFLECT] Target score reached!\n";
            result.converged = true;
            break;
        }

        std::cout << "[REFLECT] Iteration " << (iter + 1) << ": Critiquing response...\n";

        // Critique current response
        result.reflection = critique_response(question, result.improved_response);

        std::cout << "[REFLECT] Weaknesses: " << result.reflection.weaknesses.size() << "\n";
        std::cout << "[REFLECT] Improvements: " << result.reflection.improvements.size() << "\n";

        // Improve response
        std::cout << "[REFLECT] Generating improved response...\n";
        std::string new_response = improve_response(question, result.improved_response, result.reflection);
        float new_score = static_cast<float>(run_score_response(question, new_response));

        std::cout << "[REFLECT] New score: " << std::fixed << std::setprecision(1)
                  << new_score << "% (was " << result.improved_score << "%)\n\n";

        // Check if improved
        if (new_score > result.improved_score) {
            result.improved_response = new_response;
            result.improved_score = new_score;
        } else {
            std::cout << "[REFLECT] No improvement. Converging.\n";
            result.converged = true;
            break;
        }
    }

    std::cout << "[REFLECT] Self-reflection complete!\n";
    std::cout << "[REFLECT] Final score: " << std::fixed << std::setprecision(1)
              << result.improved_score << "% (started at " << result.reflection.original_score << "%)\n";
    std::cout << "[REFLECT] Iterations: " << result.iterations << "\n";
    std::cout << "[REFLECT] Converged: " << (result.converged ? "Yes" : "No") << "\n\n";

    return result;
}

// =============================================================================
// Combined Advanced Reasoning
// =============================================================================

AdvancedReasoningResult combined_reasoning(const std::string& question) {
    std::cout << "\n";
    std::cout << "╔═══════════════════════════════════════════════════════════════╗\n";
    std::cout << "║         ADVANCED REASONING - Combined Approach               ║\n";
    std::cout << "╚═══════════════════════════════════════════════════════════════╝\n";
    std::cout << "\n";

    AdvancedReasoningResult result;
    result.question = question;

    // Phase G1: Tree-of-Thought
    std::cout << "[COMBINED] Phase 1/3: Tree-of-Thought Search...\n";
    ToTConfig tot_config;
    tot_config.max_depth = 3;
    tot_config.branches_per_node = 2;
    tot_config.top_k_paths = 2;
    result.tot_result = tree_of_thought(question, tot_config);

    // Phase G2: Multi-Agent Debate
    std::cout << "[COMBINED] Phase 2/3: Multi-Agent Debate...\n";
    result.debate_result = multi_agent_debate(question, 3, 2);

    // Phase G3: Self-Reflection
    std::cout << "[COMBINED] Phase 3/3: Self-Reflection...\n";
    result.reflection_result = self_reflect(question, 85.0f, 3);

    // Combine results
    std::cout << "[COMBINED] Combining results...\n";
    result.final_answer = combine_results(result.tot_result,
                                          result.debate_result,
                                          result.reflection_result);

    // Calculate weighted confidence
    result.final_confidence =
        result.tot_result.confidence * result.tot_weight +
        result.debate_result.consensus_confidence * result.debate_weight +
        result.reflection_result.improved_score * result.reflection_weight;

    std::cout << "\n[COMBINED] ========== FINAL RESULT ==========\n";
    std::cout << "[COMBINED] Combined confidence: " << std::fixed << std::setprecision(1)
              << result.final_confidence << "%\n";
    std::cout << "[COMBINED] Answer: " << result.final_answer.substr(0, 100) << "...\n\n";

    return result;
}

std::string combine_results(const ToTResult& tot,
                            const DebateResult& debate,
                            const ReflectionResult& reflection) {
    // Weighted combination: use highest-scoring result as base,
    // incorporate insights from others

    std::vector<std::pair<float, std::string>> scored_results = {
        {tot.confidence, tot.final_answer},
        {debate.consensus_confidence, debate.final_consensus},
        {reflection.improved_score, reflection.improved_response}
    };

    std::sort(scored_results.begin(), scored_results.end(),
             [](const auto& a, const auto& b) { return a.first > b.first; });

    // Best result is the base
    return scored_results[0].second;
}

// =============================================================================
// Utilities
// =============================================================================

void print_thought_tree(const std::shared_ptr<ThoughtNode>& root, int indent) {
    for (int i = 0; i < indent; i++) std::cout << "  ";
    std::cout << "- [" << std::fixed << std::setprecision(1) << root->score << "%] "
              << root->thought.substr(0, 60) << "...\n";

    for (const auto& child : root->children) {
        print_thought_tree(child, indent + 1);
    }
}

void print_debate(const DebateResult& debate) {
    std::cout << "\n=== DEBATE TRANSCRIPT ===\n\n";

    for (const auto& round : debate.rounds) {
        std::cout << "Round " << round.round_number << ":\n";
        for (size_t i = 0; i < round.agent_responses.size(); i++) {
            std::cout << "  Agent " << (char)('A' + i) << " (score: "
                      << round.response_scores[i] << "): "
                      << round.agent_responses[i] << "\n\n";
        }
    }

    std::cout << "Final Consensus (" << debate.consensus_confidence << "%):\n";
    std::cout << debate.final_consensus << "\n\n";
}

void print_reflection(const ReflectionResult& reflection) {
    std::cout << "\n=== REFLECTION PROCESS ===\n\n";
    std::cout << "Iterations: " << reflection.iterations << "\n";
    std::cout << "Score improvement: " << reflection.reflection.original_score
              << "% -> " << reflection.improved_score << "%\n\n";

    std::cout << "Weaknesses identified:\n";
    for (const auto& w : reflection.reflection.weaknesses) {
        std::cout << "  - " << w << "\n";
    }

    std::cout << "\nImprovements made:\n";
    for (const auto& i : reflection.reflection.improvements) {
        std::cout << "  - " << i << "\n";
    }

    std::cout << "\nFinal answer:\n" << reflection.improved_response << "\n\n";
}

std::string tot_to_json(const ToTResult& result) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"final_answer\": \"" << result.final_answer << "\",\n";
    ss << "  \"confidence\": " << result.confidence << ",\n";
    ss << "  \"nodes_explored\": " << result.nodes_explored << ",\n";
    ss << "  \"reasoning_path\": [";
    for (size_t i = 0; i < result.reasoning_path.size(); i++) {
        ss << "\"" << result.reasoning_path[i] << "\"";
        if (i < result.reasoning_path.size() - 1) ss << ", ";
    }
    ss << "]\n";
    ss << "}";
    return ss.str();
}

std::string debate_to_json(const DebateResult& result) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"final_consensus\": \"" << result.final_consensus << "\",\n";
    ss << "  \"consensus_confidence\": " << result.consensus_confidence << ",\n";
    ss << "  \"num_rounds\": " << result.rounds.size() << "\n";
    ss << "}";
    return ss.str();
}

std::string reflection_to_json(const ReflectionResult& result) {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"improved_response\": \"" << result.improved_response << "\",\n";
    ss << "  \"improved_score\": " << result.improved_score << ",\n";
    ss << "  \"iterations\": " << result.iterations << ",\n";
    ss << "  \"converged\": " << (result.converged ? "true" : "false") << "\n";
    ss << "}";
    return ss.str();
}

} // namespace advanced_reasoning
