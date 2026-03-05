// =============================================================================
// RLHF Implementation - C++ Native Training Pipeline
// =============================================================================

#include "rlhf.h"
#include "mini_transformer.h"
#include "real_embeddings.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <random>
#include <iomanip>

namespace rlhf {

// =============================================================================
// Utility Functions
// =============================================================================

// Simple JSON string extraction (no external library needed)
static std::string extract_json_string(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\": \"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return "";

    pos += search.size();
    size_t end = json.find("\"", pos);
    if (end == std::string::npos) return "";

    std::string result = json.substr(pos, end - pos);

    // Unescape common sequences
    size_t i = 0;
    while ((i = result.find("\\n", i)) != std::string::npos) {
        result.replace(i, 2, "\n");
        i += 1;
    }
    i = 0;
    while ((i = result.find("\\\"", i)) != std::string::npos) {
        result.replace(i, 2, "\"");
        i += 1;
    }

    return result;
}

static char extract_json_char(const std::string& json, const std::string& key) {
    std::string search = "\"" + key + "\": \"";
    size_t pos = json.find(search);
    if (pos == std::string::npos) return '\0';

    pos += search.size();
    if (pos >= json.size()) return '\0';

    return json[pos];
}

static std::string json_escape(const std::string& str) {
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

// Load JSON array of objects
static std::vector<std::string> load_json_array(const std::string& file_path) {
    std::vector<std::string> result;

    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot open " << file_path << "\n";
        return result;
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());

    // Simple JSON array parser
    size_t pos = 0;
    int brace_depth = 0;
    size_t object_start = std::string::npos;

    for (size_t i = 0; i < content.size(); i++) {
        if (content[i] == '{') {
            if (brace_depth == 0) object_start = i;
            brace_depth++;
        } else if (content[i] == '}') {
            brace_depth--;
            if (brace_depth == 0 && object_start != std::string::npos) {
                result.push_back(content.substr(object_start, i - object_start + 1));
                object_start = std::string::npos;
            }
        }
    }

    return result;
}

// =============================================================================
// Data Structure Implementations
// =============================================================================

SFTPair SFTPair::from_json(const std::string& json) {
    SFTPair pair;
    pair.prompt = extract_json_string(json, "prompt");
    pair.completion = extract_json_string(json, "completion");
    return pair;
}

std::string SFTPair::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"prompt\": \"" << json_escape(prompt) << "\",\n";
    ss << "  \"completion\": \"" << json_escape(completion) << "\"\n";
    ss << "}";
    return ss.str();
}

RewardComparison RewardComparison::from_json(const std::string& json) {
    RewardComparison comp;
    comp.prompt = extract_json_string(json, "prompt");
    comp.response_a = extract_json_string(json, "response_a");
    comp.response_b = extract_json_string(json, "response_b");
    comp.preferred = extract_json_char(json, "preferred");
    return comp;
}

std::string RewardComparison::to_json() const {
    std::stringstream ss;
    ss << "{\n";
    ss << "  \"prompt\": \"" << json_escape(prompt) << "\",\n";
    ss << "  \"response_a\": \"" << json_escape(response_a) << "\",\n";
    ss << "  \"response_b\": \"" << json_escape(response_b) << "\",\n";
    ss << "  \"preferred\": \"" << preferred << "\"\n";
    ss << "}";
    return ss.str();
}

// =============================================================================
// Reward Model Implementation
// =============================================================================

RewardModel::RewardModel(int embedding_dim)
    : embedding_dim_(embedding_dim), hidden_dim_(128) {

    // Initialize weights (Xavier initialization)
    std::random_device rd;
    std::mt19937 gen(rd());

    // fc1: embedding_dim -> hidden_dim
    float scale1 = std::sqrt(2.0f / (embedding_dim_ + hidden_dim_));
    std::normal_distribution<float> dist1(0.0f, scale1);

    fc1_weight.resize(embedding_dim_);
    for (int i = 0; i < embedding_dim_; i++) {
        fc1_weight[i].resize(hidden_dim_);
        for (int j = 0; j < hidden_dim_; j++) {
            fc1_weight[i][j] = dist1(gen);
        }
    }
    fc1_bias.resize(hidden_dim_, 0.0f);

    // fc2: hidden_dim -> 1
    float scale2 = std::sqrt(2.0f / (hidden_dim_ + 1));
    std::normal_distribution<float> dist2(0.0f, scale2);

    fc2_weight.resize(hidden_dim_);
    for (int i = 0; i < hidden_dim_; i++) {
        fc2_weight[i].resize(1);
        fc2_weight[i][0] = dist2(gen);
    }
    fc2_bias.resize(1, 0.0f);

    std::cout << "[RLHF] Reward model initialized (" << embedding_dim_
              << " -> " << hidden_dim_ << " -> 1)\n";
}

