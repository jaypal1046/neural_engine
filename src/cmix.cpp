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

int Mixer::mix(const float* stretched_preds, int n) {
    float dot = 0.0f;
    for (int i = 0; i < n; ++i) {
        dot += weights[i] * stretched_preds[i];
    }
    last_p1 = squash(dot);
    // Convert float probability [0, 1] to a 12-bit integer [1, 4095]
    int p = (int)(last_p1 * 4096.0f);
    if (p < 1) p = 1;
    if (p > 4095) p = 4095;
    return p;
}

void Mixer::update(int actual_bit, const float* stretched_preds, int n) {
    // Dynamic learning rate: learn faster on chaotic data (p1 near 0.5)
    // Slower on highly confident data (p1 near 0.0 or 1.0)
    float confidence = std::abs(last_p1 - 0.5f);
    float lr = 0.008f - (confidence * 0.012f);
    if (lr < 0.001f) lr = 0.001f;
    
    float error = (float)actual_bit - last_p1;
    for (int i = 0; i < n; ++i) {
        weights[i] += lr * error * stretched_preds[i];
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
    
    ContextModel(int bits, int max_hash_bits = 22) {
        int hash_bits = bits;
        if (hash_bits > max_hash_bits) hash_bits = max_hash_bits; // limit dynamic size
        
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
    int num_models = 68;
    Mixer mixer(num_models);
    
    std::vector<ContextModel> models;
    models.reserve(num_models);
    models.emplace_back(0, 10); // 0: Order-0
    for (int i = 1; i <= 8; i++) models.emplace_back(i * 8, 22); // 1-8: Bytes
    for (int i = 1; i <= 7; i++) models.emplace_back(i, 20);     // 9-15: Bits
    models.emplace_back(64, 22); // 16: Word Matcher
    models.emplace_back(64, 20); // 17: Skip Matcher
    for (int i = 1; i <= 16; i++) models.emplace_back(64, 18);   // 18-33: Sparce Bytes
    for (int i = 1; i <= 16; i++) models.emplace_back(64, 18);   // 34-49: Audio Diffs
    for (int i = 1; i <= 8; i++) models.emplace_back(64, 19);    // 50-57: Word Combinations
    for (int i = 1; i <= 10; i++) models.emplace_back(64, 18);   // 58-67: Context Alignments
    
    uint64_t history = 0; 
    uint32_t word_hash = 0;
    uint32_t current_word = 0;
    std::vector<uint8_t> last_bytes(32, 0);
    std::vector<uint32_t> recent_words(16, 0);
    std::vector<uint8_t> last_seen(256, 0);
    
    float stretched[68];
    uint64_t ctxs[68];
    uint64_t masks[10] = {
        0x5555555555555555ULL, 0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL,
        0xFEFEFEFEFEFEFEFEULL, 0x7F7F7F7F7F7F7F7FULL, 0xAAAAAAAAAAAAAAAAULL,
        0xCCCCCCCCCCCCCCCCULL
    };

    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        
        for (int b = 7; b >= 0; --b) {
            int bit = (byte >> b) & 1;
            
            ctxs[0] = 0;
            for(int m=1; m<=15; ++m) ctxs[m] = history;
            ctxs[16] = ((uint64_t)word_hash << 8) | (history & 0xFF);
            ctxs[17] = ((uint64_t)last_bytes[1] << 16) | ((uint64_t)last_seen[last_bytes[1]] << 8) | (history & 0xFF);
            
            for(int m=0; m<16; ++m) ctxs[18+m] = ((uint64_t)last_bytes[m] << 8) | (history & 0xFF);
            for(int m=0; m<16; ++m) {
                uint8_t diff = last_bytes[0] - last_bytes[m+1];
                ctxs[34+m] = ((uint64_t)diff << 8) | (history & 0xFF);
            }
            for(int m=0; m<8; ++m) ctxs[50+m] = ((uint64_t)recent_words[m] << 8) | (history & 0xFF);
            for(int m=0; m<10; ++m) ctxs[58+m] = history & masks[m];

            for (int m = 0; m < 68; ++m) {
                stretched[m] = stretch(models[m].predict(ctxs[m]));
            }
            
            int p1 = mixer.mix(stretched, 68);
            enc.encode(bit, p1);
            mixer.update(bit, stretched, 68);
            
            for (int m = 0; m < 68; ++m) {
                models[m].update(ctxs[m], bit);
            }
            history = (history << 1) | bit;
        }
        
        if (byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t' || byte == '.' || byte == ',') {
            for (int w = 15; w > 0; --w) recent_words[w] = recent_words[w - 1];
            recent_words[0] = current_word;
            word_hash = (word_hash * 31) ^ current_word;
            current_word = 0;
        } else {
            current_word = (current_word * 31) + byte;
        }
        
        last_seen[last_bytes[0]] = byte;
        for (int m = 31; m > 0; --m) last_bytes[m] = last_bytes[m - 1];
        last_bytes[0] = byte;
    }
    return enc.finish();
}

std::vector<uint8_t> cmix_decode(const uint8_t* coded, size_t coded_len, size_t sym_count) {
    BinRangeDecoder dec;
    dec.init(coded, coded_len);
    
    int num_models = 68;
    Mixer mixer(num_models);
    
    std::vector<ContextModel> models;
    models.reserve(num_models);
    models.emplace_back(0, 10); 
    for (int i = 1; i <= 8; i++) models.emplace_back(i * 8, 22);
    for (int i = 1; i <= 7; i++) models.emplace_back(i, 20);
    models.emplace_back(64, 22);
    models.emplace_back(64, 20);
    for (int i = 1; i <= 16; i++) models.emplace_back(64, 18);
    for (int i = 1; i <= 16; i++) models.emplace_back(64, 18);
    for (int i = 1; i <= 8; i++) models.emplace_back(64, 19);
    for (int i = 1; i <= 10; i++) models.emplace_back(64, 18);
    
    std::vector<uint8_t> out;
    out.reserve(sym_count);
    
    uint64_t history = 0;
    uint32_t word_hash = 0;
    uint32_t current_word = 0;
    std::vector<uint8_t> last_bytes(32, 0);
    std::vector<uint32_t> recent_words(16, 0);
    std::vector<uint8_t> last_seen(256, 0);
    
    float stretched[68];
    uint64_t ctxs[68];
    uint64_t masks[10] = {
        0x5555555555555555ULL, 0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL,
        0xFEFEFEFEFEFEFEFEULL, 0x7F7F7F7F7F7F7F7FULL, 0xAAAAAAAAAAAAAAAAULL,
        0xCCCCCCCCCCCCCCCCULL
    };

    for (size_t i = 0; i < sym_count; ++i) {
        uint8_t byte = 0;
        
        for (int b = 7; b >= 0; --b) {
            ctxs[0] = 0;
            for(int m=1; m<=15; ++m) ctxs[m] = history;
            ctxs[16] = ((uint64_t)word_hash << 8) | (history & 0xFF);
            ctxs[17] = ((uint64_t)last_bytes[1] << 16) | ((uint64_t)last_seen[last_bytes[1]] << 8) | (history & 0xFF);
            
            for(int m=0; m<16; ++m) ctxs[18+m] = ((uint64_t)last_bytes[m] << 8) | (history & 0xFF);
            for(int m=0; m<16; ++m) {
                uint8_t diff = last_bytes[0] - last_bytes[m+1];
                ctxs[34+m] = ((uint64_t)diff << 8) | (history & 0xFF);
            }
            for(int m=0; m<8; ++m) ctxs[50+m] = ((uint64_t)recent_words[m] << 8) | (history & 0xFF);
            for(int m=0; m<10; ++m) ctxs[58+m] = history & masks[m];

            for (int m = 0; m < 68; ++m) {
                stretched[m] = stretch(models[m].predict(ctxs[m]));
            }
            
            int p1 = mixer.mix(stretched, 68);
            int bit = dec.decode(p1);
            mixer.update(bit, stretched, 68);
            
            for (int m = 0; m < 68; ++m) {
                models[m].update(ctxs[m], bit);
            }
            
            history = (history << 1) | bit;
            byte |= (bit << b);
        }
        
        if (byte == ' ' || byte == '\n' || byte == '\r' || byte == '\t' || byte == '.' || byte == ',') {
            for (int w = 15; w > 0; --w) recent_words[w] = recent_words[w - 1];
            recent_words[0] = current_word;
            word_hash = (word_hash * 31) ^ current_word;
            current_word = 0;
        } else {
            current_word = (current_word * 31) + byte;
        }
        
        last_seen[last_bytes[0]] = byte;
        for (int m = 31; m > 0; --m) last_bytes[m] = last_bytes[m - 1];
        last_bytes[0] = byte;
        out.push_back(byte);
    }
    return out;
}
