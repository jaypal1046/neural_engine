#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <vector>
#include <cmath>
#include <algorithm>

// Adam Optimizer: Adaptive Moment Estimation
// Combines momentum (moving average of gradients) with RMSProp (moving average of squared gradients)
// Paper: "Adam: A Method for Stochastic Optimization" (Kingma & Ba, 2014)

class AdamOptimizer {
public:
    AdamOptimizer(
        float learning_rate = 0.001f,
        float beta1 = 0.9f,          // Momentum decay
        float beta2 = 0.999f,         // RMSProp decay
        float epsilon = 1e-8f,        // Numerical stability
        float weight_decay = 0.0f     // L2 regularization
    );

    // Update a single parameter
    void update(float* param, const float* grad, int size);

    // Update with gradient clipping (prevents exploding gradients)
    void update_with_clipping(float* param, const float* grad, int size, float max_norm = 5.0f);

    // Update learning rate (for learning rate scheduling)
    void set_learning_rate(float lr) { learning_rate_ = lr; }
    float get_learning_rate() const { return learning_rate_; }

    // Step counter (for bias correction)
    int get_step() const { return step_; }
    void increment_step() { step_++; }

    // Reset optimizer state (for new training run)
    void reset();

private:
    float learning_rate_;
    float beta1_;
    float beta2_;
    float epsilon_;
    float weight_decay_;
    int step_;

    // Momentum terms (first moment estimate)
    std::vector<std::vector<float>> m_;

    // RMSProp terms (second moment estimate)
    std::vector<std::vector<float>> v_;

    // Parameter group tracking
    std::vector<int> param_sizes_;
    int num_param_groups_;

    // Helper: Get or create momentum/RMSProp buffers for parameter group
    int get_param_group_id(int size);
};

// Learning Rate Schedulers
class LearningRateScheduler {
public:
    virtual ~LearningRateScheduler() = default;
    virtual float get_lr(int step, float base_lr) const = 0;
};

// Cosine annealing with warm restarts
class CosineAnnealingLR : public LearningRateScheduler {
public:
    CosineAnnealingLR(int T_max, float eta_min = 0.0f)
        : T_max_(T_max), eta_min_(eta_min) {}

    float get_lr(int step, float base_lr) const override {
        float progress = static_cast<float>(step % T_max_) / T_max_;
        return eta_min_ + (base_lr - eta_min_) * 0.5f * (1.0f + std::cos(3.14159265359f * progress));
    }

private:
    int T_max_;
    float eta_min_;
};

// Linear warmup + exponential decay
class WarmupExponentialLR : public LearningRateScheduler {
public:
    WarmupExponentialLR(int warmup_steps, float gamma = 0.95f)
        : warmup_steps_(warmup_steps), gamma_(gamma) {}

    float get_lr(int step, float base_lr) const override {
        if (step < warmup_steps_) {
            // Linear warmup
            return base_lr * (static_cast<float>(step) / warmup_steps_);
        } else {
            // Exponential decay
            int decay_steps = step - warmup_steps_;
            return base_lr * std::pow(gamma_, static_cast<float>(decay_steps) / 1000.0f);
        }
    }

private:
    int warmup_steps_;
    float gamma_;
};

// Helper functions for gradient operations
namespace GradientUtils {
    // Clip gradients by norm (prevents exploding gradients)
    float clip_grad_norm(float* grad, int size, float max_norm);

    // Clip gradients by value
    void clip_grad_value(float* grad, int size, float clip_value);

    // Compute gradient norm
    float grad_norm(const float* grad, int size);

    // Zero out gradients
    void zero_grad(float* grad, int size);
}

#endif // OPTIMIZER_H