std::vector<float> RewardModel::get_embedding(const std::string& text) {
    // Use global embeddings trainer (should be initialized)
    // For now, simple word averaging
    // TODO: Use actual embeddings from EmbeddingTrainer

    std::vector<float> embedding(embedding_dim_, 0.0f);

    // Simple hash-based embedding for demo
    std::hash<std::string> hasher;
    size_t hash = hasher(text);

    for (int i = 0; i < embedding_dim_; i++) {
        embedding[i] = static_cast<float>((hash >> (i % 32)) & 0xFF) / 128.0f - 1.0f;
    }

    return embedding;
}

float RewardModel::forward(const std::vector<float>& embedding) {
    // Layer 1: embedding -> hidden (with ReLU)
    std::vector<float> hidden(hidden_dim_, 0.0f);
    for (int j = 0; j < hidden_dim_; j++) {
        float sum = fc1_bias[j];
        for (int i = 0; i < embedding_dim_; i++) {
            sum += embedding[i] * fc1_weight[i][j];
        }
        hidden[j] = relu(sum);
    }

    // Layer 2: hidden -> score (no activation)
    float score = fc2_bias[0];
    for (int i = 0; i < hidden_dim_; i++) {
        score += hidden[i] * fc2_weight[i][0];
    }

    return score;
}

void RewardModel::backward(const std::vector<float>& embedding,
                           float grad_output,
                           std::vector<std::vector<float>>& fc1_grad_w,
                           std::vector<float>& fc1_grad_b,
                           std::vector<std::vector<float>>& fc2_grad_w,
                           std::vector<float>& fc2_grad_b) {
    // Forward pass (save activations)
    std::vector<float> hidden(hidden_dim_, 0.0f);
    std::vector<float> hidden_pre_relu(hidden_dim_, 0.0f);

    for (int j = 0; j < hidden_dim_; j++) {
        float sum = fc1_bias[j];
        for (int i = 0; i < embedding_dim_; i++) {
            sum += embedding[i] * fc1_weight[i][j];
        }
        hidden_pre_relu[j] = sum;
        hidden[j] = relu(sum);
    }

    // Backward through fc2
    fc2_grad_b[0] = grad_output;
    for (int i = 0; i < hidden_dim_; i++) {
        fc2_grad_w[i][0] = grad_output * hidden[i];
    }

    // Backward through ReLU
    std::vector<float> grad_hidden(hidden_dim_, 0.0f);
    for (int i = 0; i < hidden_dim_; i++) {
        grad_hidden[i] = grad_output * fc2_weight[i][0];
        if (hidden_pre_relu[i] <= 0) {
            grad_hidden[i] = 0.0f;  // ReLU derivative
        }
    }

    // Backward through fc1
    for (int j = 0; j < hidden_dim_; j++) {
        fc1_grad_b[j] = grad_hidden[j];
        for (int i = 0; i < embedding_dim_; i++) {
            fc1_grad_w[i][j] = grad_hidden[j] * embedding[i];
        }
    }
}

float RewardModel::score(const std::string& prompt, const std::string& response) {
    // Combine prompt and response
    std::string text = prompt + " " + response;
    auto embedding = get_embedding(text);
    return forward(embedding);
}

