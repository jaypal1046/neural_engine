// Gradient Checkpointing Implementation
// Algorithm adapted from: HuggingFace Transformers (Apache 2.0 License)
// Original: https://github.com/huggingface/transformers
// Modifications: CPU implementation, zero dependencies

#include "gradient_checkpoint.h"
#include <cmath>
#include <algorithm>
#include <iostream>
#include <iomanip>

namespace GradientCheckpoint {

// ============================================================================
// Checkpointed Layer
// ============================================================================

CheckpointedLayer::CheckpointedLayer(
    int layer_idx,
    int activation_size,
    ForwardFunction forward_fn,
    BackwardFunction backward_fn,
    bool is_checkpoint
)
    : layer_idx_(layer_idx),
      activation_size_(activation_size),
      forward_fn_(forward_fn),
      backward_fn_(backward_fn),
      is_checkpoint_(is_checkpoint)
{
    if (is_checkpoint_) {
        saved_activations_.resize(activation_size);
    }
}

void CheckpointedLayer::forward(const float* input, float* output) {
    // Run forward function
    forward_fn_(input, output, layer_idx_);

    // Save activations if this is a checkpoint
    if (is_checkpoint_) {
        std::copy(output, output + activation_size_, saved_activations_.begin());
    }
}

void CheckpointedLayer::backward(
    const float* grad_output,
    float* grad_input,
    const float* input
) {
    // If activations were saved, use them
    // Otherwise, recompute from input
    const float* activations = nullptr;
    std::vector<float> recomputed;

    if (!saved_activations_.empty()) {
        activations = saved_activations_.data();
    } else {
        // Recompute forward pass
        recomputed.resize(activation_size_);
        forward_fn_(input, recomputed.data(), layer_idx_);
        activations = recomputed.data();
    }

    // Run backward function with activations
    backward_fn_(grad_output, grad_input, activations, layer_idx_);
}

// ============================================================================
// Checkpoint Manager
// ============================================================================

CheckpointManager::CheckpointManager(
    int num_layers,
    int activation_size_per_layer,
    CheckpointStrategy strategy
)
    : num_layers_(num_layers),
      activation_size_(activation_size_per_layer),
      strategy_(strategy),
      num_recomputations_(0)
{
    compute_checkpoint_layers();
    temp_activations_.resize(activation_size_);
}

void CheckpointManager::set_checkpoint_layers(const std::vector<int>& checkpoint_layers) {
    checkpoint_layers_ = checkpoint_layers;
    strategy_ = CheckpointStrategy::MANUAL;
}

void CheckpointManager::add_layer(
    int layer_idx,
    ForwardFunction forward_fn,
    BackwardFunction backward_fn
) {
    bool is_ckpt = should_checkpoint(layer_idx);
    layers_.emplace_back(std::make_unique<CheckpointedLayer>(
        layer_idx,
        activation_size_,
        forward_fn,
        backward_fn,
        is_ckpt
    ));
}

void CheckpointManager::forward(
    const float* input,
    float* output,
    int batch_size,
    int seq_len
) {
    const float* current_input = input;
    float* current_output = output;

    for (auto& layer : layers_) {
        layer->forward(current_input, current_output);
        current_input = current_output;
    }
}

void CheckpointManager::backward(
    const float* grad_output,
    float* grad_input,
    const float* original_input
) {
    num_recomputations_ = 0;

    // Backward pass through layers in reverse
    const float* current_grad_output = grad_output;
    float* current_grad_input = grad_input;

    for (int i = layers_.size() - 1; i >= 0; i--) {
        // If this layer doesn't have saved activations, it will recompute
        if (!layers_[i]->is_checkpoint()) {
            num_recomputations_++;
        }

        layers_[i]->backward(
            current_grad_output,
            current_grad_input,
            i > 0 ? layers_[i-1]->get_saved_activations() : original_input
        );

        current_grad_output = current_grad_input;
    }
}

CheckpointManager::MemoryStats CheckpointManager::get_memory_stats() const {
    MemoryStats stats;

    stats.total_memory_no_checkpoint = num_layers_ * activation_size_ * sizeof(float);
    stats.num_checkpoints = checkpoint_layers_.size();
    stats.total_memory_with_checkpoint = stats.num_checkpoints * activation_size_ * sizeof(float);

    if (stats.total_memory_with_checkpoint > 0) {
        stats.memory_savings_ratio = (float)stats.total_memory_no_checkpoint / stats.total_memory_with_checkpoint;
    } else {
        stats.memory_savings_ratio = 1.0f;
    }

    stats.num_recomputations = num_recomputations_;

    return stats;
}

bool CheckpointManager::should_checkpoint(int layer_idx) const {
    if (strategy_ == CheckpointStrategy::NONE) {
        return true;  // Save all activations
    }

    return std::find(checkpoint_layers_.begin(), checkpoint_layers_.end(), layer_idx) != checkpoint_layers_.end();
}

void CheckpointManager::compute_checkpoint_layers() {
    checkpoint_layers_.clear();

    switch (strategy_) {
        case CheckpointStrategy::NONE:
            // Checkpoint every layer (no memory savings)
            for (int i = 0; i < num_layers_; i++) {
                checkpoint_layers_.push_back(i);
            }
            break;

        case CheckpointStrategy::UNIFORM: {
            // Checkpoint every 4th layer
            int interval = 4;
            for (int i = 0; i < num_layers_; i += interval) {
                checkpoint_layers_.push_back(i);
            }
            // Always checkpoint last layer
            if (checkpoint_layers_.back() != num_layers_ - 1) {
                checkpoint_layers_.push_back(num_layers_ - 1);
            }
            break;
        }

        case CheckpointStrategy::SQUARE_ROOT: {
            // Optimal: sqrt(L) checkpoints
            int num_checkpoints = std::max(1, (int)std::sqrt(num_layers_));
            int interval = num_layers_ / num_checkpoints;
            for (int i = 0; i < num_layers_; i += interval) {
                checkpoint_layers_.push_back(i);
            }
            // Always checkpoint last layer
            if (checkpoint_layers_.back() != num_layers_ - 1) {
                checkpoint_layers_.push_back(num_layers_ - 1);
            }
            break;
        }

        case CheckpointStrategy::EVERY_OTHER:
            // Checkpoint every other layer (50% memory)
            for (int i = 0; i < num_layers_; i += 2) {
                checkpoint_layers_.push_back(i);
            }
            // Always checkpoint last layer
            if (checkpoint_layers_.back() != num_layers_ - 1) {
                checkpoint_layers_.push_back(num_layers_ - 1);
            }
            break;

        case CheckpointStrategy::MANUAL:
            // User-specified (already set)
            break;
    }

    // Sort checkpoint layers
    std::sort(checkpoint_layers_.begin(), checkpoint_layers_.end());
}

// ============================================================================
// Utilities
// ============================================================================

int calculate_optimal_checkpoints(int num_layers) {
    // Square root rule: optimal trade-off
    return std::max(1, (int)std::sqrt(num_layers));
}

float estimate_memory_savings(
    int num_layers,
    int activation_size,
    int num_checkpoints
) {
    if (num_checkpoints == 0) return 1.0f;

    float no_checkpoint_mem = num_layers * activation_size;
    float checkpoint_mem = num_checkpoints * activation_size;

    return no_checkpoint_mem / checkpoint_mem;
}

void print_checkpoint_strategy(
    int num_layers,
    const std::vector<int>& checkpoint_layers
) {
    std::cout << "\n=== Checkpointing Strategy ===" << std::endl;
    std::cout << "Total layers: " << num_layers << std::endl;
    std::cout << "Checkpoints: " << checkpoint_layers.size() << std::endl;
    std::cout << "Checkpoint layers: ";
    for (size_t i = 0; i < checkpoint_layers.size(); i++) {
        std::cout << checkpoint_layers[i];
        if (i < checkpoint_layers.size() - 1) std::cout << ", ";
    }
    std::cout << std::endl;

    int memory_saved = ((num_layers - checkpoint_layers.size()) * 100) / num_layers;
    std::cout << "Memory saved: ~" << memory_saved << "%" << std::endl;
    std::cout << "==============================" << std::endl;
}

} // namespace GradientCheckpoint
