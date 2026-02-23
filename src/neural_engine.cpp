/*
 * Neural Engine — C++ Core for the Neural Brain
 * 
 * High-performance implementation of:
 * 1. Transformer (self-attention, multi-head, positional encoding)
 * 2. Math Engine (expression evaluation, statistics, entropy)
 * 3. Word Embeddings (vectors, similarity, analogy)
 * 4. Tokenizer (fast text processing)
 * 5. N-gram Language Model (predict next word)
 * 6. TF-IDF Retrieval (knowledge search)
 *
 * Compiles with: g++ -O3 -std=c++17 -o neural_engine neural_engine.cpp -lm
 * Or MSVC:       cl /O2 /std:c++17 neural_engine.cpp
 *
 * Usage:
 *   neural_engine train <text_file>         Train on text
 *   neural_engine ask <question>            Query knowledge
 *   neural_engine math <expression>         Evaluate math
 *   neural_engine embed <text>              Get text embedding
 *   neural_engine predict <context>         Predict next word
 *   neural_engine generate <prompt> [n]     Generate n words
 *   neural_engine similarity <t1> <t2>      Semantic similarity
 *   neural_engine stats                     Show model stats
 *   neural_engine attention <text>          Show attention map
 *
 * This engine is called by the Python server for heavy computation.
 * Same architecture as llama.cpp — C++ core, Python/REST frontend.
 */

#define _USE_MATH_DEFINES
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <ctime>
#include <numeric>
#include <random>
#include <functional>
#include <chrono>
#include <cassert>
#include <regex>

// Smart Brain integration
#ifdef INCLUDE_SMART_BRAIN
#include "knowledge_manager.h"
#endif

// Compression integration
#include "compressor.h"

// Advanced AI integration (Phases 13-17)
#include "word_tokenizer.h"
#include "word_ppm.h"
#include "embedding_trainer.h"
#include "rag_engine.h"
#include "conversation_memory.h"
#include "reasoning_engine.h"
#include "bpe_tokenizer.h"
#include "real_embeddings.h"
#include "mini_transformer.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

// =============================================================================
// Configuration
// =============================================================================

static const int D_MODEL      = 64;    // Embedding dimension
static const int NUM_HEADS     = 4;     // Attention heads
static const int D_K           = D_MODEL / NUM_HEADS; // Per-head dimension
static const int D_FF          = 128;   // Feed-forward hidden dim
static const int NUM_LAYERS    = 2;     // Transformer layers
static const int MAX_SEQ_LEN   = 128;   // Max sequence length
static const int MAX_VOCAB     = 50000; // Max vocabulary size
static const int EMBED_DIM     = 64;    // Word embedding dimension
static const int NGRAM_ORDER   = 4;     // N-gram order

// Random number generator
static std::mt19937 rng(42);

static float rand_normal(float mean = 0.0f, float std = 0.1f) {
    std::normal_distribution<float> dist(mean, std);
    return dist(rng);
}

static float rand_uniform() {
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(rng);
}

// =============================================================================
// 1. Tokenizer — Fast text processing
// =============================================================================

class Tokenizer {
public:
    std::unordered_map<std::string, int> word2idx;
    std::vector<std::string> idx2word;
    std::unordered_map<std::string, int> word_counts;
    int vocab_size = 0;

    // Stop words for TF-IDF
    std::unordered_set<std::string> stop_words;

    Tokenizer() {
        // Special tokens
        add_word("<pad>");
        add_word("<unk>");
        add_word("<s>");
        add_word("</s>");

        // Common English stop words
       // Extended English stop words list
const char* sw[] = {
    "a","about","above","after","again","against","all","am","an","and","any","are","aren't","as","at",
    "be","because","been","before","being","below","between","both","but","by",
    "can","can't","cannot","could","couldn't",
    "did","didn't","do","does","doesn't","doing","don't","down","during",
    "each",
    "few","for","from","further",
    "had","hadn't","has","hasn't","have","haven't","having","he","he'd","he'll","he's",
    "her","here","here's","hers","herself","him","himself","his","how","how's",
    "i","i'd","i'll","i'm","i've","if","in","into","is","isn't","it","it's","its","itself",
    "just",
    "me","more","most","mustn't","my","myself",
    "no","nor","not",
    "of","off","on","once","only","or","other","our","ours","ourselves","out","over","own",
    "same","she","she'd","she'll","she's","should","shouldn't","so","some","such",
    "than","that","that's","the","their","theirs","them","themselves","then","there","there's",
    "these","they","they'd","they'll","they're","they've","this","those","through","to","too",
    "under","until","up",
    "very",
    "was","wasn't","we","we'd","we'll","we're","we've","were","weren't","what","what's",
    "when","when's","where","where's","which","while","who","who's","whom","why","why's",
    "with","won't","would","wouldn't",
    "you","you'd","you'll","you're","you've","your","yours","yourself","yourselves",
    nullptr
};
        for (int i = 0; sw[i]; i++) stop_words.insert(sw[i]);
    }

    int add_word(const std::string& word) {
        if (word2idx.count(word)) return word2idx[word];
        int idx = vocab_size++;
        word2idx[word] = idx;
        idx2word.push_back(word);
        return idx;
    }

    int get_idx(const std::string& word) const {
        auto it = word2idx.find(word);
        return it != word2idx.end() ? it->second : 1; // 1 = <unk>
    }

    std::vector<std::string> tokenize(const std::string& text) const {
        std::vector<std::string> tokens;
        std::string lower;
        lower.reserve(text.size());

        for (char c : text) {
            if (c >= 'A' && c <= 'Z') lower += (c + 32);
            else if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == ' ')
                lower += c;
            else lower += ' ';
        }

        std::istringstream iss(lower);
        std::string word;
        while (iss >> word) {
            if (word.size() > 1) tokens.push_back(word);
        }
        return tokens;
    }

    std::vector<std::string> tokenize_no_stop(const std::string& text) const {
        auto tokens = tokenize(text);
        std::vector<std::string> filtered;
        for (auto& t : tokens) {
            if (!stop_words.count(t)) filtered.push_back(t);
        }
        return filtered;
    }

    std::vector<int> encode(const std::string& text) {
        auto tokens = tokenize(text);
        std::vector<int> ids;
        for (auto& t : tokens) {
            ids.push_back(get_idx(t));
        }
        return ids;
    }

    // Ensure all words in text are in vocabulary
    void build_vocab(const std::string& text) {
        auto tokens = tokenize(text);
        for (auto& t : tokens) {
            add_word(t);
            word_counts[t]++;
        }
    }
};

// =============================================================================
// 2. Matrix Operations — The foundation of neural networks
// =============================================================================

// Row-major matrix stored as flat vector
struct Matrix {
    std::vector<float> data;
    int rows, cols;

    Matrix() : rows(0), cols(0) {}
    Matrix(int r, int c) : rows(r), cols(c), data(r * c, 0.0f) {}

    float& at(int r, int c) { return data[r * cols + c]; }
    float at(int r, int c) const { return data[r * cols + c]; }

    void randomize(float scale = 0.1f) {
        float s = scale / std::sqrt((float)cols);
        for (auto& v : data) v = rand_normal(0, s);
    }

    void zero() { std::fill(data.begin(), data.end(), 0.0f); }
};

// Matrix multiply: (M x K) @ (K x N) -> (M x N)
Matrix matmul(const Matrix& a, const Matrix& b) {
    assert(a.cols == b.rows);
    Matrix c(a.rows, b.cols);
    for (int i = 0; i < a.rows; i++) {
        for (int k = 0; k < a.cols; k++) {
            float aik = a.at(i, k);
            if (aik == 0.0f) continue;
            for (int j = 0; j < b.cols; j++) {
                c.at(i, j) += aik * b.at(k, j);
            }
        }
    }
    return c;
}

Matrix transpose(const Matrix& m) {
    Matrix t(m.cols, m.rows);
    for (int i = 0; i < m.rows; i++)
        for (int j = 0; j < m.cols; j++)
            t.at(j, i) = m.at(i, j);
    return t;
}

// Softmax over a row
void softmax_row(float* data, int n) {
    float max_val = *std::max_element(data, data + n);
    float sum = 0;
    for (int i = 0; i < n; i++) {
        data[i] = std::exp(data[i] - max_val);
        sum += data[i];
    }
    if (sum > 0) for (int i = 0; i < n; i++) data[i] /= sum;
}

