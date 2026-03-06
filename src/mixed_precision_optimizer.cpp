#include "mixed_precision_optimizer.h"
#include <cmath>
#include <iostream>

namespace MixedPrecision {

using PMode = MixedPrecisionOptimizer::PrecisionMode;

// ============================================================================
// MixedPrecisionOptimizer Implementation
// ============================================================================

MixedPrecisionOptimizer::MixedPrecisionOptimizer(PrecisionMode mode)
    : mode_(mode) {}

void MixedPrecisionOptimizer::set_precision_mode(PrecisionMode mode) {
    mode_ = mode;
}

MixedPrecisionOptimizer::PrecisionMode MixedPrecisionOptimizer::get_precision_mode() const {
    return mode_;
}

void MixedPrecisionOptimizer::convert_to_precision(
    std::vector<std::vector<float>>& weights,
    PrecisionMode mode
) const {
    using namespace PrecisionUtils;

    if (mode == PMode::FP32) {
        return;  // No conversion needed
    }

    // Round-trip conversion to simulate precision loss
    for (auto& row : weights) {
        for (float& val : row) {
            if (mode == PMode::FP16) {
                uint16_t fp16 = fp32_to_fp16(val);
                val = fp16_to_fp32(fp16);
            } else if (mode == PMode::BF16) {
                uint16_t bf16 = fp32_to_bf16(val);
                val = bf16_to_fp32(bf16);
            }
        }
    }
}

void MixedPrecisionOptimizer::convert_weights_to_precision(
    std::vector<std::vector<float>>& weights
) const {
    convert_to_precision(weights, mode_);
}

// ============================================================================
// MixedPrecisionTrainer Implementation
// ============================================================================

MixedPrecisionTrainer::MixedPrecisionTrainer(PMode mode)
    : mode_(mode)
    , loss_scale_(1.0f)
{
    // Set default loss scale based on precision mode
    if (mode_ == PMode::FP16) {
        loss_scale_ = 1024.0f;  // Prevent gradient underflow in FP16
    } else {
        loss_scale_ = 1.0f;  // No scaling needed for FP32/BF16
    }
}

void MixedPrecisionTrainer::set_master_weights(
    const std::string& name,
    const std::vector<std::vector<float>>& weights
) {
    // Master weights are always stored in FP32
    master_weights_[name] = weights;
}

std::vector<std::vector<float>> MixedPrecisionTrainer::get_master_weights(
    const std::string& name
) const {
    auto it = master_weights_.find(name);
    if (it != master_weights_.end()) {
        return it->second;
    }
    return {};  // Return empty if not found
}

bool MixedPrecisionTrainer::has_master_weights(const std::string& name) const {
    return master_weights_.find(name) != master_weights_.end();
}

std::vector<std::vector<float>> MixedPrecisionTrainer::get_working_weights(
    const std::string& name
) const {
    auto it = master_weights_.find(name);
    if (it == master_weights_.end()) {
        return {};  // Return empty if not found
    }

    // If FP32 mode, return master weights directly (no conversion)
    if (mode_ == PMode::FP32) {
        return it->second;
    }

    // Convert FP32 master → FP16/BF16 working copy
    auto working = it->second;
    apply_precision_conversion(working);
    return working;
}

void MixedPrecisionTrainer::update_from_gradients(
    const std::string& name,
    const std::vector<std::vector<float>>& gradients,
    float learning_rate
) {
    auto it = master_weights_.find(name);
    if (it == master_weights_.end()) {
        std::cerr << "Warning: No master weights found for " << name << "\n";
        return;
    }

    auto& master = it->second;

    // Unscale gradients if loss scaling was applied
    auto scaled_grads = gradients;
    if (loss_scale_ != 1.0f) {
        for (auto& row : scaled_grads) {
            for (float& grad : row) {
                grad /= loss_scale_;
            }
        }
    }

    // Update master weights (always in FP32 for numerical stability)
    if (master.size() != scaled_grads.size()) {
        std::cerr << "Error: Gradient size mismatch for " << name << "\n";
        return;
    }

    for (size_t i = 0; i < master.size(); i++) {
        if (master[i].size() != scaled_grads[i].size()) {
            std::cerr << "Error: Gradient row size mismatch for " << name << "\n";
            return;
        }

        for (size_t j = 0; j < master[i].size(); j++) {
            master[i][j] -= learning_rate * scaled_grads[i][j];
        }
    }
}

void MixedPrecisionTrainer::set_loss_scale(float scale) {
    loss_scale_ = scale;
}

float MixedPrecisionTrainer::get_loss_scale() const {
    return loss_scale_;
}

void MixedPrecisionTrainer::scale_gradients(
    std::vector<std::vector<float>>& gradients
) const {
    if (loss_scale_ == 1.0f) {
        return;  // No scaling needed
    }

    for (auto& row : gradients) {
        for (float& grad : row) {
            grad *= loss_scale_;
        }
    }
}

void MixedPrecisionTrainer::unscale_gradients(
    std::vector<std::vector<float>>& gradients
) const {
    if (loss_scale_ == 1.0f) {
        return;  // No scaling needed
    }

    for (auto& row : gradients) {
        for (float& grad : row) {
            grad /= loss_scale_;
        }
    }
}

void MixedPrecisionTrainer::set_precision_mode(PMode mode) {
    mode_ = mode;

    // Update loss scale based on new mode
    if (mode_ == PMode::FP16) {
        loss_scale_ = 1024.0f;
    } else {
        loss_scale_ = 1.0f;
    }
}

MixedPrecisionOptimizer::PrecisionMode MixedPrecisionTrainer::get_precision_mode() const {
    return mode_;
}

size_t MixedPrecisionTrainer::get_num_parameters() const {
    size_t total = 0;
    for (const auto& [name, weights] : master_weights_) {
        for (const auto& row : weights) {
            total += row.size();
        }
    }
    return total;
}

float MixedPrecisionTrainer::get_memory_usage_mb() const {
    size_t num_params = get_num_parameters();

    // Master weights: always FP32 (4 bytes per param)
    float master_mb = (num_params * 4) / (1024.0f * 1024.0f);

    // Working weights: FP32 (4 bytes) or FP16/BF16 (2 bytes)
    float working_mb = 0.0f;
    if (mode_ == PMode::FP32) {
        working_mb = 0.0f;  // Same as master, no extra copy
    } else {
        working_mb = (num_params * 2) / (1024.0f * 1024.0f);
    }

    return master_mb + working_mb;
}

void MixedPrecisionTrainer::apply_precision_conversion(
    std::vector<std::vector<float>>& weights
) const {
    using namespace PrecisionUtils;

    if (mode_ == PMode::FP32) {
        return;  // No conversion needed
    }

    // Round-trip conversion FP32 → FP16/BF16 → FP32
    for (auto& row : weights) {
        for (float& val : row) {
            if (mode_ == PMode::FP16) {
                uint16_t fp16 = fp32_to_fp16(val);
                val = fp16_to_fp32(fp16);
            } else if (mode_ == PMode::BF16) {
                uint16_t bf16 = fp32_to_bf16(val);
                val = bf16_to_fp32(bf16);
            }
        }
    }
}

} // namespace MixedPrecision