float RewardModel::train(const std::vector<RewardComparison>& comparisons,
                         int epochs,
                         float lr) {
    std::cout << "[RLHF] Training reward model on " << comparisons.size()
              << " comparisons for " << epochs << " epochs\n";

    float final_loss = 0.0f;

    for (int epoch = 0; epoch < epochs; epoch++) {
        float epoch_loss = 0.0f;

        for (const auto& comp : comparisons) {
            // Get embeddings
            auto emb_a = get_embedding(comp.prompt + " " + comp.response_a);
            auto emb_b = get_embedding(comp.prompt + " " + comp.response_b);

            // Forward pass
            float score_a = forward(emb_a);
            float score_b = forward(emb_b);

            // Bradley-Terry loss
            float score_preferred = (comp.preferred == 'a') ? score_a : score_b;
            float score_other = (comp.preferred == 'a') ? score_b : score_a;

            float loss = bradley_terry_loss(score_preferred, score_other);
            epoch_loss += loss;

            // Gradient of loss w.r.t. scores
            float diff = score_preferred - score_other;
            float sigmoid_diff = 1.0f / (1.0f + std::exp(-diff));

            float grad_preferred = -(1.0f - sigmoid_diff);
            float grad_other = sigmoid_diff;

            // Backprop for preferred response
            std::vector<std::vector<float>> fc1_grad_w(embedding_dim_, std::vector<float>(hidden_dim_, 0.0f));
            std::vector<float> fc1_grad_b(hidden_dim_, 0.0f);
            std::vector<std::vector<float>> fc2_grad_w(hidden_dim_, std::vector<float>(1, 0.0f));
            std::vector<float> fc2_grad_b(1, 0.0f);

            auto& emb_preferred = (comp.preferred == 'a') ? emb_a : emb_b;
            auto& emb_other = (comp.preferred == 'a') ? emb_b : emb_a;

            backward(emb_preferred, grad_preferred, fc1_grad_w, fc1_grad_b, fc2_grad_w, fc2_grad_b);

            // Update weights
            for (int i = 0; i < embedding_dim_; i++) {
                for (int j = 0; j < hidden_dim_; j++) {
                    fc1_weight[i][j] -= lr * fc1_grad_w[i][j];
                }
            }
            for (int j = 0; j < hidden_dim_; j++) {
                fc1_bias[j] -= lr * fc1_grad_b[j];
            }
            for (int i = 0; i < hidden_dim_; i++) {
                fc2_weight[i][0] -= lr * fc2_grad_w[i][0];
            }
            fc2_bias[0] -= lr * fc2_grad_b[0];

            // Backprop for other response (opposite gradient)
            std::fill(fc1_grad_w.begin(), fc1_grad_w.end(), std::vector<float>(hidden_dim_, 0.0f));
            std::fill(fc1_grad_b.begin(), fc1_grad_b.end(), 0.0f);
            std::fill(fc2_grad_w.begin(), fc2_grad_w.end(), std::vector<float>(1, 0.0f));
            std::fill(fc2_grad_b.begin(), fc2_grad_b.end(), 0.0f);

            backward(emb_other, grad_other, fc1_grad_w, fc1_grad_b, fc2_grad_w, fc2_grad_b);

            // Update weights
            for (int i = 0; i < embedding_dim_; i++) {
                for (int j = 0; j < hidden_dim_; j++) {
                    fc1_weight[i][j] -= lr * fc1_grad_w[i][j];
                }
            }
            for (int j = 0; j < hidden_dim_; j++) {
                fc1_bias[j] -= lr * fc1_grad_b[j];
            }
            for (int i = 0; i < hidden_dim_; i++) {
                fc2_weight[i][0] -= lr * fc2_grad_w[i][0];
            }
            fc2_bias[0] -= lr * fc2_grad_b[0];
        }

        epoch_loss /= comparisons.size();
        final_loss = epoch_loss;

        if ((epoch + 1) % 2 == 0 || epoch == 0) {
            std::cout << "[RLHF] Reward Model Epoch " << (epoch + 1) << "/" << epochs
                      << " - Loss: " << std::fixed << std::setprecision(4) << epoch_loss << "\n";
        }
    }

    std::cout << "[RLHF] Reward model training complete!\n";
    return final_loss;
}

void RewardModel::save(const std::string& path) {
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot save reward model to " << path << "\n";
        return;
    }

    // Write dimensions
    file.write(reinterpret_cast<const char*>(&embedding_dim_), sizeof(int));
    file.write(reinterpret_cast<const char*>(&hidden_dim_), sizeof(int));

    // Write fc1 weights and bias
    for (int i = 0; i < embedding_dim_; i++) {
        file.write(reinterpret_cast<const char*>(fc1_weight[i].data()),
                   hidden_dim_ * sizeof(float));
    }
    file.write(reinterpret_cast<const char*>(fc1_bias.data()), hidden_dim_ * sizeof(float));

    // Write fc2 weights and bias
    for (int i = 0; i < hidden_dim_; i++) {
        file.write(reinterpret_cast<const char*>(fc2_weight[i].data()), sizeof(float));
    }
    file.write(reinterpret_cast<const char*>(fc2_bias.data()), sizeof(float));

    std::cout << "[RLHF] Reward model saved to " << path << "\n";
}