// GELU activation
float gelu(float x) {
    return 0.5f * x * (1.0f + std::tanh(std::sqrt(2.0f / M_PI) * (x + 0.044715f * x * x * x)));
}

// Layer normalization
void layer_norm(float* data, const float* gamma, const float* beta, int n) {
    float mean = 0, var = 0;
    for (int i = 0; i < n; i++) mean += data[i];
    mean /= n;
    for (int i = 0; i < n; i++) var += (data[i] - mean) * (data[i] - mean);
    var /= n;
    float std_inv = 1.0f / std::sqrt(var + 1e-5f);
    for (int i = 0; i < n; i++)
        data[i] = (data[i] - mean) * std_inv * gamma[i] + beta[i];
}

float dot_product(const float* a, const float* b, int n) {
    float sum = 0;
    for (int i = 0; i < n; i++) sum += a[i] * b[i];
    return sum;
}

float cosine_sim(const float* a, const float* b, int n) {
    float dot = 0, ma = 0, mb = 0;
    for (int i = 0; i < n; i++) {
        dot += a[i] * b[i];
        ma += a[i] * a[i];
        mb += b[i] * b[i];
    }
    ma = std::sqrt(ma);
    mb = std::sqrt(mb);
    return (ma > 0 && mb > 0) ? dot / (ma * mb) : 0.0f;
}

// =============================================================================
// 3. Positional Encoding — sin/cos position embeddings
// =============================================================================

Matrix positional_encoding(int seq_len, int d_model) {
    Matrix pe(seq_len, d_model);
    for (int pos = 0; pos < seq_len; pos++) {
        for (int i = 0; i < d_model; i += 2) {
            float div = std::pow(10000.0f, (float)(2 * i) / d_model);
            pe.at(pos, i) = std::sin(pos / div);
            if (i + 1 < d_model)
                pe.at(pos, i + 1) = std::cos(pos / div);
        }
    }
    return pe;
}

// =============================================================================
// 4. Self-Attention
// =============================================================================

struct AttentionHead {
    Matrix W_q, W_k, W_v;  // (d_model x d_k)
    float scale;

    AttentionHead() {}
    AttentionHead(int d_model, int d_k) : scale(std::sqrt((float)d_k)) {
        W_q = Matrix(d_model, d_k); W_q.randomize();
        W_k = Matrix(d_model, d_k); W_k.randomize();
        W_v = Matrix(d_model, d_k); W_v.randomize();
    }

    // Forward: x (seq x d_model) -> output (seq x d_k), weights (seq x seq)
    std::pair<Matrix, Matrix> forward(const Matrix& x) {
        Matrix Q = matmul(x, W_q);
        Matrix K = matmul(x, W_k);
        Matrix V = matmul(x, W_v);

        Matrix K_t = transpose(K);
        Matrix scores = matmul(Q, K_t);

        int seq = x.rows;
        for (int i = 0; i < seq; i++) {
            for (int j = 0; j < seq; j++)
                scores.at(i, j) /= scale;
            softmax_row(&scores.data[i * seq], seq);
        }

        Matrix output = matmul(scores, V);
        return {output, scores};
    }
};

// =============================================================================
// 5. Multi-Head Attention
// =============================================================================

struct MultiHeadAttention {
    std::vector<AttentionHead> heads;
    Matrix W_o;  // (num_heads * d_k x d_model)
    int num_heads, d_k;

    MultiHeadAttention() {}
    MultiHeadAttention(int d_model, int nh) : num_heads(nh), d_k(d_model / nh) {
        for (int i = 0; i < nh; i++)
            heads.emplace_back(d_model, d_k);
        W_o = Matrix(nh * d_k, d_model);
        W_o.randomize();
    }

    Matrix forward(const Matrix& x) {
        int seq = x.rows;
        Matrix concat(seq, num_heads * d_k);

        for (int h = 0; h < num_heads; h++) {
            auto [out, _] = heads[h].forward(x);
            for (int i = 0; i < seq; i++)
                for (int j = 0; j < d_k; j++)
                    concat.at(i, h * d_k + j) = out.at(i, j);
        }

        return matmul(concat, W_o);
    }
};

// =============================================================================
// 6. Feed-Forward Network
// =============================================================================

struct FeedForward {
    Matrix W1, W2;
    std::vector<float> b1, b2;

    FeedForward() {}
    FeedForward(int d_model, int d_ff) {
        W1 = Matrix(d_model, d_ff); W1.randomize();
        W2 = Matrix(d_ff, d_model); W2.randomize();
        b1.resize(d_ff, 0.0f);
        b2.resize(d_model, 0.0f);
    }

    Matrix forward(const Matrix& x) {
        int seq = x.rows, d_ff = W1.cols, d_model = W2.cols;
        Matrix hidden = matmul(x, W1);
        for (int i = 0; i < seq; i++)
            for (int j = 0; j < d_ff; j++)
                hidden.at(i, j) = gelu(hidden.at(i, j) + b1[j]);
        Matrix out = matmul(hidden, W2);
        for (int i = 0; i < seq; i++)
            for (int j = 0; j < d_model; j++)
                out.at(i, j) += b2[j];
        return out;
    }
};

// =============================================================================
// 7. Transformer Block
// =============================================================================

struct TransformerBlock {
    MultiHeadAttention attn;
    FeedForward ffn;
    std::vector<float> ln1_g, ln1_b, ln2_g, ln2_b;

    TransformerBlock() {}
    TransformerBlock(int d_model, int num_heads, int d_ff) :
        attn(d_model, num_heads), ffn(d_model, d_ff)
    {
        ln1_g.resize(d_model, 1.0f);
        ln1_b.resize(d_model, 0.0f);
        ln2_g.resize(d_model, 1.0f);
        ln2_b.resize(d_model, 0.0f);
    }

    Matrix forward(const Matrix& x) {
        int seq = x.rows, d = x.cols;

        // Attention + residual + layer norm
        Matrix attn_out = attn.forward(x);
        Matrix res1(seq, d);
        for (int i = 0; i < seq; i++) {
            for (int j = 0; j < d; j++)
                res1.at(i, j) = x.at(i, j) + attn_out.at(i, j);
            layer_norm(&res1.data[i * d], ln1_g.data(), ln1_b.data(), d);
        }

        // FFN + residual + layer norm
        Matrix ffn_out = ffn.forward(res1);
        Matrix res2(seq, d);
        for (int i = 0; i < seq; i++) {
            for (int j = 0; j < d; j++)
                res2.at(i, j) = res1.at(i, j) + ffn_out.at(i, j);
            layer_norm(&res2.data[i * d], ln2_g.data(), ln2_b.data(), d);
        }

        return res2;
    }
};

// =============================================================================
// 8. The Transformer Model
// =============================================================================

class Transformer {
public:
    Tokenizer tokenizer;
    Matrix pos_encoding;
    std::vector<Matrix> token_embeddings; // vocab_size vectors of d_model
    std::vector<TransformerBlock> layers;
    int d_model, training_steps;

    Transformer() : d_model(D_MODEL), training_steps(0) {
        pos_encoding = positional_encoding(MAX_SEQ_LEN, D_MODEL);
        for (int i = 0; i < NUM_LAYERS; i++)
            layers.emplace_back(D_MODEL, NUM_HEADS, D_FF);
    }

    void ensure_embedding(int idx) {
        while ((int)token_embeddings.size() <= idx) {
            Matrix emb(1, d_model);
            emb.randomize();
            token_embeddings.push_back(emb);
        }
    }

    // Get embeddings for token ids + positional encoding
    Matrix get_embeddings(const std::vector<int>& ids) {
        int seq = std::min((int)ids.size(), MAX_SEQ_LEN);
        Matrix x(seq, d_model);
        for (int i = 0; i < seq; i++) {
            ensure_embedding(ids[i]);
            for (int j = 0; j < d_model; j++)
                x.at(i, j) = token_embeddings[ids[i]].at(0, j) + pos_encoding.at(i, j);
        }
        return x;
    }

