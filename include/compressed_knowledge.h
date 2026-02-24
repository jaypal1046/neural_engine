#pragma once

#include "compressor.h"
#include <string>
#include <vector>
#include <map>
#include <memory>

// =============================================================================
// Compressed Knowledge Module System
// Provides high-level API for querying compressed knowledge modules
// =============================================================================

// Single compressed knowledge module (.aiz file)
class CompressedKnowledgeModule {
public:
    explicit CompressedKnowledgeModule(const std::string& path);
    ~CompressedKnowledgeModule();

    // Open/close module
    bool open();
    void close();
    bool is_open() const;

    // Module info
    std::string get_module_name() const;
    uint32_t get_block_count() const;
    uint64_t get_original_size() const;

    // Block access
    std::vector<uint8_t> read_block(uint32_t block_id);
    std::string read_block_as_text(uint32_t block_id);

    // Search and extract context
    std::string search_and_extract(const std::string& keyword, size_t max_context = 2000);

    // Error handling
    std::string get_last_error() const;

private:
    std::string module_path;
    void* handle;
    BlockFileInfo file_info;
    std::string last_error;
};

// Manager for multiple knowledge modules
class KnowledgeModuleManager {
public:
    explicit KnowledgeModuleManager(const std::string& modules_dir);
    ~KnowledgeModuleManager();

    // Module management
    bool load_module(const std::string& module_name);
    void unload_module(const std::string& module_name);
    CompressedKnowledgeModule* get_module(const std::string& module_name);
    std::vector<std::string> list_loaded_modules() const;

    // Query across all loaded modules
    std::string query(const std::string& question, size_t max_context = 5000);

    // Error handling
    std::string get_last_error() const;

private:
    std::string modules_directory;
    std::map<std::string, std::unique_ptr<CompressedKnowledgeModule>> modules;
    std::string last_error;
};