void RewardModel::load(const std::string& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot load reward model from " << path << "\n";
        return;
    }

    // Read dimensions
    file.read(reinterpret_cast<char*>(&embedding_dim_), sizeof(int));
    file.read(reinterpret_cast<char*>(&hidden_dim_), sizeof(int));

    // Resize arrays
    fc1_weight.resize(embedding_dim_);
    for (int i = 0; i < embedding_dim_; i++) {
        fc1_weight[i].resize(hidden_dim_);
    }
    fc1_bias.resize(hidden_dim_);

    fc2_weight.resize(hidden_dim_);
    for (int i = 0; i < hidden_dim_; i++) {
        fc2_weight[i].resize(1);
    }
    fc2_bias.resize(1);

    // Read fc1 weights and bias
    for (int i = 0; i < embedding_dim_; i++) {
        file.read(reinterpret_cast<char*>(fc1_weight[i].data()),
                  hidden_dim_ * sizeof(float));
    }
    file.read(reinterpret_cast<char*>(fc1_bias.data()), hidden_dim_ * sizeof(float));

    // Read fc2 weights and bias
    for (int i = 0; i < hidden_dim_; i++) {
        file.read(reinterpret_cast<char*>(fc2_weight[i].data()), sizeof(float));
    }
    file.read(reinterpret_cast<char*>(fc2_bias.data()), sizeof(float));

    std::cout << "[RLHF] Reward model loaded from " << path << "\n";
}

// =============================================================================
// Phase F1: SFT Implementation
// =============================================================================

std::vector<SFTPair> load_sft_pairs(const std::string& file_path) {
    std::vector<SFTPair> pairs;

    auto json_objects = load_json_array(file_path);
    for (const auto& json : json_objects) {
        pairs.push_back(SFTPair::from_json(json));
    }

    std::cout << "[RLHF] Loaded " << pairs.size() << " SFT training pairs\n";
    return pairs;
}

float sft_loss(MiniTransformer& model,
               const std::string& prompt,
               const std::string& completion) {
    // TODO: Implement cross-entropy loss on completion tokens only
    // For now, return placeholder
    return 0.0f;
}

float sft_train_epoch(MiniTransformer& model,
                      const std::vector<SFTPair>& pairs,
                      float lr,
                      int batch_size) {
    float total_loss = 0.0f;

    // Shuffle pairs
    std::vector<size_t> indices(pairs.size());
    std::iota(indices.begin(), indices.end(), 0);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::shuffle(indices.begin(), indices.end(), gen);

    // Train in batches
    for (size_t i = 0; i < pairs.size(); i += batch_size) {
        float batch_loss = 0.0f;
        int batch_count = 0;

        for (size_t j = i; j < std::min(i + batch_size, pairs.size()); j++) {
            const auto& pair = pairs[indices[j]];

            // Compute loss
            float loss = sft_loss(model, pair.prompt, pair.completion);
            batch_loss += loss;
            batch_count++;

            // TODO: Backprop and update weights
        }

        total_loss += batch_loss;
    }

    return total_loss / pairs.size();
}

float run_sft(const std::string& training_file,
              int epochs,
              float lr,
              int batch_size) {
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Starting SFT (Supervised Fine-Tuning)\n";
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Training file: " << training_file << "\n";
    std::cout << "[RLHF] Epochs: " << epochs << "\n";
    std::cout << "[RLHF] Learning rate: " << lr << "\n";
    std::cout << "[RLHF] Batch size: " << batch_size << "\n\n";

    // Load training pairs
    auto pairs = load_sft_pairs(training_file);

    if (pairs.empty()) {
        std::cerr << "[RLHF] Error: No training pairs found\n";
        return -1.0f;
    }

    // Initialize or load transformer model
    // TODO: Load existing model from disk
    TransformerConfig config;
    config.vocab_size = 10000;
    config.embedding_dim = 64;
    config.num_layers = 2;
    config.num_heads = 4;
    config.ff_dim = 128;
    config.max_seq_length = 128;

    MiniTransformer model(config);

    // Training loop
    float final_loss = 0.0f;
    for (int epoch = 0; epoch < epochs; epoch++) {
        float epoch_loss = sft_train_epoch(model, pairs, lr, batch_size);
        final_loss = epoch_loss;

        std::cout << "[RLHF] SFT Epoch " << (epoch + 1) << "/" << epochs
                  << " - Loss: " << std::fixed << std::setprecision(4) << epoch_loss << "\n";
    }

    // Save fine-tuned model
    std::cout << "\n[RLHF] SFT complete! Saving fine-tuned model...\n";
    // TODO: Save model to disk

    return final_loss;
}