    // Full forward pass
    Matrix forward(const std::string& text) {
        tokenizer.build_vocab(text);
        auto tokens = tokenizer.tokenize(text);
        if (tokens.empty()) return Matrix(0, 0);

        std::vector<int> ids;
        for (auto& t : tokens) ids.push_back(tokenizer.get_idx(t));

        Matrix x = get_embeddings(ids);
        for (auto& layer : layers) x = layer.forward(x);
        return x;
    }

    // Encode text to a single vector (mean pooling)
    std::vector<float> encode(const std::string& text) {
        Matrix out = forward(text);
        std::vector<float> vec(d_model, 0.0f);
        if (out.rows == 0) return vec;
        for (int i = 0; i < out.rows; i++)
            for (int j = 0; j < d_model; j++)
                vec[j] += out.at(i, j);
        for (auto& v : vec) v /= out.rows;
        return vec;
    }

    float similarity(const std::string& t1, const std::string& t2) {
        auto v1 = encode(t1);
        auto v2 = encode(t2);
        return cosine_sim(v1.data(), v2.data(), d_model);
    }

    // Train on text — processes in chunks for large files
    void train(const std::string& text, float lr = 0.001f) {
        tokenizer.build_vocab(text);
        auto tokens = tokenizer.tokenize(text);
        if (tokens.size() < 3) return;

        // Process in chunks of MAX_SEQ_LEN to avoid O(n^3) explosion
        int chunk_size = std::min((int)tokens.size(), MAX_SEQ_LEN);
        
        for (int start = 0; start < (int)tokens.size(); start += chunk_size / 2) {
            int end = std::min(start + chunk_size, (int)tokens.size());
            if (end - start < 3) break;
            
            std::vector<int> ids;
            for (int i = start; i < end; i++)
                ids.push_back(tokenizer.get_idx(tokens[i]));

            Matrix x = get_embeddings(ids);
            for (auto& layer : layers) x = layer.forward(x);

            // Next-token prediction training
            for (int i = 0; i < (int)ids.size() - 1; i++) {
                int target = ids[i + 1];
                ensure_embedding(target);
                for (int d = 0; d < d_model; d++) {
                    float grad = (x.at(i, d) - token_embeddings[target].at(0, d)) * lr;
                    token_embeddings[target].at(0, d) += grad * 0.5f;
                }
            }
            training_steps++;
        }
    }

    // Predict next word
    std::vector<std::pair<std::string, float>> predict_next(const std::string& text, int top_k = 10) {
        Matrix out = forward(text);
        if (out.rows == 0) return {};

        float* last = &out.data[(out.rows - 1) * d_model];
        std::vector<std::pair<float, int>> scores;

        for (int i = 4; i < tokenizer.vocab_size; i++) { // Skip special tokens
            ensure_embedding(i);
            float score = dot_product(last, token_embeddings[i].data.data(), d_model);
            scores.push_back({score, i});
        }

        std::sort(scores.begin(), scores.end(), [](auto& a, auto& b) { return a.first > b.first; });

        // Softmax over top scores
        std::vector<std::pair<std::string, float>> result;
        if (!scores.empty()) {
            float max_s = scores[0].first;
            float sum = 0;
            int n = std::min(top_k, (int)scores.size());
            for (int i = 0; i < n; i++) sum += std::exp(scores[i].first - max_s);
            for (int i = 0; i < n; i++) {
                float prob = std::exp(scores[i].first - max_s) / sum;
                result.push_back({tokenizer.idx2word[scores[i].second], prob});
            }
        }
        return result;
    }

    // Generate text
    std::string generate(const std::string& prompt, int max_tokens = 30, float temp = 0.7f) {
        auto tokens = tokenizer.tokenize(prompt);
        std::vector<std::string> generated;

        for (int t = 0; t < max_tokens; t++) {
            std::string ctx;
            int start = std::max(0, (int)tokens.size() - 20);
            for (int i = start; i < (int)tokens.size(); i++) {
                if (!ctx.empty()) ctx += " ";
                ctx += tokens[i];
            }

            auto preds = predict_next(ctx, 20);
            if (preds.empty()) break;

            // Temperature sampling
            std::string next;
            if (temp <= 0.01f) {
                next = preds[0].first;
            } else {
                float sum = 0;
                std::vector<float> adjusted;
                for (auto& [w, p] : preds) {
                    float ap = std::pow(p, 1.0f / temp);
                    adjusted.push_back(ap);
                    sum += ap;
                }
                float r = rand_uniform() * sum;
                float cumsum = 0;
                next = preds.back().first;
                for (int i = 0; i < (int)preds.size(); i++) {
                    cumsum += adjusted[i];
                    if (r <= cumsum) { next = preds[i].first; break; }
                }
            }

            tokens.push_back(next);
            generated.push_back(next);
        }

        std::string result;
        for (auto& w : generated) { if (!result.empty()) result += " "; result += w; }
        return result;
    }

    int param_count() const {
        int count = tokenizer.vocab_size * d_model; // Embeddings
        int per_layer = NUM_HEADS * 3 * d_model * D_K + NUM_HEADS * D_K * d_model
                      + d_model * D_FF + D_FF + D_FF * d_model + d_model + 4 * d_model;
        count += per_layer * NUM_LAYERS;
        return count;
    }
};

// =============================================================================
// 9. Math Engine
// =============================================================================

// Recursive descent parser for safe expression evaluation
class MathParser {
    std::string expr;
    size_t pos;

public:
    MathParser(const std::string& e) : expr(e), pos(0) {}

    double parse() {
        double result = addition();
        return result;
    }

private:
    void skip_ws() { while (pos < expr.size() && expr[pos] == ' ') pos++; }

    double addition() {
        double left = multiplication();
        while (pos < expr.size()) {
            skip_ws();
            if (pos < expr.size() && (expr[pos] == '+' || expr[pos] == '-')) {
                char op = expr[pos++];
                double right = multiplication();
                left = (op == '+') ? left + right : left - right;
            } else break;
        }
        return left;
    }

    double multiplication() {
        double left = power();
        while (pos < expr.size()) {
            skip_ws();
            if (pos < expr.size() && (expr[pos] == '*' || expr[pos] == '/' || expr[pos] == '%')) {
                char op = expr[pos++];
                double right = power();
                if (op == '*') left *= right;
                else if (op == '/') left = (right != 0) ? left / right : 0;
                else left = std::fmod(left, right);
            } else break;
        }
        return left;
    }

    double power() {
        double base = unary();
        skip_ws();
        if (pos < expr.size() && (expr[pos] == '^')) {
            pos++;
            double exp = unary();
            return std::pow(base, exp);
        }
        if (pos + 1 < expr.size() && expr[pos] == '*' && expr[pos+1] == '*') {
            pos += 2;
            double exp = unary();
            return std::pow(base, exp);
        }
        return base;
    }

    double unary() {
        skip_ws();
        if (pos < expr.size() && expr[pos] == '-') { pos++; return -unary(); }
        if (pos < expr.size() && expr[pos] == '+') { pos++; return unary(); }
        return atom();
    }

    double atom() {
        skip_ws();
        if (pos < expr.size() && expr[pos] == '(') {
            pos++;
            double result = addition();
            skip_ws();
            if (pos < expr.size() && expr[pos] == ')') pos++;
            return result;
        }

        // Functions
        std::string funcs[] = {"sqrt","log2","log10","log","sin","cos","tan","exp","abs","ceil","floor","factorial"};
        for (auto& fn : funcs) {
            if (expr.substr(pos, fn.size()) == fn && pos + fn.size() < expr.size() && expr[pos + fn.size()] == '(') {
                pos += fn.size() + 1;
                double arg = addition();
                skip_ws();
                if (pos < expr.size() && expr[pos] == ')') pos++;
                if (fn == "sqrt") return std::sqrt(arg);
                if (fn == "log2") return std::log2(arg);
                if (fn == "log10") return std::log10(arg);
                if (fn == "log") return std::log(arg);
                if (fn == "sin") return std::sin(arg);
                if (fn == "cos") return std::cos(arg);
                if (fn == "tan") return std::tan(arg);
                if (fn == "exp") return std::exp(arg);
                if (fn == "abs") return std::abs(arg);
                if (fn == "ceil") return std::ceil(arg);
                if (fn == "floor") return std::floor(arg);
                if (fn == "factorial") {
                    double r = 1; for (int i = 2; i <= (int)arg; i++) r *= i; return r;
                }
            }
        }

        // Constants
        if (expr.substr(pos, 2) == "pi") { pos += 2; return M_PI; }
        if (expr.substr(pos, 1) == "e" && (pos + 1 >= expr.size() || !isalpha(expr[pos+1]))) { pos++; return M_E; }

        // Number
        size_t start = pos;
        while (pos < expr.size() && (isdigit(expr[pos]) || expr[pos] == '.')) pos++;
        if (pos > start) return std::stod(expr.substr(start, pos - start));

        return 0;
    }
};

