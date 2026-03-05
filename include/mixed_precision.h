#ifndef MIXED_PRECISION_H
#define MIXED_PRECISION_H

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <limits>
#include <cstring>

// Mixed Precision Training (FP16/BF16 + FP32)
// Algorithm adapted from: NVIDIA Apex (BSD-3 License)
// Original: https://github.com/NVIDIA/apex
// Paper: "Mixed Precision Training" (Micikevicius et al., 2018)
// Modifications: CPU implementation, no CUDA dependency

namespace MixedPrecision {

// ============================================================================
// Half-Precision Formats
// ============================================================================

// FP16 (IEEE 754 half-precision)
// - 1 sign bit, 5 exponent bits, 10 mantissa bits
// - Range: ±65504, Precision: ~3 decimal digits
// - Needs loss scaling to prevent gradient underflow

// BF16 (bfloat16 - Google Brain format)
// - 1 sign bit, 8 exponent bits, 7 mantissa bits
// - Same range as FP32, less precision
// - No loss scaling needed (better for training)

// ============================================================================
// FP16 Conversion
// ============================================================================

// Convert FP32 to FP16 (IEEE 754)
inline uint16_t fp32_to_fp16(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));

    uint32_t sign = (bits >> 16) & 0x8000;
    uint32_t exp = ((bits >> 23) & 0xff) - 127 + 15;
    uint32_t mantissa = bits & 0x7fffff;

    // Handle special cases
    if (exp <= 0) {
        // Subnormal or zero
        return static_cast<uint16_t>(sign);
    } else if (exp >= 31) {
        // Infinity or too large
        return static_cast<uint16_t>(sign | 0x7c00);
    }

    // Normal number
    return static_cast<uint16_t>(sign | (exp << 10) | (mantissa >> 13));
}

// Convert FP16 to FP32
inline float fp16_to_fp32(uint16_t value) {
    uint32_t sign = (value & 0x8000) << 16;
    uint32_t exp = (value & 0x7c00) >> 10;
    uint32_t mantissa = value & 0x3ff;

    if (exp == 0) {
        // Subnormal or zero
        if (mantissa == 0) {
            uint32_t result = sign;
            float f;
            std::memcpy(&f, &result, sizeof(float));
            return f;
        }
        // Subnormal: convert to normal
        exp = 1;
        while ((mantissa & 0x400) == 0) {
            mantissa <<= 1;
            exp--;
        }
        mantissa &= 0x3ff;
    } else if (exp == 31) {
        // Infinity or NaN
        uint32_t result = sign | 0x7f800000 | (mantissa << 13);
        float f;
        std::memcpy(&f, &result, sizeof(float));
        return f;
    }

    // Normal number
    uint32_t result = sign | ((exp + 127 - 15) << 23) | (mantissa << 13);
    float f;
    std::memcpy(&f, &result, sizeof(float));
    return f;
}

// ============================================================================
// BF16 Conversion (Simpler than FP16)
// ============================================================================

// Convert FP32 to BF16 (truncate mantissa)
inline uint16_t fp32_to_bf16(float value) {
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(float));

    // Round to nearest even (RNE)
    uint32_t rounding_bias = 0x7fff + ((bits >> 16) & 1);
    bits += rounding_bias;

    // Truncate to 16 bits (keep sign + exponent + top 7 mantissa bits)
    return static_cast<uint16_t>(bits >> 16);
}

// Convert BF16 to FP32 (zero-extend mantissa)
inline float bf16_to_fp32(uint16_t value) {
    uint32_t bits = static_cast<uint32_t>(value) << 16;
    float f;
    std::memcpy(&f, &bits, sizeof(float));
    return f;
}

// ============================================================================
// Loss Scaling (for FP16 training)
// ============================================================================

// Dynamic loss scaler (prevents gradient underflow)
class DynamicLossScaler {
public:
    DynamicLossScaler(
        float init_scale = 65536.0f,     // 2^16 (good default)
        float scale_factor = 2.0f,        // Multiply/divide by 2
        int scale_window = 2000,          // Scale up after N consecutive successful steps
        float min_scale = 1.0f,
        float max_scale = 65536.0f
    );

    // Scale loss before backward (multiply by scale)
    float scale_loss(float loss) const {
        return loss * scale_;
    }

    // Unscale gradients after backward (divide by scale)
    void unscale_gradients(float* gradients, int size) const;

    // Check for overflow/underflow and update scale
    bool update(bool overflow_detected);

