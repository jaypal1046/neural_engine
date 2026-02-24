#pragma once
#include <vector>
#include <string>
#include <cstdint>

// =============================================================================
// Vector Index - Fast similarity search using SIMD-optimized cosine distance
// =============================================================================

struct KnowledgeEntry {
    std::string topic;                // e.g., "quantum_entanglement"
    std::string file_path;            // e.g., "brain/knowledge/quantum.aiz"
    std::vector<float> embedding;     // 64-dim vector
    uint64_t original_size;           // Size before compression
    uint64_t compressed_size;         // Size after compression
    uint64_t timestamp;               // Unix timestamp
    std::string content_hash;         // SHA-256 for deduplication
};

class VectorIndex {
public:
    VectorIndex();

    // Add new entry to index
    void add(const KnowledgeEntry& entry);

    // Find similar entries (cosine similarity)
    // Returns: vector of (similarity_score, entry_pointer) sorted by score descending
    std::vector<std::pair<float, const KnowledgeEntry*>>
        find_similar(const std::vector<float>& query_vec, int top_k = 5);

    // Find by topic name (exact match)
    const KnowledgeEntry* find_by_topic(const std::string& topic) const;

    // Check if content hash exists (for deduplication)
    bool has_content_hash(const std::string& hash) const;

    // Remove entry by topic
    bool remove(const std::string& topic);

    // Save/load index to/from disk (binary format)
    void save(const std::string& path) const;
    void load(const std::string& path);

    // Stats
    size_t size() const { return entries.size(); }
    uint64_t total_original_size() const;
    uint64_t total_compressed_size() const;
    float average_compression_ratio() const;

private:
    std::vector<KnowledgeEntry> entries;

    // SIMD-accelerated cosine similarity (AVX if available, else scalar)
    float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const;
};
