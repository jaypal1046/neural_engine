#include "knowledge_manager.h"
#include "vector_index.h"
#include "persistent_mixer.h"
#include "web_fetcher.h"
#include "html_parser.h"
#include "compressor.h"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstring>

namespace fs = std::filesystem;

// Global state
static VectorIndex g_index;
static PersistentMixer g_mixer;
static bool g_initialized = false;

void init_brain() {
    if (g_initialized) return;

    // Create brain directory structure
    fs::create_directories("brain/knowledge");
    fs::create_directories("brain/temp");

    // Load index
    try {
        g_index.load("brain/index.bin");
        std::cerr << "[BRAIN] Loaded index with " << g_index.size() << " entries\n";
    } catch (const std::exception& e) {
        std::cerr << "[BRAIN] Starting with empty index\n";
    }

    // Load mixers (happens in constructor)
    std::cerr << "[BRAIN] Initialized with " << g_mixer.get_num_mixers() << " specialized mixers\n";

    g_initialized = true;
}

std::string generate_topic_name(const std::string& source) {
    std::string topic;

    if (source.find("http://") == 0 || source.find("https://") == 0) {
        // Extract from URL
        size_t last_slash = source.find_last_of('/');
        if (last_slash != std::string::npos && last_slash + 1 < source.size()) {
            topic = source.substr(last_slash + 1);
        } else {
            // Use domain name
            size_t start = source.find("://");
            if (start != std::string::npos) {
                start += 3;
                size_t end = source.find('/', start);
                topic = source.substr(start, end - start);
            }
        }
    } else {
        // Extract from file path
        topic = fs::path(source).stem().string();
    }

    // Clean topic name (remove special chars)
    std::string clean;
    for (char c : topic) {
        if (std::isalnum(c) || c == '_' || c == '-') {
            clean += std::tolower(c);
        } else if (c == ' ' || c == '.') {
            clean += '_';
        }
    }

    // Remove duplicate underscores
    auto last = std::unique(clean.begin(), clean.end(),
        [](char a, char b) { return a == '_' && b == '_'; });
    clean.erase(last, clean.end());

    // Limit length
    if (clean.size() > 50) clean.resize(50);

    return clean.empty() ? "unknown" : clean;
}

std::vector<float> compute_embedding(const std::string& text) {
    // TODO: Call neural_engine.exe to compute actual embeddings
    // For now, return a dummy 64-dim vector
    // In production, this would do:
    //   subprocess("./bin/neural_engine.exe embed <text>")
    //   parse JSON output with embedding vector

    std::vector<float> embedding(64, 0.0f);

    // Simple hash-based embedding (placeholder)
    size_t hash = std::hash<std::string>{}(text);
    for (int i = 0; i < 64; i++) {
        embedding[i] = ((hash >> i) & 1) ? 1.0f : -1.0f;
    }

    return embedding;
}