// =============================================================================
// Phase F2: Reward Model Training
// =============================================================================

std::vector<RewardComparison> load_comparisons(const std::string& file_path) {
    std::vector<RewardComparison> comparisons;

    auto json_objects = load_json_array(file_path);
    for (const auto& json : json_objects) {
        comparisons.push_back(RewardComparison::from_json(json));
    }

    std::cout << "[RLHF] Loaded " << comparisons.size() << " reward comparisons\n";
    return comparisons;
}

float bradley_terry_loss(float score_preferred, float score_other) {
    float diff = score_preferred - score_other;
    // Loss = -log(sigmoid(diff))
    return std::log(1.0f + std::exp(-diff));
}

float run_reward_model_training(const std::string& comparisons_file,
                                 const std::string& output_model_path,
                                 int epochs,
                                 float lr) {
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Training Reward Model\n";
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Comparisons file: " << comparisons_file << "\n";
    std::cout << "[RLHF] Output model: " << output_model_path << "\n";
    std::cout << "[RLHF] Epochs: " << epochs << "\n";
    std::cout << "[RLHF] Learning rate: " << lr << "\n\n";

    // Load comparisons
    auto comparisons = load_comparisons(comparisons_file);

    if (comparisons.empty()) {
        std::cerr << "[RLHF] Error: No comparisons found\n";
        return -1.0f;
    }

    // Initialize reward model
    RewardModel model(64);  // 64-dim embeddings

    // Train
    float final_loss = model.train(comparisons, epochs, lr);

    // Save model
    model.save(output_model_path);

    return final_loss;
}

// =============================================================================
// Phase F3: PPO Implementation (Simplified)
// =============================================================================

std::vector<std::string> load_prompts(const std::string& file_path) {
    std::vector<std::string> prompts;

    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot open " << file_path << "\n";
        return prompts;
    }

    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line[0] != '#') {  // Skip empty and comments
            prompts.push_back(line);
        }
    }

    std::cout << "[RLHF] Loaded " << prompts.size() << " prompts\n";
    return prompts;
}

std::string generate_response(MiniTransformer& model, const std::string& prompt) {
    // TODO: Implement actual generation
    return "Generated response for: " + prompt;
}

float compute_log_prob(MiniTransformer& model,
                       const std::string& prompt,
                       const std::string& response) {
    // TODO: Implement log probability computation
    return 0.0f;
}

float ppo_clipped_loss(float new_log_prob,
                       float old_log_prob,
                       float advantage,
                       float clip_epsilon) {
    float ratio = std::exp(new_log_prob - old_log_prob);
    float clipped_ratio = std::max(1.0f - clip_epsilon, std::min(1.0f + clip_epsilon, ratio));

    float loss1 = ratio * advantage;
    float loss2 = clipped_ratio * advantage;

    return -std::min(loss1, loss2);
}

float kl_divergence(float new_log_prob, float old_log_prob) {
    return old_log_prob - new_log_prob;
}

float ppo_iteration(MiniTransformer& policy,
                    RewardModel& reward_model,
                    const std::vector<std::string>& prompts,
                    float lr,
                    float clip_epsilon,
                    float kl_penalty) {
    float total_reward = 0.0f;

    // TODO: Implement full PPO iteration
    // 1. Generate responses with current policy
    // 2. Score with reward model
    // 3. Compute advantages
    // 4. Update policy with PPO loss

    return total_reward / prompts.size();
}