double math_eval(const std::string& expr) {
    MathParser parser(expr);
    return parser.parse();
}

// Shannon entropy
double shannon_entropy(const std::vector<double>& probs) {
    double h = 0;
    for (double p : probs) {
        if (p > 0) h -= p * std::log2(p);
    }
    return h;
}

// Byte entropy
double byte_entropy(const std::vector<uint8_t>& data) {
    if (data.empty()) return 0;
    int counts[256] = {};
    for (uint8_t b : data) counts[b]++;
    std::vector<double> probs;
    for (int i = 0; i < 256; i++) {
        if (counts[i] > 0) probs.push_back((double)counts[i] / data.size());
    }
    return shannon_entropy(probs);
}

// Statistics
struct Stats {
    double mean, median, std_dev, variance, min_val, max_val;
    int count;
};

Stats compute_stats(std::vector<double> vals) {
    Stats s = {};
    if (vals.empty()) return s;
    s.count = vals.size();
    std::sort(vals.begin(), vals.end());
    s.min_val = vals.front();
    s.max_val = vals.back();
    s.mean = std::accumulate(vals.begin(), vals.end(), 0.0) / s.count;
    s.median = (s.count % 2) ? vals[s.count/2] : (vals[s.count/2-1] + vals[s.count/2]) / 2.0;
    s.variance = 0;
    for (double v : vals) s.variance += (v - s.mean) * (v - s.mean);
    s.variance /= std::max(s.count - 1, 1);
    s.std_dev = std::sqrt(s.variance);
    return s;
}

// =============================================================================
// 10. N-gram Language Model — Predict next word from context
// =============================================================================

class NGramModel {
public:
    // ngrams[context_key] = counter of next words
    std::unordered_map<std::string, std::unordered_map<std::string, int>> bigrams;
    std::unordered_map<std::string, std::unordered_map<std::string, int>> trigrams;
    int total_words = 0;

    void train(const std::string& text) {
        Tokenizer tok;
        auto words = tok.tokenize(text);
        total_words += words.size();

        for (int i = 0; i < (int)words.size() - 1; i++) {
            bigrams[words[i]][words[i+1]]++;
            if (i + 2 < (int)words.size()) {
                std::string ctx = words[i] + " " + words[i+1];
                trigrams[ctx][words[i+2]]++;
            }
        }
    }

    std::vector<std::pair<std::string, float>> predict(const std::string& context, int top_k = 10) {
        Tokenizer tok;
        auto words = tok.tokenize(context);
        std::unordered_map<std::string, float> scores;

        // Trigram lookup
        if (words.size() >= 2) {
            std::string ctx = words[words.size()-2] + " " + words[words.size()-1];
            if (trigrams.count(ctx)) {
                int total = 0;
                for (auto& [w, c] : trigrams[ctx]) total += c;
                for (auto& [w, c] : trigrams[ctx])
                    scores[w] = std::max(scores[w], (float)c / total * 0.8f);
            }
        }

        // Bigram lookup
        if (!words.empty()) {
            auto& last = words.back();
            if (bigrams.count(last)) {
                int total = 0;
                for (auto& [w, c] : bigrams[last]) total += c;
                for (auto& [w, c] : bigrams[last])
                    scores[w] = std::max(scores[w], (float)c / total * 0.5f);
            }
        }

        std::vector<std::pair<std::string, float>> result(scores.begin(), scores.end());
        std::sort(result.begin(), result.end(), [](auto& a, auto& b) { return a.second > b.second; });
        if ((int)result.size() > top_k) result.resize(top_k);
        return result;
    }
};

// =============================================================================
// 11. Word Embeddings — Word analogy & similarity
// =============================================================================

class WordEmbeddings {
public:
    std::unordered_map<std::string, std::vector<float>> vectors;
    int dim;

    WordEmbeddings(int d = EMBED_DIM) : dim(d) {}

    void train(const std::string& text, int window = 3, float lr = 0.02f) {
        Tokenizer tok;
        auto words = tok.tokenize(text);

        // Initialize vectors for new words
        for (auto& w : words) {
            if (!vectors.count(w)) {
                vectors[w].resize(dim);
                for (auto& v : vectors[w]) v = rand_normal(0, 0.1f);
            }
        }

        // Skip-gram: push co-occurring words' vectors together
        for (int i = 0; i < (int)words.size(); i++) {
            int start = std::max(0, i - window);
            int end = std::min((int)words.size(), i + window + 1);
            for (int j = start; j < end; j++) {
                if (j == i) continue;
                auto& cv = vectors[words[i]];
                auto& xv = vectors[words[j]];
                float sim = cosine_sim(cv.data(), xv.data(), dim);
                float err = (1.0f - sim) * lr;
                for (int d = 0; d < dim; d++) {
                    cv[d] += xv[d] * err * 0.05f;
                    xv[d] += cv[d] * err * 0.05f;
                }
            }
        }
    }

    // Find most similar words
    std::vector<std::pair<std::string, float>> most_similar(const std::string& word, int top_n = 10) {
        Tokenizer tok;
        std::string w = tok.tokenize(word).empty() ? word : tok.tokenize(word)[0];
        if (!vectors.count(w)) return {};
        auto& target = vectors[w];

        std::vector<std::pair<std::string, float>> result;
        for (auto& [other, vec] : vectors) {
            if (other == w) continue;
            float sim = cosine_sim(target.data(), vec.data(), dim);
            result.push_back({other, sim});
        }
        std::sort(result.begin(), result.end(), [](auto& a, auto& b) { return a.second > b.second; });
        if ((int)result.size() > top_n) result.resize(top_n);
        return result;
    }

    // Word analogy: a is to b as c is to ? 
    // king - man + woman = queen
    std::vector<std::pair<std::string, float>> analogy(
        const std::string& a, const std::string& b, const std::string& c, int top_n = 5)
    {
        if (!vectors.count(a) || !vectors.count(b) || !vectors.count(c)) return {};
        
        // target = b - a + c
        std::vector<float> target(dim);
        for (int i = 0; i < dim; i++)
            target[i] = vectors[b][i] - vectors[a][i] + vectors[c][i];

        std::unordered_set<std::string> exclude = {a, b, c};
        std::vector<std::pair<std::string, float>> result;
        for (auto& [word, vec] : vectors) {
            if (exclude.count(word)) continue;
            float sim = cosine_sim(target.data(), vec.data(), dim);
            result.push_back({word, sim});
        }
        std::sort(result.begin(), result.end(), [](auto& x, auto& y) { return x.second > y.second; });
        if ((int)result.size() > top_n) result.resize(top_n);
        return result;
    }

    float text_similarity(const std::string& t1, const std::string& t2) {
        auto v1 = sentence_vec(t1);
        auto v2 = sentence_vec(t2);
        return cosine_sim(v1.data(), v2.data(), dim);
    }

    std::vector<float> sentence_vec(const std::string& text) {
        Tokenizer tok;
        auto words = tok.tokenize(text);
        std::vector<float> avg(dim, 0.0f);
        int count = 0;
        for (auto& w : words) {
            if (vectors.count(w)) {
                for (int i = 0; i < dim; i++) avg[i] += vectors[w][i];
                count++;
            }
        }
        if (count > 0) for (auto& v : avg) v /= count;
        return avg;
    }
};

// =============================================================================
// 12. Inference Engine — Chain facts for reasoning
// =============================================================================

struct Fact {
    std::string subject, relation, object;
};

class InferenceEngine {
public:
    std::vector<Fact> facts;

    void extract_facts(const std::string& text) {
        Tokenizer tok;
        // Split into sentences
        std::string current;
        for (char c : text) {
            if (c == '.' || c == '!' || c == '?' || c == '\n') {
                process_sentence(current);
                current.clear();
            } else {
                current += c;
            }
        }
        if (!current.empty()) process_sentence(current);
    }

