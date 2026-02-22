#include "cmix.h"
#include <cmath>
#include <vector>
#include <fstream>
#include <string>
#include <cstdint>

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

Mixer::Mixer(int num_models) : global_weights(num_models, 0.0f), niche_weights(num_models * 256, 0.0f), last_p1(0.5f) {}

int Mixer::mix(const std::vector<float>& stretched_preds, const std::vector<int>& active_models, uint8_t last_byte) {
    float dot = 0.0f;
    int n = active_models.size();
    #pragma GCC ivdep
    #pragma GCC unroll 8
    for (int i = 0; i < n; ++i) {
        int m = active_models[i];
        float combined_weight = global_weights[m] * 0.3f + niche_weights[m * 256 + last_byte] * 0.7f;
        dot += combined_weight * stretched_preds[m];
    }
    last_p1 = squash(dot);
    // Convert float probability [0, 1] to a 12-bit integer [1, 4095]
    int p = (int)(last_p1 * 4096.0f);
    if (p < 1) p = 1;
    if (p > 4095) p = 4095;
    return p;
}

void Mixer::update(int actual_bit, const std::vector<float>& stretched_preds, const std::vector<int>& active_models, uint8_t last_byte) {
    // Dynamic learning rate: learn faster on chaotic data (p1 near 0.5)
    // Slower on highly confident data (p1 near 0.0 or 1.0)
    float confidence = std::abs(last_p1 - 0.5f);
    float lr = 0.008f - (confidence * 0.012f);
    if (lr < 0.001f) lr = 0.001f;

    float error = (float)actual_bit - last_p1;
    int n = active_models.size();
    #pragma GCC ivdep
    #pragma GCC unroll 8
    for (int i = 0; i < n; ++i) {
        int m = active_models[i];
        float update_val = lr * error * stretched_preds[m];
        global_weights[m] += update_val;
        niche_weights[m * 256 + last_byte] += update_val * 2.0f; // Niche expertise learns faster
    }
}

void Mixer::save_weights(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;  // Silently fail if can't save

    // Write global weights
    uint32_t n_global = global_weights.size();
    f.write((char*)&n_global, sizeof(n_global));
    f.write((char*)global_weights.data(), n_global * sizeof(float));

    // Write niche weights
    uint32_t n_niche = niche_weights.size();
    f.write((char*)&n_niche, sizeof(n_niche));
    f.write((char*)niche_weights.data(), n_niche * sizeof(float));
}

