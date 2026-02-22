#include "vector_index.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>

// SIMD intrinsics (optional, falls back to scalar if not available)
#if defined(__AVX__) || defined(__AVX2__)
#include <immintrin.h>
#define USE_AVX 1
#elif defined(__SSE2__)
#include <emmintrin.h>
#define USE_SSE2 1
#endif

VectorIndex::VectorIndex() {}

void VectorIndex::add(const KnowledgeEntry& entry) {
    entries.push_back(entry);
}

const KnowledgeEntry* VectorIndex::find_by_topic(const std::string& topic) const {
    for (const auto& e : entries) {
        if (e.topic == topic) return &e;
    }
    return nullptr;
}

bool VectorIndex::has_content_hash(const std::string& hash) const {
    for (const auto& e : entries) {
        if (e.content_hash == hash) return true;
    }
    return false;
}

bool VectorIndex::remove(const std::string& topic) {
    auto it = std::remove_if(entries.begin(), entries.end(),
        [&topic](const KnowledgeEntry& e) { return e.topic == topic; });

    if (it != entries.end()) {
        entries.erase(it, entries.end());
        return true;
    }
    return false;
}

float VectorIndex::cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) const {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    size_t n = a.size();
    float dot = 0.0f, norm_a = 0.0f, norm_b = 0.0f;

#ifdef USE_AVX
    // AVX: process 8 floats at a time
    size_t i = 0;
    __m256 sum_dot = _mm256_setzero_ps();
    __m256 sum_a = _mm256_setzero_ps();
    __m256 sum_b = _mm256_setzero_ps();

    for (; i + 7 < n; i += 8) {
        __m256 va = _mm256_loadu_ps(&a[i]);
        __m256 vb = _mm256_loadu_ps(&b[i]);

        sum_dot = _mm256_add_ps(sum_dot, _mm256_mul_ps(va, vb));
        sum_a = _mm256_add_ps(sum_a, _mm256_mul_ps(va, va));
        sum_b = _mm256_add_ps(sum_b, _mm256_mul_ps(vb, vb));
    }

    // Horizontal sum
    float temp[8];
    _mm256_storeu_ps(temp, sum_dot);
    for (int j = 0; j < 8; j++) dot += temp[j];

    _mm256_storeu_ps(temp, sum_a);
    for (int j = 0; j < 8; j++) norm_a += temp[j];

    _mm256_storeu_ps(temp, sum_b);
    for (int j = 0; j < 8; j++) norm_b += temp[j];

    // Tail
    for (; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

#elif defined(USE_SSE2)
    // SSE2: process 4 floats at a time
    size_t i = 0;
    __m128 sum_dot = _mm_setzero_ps();
    __m128 sum_a = _mm_setzero_ps();
    __m128 sum_b = _mm_setzero_ps();

    for (; i + 3 < n; i += 4) {
        __m128 va = _mm_loadu_ps(&a[i]);
        __m128 vb = _mm_loadu_ps(&b[i]);

        sum_dot = _mm_add_ps(sum_dot, _mm_mul_ps(va, vb));
        sum_a = _mm_add_ps(sum_a, _mm_mul_ps(va, va));
        sum_b = _mm_add_ps(sum_b, _mm_mul_ps(vb, vb));
    }

    float temp[4];
    _mm_storeu_ps(temp, sum_dot);
    for (int j = 0; j < 4; j++) dot += temp[j];

    _mm_storeu_ps(temp, sum_a);
    for (int j = 0; j < 4; j++) norm_a += temp[j];

    _mm_storeu_ps(temp, sum_b);
    for (int j = 0; j < 4; j++) norm_b += temp[j];

    // Tail
    for (; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

#else
    // Scalar fallback
    for (size_t i = 0; i < n; i++) {
        dot += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }
#endif

    if (norm_a <= 0.0f || norm_b <= 0.0f) return 0.0f;
    return dot / (std::sqrt(norm_a) * std::sqrt(norm_b));
}

std::vector<std::pair<float, const KnowledgeEntry*>>
VectorIndex::find_similar(const std::vector<float>& query_vec, int top_k) {
    std::vector<std::pair<float, const KnowledgeEntry*>> results;
    results.reserve(entries.size());

    // Compute similarity to all entries
    for (auto& entry : entries) {
        float sim = cosine_similarity(query_vec, entry.embedding);
        results.push_back({sim, &entry});
    }

    // Partial sort: get top K
    int k = std::min(top_k, (int)results.size());
    std::partial_sort(results.begin(),
                     results.begin() + k,
                     results.end(),
                     [](const auto& a, const auto& b) { return a.first > b.first; });

    if (results.size() > (size_t)k) {
        results.resize(k);
    }

    return results;
}

uint64_t VectorIndex::total_original_size() const {
    uint64_t total = 0;
    for (const auto& e : entries) total += e.original_size;
    return total;
}

uint64_t VectorIndex::total_compressed_size() const {
    uint64_t total = 0;
    for (const auto& e : entries) total += e.compressed_size;
    return total;
}

float VectorIndex::average_compression_ratio() const {
    uint64_t orig = total_original_size();
    uint64_t comp = total_compressed_size();
    if (orig == 0) return 0.0f;
    return (float)comp / (float)orig;
}

void VectorIndex::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) throw std::runtime_error("Cannot open index file for writing");

    // Write magic and version
    uint32_t magic = 0x42524149;  // "BRAI" in hex
    uint32_t version = 1;
    f.write((char*)&magic, sizeof(magic));
    f.write((char*)&version, sizeof(version));

    // Write entry count
    uint32_t count = entries.size();
    f.write((char*)&count, sizeof(count));

    // Write each entry
    for (const auto& e : entries) {
        // Write topic (length + data)
        uint32_t len = e.topic.size();
        f.write((char*)&len, sizeof(len));
        f.write(e.topic.data(), len);

        // Write file_path
        len = e.file_path.size();
        f.write((char*)&len, sizeof(len));
        f.write(e.file_path.data(), len);

        // Write embedding
        uint32_t emb_size = e.embedding.size();
        f.write((char*)&emb_size, sizeof(emb_size));
        f.write((char*)e.embedding.data(), emb_size * sizeof(float));

        // Write metadata
        f.write((char*)&e.original_size, sizeof(e.original_size));
        f.write((char*)&e.compressed_size, sizeof(e.compressed_size));
        f.write((char*)&e.timestamp, sizeof(e.timestamp));

        // Write content_hash
        len = e.content_hash.size();
        f.write((char*)&len, sizeof(len));
        f.write(e.content_hash.data(), len);
    }
}