    // Get current scale
    float get_scale() const { return scale_; }

private:
    float scale_;              // Current loss scale
    float scale_factor_;       // Growth/shrink factor
    int scale_window_;         // Steps before scale increase
    float min_scale_;
    float max_scale_;
    int consecutive_steps_;    // Steps without overflow
};

// ============================================================================
// Mixed Precision Optimizer
// ============================================================================

// Optimizer wrapper for mixed precision training
// - Stores master weights in FP32 (high precision)
// - Performs forward/backward in FP16/BF16 (fast)
// - Updates master weights (stability)
class MixedPrecisionOptimizer {
public:
    enum class PrecisionMode {
        FP16,      // IEEE half-precision (needs loss scaling)
        BF16,      // bfloat16 (no loss scaling needed)
        FP32       // Full precision (baseline)
    };

    MixedPrecisionOptimizer(
        PrecisionMode mode = PrecisionMode::FP16,
        float learning_rate = 0.001f,
        bool use_loss_scaling = true
    );

    // Convert model weights to half precision for forward pass
    void weights_to_half(const float* fp32_weights, uint16_t* half_weights, int size);

    // Convert activations back to FP32 for loss computation
    void activations_to_fp32(const uint16_t* half_activations, float* fp32_activations, int size);

    // Convert gradients from half to FP32 and unscale
    void gradients_to_fp32(const uint16_t* half_gradients, float* fp32_gradients, int size);

    // Update master weights (FP32) using FP32 gradients
    void update_weights(float* master_weights, const float* gradients, int size);

    // Check for gradient overflow (inf/nan)
    bool check_overflow(const float* gradients, int size) const;

    // Get loss scaler
    DynamicLossScaler& get_loss_scaler() { return loss_scaler_; }

private:
    PrecisionMode mode_;
    float learning_rate_;
    bool use_loss_scaling_;
    DynamicLossScaler loss_scaler_;
};

// ============================================================================
// Mixed Precision Training Loop Helpers
// ============================================================================

// Training step with mixed precision
struct MixedPrecisionTrainer {
    MixedPrecisionOptimizer optimizer;
    std::vector<float> master_weights;      // FP32 master weights
    std::vector<uint16_t> half_weights;     // FP16/BF16 working copy
    std::vector<uint16_t> half_gradients;   // FP16/BF16 gradients
    std::vector<float> fp32_gradients;      // FP32 gradients (for update)

    MixedPrecisionTrainer(
        int num_params,
        MixedPrecisionOptimizer::PrecisionMode mode = MixedPrecisionOptimizer::PrecisionMode::FP16
    );

    // Perform one training step
    // Returns: true if successful, false if overflow detected
    bool train_step(
        const float* input,
        const float* target,
        int batch_size,
        int seq_len
    );

    // Get current master weights
    const float* get_weights() const { return master_weights.data(); }
    float* get_weights() { return master_weights.data(); }
};

// ============================================================================
// Utilities
// ============================================================================

// Convert entire tensor FP32 → FP16
void convert_tensor_fp32_to_fp16(
    const float* src,
    uint16_t* dst,
    int size
);

// Convert entire tensor FP16 → FP32
void convert_tensor_fp16_to_fp32(
    const uint16_t* src,
    float* dst,
    int size
);

// Convert entire tensor FP32 → BF16
void convert_tensor_fp32_to_bf16(
    const float* src,
    uint16_t* dst,
    int size
);

// Convert entire tensor BF16 → FP32
void convert_tensor_bf16_to_fp32(
    const uint16_t* src,
    float* dst,
    int size
);

// Detect overflow in gradients (inf/nan)
bool has_overflow(const float* gradients, int size);

// ============================================================================
// Benefits of Mixed Precision Training
// ============================================================================

// 1. Memory Savings:
//    - FP16/BF16 weights: 2 bytes vs 4 bytes (50% less memory)
//    - Can train 2x larger models in same memory
//
// 2. Speed:
//    - Modern CPUs have FP16 SIMD instructions (2x throughput)
//    - Less memory bandwidth (2x faster transfers)
//    - Combined: 2-3x faster training
//
// 3. Accuracy:
//    - FP32 master weights prevent drift
//    - Loss scaling prevents gradient underflow (FP16)
//    - BF16 has same range as FP32 (no loss scaling needed)
//
// 4. When to Use:
//    - FP16: Training large models (needs loss scaling)
//    - BF16: Training medium models (easier, no scaling)
//    - FP32: Small models or debugging (baseline)

} // namespace MixedPrecision

#endif // MIXED_PRECISION_H
