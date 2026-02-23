#include "word_ppm.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <random>
#include <iostream>

WordPPM::WordPPM(int order, float alpha)
    : order(order), alpha(alpha), total_ngrams(0) {
    // Initialize context storage for each order (1-gram to N-gram)
    contexts.resize(order);
    context_totals.resize(order);
}

std::string WordPPM::build_context_key(const std::vector<int>& ids, int start, int len) const {
    std::string key;
    for (int i = start; i < start + len && i < static_cast<int>(ids.size()); i++) {
        if (i > start) key += " ";
        key += std::to_string(ids[i]);
    }
    return key;
}

void WordPPM::train_from_tokens(const std::vector<int>& token_ids) {
    if (token_ids.size() < 2) return;

    // Build N-grams of all orders
    for (size_t i = 0; i < token_ids.size() - 1; i++) {
        int next_word = token_ids[i + 1];

        // Train for each order (1-gram to N-gram)
        for (int ord = 0; ord < order; ord++) {
            int context_len = ord + 1;
            if (static_cast<int>(i) < context_len) continue;

            // Build context from previous words
            std::string ctx_key = build_context_key(token_ids, i - context_len + 1, context_len);

            // Update counts
            contexts[ord][ctx_key][next_word]++;
            context_totals[ord][ctx_key]++;
            total_ngrams++;
        }
    }
}

void WordPPM::train_from_text(const std::string& text, WordTokenizer& tokenizer) {
    auto token_ids = tokenizer.encode(text);
    train_from_tokens(token_ids);
}

void WordPPM::train_from_file(const std::string& file_path, WordTokenizer& tokenizer) {
    std::ifstream file(file_path);
    if (!file) {
        std::cerr << "[WordPPM] Error: Cannot open file " << file_path << "\n";
        return;
    }

    std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    train_from_text(text, tokenizer);

    std::cout << "[WordPPM] Trained on " << total_ngrams << " N-grams\n";
    std::cout << "[WordPPM] Context count: " << get_context_count() << "\n";
}

std::map<int, float> WordPPM::get_distribution(const std::vector<int>& context_ids, int ctx_order) const {
    std::map<int, float> dist;

    if (ctx_order < 0 || ctx_order >= order) return dist;
    if (context_ids.empty()) return dist;

    int context_len = ctx_order + 1;
    if (static_cast<int>(context_ids.size()) < context_len) return dist;

    // Build context key from last N words
    int start = context_ids.size() - context_len;
    std::string ctx_key = build_context_key(context_ids, start, context_len);

    // Check if context exists
    auto ctx_it = contexts[ctx_order].find(ctx_key);
    if (ctx_it == contexts[ctx_order].end()) return dist;

    // Get total count for normalization
    int total = context_totals[ctx_order].at(ctx_key);

    // Compute probability distribution with Laplace smoothing
    const auto& word_counts = ctx_it->second;
    int vocab_size = 50000;  // Approximate vocab size for smoothing

    for (const auto& [word_id, count] : word_counts) {
        float prob = (count + alpha) / (total + alpha * vocab_size);
        dist[word_id] = prob;
    }

    return dist;
}

std::map<int, float> WordPPM::combine_distributions(const std::vector<int>& context_ids) const {
    std::map<int, float> combined;

    // Try orders from highest to lowest (backoff)
    // Higher orders get more weight
    std::vector<float> weights = {0.5f, 0.25f, 0.15f, 0.07f, 0.03f};  // Weights for 5-gram down to 1-gram

    for (int ord = order - 1; ord >= 0; ord--) {
        auto dist = get_distribution(context_ids, ord);
        if (dist.empty()) continue;

        float weight = (ord < static_cast<int>(weights.size())) ? weights[ord] : 0.01f;

        for (const auto& [word_id, prob] : dist) {
            combined[word_id] += prob * weight;
        }
    }

    // Normalize
    float total = 0.0f;
    for (const auto& [_, p] : combined) {
        total += p;
    }
    if (total > 0) {
        for (auto& [_, p] : combined) {
            p /= total;
        }
    }

    return combined;
}

