// =============================================================================
// Compressed Knowledge Module Manager
// Integrates block-based random access with AI knowledge queries
// =============================================================================

#include "compressed_knowledge.h"
#include "compressor.h"
#include <algorithm>
#include <cstring>
#include <fstream>
#include <sstream>

// -----------------------------------------------------------------------------
// CompressedKnowledgeModule implementation
// -----------------------------------------------------------------------------

CompressedKnowledgeModule::CompressedKnowledgeModule(const std::string& path)
    : module_path(path), handle(nullptr) {
}

CompressedKnowledgeModule::~CompressedKnowledgeModule() {
    close();
}

bool CompressedKnowledgeModule::open() {
    if (handle) return true;  // Already open

    BlockFileInfo info;
    handle = block_open(module_path, info);
    if (!handle) {
        last_error = "Failed to open: " + module_path;
        return false;
    }

    file_info = info;
    return true;
}

void CompressedKnowledgeModule::close() {
    if (handle) {
        block_close(handle);
        handle = nullptr;
    }
}

bool CompressedKnowledgeModule::is_open() const {
    return handle != nullptr;
}

std::string CompressedKnowledgeModule::get_module_name() const {
    // Extract filename without extension
    size_t last_slash = module_path.find_last_of("/\\");
    size_t last_dot = module_path.find_last_of(".");

    std::string name = module_path;
    if (last_slash != std::string::npos) {
        name = name.substr(last_slash + 1);
    }
    if (last_dot != std::string::npos && last_dot > last_slash) {
        name = name.substr(0, last_dot - (last_slash + 1));
    }

    return name;
}

uint32_t CompressedKnowledgeModule::get_block_count() const {
    return file_info.block_count;
}

uint64_t CompressedKnowledgeModule::get_original_size() const {
    return file_info.orig_size;
}

std::vector<uint8_t> CompressedKnowledgeModule::read_block(uint32_t block_id) {
    if (!handle) {
        last_error = "Module not open";
        return {};
    }

    auto data = block_decompress(handle, block_id);
    if (data.empty()) {
        last_error = "Failed to decompress block " + std::to_string(block_id);
    }
    return data;
}

std::string CompressedKnowledgeModule::read_block_as_text(uint32_t block_id) {
    auto data = read_block(block_id);
    if (data.empty()) return "";

    return std::string(data.begin(), data.end());
}

std::string CompressedKnowledgeModule::search_and_extract(const std::string& keyword, size_t max_context) {
    if (!handle) {
        last_error = "Module not open";
        return "";
    }

    // Simple search: scan all blocks for keyword
    // In production, you'd use a pre-built index file
    std::string result;
    size_t total_found = 0;

    for (uint32_t b = 0; b < file_info.block_count && total_found < max_context; ++b) {
        auto text = read_block_as_text(b);
        if (text.empty()) continue;

        // Case-insensitive search
        std::string lower_text = text;
        std::string lower_keyword = keyword;
        std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
        std::transform(lower_keyword.begin(), lower_keyword.end(), lower_keyword.begin(), ::tolower);

        size_t pos = lower_text.find(lower_keyword);
        if (pos != std::string::npos) {
            // Found! Extract context around keyword
            size_t start = (pos > 200) ? pos - 200 : 0;
            size_t end = std::min(pos + keyword.length() + 200, text.length());

            result += "\n[Block " + std::to_string(b) + "]\n";
            result += text.substr(start, end - start);
            result += "\n...\n";

            total_found += (end - start);
        }
    }

    if (result.empty()) {
        last_error = "Keyword not found: " + keyword;
    }

    return result;
}

std::string CompressedKnowledgeModule::get_last_error() const {
    return last_error;
}

// -----------------------------------------------------------------------------
// KnowledgeModuleManager implementation
// -----------------------------------------------------------------------------

KnowledgeModuleManager::KnowledgeModuleManager(const std::string& modules_dir)
    : modules_directory(modules_dir) {
}

KnowledgeModuleManager::~KnowledgeModuleManager() {
    // Close all modules
    for (auto& pair : modules) {
        pair.second->close();
    }
}

bool KnowledgeModuleManager::load_module(const std::string& module_name) {
    // Check if already loaded
    if (modules.find(module_name) != modules.end()) {
        return true;
    }

    // Construct path
    std::string path = modules_directory + "/" + module_name + ".aiz";

    // Create and open module
    auto module = std::make_unique<CompressedKnowledgeModule>(path);
    if (!module->open()) {
        last_error = module->get_last_error();
        return false;
    }

    modules[module_name] = std::move(module);
    return true;
}

void KnowledgeModuleManager::unload_module(const std::string& module_name) {
    auto it = modules.find(module_name);
    if (it != modules.end()) {
        it->second->close();
        modules.erase(it);
    }
}

CompressedKnowledgeModule* KnowledgeModuleManager::get_module(const std::string& module_name) {
    auto it = modules.find(module_name);
    if (it == modules.end()) return nullptr;
    return it->second.get();
}

std::vector<std::string> KnowledgeModuleManager::list_loaded_modules() const {
    std::vector<std::string> names;
    for (const auto& pair : modules) {
        names.push_back(pair.first);
    }
    return names;
}

std::string KnowledgeModuleManager::query(const std::string& question, size_t max_context) {
    if (modules.empty()) {
        last_error = "No modules loaded";
        return "";
    }

    // Extract keywords from question (simple approach)
    std::vector<std::string> keywords;
    std::istringstream iss(question);
    std::string word;
    while (iss >> word) {
        // Skip common words
        if (word.length() > 3 &&
            word != "what" && word != "when" && word != "where" &&
            word != "which" && word != "how" && word != "why") {
            keywords.push_back(word);
        }
    }

    if (keywords.empty()) {
        last_error = "No keywords found in question";
        return "";
    }

    // Search all loaded modules
    std::string combined_context;
    for (auto& pair : modules) {
        for (const auto& keyword : keywords) {
            auto context = pair.second->search_and_extract(keyword, max_context / keywords.size());
            if (!context.empty()) {
                combined_context += "\n=== Module: " + pair.first + " ===\n";
                combined_context += context;
            }
        }
    }

    if (combined_context.empty()) {
        last_error = "No relevant information found";
    }

    return combined_context;
}

std::string KnowledgeModuleManager::get_last_error() const {
    return last_error;
}
