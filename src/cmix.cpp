#include "cmix.h"
#include <cmath>
#include <vector>

// -----------------------------------------------------------------------------
// Logistic Mixer
// -----------------------------------------------------------------------------
static inline float squash(float x) {
    return 1.0f / (1.0f + std::exp(-x));
}

static inline float stretch(float p) {
    if (p <= 0.001f) p = 0.001f;
    if (p >= 0.999f) p = 0.999f;
    return std::log(p / (1.0f - p));
}

int Mixer::mix(const std::vector<float>& predictions) {
    float dot = 0.0f;
    for (size_t i = 0; i < weights.size(); ++i) {
        dot += weights[i] * stretch(predictions[i]);
    }
    float p1 = squash(dot);
    // Convert float probability [0, 1] to a 12-bit integer [1, 4095]
    int p = (int)(p1 * 4096.0f);
    if (p < 1) p = 1;
    if (p > 4095) p = 4095;
    return p;
}

void Mixer::update(int actual_bit, const std::vector<float>& predictions) {
    // Learning rate
    const float lr = 0.005f;
    float dot = 0.0f;
    for (size_t i = 0; i < weights.size(); ++i) {
        dot += weights[i] * stretch(predictions[i]);
    }
    float p1 = squash(dot);
    float error = (float)actual_bit - p1;
    
    for (size_t i = 0; i < weights.size(); ++i) {
        weights[i] += lr * error * stretch(predictions[i]);
    }
}

// -----------------------------------------------------------------------------
// Binary Arithmetic Coder
// -----------------------------------------------------------------------------
struct BinRangeEncoder {
    uint32_t low = 0;
    uint32_t range = 0xFFFFFFFFu;
    std::vector<uint8_t> buf;

    void encode(int bit, int p1_4096) {
        // p1_4096 is probability of bit == 1, out of 4096
        uint32_t r1 = (range >> 12) * p1_4096;
        uint32_t r0 = range - r1;
        
        if (bit == 0) {
            range = r0;
        } else {
            low += r0;
            range = r1;
        }
        
        while ((low ^ (low + range)) < (1u << 24)) {
            buf.push_back((uint8_t)(low >> 24));
            low <<= 8;
            range <<= 8;
        }
        while (range < (1u << 16)) {
            buf.push_back((uint8_t)(low >> 24));
            range = (~low) << 8;
            low <<= 8;
        }
    }
    
    std::vector<uint8_t> finish() {
        for (int i = 3; i >= 0; --i)
            buf.push_back((uint8_t)(low >> (i * 8)));
        return std::move(buf);
    }
};

struct BinRangeDecoder {
    uint32_t low = 0;
    uint32_t range = 0xFFFFFFFFu;
    uint32_t code = 0;
    const uint8_t* src;
    size_t pos = 0;
    size_t len;

    void init(const uint8_t* data, size_t size) {
        src = data; len = size;
        for (int i = 0; i < 4; ++i)
            code = (code << 8) | (pos < len ? src[pos++] : 0);
    }

    int decode(int p1_4096) {
        uint32_t r1 = (range >> 12) * p1_4096;
        uint32_t r0 = range - r1;
        
        int bit = 0;
        if ((code - low) >= r0) {
            bit = 1;
            low += r0;
            range = r1;
        } else {
            bit = 0;
            range = r0;
        }
        
        while ((low ^ (low + range)) < (1u << 24)) {
            code = (code << 8) | (pos < len ? src[pos++] : 0);
            low <<= 8;
            range <<= 8;
        }
        while (range < (1u << 16)) {
            code = (code << 8) | (pos < len ? src[pos++] : 0);
            range = (~low) << 8;
            low <<= 8;
        }
        return bit;
    }
};

// -----------------------------------------------------------------------------
// Models
// -----------------------------------------------------------------------------
struct ContextModel {
    std::vector<uint32_t> counts; // counts[hash][0] and counts[hash][1]
    uint64_t mask;
    uint32_t size_mask;
    
    ContextModel(int bits) {
        int hash_bits = bits;
        if (hash_bits > 22) hash_bits = 22; // MAX: 4 million states = 32 MB RAM per active model
        
        mask = (bits == 64) ? ~0ULL : (1ULL << bits) - 1;
        size_mask = (1U << hash_bits) - 1;
        
        counts.resize((size_mask + 1) * 2, 1); // init to 1 for smoothing
    }
    
    uint32_t hash(uint64_t ctx) const {
        ctx &= mask;
        if (mask <= size_mask) return (uint32_t)ctx;
        
        // Fast hash for predicting histories longer than our exact memory
        ctx ^= ctx >> 32;
        ctx *= 0xbf58476d1ce4e5b9ULL;
        ctx ^= ctx >> 32;
        ctx *= 0x94d049bb133111ebULL;
        ctx ^= ctx >> 32;
        return (uint32_t)ctx & size_mask;
    }
    
