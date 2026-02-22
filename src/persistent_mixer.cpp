#include "persistent_mixer.h"
#include <fstream>
#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

void save_mixer_weights(const Mixer& mixer, const std::string& path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return;  // Silently fail if can't save

    // Write global weights
    uint32_t n_global = mixer.global_weights.size();
    f.write((char*)&n_global, sizeof(n_global));
    f.write((char*)mixer.global_weights.data(), n_global * sizeof(float));

    // Write niche weights
    uint32_t n_niche = mixer.niche_weights.size();
    f.write((char*)&n_niche, sizeof(n_niche));
    f.write((char*)mixer.niche_weights.data(), n_niche * sizeof(float));
}

bool load_mixer_weights(Mixer& mixer, const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;  // File doesn't exist yet

    // Read global weights
    uint32_t n_global;
    f.read((char*)&n_global, sizeof(n_global));
    if (n_global != mixer.global_weights.size()) return false;  // Size mismatch
    f.read((char*)mixer.global_weights.data(), n_global * sizeof(float));

    // Read niche weights
    uint32_t n_niche;
    f.read((char*)&n_niche, sizeof(n_niche));
    if (n_niche != mixer.niche_weights.size()) return false;
    f.read((char*)mixer.niche_weights.data(), n_niche * sizeof(float));

    return true;
}

PersistentMixer::PersistentMixer() {
    // Create mixers (6 models each)
    general_mixer = std::make_unique<Mixer>(6);
    text_mixer = std::make_unique<Mixer>(6);
    code_mixer = std::make_unique<Mixer>(6);
    json_mixer = std::make_unique<Mixer>(6);

    // Create brain directory if it doesn't exist
    fs::create_directories(weights_dir);

    // Load existing weights
    load_all();
}

PersistentMixer::~PersistentMixer() {
    // Auto-save on destruction
    save_all();
}

void PersistentMixer::save_all() {
    save_mixer_weights(*general_mixer, weights_dir + "/mixer_general.weights");
    save_mixer_weights(*text_mixer, weights_dir + "/mixer_text.weights");
    save_mixer_weights(*code_mixer, weights_dir + "/mixer_code.weights");
    save_mixer_weights(*json_mixer, weights_dir + "/mixer_json.weights");
}

void PersistentMixer::load_all() {
    bool g = load_mixer_weights(*general_mixer, weights_dir + "/mixer_general.weights");
    bool t = load_mixer_weights(*text_mixer, weights_dir + "/mixer_text.weights");
    bool c = load_mixer_weights(*code_mixer, weights_dir + "/mixer_code.weights");
    bool j = load_mixer_weights(*json_mixer, weights_dir + "/mixer_json.weights");

    int loaded = g + t + c + j;
    if (loaded > 0) {
        fprintf(stderr, "[MIXER] Loaded %d pre-trained weight sets\n", loaded);
    }
}

FileType PersistentMixer::detect_file_type(const uint8_t* data, size_t len, const std::string& filename) {
    // 1. Check file extension
    std::string ext;
    size_t dot = filename.find_last_of('.');
    if (dot != std::string::npos) {
        ext = filename.substr(dot);
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

        // Text files
        if (ext == ".txt" || ext == ".log" || ext == ".md" || ext == ".rst") {
            return FileType::TEXT;
        }

        // Code files
        if (ext == ".cpp" || ext == ".h" || ext == ".c" || ext == ".hpp" ||
            ext == ".py" || ext == ".js" || ext == ".ts" || ext == ".tsx" ||
            ext == ".java" || ext == ".rs" || ext == ".go" || ext == ".rb" ||
            ext == ".php" || ext == ".cs" || ext == ".swift") {
            return FileType::CODE;
        }

        // Structured data
        if (ext == ".json" || ext == ".xml" || ext == ".yaml" || ext == ".yml" ||
            ext == ".toml" || ext == ".ini" || ext == ".cfg") {
            return FileType::JSON;
        }
    }

    // 2. Content-based detection (sample first 4KB)
    size_t sample_len = std::min(len, (size_t)4096);
    std::string sample((char*)data, sample_len);

    // Check for JSON
    if (sample[0] == '{' || sample[0] == '[') {
        return FileType::JSON;
    }

    // Check for XML
    if (sample.find("<?xml") != std::string::npos ||
        sample.find("<!DOCTYPE") != std::string::npos) {
        return FileType::JSON;
    }

    // Check for code patterns
    if (sample.find("function ") != std::string::npos ||
        sample.find("class ") != std::string::npos ||
        sample.find("#include") != std::string::npos ||
        sample.find("import ") != std::string::npos ||
        sample.find("def ") != std::string::npos ||
        sample.find("const ") != std::string::npos) {
        return FileType::CODE;
    }

    // Check if mostly printable text
    int text_chars = 0;
    for (size_t i = 0; i < sample_len; i++) {
        uint8_t c = data[i];
        if ((c >= 32 && c <= 126) || c == '\n' || c == '\r' || c == '\t') {
            text_chars++;
        }
    }

    float text_ratio = (float)text_chars / (float)sample_len;
    if (text_ratio > 0.85f) {
        return FileType::TEXT;
    }

    // Default: binary
    return FileType::BINARY;
}

Mixer* PersistentMixer::get_mixer_for_type(FileType type) {
    switch (type) {
        case FileType::TEXT:   return text_mixer.get();
        case FileType::CODE:   return code_mixer.get();
        case FileType::JSON:   return json_mixer.get();
        default:               return general_mixer.get();
    }
}

Mixer* PersistentMixer::get_mixer_for_file(const std::string& file_path) {
    // Read first 4KB to detect type
    std::ifstream f(file_path, std::ios::binary);
    if (!f) return general_mixer.get();

    std::vector<uint8_t> sample(4096);
    f.read((char*)sample.data(), sample.size());
    size_t bytes_read = f.gcount();

    FileType type = detect_file_type(sample.data(), bytes_read, file_path);
    return get_mixer_for_type(type);
}