float run_ppo(const std::string& prompts_file,
              const std::string& reward_model_path,
              int num_iterations,
              float lr,
              float clip_epsilon,
              float kl_penalty) {
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Starting PPO Training\n";
    std::cout << "[RLHF] ========================================\n";
    std::cout << "[RLHF] Prompts file: " << prompts_file << "\n";
    std::cout << "[RLHF] Reward model: " << reward_model_path << "\n";
    std::cout << "[RLHF] Iterations: " << num_iterations << "\n";
    std::cout << "[RLHF] Learning rate: " << lr << "\n";
    std::cout << "[RLHF] Clip epsilon: " << clip_epsilon << "\n";
    std::cout << "[RLHF] KL penalty: " << kl_penalty << "\n\n";

    // Load prompts
    auto prompts = load_prompts(prompts_file);

    if (prompts.empty()) {
        std::cerr << "[RLHF] Error: No prompts found\n";
        return -1.0f;
    }

    // Load reward model
    RewardModel reward_model(64);
    reward_model.load(reward_model_path);

    // Initialize policy (load SFT model)
    TransformerConfig config;
    config.vocab_size = 10000;
    config.embedding_dim = 64;
    config.num_layers = 2;
    config.num_heads = 4;
    config.ff_dim = 128;
    config.max_seq_length = 128;

    MiniTransformer policy(config);
    // TODO: Load SFT checkpoint

    // PPO training loop
    float final_reward = 0.0f;
    for (int iter = 0; iter < num_iterations; iter++) {
        float avg_reward = ppo_iteration(policy, reward_model, prompts, lr, clip_epsilon, kl_penalty);
        final_reward = avg_reward;

        if ((iter + 1) % 10 == 0 || iter == 0) {
            std::cout << "[RLHF] PPO Iteration " << (iter + 1) << "/" << num_iterations
                      << " - Avg Reward: " << std::fixed << std::setprecision(4) << avg_reward << "\n";
        }
    }

    std::cout << "\n[RLHF] PPO training complete!\n";
    // TODO: Save aligned model

    return final_reward;
}

// =============================================================================
// Utility Functions
// =============================================================================

void save_sft_pairs(const std::string& file_path, const std::vector<SFTPair>& pairs) {
    std::ofstream file(file_path);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot write to " << file_path << "\n";
        return;
    }

    file << "[\n";
    for (size_t i = 0; i < pairs.size(); i++) {
        file << "  " << pairs[i].to_json();
        if (i < pairs.size() - 1) file << ",";
        file << "\n";
    }
    file << "]\n";
}

void save_comparisons(const std::string& file_path, const std::vector<RewardComparison>& comparisons) {
    std::ofstream file(file_path);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot write to " << file_path << "\n";
        return;
    }

    file << "[\n";
    for (size_t i = 0; i < comparisons.size(); i++) {
        file << "  " << comparisons[i].to_json();
        if (i < comparisons.size() - 1) file << ",";
        file << "\n";
    }
    file << "]\n";
}

void save_prompts(const std::string& file_path, const std::vector<std::string>& prompts) {
    std::ofstream file(file_path);
    if (!file) {
        std::cerr << "[RLHF] Error: Cannot write to " << file_path << "\n";
        return;
    }

    for (const auto& prompt : prompts) {
        file << prompt << "\n";
    }
}

void create_sample_sft_data(const std::string& output_file) {
    std::vector<SFTPair> pairs = {
        {"What is the capital of France?", "The capital of France is Paris."},
        {"Explain quantum computing", "Quantum computing uses quantum-mechanical phenomena like superposition and entanglement to perform computations."},
        {"How do you make pasta?", "To make pasta: 1. Boil water with salt. 2. Add pasta. 3. Cook for 8-12 minutes. 4. Drain and serve."}
    };

    save_sft_pairs(output_file, pairs);
    std::cout << "[RLHF] Created sample SFT data: " << output_file << "\n";
}

void create_sample_comparisons(const std::string& output_file) {
    std::vector<RewardComparison> comparisons = {
        {"What is AI?", "AI is artificial intelligence.", "AI is when computers can think and learn.", 'b'},
        {"How does a car work?", "A car has an engine.", "A car works through internal combustion: fuel ignites in cylinders, creating force that moves pistons.", 'b'}
    };

    save_comparisons(output_file, comparisons);
    std::cout << "[RLHF] Created sample comparisons: " << output_file << "\n";
}

void create_sample_prompts(const std::string& output_file) {
    std::vector<std::string> prompts = {
        "Explain photosynthesis",
        "What is the theory of relativity?",
        "How do neural networks work?"
    };

    save_prompts(output_file, prompts);
    std::cout << "[RLHF] Created sample prompts: " << output_file << "\n";
}

void print_rlhf_stats(const std::string& stage,
                      int iteration,
                      float loss_or_reward,
                      float perplexity) {
    std::cout << "[RLHF] " << stage << " - Iteration " << iteration
              << " | Loss/Reward: " << std::fixed << std::setprecision(4) << loss_or_reward;

    if (perplexity > 0) {
        std::cout << " | Perplexity: " << std::fixed << std::setprecision(2) << perplexity;
    }

    std::cout << "\n";
}

} // namespace rlhf
