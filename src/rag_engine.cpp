#include "rag_engine.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cmath>
#include <iostream>
#include <filesystem>
#include <regex>

namespace fs = std::filesystem;

RAGEngine::RAGEngine() {
    embeddings = std::make_unique<EmbeddingTrainer>();
    tokenizer = std::make_unique<WordTokenizer>();
    word_model = std::make_unique<WordPPM>(5);  // 5-gram model
}

void RAGEngine::load_embeddings(const std::string& embeddings_file) {
    std::cout << "[RAG] Loading embeddings from " << embeddings_file << "\n";
    embeddings->load(embeddings_file);
    stats.embeddings_loaded = true;
    std::cout << "[RAG] Embeddings loaded: " << embeddings->vocab_size() << " words\n";
}

void RAGEngine::load_knowledge_dir(const std::string& knowledge_dir) {
    std::cout << "[RAG] Loading knowledge from " << knowledge_dir << "\n";

    try {
        for (const auto& entry : fs::directory_iterator(knowledge_dir)) {
            if (entry.is_regular_file()) {
                std::string path = entry.path().string();
                std::string ext = entry.path().extension().string();

                // Only load .txt files (skip .myzip for now)
                if (ext == ".txt") {
                    std::ifstream file(path);
                    if (file) {
                        std::string content((std::istreambuf_iterator<char>(file)),
                                          std::istreambuf_iterator<char>());

                        std::string doc_id = entry.path().stem().string();
                        add_knowledge(doc_id, content);
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "[RAG] Error loading knowledge: " << e.what() << "\n";
    }

    std::cout << "[RAG] Loaded " << stats.total_documents << " documents\n";
}

void RAGEngine::add_knowledge(const std::string& doc_id, const std::string& content) {
    Document doc;
    doc.id = doc_id;
    doc.content = content;

    // Split into sentences
    std::regex sentence_regex("[.!?]+\\s+");
    std::sregex_token_iterator iter(content.begin(), content.end(), sentence_regex, -1);
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) {
        std::string sentence = iter->str();
        // Trim whitespace
        sentence.erase(0, sentence.find_first_not_of(" \t\n\r"));
        sentence.erase(sentence.find_last_not_of(" \t\n\r") + 1);

        if (sentence.length() >= 10) {  // Skip very short sentences
            doc.sentences.push_back(sentence);
            stats.total_sentences++;
        }
    }

    // Compute document embedding
    if (stats.embeddings_loaded) {
        doc.embedding = compute_doc_embedding(content);
    }

    stats.total_bytes += content.length();
    stats.total_documents++;

    knowledge_base.push_back(std::move(doc));
}

std::vector<float> RAGEngine::compute_doc_embedding(const std::string& text) {
    return embeddings->sentence_embedding(text);
}

float RAGEngine::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    float dot = 0.0f, mag_a = 0.0f, mag_b = 0.0f;
    for (size_t i = 0; i < a.size(); i++) {
        dot += a[i] * b[i];
        mag_a += a[i] * a[i];
        mag_b += b[i] * b[i];
    }

    mag_a = std::sqrt(mag_a);
    mag_b = std::sqrt(mag_b);

    if (mag_a < 1e-10f || mag_b < 1e-10f) return 0.0f;

    return dot / (mag_a * mag_b);
}

std::vector<std::pair<std::string, float>> RAGEngine::retrieve_documents(
    const std::string& query,
    int top_k
) {
    if (!stats.embeddings_loaded) {
        std::cerr << "[RAG] Warning: Embeddings not loaded, using random scores\n";
        return {};
    }

    // Embed query
    auto query_emb = embeddings->sentence_embedding(query);

    // Score all documents
    std::vector<std::pair<std::string, float>> scores;
    for (const auto& doc : knowledge_base) {
        float sim = cosine_similarity(query_emb, doc.embedding);
        scores.push_back({doc.id, sim});
    }

    // Sort by similarity
    std::sort(scores.begin(), scores.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Return top-k
    if (static_cast<int>(scores.size()) > top_k) {
        scores.resize(top_k);
    }

    return scores;
}

float RAGEngine::attention_score(const std::string& question, const std::string& sentence) {
    if (!stats.embeddings_loaded) {
        // Fallback: keyword overlap
        auto q_tokens = tokenizer->tokenize(question);
        auto s_tokens = tokenizer->tokenize(sentence);

        int overlap = 0;
        for (const auto& qt : q_tokens) {
            for (const auto& st : s_tokens) {
                if (qt == st) {
                    overlap++;
                    break;
                }
            }
        }
        return static_cast<float>(overlap) / std::max(1.0f, static_cast<float>(q_tokens.size()));
    }

    // Use semantic similarity
    auto q_emb = embeddings->sentence_embedding(question);
    auto s_emb = embeddings->sentence_embedding(sentence);

    return cosine_similarity(q_emb, s_emb);
}

std::vector<std::pair<std::string, float>> RAGEngine::extract_relevant_sentences(
    const std::string& question,
    const std::vector<std::string>& documents,
    int max_sentences
) {
    std::vector<std::pair<std::string, float>> scored_sentences;

    // Score all sentences from all documents
    for (const auto& doc_id : documents) {
        // Find document
        auto it = std::find_if(knowledge_base.begin(), knowledge_base.end(),
                              [&doc_id](const Document& d) { return d.id == doc_id; });

        if (it == knowledge_base.end()) continue;

        // Score each sentence
        for (const auto& sentence : it->sentences) {
            float score = attention_score(question, sentence);
            scored_sentences.push_back({sentence, score});
        }
    }

    // Sort by score
    std::sort(scored_sentences.begin(), scored_sentences.end(),
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // Deduplicate similar sentences
    std::vector<std::pair<std::string, float>> unique_sentences;
    for (const auto& [sent, score] : scored_sentences) {
        bool is_duplicate = false;
        for (const auto& [unique_sent, _] : unique_sentences) {
            // Simple dedup: check if 80% of words overlap
            auto tokens1 = tokenizer->tokenize(sent);
            auto tokens2 = tokenizer->tokenize(unique_sent);

            int overlap = 0;
            for (const auto& t1 : tokens1) {
                if (std::find(tokens2.begin(), tokens2.end(), t1) != tokens2.end()) {
                    overlap++;
                }
            }

            float overlap_ratio = static_cast<float>(overlap) / std::max(1.0f, static_cast<float>(tokens1.size()));
            if (overlap_ratio > 0.8f) {
                is_duplicate = true;
                break;
            }
        }

        if (!is_duplicate) {
            unique_sentences.push_back({sent, score});
            if (static_cast<int>(unique_sentences.size()) >= max_sentences) {
                break;
            }
        }
    }

    return unique_sentences;
}

std::string RAGEngine::build_generation_prompt(
    const std::string& question,
    const std::vector<std::string>& context
) {
    std::stringstream prompt;

    // Add context
    for (const auto& sent : context) {
        prompt << sent << " ";
    }

    // Add question as continuation prompt
    // The model will continue from here
    prompt << question << " ";

    return prompt.str();
}

std::string RAGEngine::generate_answer(
    const std::string& question,
    const std::vector<std::string>& context_sentences,
    int max_words
) {
    if (!stats.word_model_trained) {
        // If word model not trained, just return context
        std::stringstream answer;
        for (const auto& sent : context_sentences) {
            answer << sent << " ";
        }
        return answer.str();
    }

    // Build prompt from context
    std::string prompt = build_generation_prompt(question, context_sentences);

    // Generate continuation
    std::string generated = word_model->generate(prompt, *tokenizer, max_words, 0.7f);

    return generated;
}

float RAGEngine::compute_confidence(
    const std::vector<std::pair<std::string, float>>& doc_scores,
    const std::vector<std::pair<std::string, float>>& sentence_scores
) {
    if (doc_scores.empty() || sentence_scores.empty()) {
        return 0.0f;
    }

    // Average of top document score and top sentence score
    float doc_score = doc_scores[0].second;
    float sent_score = sentence_scores[0].second;

    float confidence = (doc_score + sent_score) / 2.0f;

    // Boost if we have multiple good matches
    if (doc_scores.size() >= 3 && doc_scores[2].second > 0.5f) {
        confidence = std::min(1.0f, confidence * 1.2f);
    }

    return confidence;
}

RAGEngine::Answer RAGEngine::answer_question(
    const std::string& question,
    int max_context,
    int max_answer_words
) {
    Answer answer;
    answer.reasoning_steps.push_back("Step 1: Retrieve relevant documents");

    // Step 1: Retrieve documents
    auto doc_scores = retrieve_documents(question, 5);

    if (doc_scores.empty()) {
        answer.text = "I don't have knowledge about this topic yet.";
        answer.confidence = 0.0f;
        answer.reasoning_steps.push_back("No relevant documents found");
        return answer;
    }

    answer.reasoning_steps.push_back("Step 2: Extract relevant sentences");

    // Step 2: Extract relevant sentences
    std::vector<std::string> doc_ids;
    for (const auto& [id, score] : doc_scores) {
        doc_ids.push_back(id);
        answer.sources.push_back(id);
    }

    auto sentence_scores = extract_relevant_sentences(question, doc_ids, max_context);
    answer.relevant_sentences = sentence_scores;

    if (sentence_scores.empty()) {
        answer.text = "Found related documents but couldn't extract relevant information.";
        answer.confidence = 0.2f;
        answer.reasoning_steps.push_back("No relevant sentences found");
        return answer;
    }

    answer.reasoning_steps.push_back("Step 3: Generate coherent answer");

    // Step 3: Generate answer
    std::vector<std::string> context;
    for (const auto& [sent, score] : sentence_scores) {
        context.push_back(sent);
    }

    if (stats.word_model_trained) {
        answer.text = generate_answer(question, context, max_answer_words);
        answer.reasoning_steps.push_back("Generated answer using word model");
    } else {
        // Fallback: just concatenate top sentences
        std::stringstream ss;
        for (size_t i = 0; i < std::min(size_t(3), context.size()); i++) {
            ss << context[i];
            if (i < context.size() - 1) ss << " ";
        }
        answer.text = ss.str();
        answer.reasoning_steps.push_back("Returned extracted sentences (word model not trained)");
    }

    // Compute confidence
    answer.confidence = compute_confidence(doc_scores, sentence_scores);
    answer.reasoning_steps.push_back("Step 4: Computed confidence score");

    return answer;
}

void RAGEngine::train_word_model(int epochs) {
    std::cout << "[RAG] Training word model on knowledge base...\n";

    // Collect all sentences
    std::vector<std::string> all_sentences;
    for (const auto& doc : knowledge_base) {
        for (const auto& sent : doc.sentences) {
            all_sentences.push_back(sent);
        }
    }

    // Train tokenizer
    std::string combined;
    for (const auto& sent : all_sentences) {
        combined += sent + " ";
    }
    tokenizer->train_from_text(combined, 50000, false);

    // Train word model
    word_model->train_from_text(combined, *tokenizer);

    stats.word_model_trained = true;
    std::cout << "[RAG] Word model trained on " << all_sentences.size() << " sentences\n";
}