std::vector<std::pair<int, float>> WordPPM::predict_id(
    const std::vector<int>& context_ids,
    int top_k
) const {
    auto dist = combine_distributions(context_ids);

    // Sort by probability
    std::vector<std::pair<int, float>> sorted(dist.begin(), dist.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Return top-k
    if (static_cast<int>(sorted.size()) > top_k) {
        sorted.resize(top_k);
    }

    return sorted;
}

std::vector<std::pair<std::string, float>> WordPPM::predict(
    const std::vector<std::string>& context,
    const WordTokenizer& tokenizer,
    int top_k
) const {
    // Convert context words to IDs
    std::vector<int> context_ids;
    for (const auto& word : context) {
        context_ids.push_back(tokenizer.get_id(word));
    }

    // Get predictions as IDs
    auto id_predictions = predict_id(context_ids, top_k);

    // Convert back to words
    std::vector<std::pair<std::string, float>> result;
    for (const auto& [word_id, prob] : id_predictions) {
        result.push_back({tokenizer.get_word(word_id), prob});
    }

    return result;
}

int WordPPM::sample_from_distribution(const std::map<int, float>& dist, float temperature) const {
    if (dist.empty()) return WordTokenizer::UNK_ID;

    // Apply temperature
    std::vector<std::pair<int, float>> items(dist.begin(), dist.end());

    if (temperature < 0.01f) {
        // Greedy: return most probable
        auto max_it = std::max_element(items.begin(), items.end(),
                                       [](const auto& a, const auto& b) { return a.second < b.second; });
        return max_it->first;
    }

    // Temperature sampling
    std::vector<float> adjusted_probs;
    float total = 0.0f;
    for (const auto& [word_id, prob] : items) {
        float adjusted = std::pow(prob, 1.0f / temperature);
        adjusted_probs.push_back(adjusted);
        total += adjusted;
    }

    // Normalize
    for (auto& p : adjusted_probs) {
        p /= total;
    }

    // Sample
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_real_distribution<float> uniform(0.0f, 1.0f);
    float r = uniform(gen);

    float cumsum = 0.0f;
    for (size_t i = 0; i < items.size(); i++) {
        cumsum += adjusted_probs[i];
        if (r <= cumsum) {
            return items[i].first;
        }
    }

    return items.back().first;
}

std::string WordPPM::generate(
    const std::string& prompt,
    WordTokenizer& tokenizer,
    int num_words,
    float temperature
) {
    auto context_ids = tokenizer.encode(prompt);
    std::vector<std::string> generated_words;

    for (int i = 0; i < num_words; i++) {
        // Predict next word
        auto dist = combine_distributions(context_ids);
        if (dist.empty()) break;

        // Sample next word
        int next_id = sample_from_distribution(dist, temperature);
        std::string next_word = tokenizer.get_word(next_id);

        generated_words.push_back(next_word);

        // Update context
        context_ids.push_back(next_id);

        // Keep context within order limit
        if (static_cast<int>(context_ids.size()) > order + 10) {
            context_ids.erase(context_ids.begin());
        }
    }

    // Join words
    std::string result;
    for (const auto& word : generated_words) {
        if (!result.empty()) result += " ";
        result += word;
    }

    return result;
}

float WordPPM::compute_perplexity(const std::string& text, WordTokenizer& tokenizer) const {
    auto token_ids = tokenizer.encode(text);
    if (token_ids.size() <= static_cast<size_t>(order)) return 1.0f;  // Not enough context

    float log_prob_sum = 0.0f;
    int count = 0;

    for (size_t i = order; i < token_ids.size(); i++) {
        // Get context
        std::vector<int> context(token_ids.begin() + i - order, token_ids.begin() + i);
        int target = token_ids[i];

        // Get distribution
        auto dist = combine_distributions(context);
        float prob = dist.count(target) ? dist[target] : alpha / 50000.0f;  // Smoothing

        if (prob > 0.0f) {  // Avoid log(0)
            log_prob_sum += std::log(prob);
            count++;
        }
    }

    if (count == 0) return 1.0f;  // No valid predictions

    float avg_log_prob = log_prob_sum / count;
    return std::exp(-avg_log_prob);
}

size_t WordPPM::get_context_count() const {
    size_t total = 0;
    for (const auto& ctx_map : contexts) {
        total += ctx_map.size();
    }
    return total;
}

void WordPPM::save(const std::string& file_path) const {
    std::ofstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[WordPPM] Error: Cannot save to " << file_path << "\n";
        return;
    }

    // Save hyperparameters
    file.write(reinterpret_cast<const char*>(&order), sizeof(order));
    file.write(reinterpret_cast<const char*>(&alpha), sizeof(alpha));
    file.write(reinterpret_cast<const char*>(&total_ngrams), sizeof(total_ngrams));

    // Save contexts for each order
    for (int ord = 0; ord < order; ord++) {
        int ctx_count = static_cast<int>(contexts[ord].size());
        file.write(reinterpret_cast<const char*>(&ctx_count), sizeof(ctx_count));

        for (const auto& [ctx_key, word_counts] : contexts[ord]) {
            // Save context key
            int key_len = static_cast<int>(ctx_key.size());
            file.write(reinterpret_cast<const char*>(&key_len), sizeof(key_len));
            file.write(ctx_key.data(), key_len);

            // Save word counts
            int word_count = static_cast<int>(word_counts.size());
            file.write(reinterpret_cast<const char*>(&word_count), sizeof(word_count));

            for (const auto& [word_id, count] : word_counts) {
                file.write(reinterpret_cast<const char*>(&word_id), sizeof(word_id));
                file.write(reinterpret_cast<const char*>(&count), sizeof(count));
            }
        }
    }

    std::cout << "[WordPPM] Saved to " << file_path << "\n";
}