    void process_sentence(const std::string& sent) {
        if (sent.size() < 10) return;
        std::string lower;
        for (char c : sent) lower += tolower(c);

        // "X is/are Y" pattern
        auto is_pos = lower.find(" is ");
        if (is_pos == std::string::npos) is_pos = lower.find(" are ");
        if (is_pos != std::string::npos) {
            std::string subj = sent.substr(0, is_pos);
            std::string obj = sent.substr(is_pos + 4);
            // Trim
            while (!subj.empty() && subj[0] == ' ') subj.erase(0, 1);
            while (!obj.empty() && obj.back() == ' ') obj.pop_back();
            if (subj.size() > 2 && subj.size() < 60 && obj.size() > 2 && obj.size() < 60) {
                facts.push_back({subj, "is", obj});
            }
        }

        // "X uses/has/contains Y"
        const char* verbs[] = {"uses", "has", "contains", "includes", "provides", "supports", nullptr};
        for (int v = 0; verbs[v]; v++) {
            std::string pattern = std::string(" ") + verbs[v] + " ";
            auto pos = lower.find(pattern);
            if (pos != std::string::npos) {
                std::string subj = sent.substr(0, pos);
                std::string obj = sent.substr(pos + pattern.size());
                while (!subj.empty() && subj[0] == ' ') subj.erase(0, 1);
                while (!obj.empty() && obj.back() == ' ') obj.pop_back();
                if (subj.size() > 2 && subj.size() < 60 && obj.size() > 2 && obj.size() < 60)
                    facts.push_back({subj, verbs[v], obj});
            }
        }
    }

    // Transitive inference: A is B, B is C → A is C
    std::vector<std::string> infer(const std::string& query) {
        std::vector<std::string> results;
        Tokenizer tok;
        auto qwords = tok.tokenize_no_stop(query);
        std::set<std::string> qset(qwords.begin(), qwords.end());

        for (size_t i = 0; i < facts.size(); i++) {
            for (size_t j = 0; j < facts.size(); j++) {
                if (i == j) continue;
                // Check if object of i matches subject of j (case-insensitive)
                std::string obj_i, subj_j;
                for (char c : facts[i].object) obj_i += tolower(c);
                for (char c : facts[j].subject) subj_j += tolower(c);
                if (obj_i.find(subj_j) != std::string::npos || subj_j.find(obj_i) != std::string::npos) {
                    std::string inf = facts[i].subject + " " + facts[i].relation + " " + facts[j].object
                                    + " (via " + facts[i].object + ")";
                    // Check relevance to query
                    auto inf_words = tok.tokenize_no_stop(inf);
                    std::set<std::string> iset(inf_words.begin(), inf_words.end());
                    std::vector<std::string> overlap;
                    for (auto& w : qset) if (iset.count(w)) overlap.push_back(w);
                    if (!overlap.empty()) results.push_back(inf);
                }
            }
            if (results.size() >= 5) break;
        }
        return results;
    }

    // Direct query
    std::vector<Fact> query(const std::string& about) {
        std::vector<Fact> result;
        Tokenizer tok;
        auto words = tok.tokenize_no_stop(about);
        for (auto& f : facts) {
            std::string combined = f.subject + " " + f.object;
            auto fwords = tok.tokenize_no_stop(combined);
            std::set<std::string> fset(fwords.begin(), fwords.end());
            for (auto& w : words) {
                if (fset.count(w)) { result.push_back(f); break; }
            }
        }
        return result;
    }
};

// =============================================================================
// 13. Main — CLI Interface
// =============================================================================