void learn_and_store(const std::string& source) {
    init_brain();

    std::string text;
    std::string topic = generate_topic_name(source);

    std::cerr << "\n[LEARN] Topic: '" << topic << "'\n";

    // 1. Fetch content
    if (source.find("http://") == 0 || source.find("https://") == 0) {
        std::cerr << "[FETCH] Downloading from web...\n";

        auto resp = fetch_url(source);
        if (resp.status_code != 200) {
            std::cerr << "[ERROR] HTTP " << resp.status_code << "\n";
            return;
        }

        std::cerr << "[FETCH] Downloaded " << resp.content_length << " bytes\n";

        // Extract clean text from HTML
        text = extract_text_from_html(resp.body);
        std::cerr << "[PARSE] Extracted " << text.size() << " bytes of clean text\n";

    } else {
        // Load from file
        std::cerr << "[LOAD] Reading from file...\n";
        std::ifstream f(source, std::ios::binary);
        if (!f) {
            std::cerr << "[ERROR] Cannot open file: " << source << "\n";
            return;
        }

        text.assign((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        std::cerr << "[LOAD] Read " << text.size() << " bytes\n";
    }

    if (text.empty()) {
        std::cerr << "[ERROR] No content to learn\n";
        return;
    }

    // 2. Compute embedding
    std::cerr << "[EMBED] Computing semantic vector...\n";
    auto embedding = compute_embedding(text);

    // 3. Check for duplicates
    auto similar = g_index.find_similar(embedding, 1);
    if (!similar.empty() && similar[0].first > 0.90f) {
        std::cerr << "[SKIP] Very similar knowledge already exists:\n";
        std::cerr << "       '" << similar[0].second->topic << "' (similarity: "
                  << similar[0].first << ")\n";
        return;
    }

    // 4. Write to temp file for compression
    std::string temp_txt = "brain/temp/" + topic + ".txt";
    std::string myzip_path = "brain/knowledge/" + topic + ".myzip";

    std::ofstream tmp(temp_txt, std::ios::binary);
    tmp.write(text.data(), text.size());
    tmp.close();

    // 5. Compress with CMIX
    std::cerr << "[COMPRESS] Compressing with CMIX neural engine...\n";

    auto start = std::chrono::steady_clock::now();
    int rc = compress_file(temp_txt, myzip_path, nullptr, CompressMode::CMIX);
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    if (rc != 0) {
        std::cerr << "[ERROR] Compression failed\n";
        fs::remove(temp_txt);
        return;
    }

    // 6. Get compressed size
    uint64_t compressed_size = fs::file_size(myzip_path);
    float ratio = (float)compressed_size / (float)text.size();

    std::cerr << "[SUCCESS] Compressed in " << elapsed << "s\n";
    std::cerr << "  Original   : " << text.size() << " bytes\n";
    std::cerr << "  Compressed : " << compressed_size << " bytes\n";
    std::cerr << "  Ratio      : " << ratio << " (" << (1.0f - ratio) * 100.0f << "% saved)\n";

    // 7. Add to index
    KnowledgeEntry entry;
    entry.topic = topic;
    entry.file_path = myzip_path;
    entry.embedding = embedding;
    entry.original_size = text.size();
    entry.compressed_size = compressed_size;
    entry.timestamp = std::chrono::system_clock::now().time_since_epoch().count();

    g_index.add(entry);
    g_index.save("brain/index.bin");

    // 8. Save improved mixer weights
    g_mixer.save_all();

    std::cerr << "[BRAIN] Knowledge stored. Total entries: " << g_index.size() << "\n\n";

    // Cleanup
    fs::remove(temp_txt);
}

void answer_from_knowledge(const std::string& question) {
    init_brain();

    std::cerr << "\n[QUERY] \"" << question << "\"\n";

    if (g_index.size() == 0) {
        std::cerr << "[EMPTY] No knowledge in brain. Use 'learn' first!\n";
        std::cout << "{\"error\": \"empty_brain\", \"message\": \"No knowledge stored yet\"}\n";
        return;
    }

    // 1. Compute query embedding
    std::cerr << "[SEARCH] Computing query embedding...\n";
    auto query_emb = compute_embedding(question);

    // 2. Find similar entries
    auto results = g_index.find_similar(query_emb, 3);

    if (results.empty()) {
        std::cerr << "[NOT FOUND] No relevant knowledge\n";
        std::cout << "{\"confidence\": 0.0, \"action\": \"learn_from_web\"}\n";
        return;
    }

    float confidence = results[0].first;
    const auto* best_entry = results[0].second;

    std::cerr << "[MATCH] Best match: '" << best_entry->topic << "' (confidence: " << confidence << ")\n";

    // 3. Check confidence threshold
    if (confidence < 0.50f) {
        std::cerr << "[LOW CONFIDENCE] Recommending web search\n";
        std::cout << "{\n"
                  << "  \"confidence\": " << confidence << ",\n"
                  << "  \"action\": \"learn_from_web\",\n"
                  << "  \"query\": \"" << question << "\"\n"
                  << "}\n";
        return;
    }

    // 4. Decompress knowledge
    std::string recovered = "brain/temp/answer_" + best_entry->topic + ".txt";

    std::cerr << "[DECOMPRESS] Extracting knowledge...\n";
    int rc = decompress_file(best_entry->file_path, recovered, nullptr);

    if (rc != 0) {
        std::cerr << "[ERROR] Decompression failed\n";
        std::cout << "{\"error\": \"decompression_failed\"}\n";
        return;
    }

    // 5. Read decompressed text
    std::ifstream f(recovered, std::ios::binary);
    std::string knowledge((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();

    // 6. Extract relevant excerpt (first 800 chars for now)
    // TODO: Use neural_engine for smarter extraction
    std::string excerpt = knowledge.substr(0, std::min((size_t)800, knowledge.size()));

    // Escape quotes for JSON
    for (size_t i = 0; i < excerpt.size(); i++) {
        if (excerpt[i] == '"') {
            excerpt.insert(i, "\\");
            i++;
        } else if (excerpt[i] == '\n') {
            excerpt[i] = ' ';
        }
    }

    // 7. Return answer
    std::cout << "{\n"
              << "  \"confidence\": " << confidence << ",\n"
              << "  \"source\": \"" << best_entry->topic << "\",\n"
              << "  \"answer\": \"" << excerpt << "\",\n"
              << "  \"compressed_size\": " << best_entry->compressed_size << ",\n"
              << "  \"original_size\": " << best_entry->original_size << "\n"
              << "}\n";

    std::cerr << "[SUCCESS] Answer generated from stored knowledge\n\n";

    // Cleanup
    fs::remove(recovered);
}

void smart_compress(const std::string& file_path) {
    init_brain();

    std::cerr << "\n[SMART COMPRESS] " << file_path << "\n";

    // Auto-detect file type and use specialized mixer
    std::string output = file_path + ".myzip";

    std::cerr << "[COMPRESS] Using CMIX with persistent neural weights...\n";

    auto start = std::chrono::steady_clock::now();
    int rc = compress_file(file_path, output, nullptr, CompressMode::CMIX);
    auto elapsed = std::chrono::duration<double>(
        std::chrono::steady_clock::now() - start).count();

    if (rc != 0) {
        std::cerr << "[ERROR] Compression failed\n";
        return;
    }

    // Save improved weights
    g_mixer.save_all();

    uint64_t orig = fs::file_size(file_path);
    uint64_t comp = fs::file_size(output);
    float ratio = (float)comp / (float)orig;

    std::cerr << "[SUCCESS] Compressed in " << elapsed << "s\n";
    std::cerr << "  Original   : " << orig << " bytes\n";
    std::cerr << "  Compressed : " << comp << " bytes\n";
    std::cerr << "  Ratio      : " << ratio << " (" << (1.0f - ratio) * 100.0f << "% saved)\n";
    std::cerr << "  Output     : " << output << "\n\n";
}

void show_brain_status() {
    init_brain();

    uint64_t total_orig = g_index.total_original_size();
    uint64_t total_comp = g_index.total_compressed_size();
    float avg_ratio = g_index.average_compression_ratio();

    std::cout << "{\n"
              << "  \"entries\": " << g_index.size() << ",\n"
              << "  \"total_original_mb\": " << (total_orig / (1024.0 * 1024.0)) << ",\n"
              << "  \"total_compressed_mb\": " << (total_comp / (1024.0 * 1024.0)) << ",\n"
              << "  \"average_ratio\": " << avg_ratio << ",\n"
              << "  \"savings_percent\": " << ((1.0f - avg_ratio) * 100.0f) << ",\n"
              << "  \"specialized_mixers\": " << g_mixer.get_num_mixers() << "\n"
              << "}\n";
}
