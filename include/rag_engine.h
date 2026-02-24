#pragma once

#include "word_tokenizer.h"
#include "word_ppm.h"
#include "embedding_trainer.h"
#include <string>
#include <vector>
#include <memory>

/**
 * RAG Engine - Retrieval-Augmented Generation
 *
 * Combines:
 * 1. Semantic retrieval (find relevant knowledge)
 * 2. Attention-based extraction (select best sentences)
 * 3. Word-level generation (synthesize coherent answer)
 *
 * This is what makes the AI truly intelligent:
 * - Understands questions (embeddings)
 * - Finds relevant knowledge (vector search)
 * - Synthesizes answers (word generation)
 *
 * Example:
 *   RAGEngine rag;
 *   rag.load_embeddings("embeddings_64d.bin");
 *   rag.load_knowledge("brain/knowledge/");
 *
 *   auto answer = rag.answer("What is BWT compression?");
 *   // Returns: coherent multi-sentence explanation
 */

class RAGEngine {
public:
    RAGEngine();

    /**
     * Load semantic embeddings
     */
    void load_embeddings(const std::string& embeddings_file);

    /**
     * Load knowledge base (directory of .txt or .aiz files)
     */
    void load_knowledge_dir(const std::string& knowledge_dir);

    /**
     * Add single knowledge document
     */
    void add_knowledge(const std::string& doc_id, const std::string& content);

    /**
     * Answer a question using RAG pipeline
     *
     * Pipeline:
     * 1. Embed question
     * 2. Search knowledge base (top-k docs)
     * 3. Extract relevant sentences (attention-based)
     * 4. Generate coherent answer (word-level model)
     *
     * @param question User's question
     * @param max_context Max sentences to use as context (default 5)
     * @param max_answer_words Max words to generate (default 50)
     * @return Answer with metadata
     */
    struct Answer {
        std::string text;                              // Generated answer
        float confidence;                              // 0.0-1.0
        std::vector<std::string> sources;              // Source doc IDs
        std::vector<std::pair<std::string, float>> relevant_sentences;  // (sentence, score)
        std::vector<std::string> reasoning_steps;      // How we got the answer
    };

    Answer answer_question(
        const std::string& question,
        int max_context = 5,
        int max_answer_words = 50
    );

    /**
     * Retrieve relevant documents (step 1 of RAG)
     * @return Vector of (doc_id, similarity) pairs
     */
    std::vector<std::pair<std::string, float>> retrieve_documents(
        const std::string& query,
        int top_k = 5
    );

    /**
     * Extract relevant sentences from documents (step 2 of RAG)
     * Uses attention mechanism to score sentences
     */
    std::vector<std::pair<std::string, float>> extract_relevant_sentences(
        const std::string& question,
        const std::vector<std::string>& documents,
        int max_sentences = 5
    );

    /**
     * Generate answer from context (step 3 of RAG)
     * Uses word-level model to synthesize coherent text
     */
    std::string generate_answer(
        const std::string& question,
        const std::vector<std::string>& context_sentences,
        int max_words = 50
    );

    /**
     * Compute confidence score for answer
     * Based on:
     * - Retrieval scores
     * - Sentence relevance
     * - Generation perplexity
     */
    float compute_confidence(
        const std::vector<std::pair<std::string, float>>& doc_scores,
        const std::vector<std::pair<std::string, float>>& sentence_scores
    );

    /**
     * Train word model on knowledge base
     * Improves generation quality
     */
    void train_word_model(int epochs = 3);

    /**
     * Statistics
     */
    struct Stats {
        int total_documents = 0;
        int total_sentences = 0;
        size_t total_bytes = 0;
        bool embeddings_loaded = false;
        bool word_model_trained = false;
    };

    Stats get_stats() const { return stats; }

private:
    // Models
    std::unique_ptr<EmbeddingTrainer> embeddings;
    std::unique_ptr<WordTokenizer> tokenizer;
    std::unique_ptr<WordPPM> word_model;

    // Knowledge base
    struct Document {
        std::string id;
        std::string content;
        std::vector<std::string> sentences;
        std::vector<float> embedding;  // Document embedding
    };

    std::vector<Document> knowledge_base;
    Stats stats;

    /**
     * Compute document embedding (average of sentence embeddings)
     */
    std::vector<float> compute_doc_embedding(const std::string& text);

    /**
     * Attention score between question and sentence
     */
    float attention_score(
        const std::string& question,
        const std::string& sentence
    );

    /**
     * Build prompt for generation
     */
    std::string build_generation_prompt(
        const std::string& question,
        const std::vector<std::string>& context
    );

    /**
     * Cosine similarity between vectors
     */
    static float cosine_similarity(
        const std::vector<float>& a,
        const std::vector<float>& b
    );
};
