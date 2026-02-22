#pragma once
#include "cmix.h"
#include <string>
#include <memory>

// =============================================================================
// Persistent Mixer - Neural network weights that persist across files
// =============================================================================

enum class FileType {
    UNKNOWN,
    TEXT,      // Plain text, logs, markdown
    CODE,      // C++, Python, JavaScript, etc.
    JSON,      // JSON, XML, YAML
    BINARY,    // Images, executables, archives
};

class PersistentMixer {
public:
    PersistentMixer();
    ~PersistentMixer();

    // Get appropriate mixer for a file
    Mixer* get_mixer_for_file(const std::string& file_path);
    Mixer* get_mixer_for_type(FileType type);

    // Detect file type from content and filename
    FileType detect_file_type(const uint8_t* data, size_t len, const std::string& filename);

    // Save all mixer weights to disk
    void save_all();

    // Load all mixer weights from disk
    void load_all();

    // Get stats
    int get_num_mixers() const { return 4; }

private:
    std::unique_ptr<Mixer> general_mixer;
    std::unique_ptr<Mixer> text_mixer;
    std::unique_ptr<Mixer> code_mixer;
    std::unique_ptr<Mixer> json_mixer;

    std::string weights_dir = "brain";
};

// Helper: Save/load mixer weights
void save_mixer_weights(const Mixer& mixer, const std::string& path);
bool load_mixer_weights(Mixer& mixer, const std::string& path);
