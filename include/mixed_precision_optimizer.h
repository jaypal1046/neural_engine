#ifndef MIXED_PRECISION_OPTIMIZER_H
#define MIXED_PRECISION_OPTIMIZER_H

#include <vector>
#include <string>
#include <unordered_map>
#include "precision_utils.h"

namespace MixedPrecision {

class MixedPrecisionOptimizer {
public:
    enum class PrecisionMode {
        FP32,  // Full precision (baseline)
        FP16,  // Half precision (IEEE 754)
        BF16   // Brain Float 16
    };

    MixedPrecisionOptimizer(PrecisionMode mode = PrecisionMode::FP32);

    // Precision mode management
    void set_precision_mode(PrecisionMode mode);
    PrecisionMode get_precision_mode() const;

    // Weight conversion (single matrix)
    void convert_to_precision(
        std::vector<std::vector<float>>& weights,
        PrecisionMode mode
    ) const;

    // Weight conversion (batched)
    void convert_weights_to_precision(
        std::vector<std::vector<float>>& weights
    ) const;

private:
    PrecisionMode mode_;
};

// ============================================================================
// Mixed Precision Trainer - Master Weights Pattern
// ============================================================================

class MixedPrecisionTrainer {
public:
    MixedPrecisionTrainer(MixedPrecisionOptimizer::PrecisionMode mode =
                         MixedPrecisionOptimizer::PrecisionMode::FP32);

    // Master weights management
    void set_master_weights(const std::string& name,
                           const std::vector<std::vector<float>>& weights);

    std::vector<std::vector<float>> get_master_weights(const std::string& name) const;

    bool has_master_weights(const std::string& name) const;

    // Convert master weights → working weights (FP32 → FP16/BF16)
    std::vector<std::vector<float>> get_working_weights(const std::string& name) const;

    // Update master weights from gradients
    void update_from_gradients(
        const std::string& name,
        const std::vector<std::vector<float>>& gradients,
        float learning_rate
    );

    // Loss scaling (for FP16 stability)
    void set_loss_scale(float scale);
    float get_loss_scale() const;

    // Scale/unscale gradients
    void scale_gradients(std::vector<std::vector<float>>& gradients) const;
    void unscale_gradients(std::vector<std::vector<float>>& gradients) const;

    // Precision mode
    void set_precision_mode(MixedPrecisionOptimizer::PrecisionMode mode);
    MixedPrecisionOptimizer::PrecisionMode get_precision_mode() const;

    // Statistics
    size_t get_num_parameters() const;
    float get_memory_usage_mb() const;

private:
    MixedPrecisionOptimizer::PrecisionMode mode_;

    // Master weights (always FP32)
    std::unordered_map<std::string, std::vector<std::vector<float>>> master_weights_;

    // Loss scaling factor (default 1024 for FP16, 1.0 for FP32/BF16)
    float loss_scale_;

    // Helper: Convert single weight matrix based on mode
    void apply_precision_conversion(std::vector<std::vector<float>>& weights) const;
};

} // namespace MixedPrecision

#endif // MIXED_PRECISION_OPTIMIZER_H
