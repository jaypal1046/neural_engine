// =============================================================================
// Advanced Reasoning Module - Tree-of-Thought, Debate, Self-Reflection
//
// Implements sophisticated reasoning strategies inspired by:
// - Tree-of-Thought (Yao et al., 2023) - Explore multiple reasoning paths
// - Multi-Agent Debate (Du et al., 2023) - Multiple AI instances debate
// - Self-Reflection (Shinn et al., 2023) - AI critiques and improves itself
//
// These techniques dramatically improve reasoning on complex problems:
// - Tree-of-Thought: 74% → 85% on complex reasoning tasks
// - Multi-Agent Debate: 70% → 82% with 3 agents, 3 rounds
// - Self-Reflection: 65% → 78% on open-ended questions
//
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

namespace advanced_reasoning {

// =============================================================================
// Phase G1: Tree-of-Thought (ToT)
// =============================================================================

// A node in the thought tree
struct ThoughtNode {
    std::string thought;           // The reasoning step at this node
    float score;                   // Quality score (0-100)
    int depth;                     // Depth in tree
    std::vector<std::shared_ptr<ThoughtNode>> children;  // Next reasoning steps
    std::weak_ptr<ThoughtNode> parent;  // Previous step

    ThoughtNode(const std::string& t, float s, int d)
        : thought(t), score(s), depth(d) {}
};

// Tree-of-Thought configuration
struct ToTConfig {
    int max_depth = 5;             // Maximum reasoning depth
    int branches_per_node = 3;     // How many alternatives to explore
    int top_k_paths = 3;           // Keep top K paths at each level
    float pruning_threshold = 40.0f;  // Prune nodes below this score
    bool use_beam_search = true;   // Use beam search vs BFS

    // Scoring function (external, calls RewardModel)
    std::function<float(const std::string&)> score_thought;
};

// Tree-of-Thought search result
struct ToTResult {
    std::string final_answer;      // Best answer found
    std::vector<std::string> reasoning_path;  // Steps taken to reach answer
    float confidence;              // Confidence in answer (0-100)
    int nodes_explored;            // Total nodes explored
    int paths_evaluated;           // Total complete paths evaluated
};

// Run Tree-of-Thought search on a problem
ToTResult tree_of_thought(const std::string& problem, const ToTConfig& config);

// Generate possible next reasoning steps from current thought
std::vector<std::string> generate_next_thoughts(const std::string& problem,
                                                  const std::string& current_thought,
                                                  int depth);

// Score a reasoning step (uses RewardModel or heuristics)
float score_thought(const std::string& problem, const std::string& thought);

// Build path from root to node
std::vector<std::string> build_path(const std::shared_ptr<ThoughtNode>& node);

// Find best leaf node in tree
std::shared_ptr<ThoughtNode> find_best_leaf(const std::shared_ptr<ThoughtNode>& root);

// =============================================================================
// Phase G2: Multi-Agent Debate
// =============================================================================

// Agent configuration
struct AgentConfig {
    std::string name;              // Agent identifier (e.g., "Agent_A")
    float temperature;             // Sampling temperature (0.7-1.5)
    int max_response_length;       // Max tokens per response
    std::string system_prompt;     // Agent's perspective/role
};

// Debate round
struct DebateRound {
    int round_number;
    std::vector<std::string> agent_responses;  // One per agent
    std::vector<float> response_scores;        // Scores for each response
    std::string best_response;                 // Highest-scored response
};

// Debate result
struct DebateResult {
    std::string question;
    std::vector<DebateRound> rounds;
    std::string final_consensus;   // Consensus answer
    float consensus_confidence;    // Confidence (0-100)
    std::vector<std::string> key_insights;  // Important points from debate
};

// Run multi-agent debate
// num_agents: How many AI instances participate (3-5 recommended)
// num_rounds: How many debate rounds (3 recommended)
DebateResult multi_agent_debate(const std::string& question,
                                 int num_agents = 3,
                                 int num_rounds = 3);

// Generate agent's response given question and previous round
std::string generate_agent_response(const std::string& question,
                                     const AgentConfig& agent,
                                     const std::vector<std::string>& previous_responses);

// Score agent response
float score_agent_response(const std::string& question, const std::string& response);

// Extract consensus from final round
std::string extract_consensus(const std::string& question,
                              const std::vector<std::string>& final_responses);

// Extract key insights from debate
std::vector<std::string> extract_key_insights(const DebateResult& debate);

// =============================================================================
// Phase G3: Self-Reflection
// =============================================================================

// Reflection on a response
struct Reflection {
    std::string original_response;
    float original_score;
    std::vector<std::string> weaknesses;   // What's wrong with response
    std::vector<std::string> strengths;    // What's good about response
    std::vector<std::string> improvements; // How to improve
    std::string critique;                  // Full critique text
};

// Self-reflection result
struct ReflectionResult {
    std::string question;
    Reflection reflection;
    std::string improved_response;
    float improved_score;
    int iterations;                // How many reflection iterations
    bool converged;                // Did it converge to high score?
};

// Run self-reflection loop
// Generates response → critiques it → improves it → repeat until score > threshold
ReflectionResult self_reflect(const std::string& question,
                              float target_score = 85.0f,
                              int max_iterations = 5);

// Generate initial response
std::string generate_initial_response(const std::string& question);

// Critique a response (identify weaknesses and strengths)
Reflection critique_response(const std::string& question, const std::string& response);

// Improve response based on critique
std::string improve_response(const std::string& question,
                             const std::string& original_response,
                             const Reflection& reflection);

// Check if reflection has converged (score > threshold or no more improvements)
bool has_converged(const ReflectionResult& result, float target_score);

// =============================================================================
// Combined Advanced Reasoning
// =============================================================================

// Use all 3 techniques together for maximum reasoning power
struct AdvancedReasoningResult {
    std::string question;

    // Tree-of-Thought
    ToTResult tot_result;
    float tot_weight = 0.4f;

    // Multi-Agent Debate
    DebateResult debate_result;
    float debate_weight = 0.3f;

    // Self-Reflection
    ReflectionResult reflection_result;
    float reflection_weight = 0.3f;

    // Final answer (weighted combination)
    std::string final_answer;
    float final_confidence;
    std::string reasoning_summary;
};

// Run all 3 advanced reasoning techniques and combine results
AdvancedReasoningResult combined_reasoning(const std::string& question);

// Combine results from multiple techniques
std::string combine_results(const ToTResult& tot,
                            const DebateResult& debate,
                            const ReflectionResult& reflection);

// =============================================================================
// Utilities
// =============================================================================

// Print tree-of-thought tree (for debugging)
void print_thought_tree(const std::shared_ptr<ThoughtNode>& root, int indent = 0);

// Print debate transcript
void print_debate(const DebateResult& debate);

// Print reflection process
void print_reflection(const ReflectionResult& reflection);

// Serialize ToT result to JSON
std::string tot_to_json(const ToTResult& result);

// Serialize debate result to JSON
std::string debate_to_json(const DebateResult& result);

// Serialize reflection result to JSON
std::string reflection_to_json(const ReflectionResult& result);

} // namespace advanced_reasoning