bool Mixer::load_weights(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;  // File doesn't exist yet

    // Read global weights
    uint32_t n_global;
    f.read((char*)&n_global, sizeof(n_global));
    if (n_global != global_weights.size()) return false;  // Size mismatch
    f.read((char*)global_weights.data(), n_global * sizeof(float));

    // Read niche weights
    uint32_t n_niche;
    f.read((char*)&n_niche, sizeof(n_niche));
    if (n_niche != niche_weights.size()) return false;
    f.read((char*)niche_weights.data(), n_niche * sizeof(float));

    return true;
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
struct GeneticDef {
    uint64_t mask;
    uint8_t seen_idx;
    uint8_t shift;
};

std::vector<uint8_t> cmix_encode(const uint8_t* data, size_t len, CmixProgressCb cb) {
    BinRangeEncoder enc;
    int num_models = 1046;
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
    
    std::vector<GeneticDef> genetics(num_models);
    uint64_t lcg_state = 0x1337BEEF;
    for(int i = 68; i < num_models; i++) {
        models.emplace_back(64, 10); // 978 custom models, 8KB max each dynamically
        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
        uint64_t m1 = lcg_state;
        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
        uint64_t m2 = lcg_state;
        
        genetics[i].mask = m1 ^ m2;
        genetics[i].seen_idx = (m1 >> 32) & 0xFF;
        genetics[i].shift = (m2 >> 32) & 0x0F;
    }
    
    std::vector<int> active_models(num_models);
    for(int i = 0; i < num_models; ++i) active_models[i] = i;

    uint64_t history = 0; 
    uint32_t word_hash = 0;
    uint32_t current_word = 0;
    std::vector<uint8_t> last_bytes(32, 0);
    std::vector<uint32_t> recent_words(16, 0);
    std::vector<uint8_t> last_seen(256, 0);
    
    std::vector<float> stretched(num_models, 0.0f);
    std::vector<uint64_t> ctxs(num_models, 0);
    uint64_t masks[10] = {
        0x5555555555555555ULL, 0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL,
        0xFEFEFEFEFEFEFEFEULL, 0x7F7F7F7F7F7F7F7FULL, 0xAAAAAAAAAAAAAAAAULL,
        0xCCCCCCCCCCCCCCCCULL
    };

    auto do_warmup = [&]() {
        const char* warmup = 
            " the and of to a in that is was he for it with as his on be at by i this had "
            "not are but from or have an they which one you were all her she there would "
            "their we him been has when who will more no out do so can up what about "
            "than into them could state only new time some could these two may then "
            "first any my like city over such body man much "
            "<html> <?xml <!DOCTYPE html> <head> <body> <div> <span> <a> <img> <p> </p> </div> "
            "function public private protected class struct int void char bool const "
            "if ( ) { } [ ] return true false null undefined let var const = == === "
            "!= !== > < >= <= && || ! + - * / % & | ^ ~ << >> "
            "0 1 2 3 4 5 6 7 8 9 00 11 22 33 44 55 66 77 88 99 0.0 1.0 "
            "\n \r\n \t , . ; : ' \" `";
            
        for (int wi=0; warmup[wi] != '\0'; ++wi) {
            uint8_t byte = warmup[wi];
            uint8_t act_lb = last_bytes[0];
            
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
                
                #pragma GCC unroll 8
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    if (m >= 68) ctxs[m] = history & genetics[m].mask;
                }
                #pragma GCC ivdep
                #pragma GCC unroll 8
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    stretched[m] = stretch(models[m].predict(ctxs[m]));
                }
                
                mixer.mix(stretched, active_models, act_lb); 
                mixer.update(bit, stretched, active_models, act_lb);
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    models[m].update(ctxs[m], bit);
                }
                history = (history << 1) | bit;
            }
            
            if (wi > 0 && (wi % 256) == 0 && active_models.size() > 68) {
                std::vector<int> elite; std::vector<int> dead; std::vector<int> next_active;
                next_active.reserve(num_models);
                for (int m : active_models) {
                    if (m < 68) { next_active.push_back(m); }
                    else if (std::abs(mixer.global_weights[m]) >= 0.001f) { next_active.push_back(m); elite.push_back(m); }
                    else { dead.push_back(m); }
                }
                if (!elite.empty() && !dead.empty()) {
                    int breed_count = std::min((int)dead.size(), (int)(active_models.size() * 0.1f));
                    if (breed_count == 0 && dead.size() > 0) breed_count = 1;
                    for (int b = 0; b < breed_count; ++b) {
                        int dead_m = dead[b];
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        int p1 = elite[lcg_state % elite.size()];
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        int p2 = elite[lcg_state % elite.size()];
                        uint64_t child_mask = (genetics[p1].mask & 0xFFFFFFFF00000000ULL) | (genetics[p2].mask & 0x00000000FFFFFFFFULL);
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        child_mask ^= (1ULL << (lcg_state % 64));
                        genetics[dead_m].mask = child_mask;
                        models[dead_m] = ContextModel(64, 10);
                        mixer.global_weights[dead_m] = 0.0f;
                        for (int xb = 0; xb < 256; ++xb) mixer.niche_weights[dead_m * 256 + xb] = 0.0f;
                        next_active.push_back(dead_m);
                    }
                }
                active_models = next_active;
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
    };
    do_warmup();

    for (size_t i = 0; i < len; ++i) {
        uint8_t byte = data[i];
        uint8_t act_lb = last_bytes[0];
        
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
            #pragma GCC unroll 8
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                if (m >= 68) ctxs[m] = history & genetics[m].mask;
            }

            #pragma GCC ivdep
            #pragma GCC unroll 8
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                stretched[m] = stretch(models[m].predict(ctxs[m]));
            }
            
            int p1 = mixer.mix(stretched, active_models, act_lb);
            enc.encode(bit, p1);
            mixer.update(bit, stretched, active_models, act_lb);
            
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                models[m].update(ctxs[m], bit);
            }
            history = (history << 1) | bit;
        }
        
        // Dynamic pruning and Evolutionary Breeding
        if (i > 0 && (i % 2048) == 0 && active_models.size() > 68) {
            std::vector<int> elite;
            std::vector<int> dead;
            std::vector<int> next_active;
            next_active.reserve(num_models);
            
            for (int m : active_models) {
                if (m < 68) {
                    next_active.push_back(m);
                } else if (std::abs(mixer.global_weights[m]) >= 0.001f) {
                    next_active.push_back(m);
                    elite.push_back(m);
                } else {
                    dead.push_back(m);
                }
            }
            
            // Breed new children to replace the dead!
            if (!elite.empty() && !dead.empty()) {
                int breed_count = std::min((int)dead.size(), (int)(active_models.size() * 0.1f));
                if (breed_count == 0 && dead.size() > 0) breed_count = 1;

                for (int b = 0; b < breed_count; ++b) {
                    int dead_m = dead[b];
                    
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    int p1 = elite[lcg_state % elite.size()];
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    int p2 = elite[lcg_state % elite.size()];
                    
                    // Crossover genetics from Elite Parents
                    uint64_t child_mask = (genetics[p1].mask & 0xFFFFFFFF00000000ULL) | (genetics[p2].mask & 0x00000000FFFFFFFFULL);
                    
                    // Mutation (Flip a random logic bit)
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    child_mask ^= (1ULL << (lcg_state % 64));

                    genetics[dead_m].mask = child_mask;
                    models[dead_m] = ContextModel(64, 10);
                    mixer.global_weights[dead_m] = 0.0f;
                    for (int xb = 0; xb < 256; ++xb) {
                        mixer.niche_weights[dead_m * 256 + xb] = 0.0f;
                    }
                    
                    next_active.push_back(dead_m);
                }
            }
            active_models = next_active;
        }

        if (cb && (i & 8191) == 0) cb(i, len, active_models.size()); // Report every 8KB instead of 256 bytes

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

