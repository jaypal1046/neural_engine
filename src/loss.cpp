#include "loss.h"
#include <iostream>
#include <numeric>
#include <cmath>

// CrossEntropyLoss implementation
float CrossEntropyLoss::forward(const std::vector<float>& logits, int target) {
    // Compute softmax probabilities
    probs_ = Softmax::stable_softmax(logits);

    // Compute negative log likelihood
    float loss = -std::log(probs_[target] + 1e-10f);  // Add epsilon for numerical stability

    // Apply label smoothing if enabled
    if (label_smoothing_ > 0.0f) {
        int vocab_size = logits.size();
        float smooth_loss = 0.0f;
        for (float prob : probs_) {
            smooth_loss -= std::log(prob + 1e-10f);
        }
        smooth_loss /= vocab_size;

        loss = (1.0f - label_smoothing_) * loss + label_smoothing_ * smooth_loss;
    }

    return loss;
}

std::vector<float> CrossEntropyLoss::backward(const std::vector<float>& logits, int target) {
    // Gradient of cross-entropy w.r.t. logits
    // grad[i] = prob[i] - (i == target ? 1 : 0)
    std::vector<float> grad(logits.size());

    for (size_t i = 0; i < logits.size(); i++) {
        grad[i] = probs_[i];
        if (static_cast<int>(i) == target) {
            grad[i] -= 1.0f;
        }

        // Apply label smoothing
        if (label_smoothing_ > 0.0f) {
            float smooth_target = label_smoothing_ / logits.size();
            grad[i] = (1.0f - label_smoothing_) * grad[i] + smooth_target;
        }
    }

    return grad;
}

float CrossEntropyLoss::forward_backward(const std::vector<float>& logits, int target, std::vector<float>& grad_out) {
    float loss = forward(logits, target);
    grad_out = backward(logits, target);
    return loss;
}

// Softmax implementation
namespace Softmax {
    std::vector<float> forward(const std::vector<float>& logits) {
        return stable_softmax(logits);
    }

    std::vector<float> stable_softmax(const std::vector<float>& logits) {
        // Find max for numerical stability
        float max_logit = *std::max_element(logits.begin(), logits.end());

        // Compute exp(logits - max)
        std::vector<float> exp_logits(logits.size());
        float sum_exp = 0.0f;

        for (size_t i = 0; i < logits.size(); i++) {
            exp_logits[i] = std::exp(logits[i] - max_logit);
            sum_exp += exp_logits[i];
        }

        // Normalize
        for (float& val : exp_logits) {
            val /= sum_exp;
        }

        return exp_logits;
    }

    std::vector<float> backward(const std::vector<float>& probs, const std::vector<float>& grad_probs) {
        // Jacobian of softmax is: J[i,j] = probs[i] * (delta[i,j] - probs[j])
        // For cross-entropy, this simplifies to: grad_logits[i] = probs[i] - targets[i]

        int n = probs.size();
        std::vector<float> grad_logits(n, 0.0f);

        // Compute dot product: sum(probs * grad_probs)
        float dot = 0.0f;
        for (int i = 0; i < n; i++) {
            dot += probs[i] * grad_probs[i];
        }

        // Compute gradient
        for (int i = 0; i < n; i++) {
            grad_logits[i] = probs[i] * (grad_probs[i] - dot);
        }

        return grad_logits;
    }
}

// MSELoss implementation
float MSELoss::forward(const std::vector<float>& predictions, const std::vector<float>& targets) {
    float sum_sq_error = 0.0f;

    for (size_t i = 0; i < predictions.size(); i++) {
        float diff = predictions[i] - targets[i];
        sum_sq_error += diff * diff;
    }

    if (reduce_mean_) {
        return sum_sq_error / predictions.size();
    } else {
        return sum_sq_error;
    }
}

std::vector<float> MSELoss::backward(const std::vector<float>& predictions, const std::vector<float>& targets) {
    std::vector<float> grad(predictions.size());

    float scale = reduce_mean_ ? (2.0f / predictions.size()) : 2.0f;

    for (size_t i = 0; i < predictions.size(); i++) {
        grad[i] = scale * (predictions[i] - targets[i]);
    }

    return grad;
}

// ContrastiveLoss implementation
float ContrastiveLoss::forward(float positive_sim, const std::vector<float>& negative_sims) {
    // Contrastive loss: -log(exp(pos) / (exp(pos) + sum(exp(neg))))
    // This encourages positive_sim to be large and negative_sims to be small

    float exp_pos = std::exp(positive_sim);
    float sum_exp_neg = 0.0f;

    for (float neg_sim : negative_sims) {
        sum_exp_neg += std::exp(neg_sim);
    }

    float loss = -std::log(exp_pos / (exp_pos + sum_exp_neg + 1e-10f));
    return loss;
}

void ContrastiveLoss::backward(float positive_sim, const std::vector<float>& negative_sims,
                                float& grad_positive, std::vector<float>& grad_negatives) {
    float exp_pos = std::exp(positive_sim);
    float sum_exp_neg = 0.0f;

    for (float neg_sim : negative_sims) {
        sum_exp_neg += std::exp(neg_sim);
    }

    float denominator = exp_pos + sum_exp_neg;

    // Gradient w.r.t. positive similarity
    grad_positive = -1.0f + (exp_pos / denominator);

    // Gradient w.r.t. negative similarities
    grad_negatives.resize(negative_sims.size());
    for (size_t i = 0; i < negative_sims.size(); i++) {
        float exp_neg = std::exp(negative_sims[i]);
        grad_negatives[i] = exp_neg / denominator;
    }
}

// Utility functions
namespace LossUtils {
    float log_sum_exp(const std::vector<float>& values) {
        if (values.empty()) return 0.0f;

        float max_val = *std::max_element(values.begin(), values.end());
        float sum_exp = 0.0f;

        for (float val : values) {
            sum_exp += std::exp(val - max_val);
        }

        return max_val + std::log(sum_exp);
    }

    float nll_loss(const std::vector<float>& log_probs, int target) {
        return -log_probs[target];
    }

    std::vector<float> smooth_labels(int vocab_size, int target, float smoothing) {
        std::vector<float> labels(vocab_size, smoothing / vocab_size);
        labels[target] += (1.0f - smoothing);
        return labels;
    }

    float perplexity(float cross_entropy_loss) {
        return std::exp(cross_entropy_loss);
    }
}
