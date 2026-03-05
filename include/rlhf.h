// =============================================================================
// RLHF (Reinforcement Learning from Human Feedback) - C++ Implementation
//
// Based on Anthropic's Claude training pipeline:
//   1. Pretraining (DONE - train_transformer)
//   2. SFT - Supervised Fine-Tuning on {prompt, completion} pairs
//   3. Reward Model - Learn human preferences from comparisons
//   4. PPO - Proximal Policy Optimization for alignment
//
// This module implements phases 2-4 entirely in C++ for maximum performance.
// =============================================================================

#pragma once

#include <string>
#include <vector>
#include <memory>
#include "mini_transformer.h"

namespace rlhf {

// =============================================================================
// Data Structures
// =============================================================================

// SFT Training Pair: {prompt, ideal_completion}
struct SFTPair {
    std::string prompt;
    std::string completion;

    // Parse from JSON
    static SFTPair from_json(const std::string& json);
    std::string to_json() const;
};

// Reward Model Comparison: {prompt, response_a, response_b, preferred}
struct RewardComparison {
    std::string prompt;
    std::string response_a;
    std::string response_b;
    char preferred;  // 'a' or 'b'

    // Parse from JSON
    static RewardComparison from_json(const std::string& json);
    std::string to_json() const;
};

// PPO Training Sample: {prompt, response, reward, old_log_prob}
struct PPOSample {
    std::string prompt;
    std::string response;
    float reward;
    float old_log_prob;  // Log probability from old policy
};

// Reward Model - Small MLP on top of embeddings
class RewardModel {
public:
    RewardModel(int embedding_dim = 64);

    // Train on comparison dataset
    // Returns final loss
    float train(const std::vector<RewardComparison>& comparisons,
                int epochs = 10,
                float lr = 0.001f);

    // Score a response (higher = better)
    float score(const std::string& prompt, const std::string& response);

    // Save/load weights
    void save(const std::string& path);
    void load(const std::string& path);

private:
    int embedding_dim_;

    // MLP layers: embedding -> hidden -> score
    std::vector<std::vector<float>> fc1_weight;  // [embedding_dim, hidden_dim]
    std::vector<float> fc1_bias;                 // [hidden_dim]
    std::vector<std::vector<float>> fc2_weight;  // [hidden_dim, 1]
    std::vector<float> fc2_bias;                 // [1]

    int hidden_dim_ = 128;

    // Forward pass
    float forward(const std::vector<float>& embedding);

    // Backward pass (compute gradients)
    void backward(const std::vector<float>& embedding,
                  float grad_output,
                  std::vector<std::vector<float>>& fc1_grad_w,
                  std::vector<float>& fc1_grad_b,
                  std::vector<std::vector<float>>& fc2_grad_w,
                  std::vector<float>& fc2_grad_b);

    // Helper: get embedding for text
    std::vector<float> get_embedding(const std::string& text);

    // Helper: ReLU activation
    float relu(float x) { return x > 0 ? x : 0; }
};

// =============================================================================
// Phase F1: Supervised Fine-Tuning (SFT)
// =============================================================================

// Train transformer on {prompt, completion} pairs
// This is the main SFT function called from command line
// Returns final loss
float run_sft(const std::string& training_file,
              int epochs = 5,
              float lr = 0.0005f,
              int batch_size = 4);

// Load SFT training pairs from JSON file
// Format: [{"prompt": "...", "completion": "..."}, ...]
std::vector<SFTPair> load_sft_pairs(const std::string& file_path);

// SFT training loop for one epoch
// Returns average loss
float sft_train_epoch(MiniTransformer& model,
                      const std::vector<SFTPair>& pairs,
                      float lr,
                      int batch_size);

// Compute SFT loss (cross-entropy on completion tokens only)
// Prompt tokens are used for context but not trained
float sft_loss(MiniTransformer& model,
               const std::string& prompt,
               const std::string& completion);

// =============================================================================
// Phase F2: Reward Model Training
// =============================================================================

// Train reward model on comparison dataset
// Returns final loss
float run_reward_model_training(const std::string& comparisons_file,
                                 const std::string& output_model_path,
                                 int epochs = 10,
                                 float lr = 0.001f);

// Load comparison dataset from JSON
// Format: [{"prompt": "...", "response_a": "...", "response_b": "...", "preferred": "a"}, ...]
std::vector<RewardComparison> load_comparisons(const std::string& file_path);

// Bradley-Terry loss for preference learning
// Loss = -log(sigmoid(score_preferred - score_other))
float bradley_terry_loss(float score_preferred, float score_other);

// =============================================================================
// Phase F3: PPO (Proximal Policy Optimization)
// =============================================================================

// Run PPO training with reward model
// Returns final average reward
float run_ppo(const std::string& prompts_file,
              const std::string& reward_model_path,
              int num_iterations = 100,
              float lr = 0.0001f,
              float clip_epsilon = 0.2f,
              float kl_penalty = 0.01f);

// Load prompts for PPO training
std::vector<std::string> load_prompts(const std::string& file_path);

// PPO training iteration
// 1. Generate responses with current policy
// 2. Score with reward model
// 3. Compute PPO loss
// 4. Update policy
float ppo_iteration(MiniTransformer& policy,
                    RewardModel& reward_model,
                    const std::vector<std::string>& prompts,
                    float lr,
                    float clip_epsilon,
                    float kl_penalty);

// Generate response with current policy
std::string generate_response(MiniTransformer& model, const std::string& prompt);

// Compute log probability of a response given prompt
float compute_log_prob(MiniTransformer& model,
                       const std::string& prompt,
                       const std::string& response);

// PPO clipped loss
// loss = -min(ratio * advantage, clip(ratio, 1-eps, 1+eps) * advantage)
// where ratio = new_prob / old_prob
float ppo_clipped_loss(float new_log_prob,
                       float old_log_prob,
                       float advantage,
                       float clip_epsilon);

// KL divergence penalty (keep policy close to original)
float kl_divergence(float new_log_prob, float old_log_prob);

// =============================================================================
// Utilities
// =============================================================================

// Save SFT pairs to JSON file
void save_sft_pairs(const std::string& file_path, const std::vector<SFTPair>& pairs);

// Save comparisons to JSON file
void save_comparisons(const std::string& file_path, const std::vector<RewardComparison>& comparisons);

// Save prompts to JSON file
void save_prompts(const std::string& file_path, const std::vector<std::string>& prompts);

// Create sample training data for testing
void create_sample_sft_data(const std::string& output_file);
void create_sample_comparisons(const std::string& output_file);
void create_sample_prompts(const std::string& output_file);

// Print RLHF statistics
void print_rlhf_stats(const std::string& stage,
                      int iteration,
                      float loss_or_reward,
                      float perplexity = 0.0f);

} // namespace rlhf