std::vector<uint8_t> cmix_decode(const uint8_t* coded, size_t coded_len, size_t sym_count, CmixProgressCb cb) {
    BinRangeDecoder dec;
    dec.init(coded, coded_len);
    
    int num_models = 1046;
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
    
    std::vector<GeneticDef> genetics(num_models);
    uint64_t lcg_state = 0x1337BEEF;
    for(int i = 68; i < num_models; i++) {
        models.emplace_back(64, 10); 
        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
        uint64_t m1 = lcg_state;
        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
        uint64_t m2 = lcg_state;
        
        genetics[i].mask = m1 ^ m2;
        genetics[i].seen_idx = (m1 >> 32) & 0xFF;
        genetics[i].shift = (m2 >> 32) & 0x0F;
    }
    
    std::vector<int> active_models(num_models);
    for(int i = 0; i < num_models; ++i) active_models[i] = i;

    std::vector<uint8_t> out;
    out.reserve(sym_count);
    
    uint64_t history = 0;
    uint32_t word_hash = 0;
    uint32_t current_word = 0;
    std::vector<uint8_t> last_bytes(32, 0);
    std::vector<uint32_t> recent_words(16, 0);
    std::vector<uint8_t> last_seen(256, 0);
    
    std::vector<float> stretched(num_models, 0.0f);
    std::vector<uint64_t> ctxs(num_models, 0);
    uint64_t masks[10] = {
        0x5555555555555555ULL, 0x3333333333333333ULL, 0x0F0F0F0F0F0F0F0FULL,
        0x00FF00FF00FF00FFULL, 0x0000FFFF0000FFFFULL, 0x00000000FFFFFFFFULL,
        0xFEFEFEFEFEFEFEFEULL, 0x7F7F7F7F7F7F7F7FULL, 0xAAAAAAAAAAAAAAAAULL,
        0xCCCCCCCCCCCCCCCCULL
    };

    auto do_warmup = [&]() {
        const char* warmup = 
            " the and of to a in that is was he for it with as his on be at by i this had "
            "not are but from or have an they which one you were all her she there would "
            "their we him been has when who will more no out do so can up what about "
            "than into them could state only new time some could these two may then "
            "first any my like city over such body man much "
            "<html> <?xml <!DOCTYPE html> <head> <body> <div> <span> <a> <img> <p> </p> </div> "
            "function public private protected class struct int void char bool const "
            "if ( ) { } [ ] return true false null undefined let var const = == === "
            "!= !== > < >= <= && || ! + - * / % & | ^ ~ << >> "
            "0 1 2 3 4 5 6 7 8 9 00 11 22 33 44 55 66 77 88 99 0.0 1.0 "
            "\n \r\n \t , . ; : ' \" `";
            
        for (int wi=0; warmup[wi] != '\0'; ++wi) {
            uint8_t byte = warmup[wi];
            uint8_t act_lb = last_bytes[0];
            
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
                
                #pragma GCC unroll 8
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    if (m >= 68) ctxs[m] = history & genetics[m].mask;
                }
                #pragma GCC ivdep
                #pragma GCC unroll 8
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    stretched[m] = stretch(models[m].predict(ctxs[m]));
                }
                
                mixer.mix(stretched, active_models, act_lb); 
                mixer.update(bit, stretched, active_models, act_lb);
                for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                    int m = active_models[a_idx];
                    models[m].update(ctxs[m], bit);
                }
                history = (history << 1) | bit;
            }
            
            if (wi > 0 && (wi % 256) == 0 && active_models.size() > 68) {
                std::vector<int> elite; std::vector<int> dead; std::vector<int> next_active;
                next_active.reserve(num_models);
                for (int m : active_models) {
                    if (m < 68) { next_active.push_back(m); }
                    else if (std::abs(mixer.global_weights[m]) >= 0.001f) { next_active.push_back(m); elite.push_back(m); }
                    else { dead.push_back(m); }
                }
                if (!elite.empty() && !dead.empty()) {
                    int breed_count = std::min((int)dead.size(), (int)(active_models.size() * 0.1f));
                    if (breed_count == 0 && dead.size() > 0) breed_count = 1;
                    for (int b = 0; b < breed_count; ++b) {
                        int dead_m = dead[b];
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        int p1 = elite[lcg_state % elite.size()];
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        int p2 = elite[lcg_state % elite.size()];
                        uint64_t child_mask = (genetics[p1].mask & 0xFFFFFFFF00000000ULL) | (genetics[p2].mask & 0x00000000FFFFFFFFULL);
                        lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                        child_mask ^= (1ULL << (lcg_state % 64));
                        genetics[dead_m].mask = child_mask;
                        models[dead_m] = ContextModel(64, 10);
                        mixer.global_weights[dead_m] = 0.0f;
                        for (int xb = 0; xb < 256; ++xb) mixer.niche_weights[dead_m * 256 + xb] = 0.0f;
                        next_active.push_back(dead_m);
                    }
                }
                active_models = next_active;
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
    };
    do_warmup();

    for (size_t i = 0; i < sym_count; ++i) {
        uint8_t byte = 0;
        uint8_t act_lb = last_bytes[0];
        
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
            
            #pragma GCC unroll 8
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                if (m >= 68) ctxs[m] = history & genetics[m].mask;
            }

            #pragma GCC ivdep
            #pragma GCC unroll 8
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                stretched[m] = stretch(models[m].predict(ctxs[m]));
            }
            
            int p1 = mixer.mix(stretched, active_models, act_lb);
            int bit = dec.decode(p1);
            mixer.update(bit, stretched, active_models, act_lb);
            
            for(size_t a_idx=0; a_idx<active_models.size(); ++a_idx) {
                int m = active_models[a_idx];
                models[m].update(ctxs[m], bit);
            }
            
            history = (history << 1) | bit;
            byte |= (bit << b);
        }
        
        // Dynamic pruning and Evolutionary Breeding
        if (i > 0 && (i % 2048) == 0 && active_models.size() > 68) {
            std::vector<int> elite;
            std::vector<int> dead;
            std::vector<int> next_active;
            next_active.reserve(num_models);
            
            for (int m : active_models) {
                if (m < 68) {
                    next_active.push_back(m);
                } else if (std::abs(mixer.global_weights[m]) >= 0.001f) {
                    next_active.push_back(m);
                    elite.push_back(m);
                } else {
                    dead.push_back(m);
                }
            }
            
            // Breed new children to replace the dead!
            if (!elite.empty() && !dead.empty()) {
                int breed_count = std::min((int)dead.size(), (int)(active_models.size() * 0.1f));
                if (breed_count == 0 && dead.size() > 0) breed_count = 1;

                for (int b = 0; b < breed_count; ++b) {
                    int dead_m = dead[b];
                    
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    int p1 = elite[lcg_state % elite.size()];
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    int p2 = elite[lcg_state % elite.size()];
                    
                    // Crossover genetics from Elite Parents
                    uint64_t child_mask = (genetics[p1].mask & 0xFFFFFFFF00000000ULL) | (genetics[p2].mask & 0x00000000FFFFFFFFULL);
                    
                    // Mutation (Flip a random logic bit)
                    lcg_state = lcg_state * 6364136223846793005ULL + 1ULL;
                    child_mask ^= (1ULL << (lcg_state % 64));

                    genetics[dead_m].mask = child_mask;
                    models[dead_m] = ContextModel(64, 10);
                    mixer.global_weights[dead_m] = 0.0f;
                    for (int xb = 0; xb < 256; ++xb) {
                        mixer.niche_weights[dead_m * 256 + xb] = 0.0f;
                    }
                    
                    next_active.push_back(dead_m);
                }
            }
            active_models = next_active;
        }

        if (cb && (i & 8191) == 0) cb(i, sym_count, active_models.size());

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
