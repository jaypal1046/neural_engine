#ifndef LOSS_H
#define LOSS_H

#include <vector>
#include <cmath>
#include <algorithm>

// Loss Functions for Neural Network Training

// Cross-Entropy Loss (for classification/language modeling)
class CrossEntropyLoss {
public:
    CrossEntropyLoss(bool reduce_mean = true, float label_smoothing = 0.0f)
        : reduce_mean_(reduce_mean), label_smoothing_(label_smoothing) {}

    // Forward: compute loss
    // logits: [vocab_size] - raw network outputs (before softmax)
    // target: target token ID
    // Returns: scalar loss value
    float forward(const std::vector<float>& logits, int target);

    // Backward: compute gradient
    // Returns: gradient w.r.t. logits [vocab_size]
    std::vector<float> backward(const std::vector<float>& logits, int target);

    // Forward + backward in one pass (more efficient)
    float forward_backward(const std::vector<float>& logits, int target, std::vector<float>& grad_out);

private:
    bool reduce_mean_;
    float label_smoothing_;

    // Cached values from forward pass
    std::vector<float> probs_;  // Softmax probabilities
};

// Softmax function (used internally by CrossEntropyLoss)
namespace Softmax {
    // Apply softmax: probs[i] = exp(logits[i]) / sum(exp(logits))
    std::vector<float> forward(const std::vector<float>& logits);

    // Softmax backward: grad_logits = probs * (grad_probs - dot(probs, grad_probs))
    // This is the gradient when softmax is followed by cross-entropy
    std::vector<float> backward(const std::vector<float>& probs, const std::vector<float>& grad_probs);

    // Numerically stable softmax (prevents overflow)
    std::vector<float> stable_softmax(const std::vector<float>& logits);
}

// Mean Squared Error Loss (for regression tasks)
class MSELoss {
public:
    MSELoss(bool reduce_mean = true) : reduce_mean_(reduce_mean) {}

    // Forward: compute loss
    float forward(const std::vector<float>& predictions, const std::vector<float>& targets);

    // Backward: compute gradient
    std::vector<float> backward(const std::vector<float>& predictions, const std::vector<float>& targets);

private:
    bool reduce_mean_;
};

// Contrastive Loss (for embedding training)
class ContrastiveLoss {
public:
    ContrastiveLoss(float margin = 1.0f) : margin_(margin) {}

    // Forward: compute loss for positive and negative pairs
    // positive_sim: similarity for positive pair
    // negative_sims: similarities for negative pairs
    float forward(float positive_sim, const std::vector<float>& negative_sims);

    // Backward: compute gradients
    void backward(float positive_sim, const std::vector<float>& negative_sims,
                  float& grad_positive, std::vector<float>& grad_negatives);

private:
    float margin_;
};

// Utility functions for loss computation
namespace LossUtils {
    // Log-sum-exp trick for numerical stability
    float log_sum_exp(const std::vector<float>& values);

    // Negative log likelihood
    float nll_loss(const std::vector<float>& log_probs, int target);

    // Label smoothing
    std::vector<float> smooth_labels(int vocab_size, int target, float smoothing);

    // Perplexity from cross-entropy loss
    float perplexity(float cross_entropy_loss);
}

#endif // LOSS_H