    float predict(uint64_t ctx) {
        uint32_t h = hash(ctx);
        uint32_t c0 = counts[h * 2 + 0];
        uint32_t c1 = counts[h * 2 + 1];
        return (float)c1 / (float)(c0 + c1);
    }
    
    void update(uint64_t ctx, int bit) {
        uint32_t h = hash(ctx);
        uint32_t& c = counts[h * 2 + bit];
        if (c < 1000000) c++; // cap to avoid overflow/extreme inertia
    }
};

// -----------------------------------------------------------------------------
// cmix_encode / cmix_decode
// -----------------------------------------------------------------------------
std::vector<uint8_t> cmix_encode(const uint8_t* data, size_t len) {
    BinRangeEncoder enc;
    Mixer mixer(8); // 8 bit-level models
    
    ContextModel m0(0);  // Order-0 bit prior (overall 1 vs 0)
    ContextModel m1(8);  // Order-1 byte context
    ContextModel m2(16); // Order-2 byte context
    ContextModel m3(24); // Order-3 byte context
    ContextModel m4(32); // Order-4 byte context
    ContextModel m5(40); // Order-5 byte context
    ContextModel m6(48); // Order-6 byte context
    ContextModel m7(56); // Order-7 byte context
    
    uint64_t history = 0; 
    
    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        for (int b = 7; b >= 0; --b) {
            int bit = (byte >> b) & 1;
            
            // Build contexts from the bit history
            int ctx0 = 0;
            int ctx1 = history & m1.mask;
            int ctx2 = history & m2.mask;
            int ctx3 = history & m3.mask;
            uint64_t ctx4 = history & m4.mask;
            uint64_t ctx5 = history & m5.mask;
            uint64_t ctx6 = history & m6.mask;
            uint64_t ctx7 = history & m7.mask;
            
            std::vector<float> preds = {
                m0.predict(ctx0),
                m1.predict(ctx1),
                m2.predict(ctx2),
                m3.predict(ctx3),
                m4.predict(ctx4),
                m5.predict(ctx5),
                m6.predict(ctx6),
                m7.predict(ctx7)
            };
            
            int p1 = mixer.mix(preds);
            enc.encode(bit, p1);
            mixer.update(bit, preds);
            
            m0.update(ctx0, bit);
            m1.update(ctx1, bit);
            m2.update(ctx2, bit);
            m3.update(ctx3, bit);
            m4.update(ctx4, bit);
            m5.update(ctx5, bit);
            m6.update(ctx6, bit);
            m7.update(ctx7, bit);
            
            history = (history << 1) | bit;
        }
    }
    return enc.finish();
}

std::vector<uint8_t> cmix_decode(const uint8_t* coded, size_t coded_len, size_t sym_count) {
    BinRangeDecoder dec;
    dec.init(coded, coded_len);
    Mixer mixer(8);
    
    ContextModel m0(0); 
    ContextModel m1(8); 
    ContextModel m2(16);
    ContextModel m3(24);
    ContextModel m4(32);
    ContextModel m5(40);
    ContextModel m6(48);
    ContextModel m7(56);
    
    std::vector<uint8_t> out;
    out.reserve(sym_count);
    
    uint64_t history = 0;
    
    for (size_t i = 0; i < sym_count; ++i) {
        uint8_t byte = 0;
        for (int b = 7; b >= 0; --b) {
            int ctx0 = 0;
            int ctx1 = history & m1.mask;
            int ctx2 = history & m2.mask;
            int ctx3 = history & m3.mask;
            uint64_t ctx4 = history & m4.mask;
            uint64_t ctx5 = history & m5.mask;
            uint64_t ctx6 = history & m6.mask;
            uint64_t ctx7 = history & m7.mask;
            
            std::vector<float> preds = {
                m0.predict(ctx0),
                m1.predict(ctx1),
                m2.predict(ctx2),
                m3.predict(ctx3),
                m4.predict(ctx4),
                m5.predict(ctx5),
                m6.predict(ctx6),
                m7.predict(ctx7)
            };
            
            int p1 = mixer.mix(preds);
            int bit = dec.decode(p1);
            mixer.update(bit, preds);
            
            m0.update(ctx0, bit);
            m1.update(ctx1, bit);
            m2.update(ctx2, bit);
            m3.update(ctx3, bit);
            m4.update(ctx4, bit);
            m5.update(ctx5, bit);
            m6.update(ctx6, bit);
            m7.update(ctx7, bit);
            
            history = (history << 1) | bit;
            byte |= (bit << b);
        }
        out.push_back(byte);
    }
    return out;
}