void print_json_string(const std::string& s) {
    std::cout << "\"";
    for (char c : s) {
        if (c == '"') std::cout << "\\\"";
        else if (c == '\\') std::cout << "\\\\";
        else if (c == '\n') std::cout << "\\n";
        else std::cout << c;
    }
    std::cout << "\"";
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cout << "{\"error\": \"Usage: neural_engine <command> [args]\"}" << std::endl;
        return 1;
    }

    std::string cmd = argv[1];
    Transformer tf;
    NGramModel ngram;
    WordEmbeddings embeddings;
    InferenceEngine inference;

    // Advanced AI components (Phases 13-17)
    static WordTokenizer word_tok;
    static WordPPM word_model;
    static EmbeddingTrainer advanced_embeddings(64);
    static RAGEngine rag;
    static ConversationMemory memory;
    static ReasoningEngine reasoner;

    if (cmd == "train" && argc >= 3) {
        // Train ALL engines on a text file
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\": \"Cannot open file\"}" << std::endl;
            return 1;
        }
        std::string text((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());

        auto start = std::chrono::high_resolution_clock::now();

        // Train transformer
        tf.train(text);
        // Train n-gram model
        ngram.train(text);
        // Train word embeddings
        embeddings.train(text);
        // Extract facts for inference
        inference.extract_facts(text);

        auto end = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration_cast<std::chrono::microseconds>(end - start).count() / 1000.0;

        std::cout << "{\"status\":\"success\",\"training_steps\":" << tf.training_steps
                  << ",\"vocab_size\":" << tf.tokenizer.vocab_size
                  << ",\"params\":" << tf.param_count()
                  << ",\"ngram_words\":" << ngram.total_words
                  << ",\"ngram_bigrams\":" << ngram.bigrams.size()
                  << ",\"embedding_words\":" << embeddings.vectors.size()
                  << ",\"facts_extracted\":" << inference.facts.size()
                  << ",\"time_ms\":" << ms << "}" << std::endl;
    }
    else if (cmd == "math" && argc >= 3) {
        std::string expr;
        for (int i = 2; i < argc; i++) { if (i > 2) expr += " "; expr += argv[i]; }
        double result = math_eval(expr);
        std::cout << "{\"status\":\"success\",\"expression\":";
        print_json_string(expr);
        std::cout << ",\"result\":" << result << "}" << std::endl;
    }
    else if (cmd == "stats") {
        std::cout << "{\"architecture\":\"Transformer-CPP\""
                  << ",\"d_model\":" << D_MODEL
                  << ",\"num_heads\":" << NUM_HEADS
                  << ",\"d_ff\":" << D_FF
                  << ",\"num_layers\":" << NUM_LAYERS
                  << ",\"vocab_size\":" << tf.tokenizer.vocab_size
                  << ",\"training_steps\":" << tf.training_steps
                  << ",\"total_parameters\":" << tf.param_count()
                  << ",\"features\":[\"transformer\",\"ngram\",\"embeddings\",\"attention\",\"inference\",\"math\"]"
                  << "}" << std::endl;
    }
    else if (cmd == "encode" && argc >= 3) {
        std::string text;
        for (int i = 2; i < argc; i++) { if (i > 2) text += " "; text += argv[i]; }
        auto vec = tf.encode(text);
        std::cout << "{\"status\":\"success\",\"dimensions\":" << vec.size() << ",\"vector\":[";
        for (int i = 0; i < (int)vec.size(); i++) {
            if (i) std::cout << ",";
            std::cout << vec[i];
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "similarity" && argc >= 4) {
        float sim = tf.similarity(argv[2], argv[3]);
        std::cout << "{\"status\":\"success\",\"similarity\":" << sim << "}" << std::endl;
    }
    else if (cmd == "predict" && argc >= 3) {
        std::string text;
        for (int i = 2; i < argc; i++) { if (i > 2) text += " "; text += argv[i]; }
        auto preds = tf.predict_next(text);
        std::cout << "{\"status\":\"success\",\"predictions\":[";
        for (int i = 0; i < (int)preds.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":";
            print_json_string(preds[i].first);
            std::cout << ",\"prob\":" << preds[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "generate" && argc >= 3) {
        std::string prompt;
        for (int i = 2; i < argc - 1; i++) { if (i > 2) prompt += " "; prompt += argv[i]; }
        int n = 30;
        if (argc > 3) {
            try { n = std::stoi(argv[argc - 1]); }
            catch (...) { prompt += " " + std::string(argv[argc-1]); }
        }
        std::string gen = tf.generate(prompt, n);
        std::cout << "{\"status\":\"success\",\"prompt\":";
        print_json_string(prompt);
        std::cout << ",\"generated\":";
        print_json_string(gen);
        std::cout << "}" << std::endl;
    }
    else if (cmd == "ngram" && argc >= 3) {
        // N-gram prediction — requires training first via a file
        // Usage: neural_engine ngram <train_file> <context>
        if (argc < 4) {
            std::cout << "{\"error\":\"Usage: neural_engine ngram <train_file> <context>\"}" << std::endl;
            return 1;
        }
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open training file\"}" << std::endl;
            return 1;
        }
        std::string train_text((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        ngram.train(train_text);

        std::string context;
        for (int i = 3; i < argc; i++) { if (i > 3) context += " "; context += argv[i]; }
        auto preds = ngram.predict(context);
        std::cout << "{\"status\":\"success\",\"model\":\"ngram\",\"total_words\":" << ngram.total_words
                  << ",\"predictions\":[";
        for (int i = 0; i < (int)preds.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":";
            print_json_string(preds[i].first);
            std::cout << ",\"prob\":" << preds[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "analogy" && argc >= 6) {
        // Word analogy: neural_engine analogy <train_file> <a> <b> <c>
        // "a is to b as c is to ?"
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open training file\"}" << std::endl;
            return 1;
        }
        std::string train_text((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        embeddings.train(train_text);

        auto results = embeddings.analogy(argv[3], argv[4], argv[5]);
        std::cout << "{\"status\":\"success\",\"analogy\":\"" << argv[3] << " is to " << argv[4]
                  << " as " << argv[5] << " is to ?\",\"results\":[";
        for (int i = 0; i < (int)results.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":";
            print_json_string(results[i].first);
            std::cout << ",\"similarity\":" << results[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "similar" && argc >= 4) {
        // Find similar words: neural_engine similar <train_file> <word>
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open training file\"}" << std::endl;
            return 1;
        }
        std::string train_text((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        embeddings.train(train_text);

        auto results = embeddings.most_similar(argv[3]);
        std::cout << "{\"status\":\"success\",\"word\":";
        print_json_string(argv[3]);
        std::cout << ",\"vocab_size\":" << embeddings.vectors.size() << ",\"similar\":[";
        for (int i = 0; i < (int)results.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":";
            print_json_string(results[i].first);
            std::cout << ",\"similarity\":" << results[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "infer" && argc >= 4) {
        // Inference: neural_engine infer <train_file> <query>
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open training file\"}" << std::endl;
            return 1;
        }
        std::string train_text((std::istreambuf_iterator<char>(file)),
                                std::istreambuf_iterator<char>());
        inference.extract_facts(train_text);

        std::string query;
        for (int i = 3; i < argc; i++) { if (i > 3) query += " "; query += argv[i]; }

        // Direct facts
        auto direct = inference.query(query);
        // Inferred facts
        auto inferred = inference.infer(query);

        std::cout << "{\"status\":\"success\",\"total_facts\":" << inference.facts.size()
                  << ",\"direct_facts\":[";
        for (int i = 0; i < std::min((int)direct.size(), 10); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"subject\":";
            print_json_string(direct[i].subject);
            std::cout << ",\"relation\":";
            print_json_string(direct[i].relation);
            std::cout << ",\"object\":";
            print_json_string(direct[i].object);
            std::cout << "}";
        }
        std::cout << "],\"inferred\":[";
        for (int i = 0; i < (int)inferred.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(inferred[i]);
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "entropy" && argc >= 3) {
        std::ifstream file(argv[2], std::ios::binary);
        if (!file) {
            std::cout << "{\"error\": \"Cannot open file\"}" << std::endl;
            return 1;
        }
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(file)),
                                   std::istreambuf_iterator<char>());
        double ent = byte_entropy(data);
        std::cout << "{\"status\":\"success\",\"entropy\":" << ent
                  << ",\"size\":" << data.size() << "}" << std::endl;
    }
    // Smart Brain commands (delegate to knowledge_manager)
    else if (cmd == "learn" && argc >= 3) {
        // neural_engine learn <url>
        #ifdef INCLUDE_SMART_BRAIN
        learn_and_store(argv[2]);
        #else
        std::cout << "{\"error\":\"Smart Brain not compiled in. Rebuild with -DINCLUDE_SMART_BRAIN\"}" << std::endl;
        #endif
    }
    else if (cmd == "ask" && argc >= 3) {
        // neural_engine ask "<question>"
        std::string question;
        for (int i = 2; i < argc; i++) { if (i > 2) question += " "; question += argv[i]; }
        #ifdef INCLUDE_SMART_BRAIN
        answer_from_knowledge(question);
        #else
        std::cout << "{\"error\":\"Smart Brain not compiled in. Rebuild with -DINCLUDE_SMART_BRAIN\"}" << std::endl;
        #endif
    }
    else if (cmd == "status" && argc == 2) {
        // neural_engine status
        #ifdef INCLUDE_SMART_BRAIN
        show_brain_status();
        #else
        std::cout << "{\"error\":\"Smart Brain not compiled in. Rebuild with -DINCLUDE_SMART_BRAIN\"}" << std::endl;
        #endif
    }
    else if (cmd == "compress" && argc >= 3) {
        // neural_engine compress <file> [--best|--ultra|--cmix]
        std::string input_file = argv[2];
        std::string output_file = input_file + ".myzip";
        CompressMode mode = CompressMode::DEFAULT;

        // Parse compression mode
        if (argc >= 4) {
            std::string flag = argv[3];
            if (flag == "--best") mode = CompressMode::BEST;
            else if (flag == "--ultra") mode = CompressMode::ULTRA;
            else if (flag == "--cmix") mode = CompressMode::CMIX;
        }

        try {
            compress_file(input_file, output_file, nullptr, mode);
            std::cout << "{\"status\":\"success\",\"input\":\"" << input_file
                      << "\",\"output\":\"" << output_file << "\"}" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "{\"error\":\"" << e.what() << "\"}" << std::endl;
            return 1;
        }
    }
    else if (cmd == "decompress" && argc >= 3) {
        // neural_engine decompress <file.myzip> [output_file]
        std::string input_file = argv[2];
        std::string output_file;

        if (argc >= 4) {
            output_file = argv[3];
        } else {
            // Remove .myzip extension
            if (input_file.size() > 6 && input_file.substr(input_file.size() - 6) == ".myzip") {
                output_file = input_file.substr(0, input_file.size() - 6);
            } else {
                output_file = input_file + ".decompressed";
            }
        }

        try {
            decompress_file(input_file, output_file, nullptr);
            std::cout << "{\"status\":\"success\",\"input\":\"" << input_file
                      << "\",\"output\":\"" << output_file << "\"}" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "{\"error\":\"" << e.what() << "\"}" << std::endl;
            return 1;
        }
    }
    // =========================================================================
    // PHASE 13-17: Advanced AI Commands
    // =========================================================================

    // Phase 15: RAG Engine
    else if (cmd == "rag_ask" && argc >= 3) {
        std::string question;
        for (int i = 2; i < argc; i++) { if (i > 2) question += " "; question += argv[i]; }

        auto answer = rag.answer_question(question);

        std::cout << "{\"status\":\"success\",\"answer\":";
        print_json_string(answer.text);
        std::cout << ",\"confidence\":" << answer.confidence;
        std::cout << ",\"sources\":[";
        for (int i = 0; i < (int)answer.sources.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(answer.sources[i]);
        }
        std::cout << "],\"reasoning\":[";
        for (int i = 0; i < (int)answer.reasoning_steps.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(answer.reasoning_steps[i]);
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "rag_add_doc" && argc >= 3) {
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open file\"}" << std::endl;
            return 1;
        }
        std::string content((std::istreambuf_iterator<char>(file)),
                            std::istreambuf_iterator<char>());
        rag.add_knowledge(argv[2], content);
        std::cout << "{\"status\":\"success\",\"document\":\"" << argv[2] << "\"}" << std::endl;
    }
    else if (cmd == "rag_load_embeddings" && argc >= 3) {
        rag.load_embeddings(argv[2]);
        std::cout << "{\"status\":\"success\",\"embeddings_loaded\":true}" << std::endl;
    }

    // Phase 14: Advanced Embeddings
    else if (cmd == "embed_train" && argc >= 3) {
        std::ifstream file(argv[2]);
        if (!file) {
            std::cout << "{\"error\":\"Cannot open file\"}" << std::endl;
            return 1;
        }
        std::string text((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());

        // Tokenize into sentences
        std::vector<std::string> sentences;
        std::string sent;
        for (char c : text) {
            if (c == '.' || c == '!' || c == '?' || c == '\n') {
                if (sent.size() > 10) sentences.push_back(sent);
                sent.clear();
            } else {
                sent += c;
            }
        }
        if (sent.size() > 10) sentences.push_back(sent);

        int epochs = 3;
        if (argc >= 5 && std::string(argv[3]) == "--epochs") {
            epochs = std::stoi(argv[4]);
        }

        advanced_embeddings.train(sentences, epochs);

        // Save if path provided
        if (argc >= 4 && std::string(argv[3]) != "--epochs") {
            advanced_embeddings.save(argv[3]);
        }

        std::cout << "{\"status\":\"success\",\"vocab_size\":" << advanced_embeddings.vocab_size()
                  << ",\"epochs\":" << epochs << "}" << std::endl;
    }
    else if (cmd == "embed_similar" && argc >= 3) {
        auto results = advanced_embeddings.most_similar(argv[2], 10);
        std::cout << "{\"status\":\"success\",\"word\":\"" << argv[2] << "\",\"similar\":[";
        for (int i = 0; i < (int)results.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":\"" << results[i].first << "\",\"similarity\":" << results[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "embed_analogy" && argc >= 6) {
        auto results = advanced_embeddings.analogy(argv[3], argv[4], argv[5], 5);
        std::cout << "{\"status\":\"success\",\"analogy\":\"" << argv[3] << " is to "
                  << argv[4] << " as " << argv[5] << " is to ?\",\"results\":[";
        for (int i = 0; i < (int)results.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"word\":\"" << results[i].first << "\",\"score\":" << results[i].second << "}";
        }
        std::cout << "]}" << std::endl;
    }
    else if (cmd == "embed_load" && argc >= 3) {
        advanced_embeddings.load(argv[2]);
        std::cout << "{\"status\":\"success\",\"vocab_size\":" << advanced_embeddings.vocab_size() << "}" << std::endl;
    }
    else if (cmd == "embed_save" && argc >= 3) {
        advanced_embeddings.save(argv[2]);
        std::cout << "{\"status\":\"success\",\"saved\":\"" << argv[2] << "\"}" << std::endl;
    }

    // Phase 13: Word-Level Language Model
// DISABLED:     else if (cmd == "word_train" && argc >= 3) {
// DISABLED:         std::ifstream file(argv[2]);
// DISABLED:         if (!file) {
// DISABLED:             std::cout << "{\"error\":\"Cannot open file\"}" << std::endl;
// DISABLED:             return 1;
// DISABLED:         }
// DISABLED:         std::string text((std::istreambuf_iterator<char>(file)),
// DISABLED:                          std::istreambuf_iterator<char>());
// DISABLED: 
// DISABLED:         // Train word tokenizer
// DISABLED:         std::vector<std::string> sentences;
// DISABLED:         std::string sent;
// DISABLED:         for (char c : text) {
// DISABLED:             if (c == '.' || c == '!' || c == '?' || c == '\n') {
// DISABLED:                 if (sent.size() > 10) sentences.push_back(sent);
// DISABLED:                 sent.clear();
// DISABLED:             } else {
// DISABLED:                 sent += c;
// DISABLED:             }
// DISABLED:         }
// DISABLED:         if (sent.size() > 10) sentences.push_back(sent);
// DISABLED: 
// DISABLED:         word_tok.train(sentences, 10000);
// DISABLED:         word_model.train(sentences, word_tok, 3); // 3 epochs
// DISABLED: 
// DISABLED:         std::cout << "{\"status\":\"success\",\"vocab_size\":" << word_tok.get_vocab().size()
// DISABLED:                   << ",\"model\":\"5-gram PPM\"}" << std::endl;
// DISABLED:     }
// DISABLED:     else if (cmd == "word_predict" && argc >= 3) {
// DISABLED:         std::string context;
// DISABLED:         for (int i = 2; i < argc; i++) { if (i > 2) context += " "; context += argv[i]; }
// DISABLED: 
// DISABLED:         auto preds = word_model.predict(word_tok.tokenize(context), word_tok, 10);
// DISABLED:         std::cout << "{\"status\":\"success\",\"predictions\":[";
// DISABLED:         for (int i = 0; i < (int)preds.size(); i++) {
// DISABLED:             if (i) std::cout << ",";
// DISABLED:             std::cout << "{\"word\":\"" << preds[i].first << "\",\"prob\":" << preds[i].second << "}";
// DISABLED:         }
// DISABLED:         std::cout << "]}" << std::endl;
// DISABLED:     }
// DISABLED:     else if (cmd == "word_generate" && argc >= 3) {
// DISABLED:         std::string prompt;
// DISABLED:         int n = 30;
// DISABLED:         for (int i = 2; i < argc; i++) {
// DISABLED:             std::string arg = argv[i];
// DISABLED:             if (arg.find_first_not_of("0123456789") == std::string::npos) {
// DISABLED:                 n = std::stoi(arg);
// DISABLED:             } else {
// DISABLED:                 if (i > 2) prompt += " ";
// DISABLED:                 prompt += arg;
// DISABLED:             }
// DISABLED:         }
// DISABLED: 
// DISABLED:         std::string generated = word_model.generate(prompt, word_tok, n, 0.8f);
// DISABLED:         std::cout << "{\"status\":\"success\",\"prompt\":";
// DISABLED:         print_json_string(prompt);
// DISABLED:         std::cout << ",\"generated\":";
// DISABLED:         print_json_string(generated);
// DISABLED:         std::cout << "}" << std::endl;
// DISABLED:     }
// DISABLED: 
    // Phase 16: Conversation Memory
    else if (cmd == "memory_record" && argc >= 5) {
        std::string question = argv[2];
        std::string answer = argv[3];
        float confidence = std::stof(argv[4]);

        memory.record_turn(question, answer, confidence);
        std::cout << "{\"status\":\"success\",\"turns\":" << memory.get_stats().total_turns << "}" << std::endl;
    }
    else if (cmd == "memory_feedback" && argc >= 3) {
        std::string feedback = argv[2];
        float rating = (feedback == "positive" || feedback == "good") ? 1.0f : 0.0f;
        int last_turn = memory.get_stats().total_turns - 1;
        if (last_turn >= 0) {
            memory.set_user_feedback(last_turn, rating);
        }
        std::cout << "{\"status\":\"success\",\"feedback_recorded\":true}" << std::endl;
    }
    else if (cmd == "memory_correct" && argc >= 3) {
        std::string correction;
        for (int i = 2; i < argc; i++) { if (i > 2) correction += " "; correction += argv[i]; }
        int last_turn = memory.get_stats().total_turns - 1;
        if (last_turn >= 0) {
            memory.record_correction(last_turn, correction);
        }
        std::cout << "{\"status\":\"success\",\"correction_recorded\":true}" << std::endl;
    }
    else if (cmd == "memory_export" && argc >= 3) {
        memory.save(argv[2]);
        std::cout << "{\"status\":\"success\",\"exported\":\"" << argv[2] << "\"}" << std::endl;
    }
    else if (cmd == "memory_import" && argc >= 3) {
        memory.load(argv[2]);
        std::cout << "{\"status\":\"success\",\"turns\":" << memory.get_stats().total_turns << "}" << std::endl;
    }

    // Phase 17: Reasoning Engine
    else if (cmd == "reason" && argc >= 3) {
        std::string question;
        for (int i = 2; i < argc; i++) { if (i > 2) question += " "; question += argv[i]; }

        auto reasoning = reasoner.reason_about(question);
        std::cout << "{\"status\":\"success\",\"question\":";
        print_json_string(reasoning.question);
        std::cout << ",\"confidence\":" << reasoning.overall_confidence;
        std::cout << ",\"verified\":" << (reasoning.self_verified ? "true" : "false");
        std::cout << ",\"steps\":[";
        for (int i = 0; i < (int)reasoning.steps.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"step\":";
            print_json_string(reasoning.steps[i].description);
            std::cout << ",\"confidence\":" << reasoning.steps[i].confidence << "}";
        }
        std::cout << "],\"answer\":";
        print_json_string(reasoning.final_answer);
        std::cout << "}" << std::endl;
    }
    else if (cmd == "verify" && argc >= 3) {
        std::string claim;
        for (int i = 2; i < argc; i++) { if (i > 2) claim += " "; claim += argv[i]; }

        // Simple verification: extract facts and check for contradictions
        auto facts = reasoner.extract_facts(claim);
        auto reasoning = reasoner.reason_about("Verify: " + claim);
        std::cout << "{\"status\":\"success\",\"claim\":";
        print_json_string(claim);
        std::cout << ",\"is_valid\":" << (reasoning.self_verified ? "true" : "false");
        std::cout << ",\"confidence\":" << reasoning.overall_confidence;
        std::cout << ",\"contradictions\":[";
        for (int i = 0; i < (int)reasoning.contradictions.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(reasoning.contradictions[i]);
        }
        std::cout << "],\"facts_extracted\":" << facts.size();
        std::cout << "}" << std::endl;
    }
    else if (cmd == "chain_of_thought" && argc >= 3) {
        std::string problem;
        for (int i = 2; i < argc; i++) { if (i > 2) problem += " "; problem += argv[i]; }

        auto sub_qs = reasoner.decompose_question(problem);
        auto reasoning = reasoner.reason_about(problem);
        std::cout << "{\"status\":\"success\",\"problem\":";
        print_json_string(problem);
        std::cout << ",\"decomposition\":[";
        for (int i = 0; i < (int)sub_qs.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(sub_qs[i]);
        }
        std::cout << "],\"reasoning_chain\":[";
        for (int i = 0; i < (int)reasoning.steps.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"step\":";
            print_json_string(reasoning.steps[i].description);
            std::cout << ",\"confidence\":" << reasoning.steps[i].confidence << "}";
        }
        std::cout << "]}" << std::endl;
    }

    // UNIFIED PIPELINE: Combine reasoning + RAG + memory
    else if (cmd == "ai_ask" && argc >= 3) {
        std::string question;
        for (int i = 2; i < argc; i++) { if (i > 2) question += " "; question += argv[i]; }

        // Step 1: Reason about the question
        auto reasoning = reasoner.reason_about(question);

        // Step 2: Use RAG to answer
        auto answer = rag.answer_question(question);

        // Step 3: Record in memory
        memory.record_turn(question, answer.text, answer.confidence);

        // Step 4: Unified output
        std::cout << "{\"status\":\"success\"";
        std::cout << ",\"question\":"; print_json_string(question);
        std::cout << ",\"answer\":"; print_json_string(answer.text);
        std::cout << ",\"confidence\":" << answer.confidence;
        std::cout << ",\"reasoning_verified\":" << (reasoning.self_verified ? "true" : "false");
        std::cout << ",\"reasoning_steps\":[";
        for (int i = 0; i < (int)reasoning.steps.size(); i++) {
            if (i) std::cout << ",";
            std::cout << "{\"step\":";
            print_json_string(reasoning.steps[i].description);
            std::cout << ",\"confidence\":" << reasoning.steps[i].confidence << "}";
        }
        std::cout << "],\"sources\":[";
        for (int i = 0; i < (int)answer.sources.size(); i++) {
            if (i) std::cout << ",";
            print_json_string(answer.sources[i]);
        }
        std::cout << "],\"overall_confidence\":" << reasoning.overall_confidence;
        std::cout << "}" << std::endl;
    }
    else if (cmd == "train_transformer" && argc >= 3) {
        // Phase 21C: Train the transformer with backpropagation!
        std::string corpus_path = argv[2];
        int epochs = (argc >= 4) ? std::atoi(argv[3]) : 10;
        float learning_rate = (argc >= 5) ? std::atof(argv[4]) : 0.0001f;
        int batch_size = (argc >= 6) ? std::atoi(argv[5]) : 8;

        std::cerr << "\n";
        std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
        std::cerr << "║     TRANSFORMER TRAINING - Phase 21C Active!             ║\n";
        std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
        std::cerr << "\n";
        std::cerr << "Training Parameters:\n";
        std::cerr << "  Corpus: " << corpus_path << "\n";
        std::cerr << "  Epochs: " << epochs << "\n";
        std::cerr << "  Learning Rate: " << learning_rate << "\n";
        std::cerr << "  Batch Size: " << batch_size << "\n";
        std::cerr << "\n";

        // Load corpus
        std::ifstream corpus_file(corpus_path);
        if (!corpus_file) {
            std::cout << "{\"error\":\"Cannot open corpus file: " << corpus_path << "\"}" << std::endl;
            return 1;
        }

        std::vector<std::string> texts;
        std::string line;
        while (std::getline(corpus_file, line)) {
            if (!line.empty()) {
                texts.push_back(line);
            }
        }

        std::cerr << "Loaded " << texts.size() << " lines from corpus\n\n";

        // Initialize BPE tokenizer
        BPETokenizer tokenizer(32000);

        // Check if tokenizer already exists
        if (std::ifstream("models/tokenizer.bin")) {
            std::cerr << "[TOKENIZER] Loading existing tokenizer from models/tokenizer.bin\n";
            tokenizer.load("models/tokenizer.bin");
        } else {
            std::cerr << "[TOKENIZER] Training new BPE tokenizer (this may take a while)...\n";
            tokenizer.train(texts, 30000);
            std::cerr << "[TOKENIZER] Saving tokenizer to models/tokenizer.bin\n";
            tokenizer.save("models/tokenizer.bin");
        }

        // Initialize transformer with smaller config for faster training
        TransformerConfig config;
        config.vocab_size = tokenizer.vocab_size();
        config.embedding_dim = 256;        // Smaller for faster training
        config.num_layers = 4;             // Fewer layers
        config.num_heads = 4;              // Fewer heads
        config.ff_dim = 1024;              // Smaller FF
        config.max_seq_length = 128;       // Shorter sequences

        std::cerr << "\n[TRANSFORMER] Initializing model...\n";
        std::cerr << "  Embedding Dim: " << config.embedding_dim << "\n";
        std::cerr << "  Layers: " << config.num_layers << "\n";
        std::cerr << "  Heads: " << config.num_heads << "\n";
        std::cerr << "  FF Dim: " << config.ff_dim << "\n";
        std::cerr << "  Max Sequence Length: " << config.max_seq_length << "\n\n";

        MiniTransformer transformer(config);

        // Train!
        auto start_time = std::chrono::high_resolution_clock::now();

        transformer.train(texts, tokenizer, epochs, learning_rate, batch_size);

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(end_time - start_time);

        std::cerr << "\n";
        std::cerr << "╔══════════════════════════════════════════════════════════╗\n";
        std::cerr << "║              TRAINING COMPLETE!                          ║\n";
        std::cerr << "╚══════════════════════════════════════════════════════════╝\n";
        std::cerr << "\nTraining took: " << duration.count() << " seconds\n";
        std::cerr << "Model saved to: models/transformer.bin\n\n";

        // Save model
        transformer.save("models/transformer.bin");

        // Test generation
        std::cerr << "\n[TEST] Generating sample text...\n";
        std::string test_prompt = "The future of AI is";
        std::string generated = transformer.generate(test_prompt, tokenizer, 20, 0.8f, 40);
        std::cerr << "Prompt: \"" << test_prompt << "\"\n";
        std::cerr << "Generated: \"" << generated << "\"\n\n";

        std::cout << "{\"status\":\"success\"";
        std::cout << ",\"epochs\":" << epochs;
        std::cout << ",\"training_time_seconds\":" << duration.count();
        std::cout << ",\"model_file\":\"models/transformer.bin\"";
        std::cout << ",\"tokenizer_file\":\"models/tokenizer.bin\"";
        std::cout << ",\"vocab_size\":" << config.vocab_size;
        std::cout << ",\"embedding_dim\":" << config.embedding_dim;
        std::cout << ",\"num_layers\":" << config.num_layers;
        std::cout << ",\"test_prompt\":\"" << test_prompt << "\"";
        std::cout << ",\"test_output\":\"" << generated << "\"";
        std::cout << "}" << std::endl;
    }
    else {
        std::cout << "{\"error\":\"Unknown command: " << cmd << "\"}" << std::endl;
        return 1;
    }

    return 0;
}