void VectorIndex::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return;  // File doesn't exist yet (first run)

    // Read magic and version
    uint32_t magic, version;
    f.read((char*)&magic, sizeof(magic));
    f.read((char*)&version, sizeof(version));

    if (magic != 0x42524149) {
        throw std::runtime_error("Invalid index file (bad magic)");
    }
    if (version != 1) {
        throw std::runtime_error("Unsupported index version");
    }

    entries.clear();

    // Read entry count
    uint32_t count;
    f.read((char*)&count, sizeof(count));
    entries.reserve(count);

    // Read each entry
    for (uint32_t i = 0; i < count; i++) {
        KnowledgeEntry e;

        // Read topic
        uint32_t len;
        f.read((char*)&len, sizeof(len));
        e.topic.resize(len);
        f.read(&e.topic[0], len);

        // Read file_path
        f.read((char*)&len, sizeof(len));
        e.file_path.resize(len);
        f.read(&e.file_path[0], len);

        // Read embedding
        uint32_t emb_size;
        f.read((char*)&emb_size, sizeof(emb_size));
        e.embedding.resize(emb_size);
        f.read((char*)e.embedding.data(), emb_size * sizeof(float));

        // Read metadata
        f.read((char*)&e.original_size, sizeof(e.original_size));
        f.read((char*)&e.compressed_size, sizeof(e.compressed_size));
        f.read((char*)&e.timestamp, sizeof(e.timestamp));

        // Read content_hash
        f.read((char*)&len, sizeof(len));
        e.content_hash.resize(len);
        f.read(&e.content_hash[0], len);

        entries.push_back(std::move(e));
    }
}
