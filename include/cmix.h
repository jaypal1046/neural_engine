#pragma once
#include <cstdint>
#include <vector>

// =============================================================================
// Context Mixing (CMIX) — The "PAQ" Architecture
// =============================================================================
//
// Traditional compression runs one algorithm (PPM, LZ77, BWT).
// Context Mixing runs MULTIPLE models simultaneously on the exact same data,
// and predicts the next *BIT* (not byte).
//
// A small Neural Network (Logistic Mixer) combines the predictions.
// If Model A is guessing correctly, its weight increases.
// If Model B is guessing wrong, its weight decreases.
//
// Components:
// 1. Bit-level Range Coder (Arithmetic Coder optimized for binary 0/1)
// 2. Models (e.g., Order-1, Order-4, Order-6, Word Matcher)
// 3. Logistic Mixer (Neural Network)
// =============================================================================

struct Mixer {
    // Neural network weights for each model
    std::vector<float> global_weights;
    std::vector<float> niche_weights; // Flattend to 1D for Cache Speed
    float last_p1;
    
    Mixer(int num_models);

    // 1. Combine probabilities from models into a single bit prediction (0-4095)
    int mix(const std::vector<float>& stretched_preds, const std::vector<int>& active_models, uint8_t last_byte);

    // 2. Gradient Descent: Adjust weights based on the actual bit that occurred
    void update(int actual_bit, const std::vector<float>& stretched_preds, const std::vector<int>& active_models, uint8_t last_byte);
};

#include <functional> // needed for std::function

// Comprees callback for progress reports
using CmixProgressCb = std::function<void(size_t, size_t, int)>;

// Compress data using Bit-level Context Mixing. Returns coded stream.
std::vector<uint8_t> cmix_encode(const uint8_t* data, size_t len, CmixProgressCb cb = nullptr);

// Decompress sym_count bytes using Bit-level Context Mixing.
std::vector<uint8_t> cmix_decode(const uint8_t* coded, size_t coded_len, size_t sym_count, CmixProgressCb cb = nullptr);
