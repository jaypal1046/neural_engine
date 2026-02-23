#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

/**
 * Training Pipeline - Downloads and processes training corpora
 *
 * Handles:
 * - Downloading Wikipedia dumps
 * - Streaming processing (memory-efficient for large files)
 * - Text cleaning and normalization
 * - Sentence segmentation
 * - Progress tracking
 *
 * Example:
 *   TrainingPipeline pipeline;
 *   pipeline.download_simple_wikipedia("data/corpora/simplewiki.txt");
 *   pipeline.process_corpus("data/corpora/simplewiki.txt", [](const std::string& sentence) {
 *       // Process each sentence
 *   });
 */

class TrainingPipeline {
public:
    TrainingPipeline();

    /**
     * Download Simple Wikipedia dump
     * @param output_path Where to save the extracted text
     * @return true if successful
     */
    bool download_simple_wikipedia(const std::string& output_path);

    /**
     * Download from custom URL
     * @param url URL to download from
     * @param output_path Where to save
     * @return true if successful
     */
    bool download_from_url(const std::string& url, const std::string& output_path);

    /**
     * Process corpus in streaming mode (memory-efficient)
     * Calls callback for each sentence
     * @param input_file Path to corpus file
     * @param callback Function called for each sentence
     */
    void process_corpus(
        const std::string& input_file,
        std::function<void(const std::string&)> callback
    );

    /**
     * Load entire corpus into memory (for small files)
     * @param input_file Path to corpus file
     * @return Vector of sentences
     */
    std::vector<std::string> load_corpus(const std::string& input_file);

    /**
     * Clean and normalize text
     * - Removes HTML tags
     * - Normalizes whitespace
     * - Handles special characters
     * - Preserves sentence structure
     */
    std::string clean_text(const std::string& raw_text);

    /**
     * Split text into sentences
     */
    std::vector<std::string> split_sentences(const std::string& text);

    /**
     * Extract text from Wikipedia XML dump
     */
    std::string extract_wikipedia_text(const std::string& xml_file);

    /**
     * Get statistics
     */
    struct Stats {
        size_t total_bytes_processed = 0;
        size_t total_sentences = 0;
        size_t total_words = 0;
        double processing_time_sec = 0.0;
    };

    Stats get_stats() const { return stats; }
    void reset_stats() { stats = Stats(); }

    /**
     * Progress callback (optional)
     * Called periodically during processing with progress percentage
     */
    void set_progress_callback(std::function<void(int)> callback);

private:
    Stats stats;
    std::function<void(int)> progress_callback;

    /**
     * Download file from URL (uses curl or WinHTTP)
     */
    bool download_file(const std::string& url, const std::string& output_path);

    /**
     * Remove HTML/XML tags from text
     */
    std::string remove_tags(const std::string& text);

    /**
     * Normalize whitespace
     */
    std::string normalize_whitespace(const std::string& text);

    /**
     * Check if file exists
     */
    bool file_exists(const std::string& path);

    /**
     * Get file size
     */
    size_t get_file_size(const std::string& path);
};
