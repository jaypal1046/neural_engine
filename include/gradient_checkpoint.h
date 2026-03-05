#ifndef GRADIENT_CHECKPOINT_H
#define GRADIENT_CHECKPOINT_H

#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

// Gradient Checkpointing (Activation Recomputation)
// Algorithm adapted from: HuggingFace Transformers (Apache 2.0 License)
// Original: https://github.com/huggingface/transformers
// Paper: "Training Deep Nets with Sublinear Memory Cost" (Chen et al., 2016)
// Modifications: CPU implementation, zero dependencies

namespace GradientCheckpoint {

// ============================================================================
// Gradient Checkpointing Overview
// ============================================================================

// Traditional Backpropagation:
// - Forward: Compute and STORE all activations (high memory)
// - Backward: Use stored activations for gradient computation
// - Memory: O(N × L) where N = activations/layer, L = num layers
//
// Gradient Checkpointing:
// - Forward: Only STORE activations at checkpoint boundaries
// - Backward: RECOMPUTE activations from last checkpoint when needed
// - Memory: O(N × sqrt(L)) with O(sqrt(L)) checkpoints
// - Trade-off: 33% slower, but 50% less memory
//
// Example (12-layer model):
// - No checkpointing: Store 12 layer activations
// - With checkpointing: Store 3 checkpoints → Recompute 3 layers in backward
// - Memory: 12 → 6 activations (50% less)

// ============================================================================
// Forward Function Type
// ============================================================================

// Function signature for layer forward pass
// - input: Layer input activations
// - output: Layer output activations
// - layer_idx: Which layer (for context)
using ForwardFunction = std::function<void(
    const float* input,
    float* output,
    int layer_idx
)>;

// Backward function signature
// - grad_output: Gradient w.r.t. output
// - grad_input: Gradient w.r.t. input (computed)
// - activations: Forward pass activations (for gradient computation)
// - layer_idx: Which layer
using BackwardFunction = std::function<void(
    const float* grad_output,
    float* grad_input,
    const float* activations,
    int layer_idx
)>;

// ============================================================================
// Checkpointed Layer
// ============================================================================

// A layer wrapper that supports gradient checkpointing
class CheckpointedLayer {
public:
    CheckpointedLayer(
        int layer_idx,
        int activation_size,
        ForwardFunction forward_fn,
        BackwardFunction backward_fn,
        bool is_checkpoint = false  // Is this a checkpoint boundary?
    );

    // Forward pass (optionally save activations)
    void forward(const float* input, float* output);

    // Backward pass (recompute activations if needed)
    void backward(
        const float* grad_output,
        float* grad_input,
        const float* input  // For recomputation
    );

    // Is this a checkpoint boundary?
    bool is_checkpoint() const { return is_checkpoint_; }

    // Get saved activations (if any)
    const float* get_saved_activations() const {
        return saved_activations_.empty() ? nullptr : saved_activations_.data();
    }

private:
    int layer_idx_;
    int activation_size_;
    ForwardFunction forward_fn_;
    BackwardFunction backward_fn_;
    bool is_checkpoint_;
    std::vector<float> saved_activations_;  // Only saved at checkpoints
};

// ============================================================================
// Checkpointing Strategy
// ============================================================================

enum class CheckpointStrategy {
    // No checkpointing (baseline - high memory)
    NONE,

    // Uniform checkpointing (every N layers)
    // Memory: O(N × L/N) = O(N)
    UNIFORM,

    // Square root checkpointing (optimal trade-off)
    // Checkpoints at layers: 0, sqrt(L), 2*sqrt(L), ..., L
    // Memory: O(N × sqrt(L))
    SQUARE_ROOT,

    // Every other layer (50% memory, 33% slower)
    EVERY_OTHER,

    // Manual checkpoints (user-specified layers)
    MANUAL
};

// ============================================================================
// Gradient Checkpoint Manager
// ============================================================================

class CheckpointManager {
public:
    CheckpointManager(
        int num_layers,
        int activation_size_per_layer,
        CheckpointStrategy strategy = CheckpointStrategy::SQUARE_ROOT
    );

    // Set checkpoint layers (for MANUAL strategy)
    void set_checkpoint_layers(const std::vector<int>& checkpoint_layers);