void WordPPM::load(const std::string& file_path) {
    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "[WordPPM] Error: Cannot load from " << file_path << "\n";
        return;
    }

    // Load hyperparameters
    file.read(reinterpret_cast<char*>(&order), sizeof(order));
    file.read(reinterpret_cast<char*>(&alpha), sizeof(alpha));
    file.read(reinterpret_cast<char*>(&total_ngrams), sizeof(total_ngrams));

    // Resize storage
    contexts.clear();
    context_totals.clear();
    contexts.resize(order);
    context_totals.resize(order);

    // Load contexts for each order
    for (int ord = 0; ord < order; ord++) {
        int ctx_count;
        file.read(reinterpret_cast<char*>(&ctx_count), sizeof(ctx_count));

        for (int c = 0; c < ctx_count; c++) {
            // Load context key
            int key_len;
            file.read(reinterpret_cast<char*>(&key_len), sizeof(key_len));
            std::string ctx_key(key_len, '\0');
            file.read(&ctx_key[0], key_len);

            // Load word counts
            int word_count;
            file.read(reinterpret_cast<char*>(&word_count), sizeof(word_count));

            int total_count = 0;
            for (int w = 0; w < word_count; w++) {
                int word_id, count;
                file.read(reinterpret_cast<char*>(&word_id), sizeof(word_id));
                file.read(reinterpret_cast<char*>(&count), sizeof(count));

                contexts[ord][ctx_key][word_id] = count;
                total_count += count;
            }

            context_totals[ord][ctx_key] = total_count;
        }
    }

    std::cout << "[WordPPM] Loaded from " << file_path << "\n";
    std::cout << "[WordPPM] Order: " << order << ", N-grams: " << total_ngrams << "\n";
}
