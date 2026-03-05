// Mixed Precision Training Implementation
// Algorithm adapted from: NVIDIA Apex (BSD-3 License)
// Original: https://github.com/NVIDIA/apex
// Modifications: CPU implementation, no CUDA dependency

#include "mixed_precision.h"
#include <cstring>
#include <iostream>

namespace MixedPrecision {

// ============================================================================
// Dynamic Loss Scaler
// ============================================================================

DynamicLossScaler::DynamicLossScaler(
    float init_scale,
    float scale_factor,
    int scale_window,
    float min_scale,
    float max_scale
)
    : scale_(init_scale),
      scale_factor_(scale_factor),
      scale_window_(scale_window),
      min_scale_(min_scale),
      max_scale_(max_scale),
      consecutive_steps_(0)
{
}

void DynamicLossScaler::unscale_gradients(float* gradients, int size) const {
    float inv_scale = 1.0f / scale_;
    for (int i = 0; i < size; i++) {
        gradients[i] *= inv_scale;
    }
}

bool DynamicLossScaler::update(bool overflow_detected) {
    if (overflow_detected) {
        // Reduce scale on overflow
        scale_ = std::max(scale_ / scale_factor_, min_scale_);
        consecutive_steps_ = 0;
        return false;  // Indicate overflow
    } else {
        // Increase scale after successful steps
        consecutive_steps_++;
        if (consecutive_steps_ >= scale_window_) {
            scale_ = std::min(scale_ * scale_factor_, max_scale_);
            consecutive_steps_ = 0;
        }
        return true;  // Indicate success
    }
}

// ============================================================================
// Mixed Precision Optimizer
// ============================================================================

MixedPrecisionOptimizer::MixedPrecisionOptimizer(
    PrecisionMode mode,
    float learning_rate,
    bool use_loss_scaling
)
    : mode_(mode),
      learning_rate_(learning_rate),
      use_loss_scaling_(use_loss_scaling && mode == PrecisionMode::FP16),
      loss_scaler_()
{
}

void MixedPrecisionOptimizer::weights_to_half(
    const float* fp32_weights,
    uint16_t* half_weights,
    int size
) {
    if (mode_ == PrecisionMode::FP16) {
        convert_tensor_fp32_to_fp16(fp32_weights, half_weights, size);
    } else if (mode_ == PrecisionMode::BF16) {
        convert_tensor_fp32_to_bf16(fp32_weights, half_weights, size);
    } else {
        // FP32 mode: just copy (interpret as FP32)
        std::memcpy(half_weights, fp32_weights, size * sizeof(float));
    }
}

void MixedPrecisionOptimizer::activations_to_fp32(
    const uint16_t* half_activations,
    float* fp32_activations,
    int size
) {
    if (mode_ == PrecisionMode::FP16) {
        convert_tensor_fp16_to_fp32(half_activations, fp32_activations, size);
    } else if (mode_ == PrecisionMode::BF16) {
        convert_tensor_bf16_to_fp32(half_activations, fp32_activations, size);
    } else {
        // FP32 mode: just copy
        std::memcpy(fp32_activations, half_activations, size * sizeof(float));
    }
}

void MixedPrecisionOptimizer::gradients_to_fp32(
    const uint16_t* half_gradients,
    float* fp32_gradients,
    int size
) {
    // Convert to FP32
    activations_to_fp32(half_gradients, fp32_gradients, size);

    // Unscale if using loss scaling
    if (use_loss_scaling_) {
        loss_scaler_.unscale_gradients(fp32_gradients, size);
    }
}

void MixedPrecisionOptimizer::update_weights(
    float* master_weights,
    const float* gradients,
    int size
) {
    // Simple SGD update: w = w - lr * grad
    for (int i = 0; i < size; i++) {
        master_weights[i] -= learning_rate_ * gradients[i];
    }
}

bool MixedPrecisionOptimizer::check_overflow(const float* gradients, int size) const {
    return has_overflow(gradients, size);
}

// ============================================================================
// Mixed Precision Trainer
// ============================================================================

MixedPrecisionTrainer::MixedPrecisionTrainer(
    int num_params,
    MixedPrecisionOptimizer::PrecisionMode mode
)
    : optimizer(mode),
      master_weights(num_params),
      half_weights(num_params),
      half_gradients(num_params),
      fp32_gradients(num_params)
{
    // Initialize master weights to small random values
    for (int i = 0; i < num_params; i++) {
        master_weights[i] = 0.01f * (rand() / (float)RAND_MAX - 0.5f);
    }
}

bool MixedPrecisionTrainer::train_step(
    const float* input,
    const float* target,
    int batch_size,
    int seq_len
) {
    // 1. Convert master weights to half precision
    optimizer.weights_to_half(
        master_weights.data(),
        half_weights.data(),
        master_weights.size()
    );

    // 2. Forward pass (would use half_weights here)
    // ... (placeholder - actual forward pass goes here)

    // 3. Compute loss (in FP32 for stability)
    float loss = 0.0f;  // placeholder
    if (optimizer.get_loss_scaler().get_scale() > 1.0f) {
        loss = optimizer.get_loss_scaler().scale_loss(loss);
    }

    // 4. Backward pass (compute half_gradients)
    // ... (placeholder - actual backward pass goes here)

    // 5. Convert gradients to FP32 and unscale
    optimizer.gradients_to_fp32(
        half_gradients.data(),
        fp32_gradients.data(),
        fp32_gradients.size()
    );

    // 6. Check for overflow
    bool overflow = optimizer.check_overflow(
        fp32_gradients.data(),
        fp32_gradients.size()
    );

    // 7. Update loss scaler
    bool success = optimizer.get_loss_scaler().update(overflow);
    if (!success) {
        return false;  // Skip weight update on overflow
    }

    // 8. Update master weights (FP32)
    optimizer.update_weights(
        master_weights.data(),
        fp32_gradients.data(),
        master_weights.size()
    );

    return true;
}

// ============================================================================
// Tensor Conversion Utilities
// ============================================================================

void convert_tensor_fp32_to_fp16(
    const float* src,
    uint16_t* dst,
    int size
) {
    for (int i = 0; i < size; i++) {
        dst[i] = fp32_to_fp16(src[i]);
    }
}

void convert_tensor_fp16_to_fp32(
    const uint16_t* src,
    float* dst,
    int size
) {
    for (int i = 0; i < size; i++) {
        dst[i] = fp16_to_fp32(src[i]);
    }
}

void convert_tensor_fp32_to_bf16(
    const float* src,
    uint16_t* dst,
    int size
) {
    for (int i = 0; i < size; i++) {
        dst[i] = fp32_to_bf16(src[i]);
    }
}

void convert_tensor_bf16_to_fp32(
    const uint16_t* src,
    float* dst,
    int size
) {
    for (int i = 0; i < size; i++) {
        dst[i] = bf16_to_fp32(src[i]);
    }
}

bool has_overflow(const float* gradients, int size) {
    for (int i = 0; i < size; i++) {
        if (std::isinf(gradients[i]) || std::isnan(gradients[i])) {
            return true;
        }
    }
    return false;
}

} // namespace MixedPrecision