    // Add a layer with forward/backward functions
    void add_layer(
        int layer_idx,
        ForwardFunction forward_fn,
        BackwardFunction backward_fn
    );

    // Forward pass through all layers
    void forward(
        const float* input,
        float* output,
        int batch_size,
        int seq_len
    );

    // Backward pass (with selective recomputation)
    void backward(
        const float* grad_output,
        float* grad_input,
        const float* original_input  // For recomputation
    );

    // Get memory statistics
    struct MemoryStats {
        size_t total_memory_no_checkpoint;   // Bytes without checkpointing
        size_t total_memory_with_checkpoint; // Bytes with checkpointing
        float memory_savings_ratio;          // Savings multiplier
        int num_checkpoints;                 // Number of checkpoint layers
        int num_recomputations;              // Recomputations in last backward
    };

    MemoryStats get_memory_stats() const;

private:
    int num_layers_;
    int activation_size_;
    CheckpointStrategy strategy_;
    std::vector<int> checkpoint_layers_;
    std::vector<std::unique_ptr<CheckpointedLayer>> layers_;
    std::vector<float> temp_activations_;  // For recomputation
    int num_recomputations_;

    // Determine if layer should be a checkpoint
    bool should_checkpoint(int layer_idx) const;

    // Compute checkpoint layers based on strategy
    void compute_checkpoint_layers();
};

// ============================================================================
// Simplified Checkpointing API
// ============================================================================

// Checkpoint a sequence of operations
// - forward_fn: Function that computes forward pass
// - backward_fn: Function that computes backward pass
// - input: Input tensor
// - output: Output tensor (computed)
// - Automatically handles activation saving/recomputation
template<typename T>
class CheckpointedSequence {
public:
    CheckpointedSequence(int num_segments = 4);

    // Add a segment to checkpoint
    void add_segment(
        std::function<void(const T*, T*)> forward_fn,
        std::function<void(const T*, T*, const T*)> backward_fn
    );

    // Run forward pass
    void forward(const T* input, T* output);

    // Run backward pass
    void backward(const T* grad_output, T* grad_input);

private:
    int num_segments_;
    std::vector<std::function<void(const T*, T*)>> forward_fns_;
    std::vector<std::function<void(const T*, T*, const T*)>> backward_fns_;
    std::vector<std::vector<T>> checkpoints_;  // Saved at segment boundaries
};

// ============================================================================
// Utilities
// ============================================================================

// Calculate optimal number of checkpoints
// - num_layers: Total number of layers
// - Returns: Optimal checkpoint count (square root rule)
int calculate_optimal_checkpoints(int num_layers);

// Estimate memory savings
// - num_layers: Total layers
// - activation_size: Size per layer (in floats)
// - num_checkpoints: Number of checkpoints
// - Returns: Memory savings multiplier
float estimate_memory_savings(
    int num_layers,
    int activation_size,
    int num_checkpoints
);

// Print checkpointing strategy
void print_checkpoint_strategy(
    int num_layers,
    const std::vector<int>& checkpoint_layers
);

// ============================================================================
// Benefits Analysis
// ============================================================================

// Memory vs Compute Trade-off:
//
// No Checkpointing:
// - Memory: O(N × L) = N activations per layer × L layers
// - Compute: 1x (forward + backward)
// - Example: 12 layers × 1MB/layer = 12 MB
//
// Square Root Checkpointing (optimal):
// - Memory: O(N × sqrt(L)) = sqrt(L) checkpoints
// - Compute: ~1.33x (forward + backward + recomputation)
// - Example: 12 layers → 4 checkpoints = 4 MB (67% less memory)
//
// Every Other Layer:
// - Memory: O(N × L/2) = 50% of original
// - Compute: ~1.33x
// - Example: 12 layers → 6 checkpoints = 6 MB (50% less memory)
//
// Uniform (N checkpoints):
// - Memory: O(N × N_ckpt)
// - Compute: ~(1 + (L / N_ckpt)) x
// - Example: 12 layers → 3 checkpoints = 3 MB (75% less, but 5x compute)
//
// Recommendation:
// - Use SQUARE_ROOT for balanced memory/compute trade-off
// - Use EVERY_OTHER for moderate memory savings with low overhead
// - Use UNIFORM with few checkpoints for maximum memory savings

} // namespace GradientCheckpoint

#endif // GRADIENT_CHECKPOINT_H
