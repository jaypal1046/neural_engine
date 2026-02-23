#include "optimizer.h"
#include <iostream>
#include <cstring>

AdamOptimizer::AdamOptimizer(float learning_rate, float beta1, float beta2, float epsilon, float weight_decay)
    : learning_rate_(learning_rate)
    , beta1_(beta1)
    , beta2_(beta2)
    , epsilon_(epsilon)
    , weight_decay_(weight_decay)
    , step_(0)
    , num_param_groups_(0) {
}

int AdamOptimizer::get_param_group_id(int size) {
    // Find existing parameter group with this size
    for (int i = 0; i < num_param_groups_; i++) {
        if (param_sizes_[i] == size) {
            return i;
        }
    }

    // Create new parameter group
    param_sizes_.push_back(size);
    m_.push_back(std::vector<float>(size, 0.0f));
    v_.push_back(std::vector<float>(size, 0.0f));
    return num_param_groups_++;
}

void AdamOptimizer::update(float* param, const float* grad, int size) {
    // Get or create parameter group
    int group_id = get_param_group_id(size);
    auto& m = m_[group_id];
    auto& v = v_[group_id];

    // Increment step counter
    step_++;

    // Bias correction factors
    float bias_correction1 = 1.0f - std::pow(beta1_, static_cast<float>(step_));
    float bias_correction2 = 1.0f - std::pow(beta2_, static_cast<float>(step_));

    // Update parameters
    for (int i = 0; i < size; i++) {
        float g = grad[i];

        // L2 regularization (weight decay)
        if (weight_decay_ > 0.0f) {
            g += weight_decay_ * param[i];
        }

        // Update biased first moment estimate (momentum)
        m[i] = beta1_ * m[i] + (1.0f - beta1_) * g;

        // Update biased second moment estimate (RMSProp)
        v[i] = beta2_ * v[i] + (1.0f - beta2_) * g * g;

        // Compute bias-corrected first moment
        float m_hat = m[i] / bias_correction1;

        // Compute bias-corrected second moment
        float v_hat = v[i] / bias_correction2;

        // Update parameter
        param[i] -= learning_rate_ * m_hat / (std::sqrt(v_hat) + epsilon_);
    }
}

void AdamOptimizer::update_with_clipping(float* param, const float* grad, int size, float max_norm) {
    // Clip gradients first
    std::vector<float> clipped_grad(size);
    std::memcpy(clipped_grad.data(), grad, size * sizeof(float));

    float norm = GradientUtils::clip_grad_norm(clipped_grad.data(), size, max_norm);

    // Log if gradients were clipped (disabled for speed - was slowing training 10x)
    // if (norm > max_norm) {
    //     std::cerr << "[OPTIMIZER] Clipped gradients: norm=" << norm << " -> " << max_norm << "\n";
    // }

    // Update with clipped gradients
    update(param, clipped_grad.data(), size);
}

void AdamOptimizer::reset() {
    step_ = 0;
    for (auto& m_group : m_) {
        std::fill(m_group.begin(), m_group.end(), 0.0f);
    }
    for (auto& v_group : v_) {
        std::fill(v_group.begin(), v_group.end(), 0.0f);
    }
}

// Gradient utility functions
namespace GradientUtils {
    float clip_grad_norm(float* grad, int size, float max_norm) {
        // Compute gradient norm
        float norm = grad_norm(grad, size);

        // Clip if necessary
        if (norm > max_norm) {
            float scale = max_norm / (norm + 1e-6f);
            for (int i = 0; i < size; i++) {
                grad[i] *= scale;
            }
        }

        return norm;
    }

    void clip_grad_value(float* grad, int size, float clip_value) {
        for (int i = 0; i < size; i++) {
            grad[i] = std::max(-clip_value, std::min(clip_value, grad[i]));
        }
    }

    float grad_norm(const float* grad, int size) {
        float sum_sq = 0.0f;
        for (int i = 0; i < size; i++) {
            sum_sq += grad[i] * grad[i];
        }
        return std::sqrt(sum_sq);
    }

    void zero_grad(float* grad, int size) {
        std::memset(grad, 0, size * sizeof(float));
    }
}
