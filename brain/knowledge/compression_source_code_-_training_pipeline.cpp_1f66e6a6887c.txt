#include "training_pipeline.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <regex>
#include <chrono>
#include <cstring>
#include <sys/stat.h>

#ifdef _WIN32
    #include <windows.h>
    #include <wininet.h>
    #pragma comment(lib, "wininet.lib")
#else
    #include <unistd.h>
#endif

TrainingPipeline::TrainingPipeline() {
    reset_stats();
}

bool TrainingPipeline::file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

size_t TrainingPipeline::get_file_size(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        return st.st_size;
    }
    return 0;
}

bool TrainingPipeline::download_file(const std::string& url, const std::string& output_path) {
#ifdef _WIN32
    // Windows: Use WinINet
    std::cout << "[Pipeline] Downloading: " << url << "\n";

    HINTERNET hInternet = InternetOpenA("TrainingPipeline/1.0",
                                        INTERNET_OPEN_TYPE_DIRECT,
                                        NULL, NULL, 0);
    if (!hInternet) {
        std::cerr << "[Pipeline] Error: Failed to initialize WinINet\n";
        return false;
    }

    HINTERNET hUrl = InternetOpenUrlA(hInternet, url.c_str(), NULL, 0,
                                      INTERNET_FLAG_RELOAD, 0);
    if (!hUrl) {
        std::cerr << "[Pipeline] Error: Failed to open URL\n";
        InternetCloseHandle(hInternet);
        return false;
    }

    std::ofstream out(output_path, std::ios::binary);
    if (!out) {
        std::cerr << "[Pipeline] Error: Cannot create output file\n";
        InternetCloseHandle(hUrl);
        InternetCloseHandle(hInternet);
        return false;
    }

    char buffer[4096];
    DWORD bytes_read;
    size_t total_bytes = 0;

    while (InternetReadFile(hUrl, buffer, sizeof(buffer), &bytes_read) && bytes_read > 0) {
        out.write(buffer, bytes_read);
        total_bytes += bytes_read;

        if (progress_callback && total_bytes % (1024 * 1024) == 0) {
            // Update every MB (we don't know total size, so just report bytes)
            progress_callback(total_bytes / (1024 * 1024));
        }
    }

    out.close();
    InternetCloseHandle(hUrl);
    InternetCloseHandle(hInternet);

    std::cout << "[Pipeline] Downloaded " << (total_bytes / (1024.0 * 1024.0)) << " MB\n";
    return true;
#else
    // Linux/Mac: Use curl command
    std::string cmd = "curl -L -o \"" + output_path + "\" \"" + url + "\"";
    int result = system(cmd.c_str());
    return result == 0;
#endif
}

bool TrainingPipeline::download_simple_wikipedia(const std::string& output_path) {
    // Simple Wikipedia latest dump (text only)
    // Note: This URL might change - update as needed
    std::string url = "https://dumps.wikimedia.org/simplewiki/latest/simplewiki-latest-pages-articles.xml.bz2";

    std::cout << "[Pipeline] Downloading Simple Wikipedia\n";
    std::cout << "[Pipeline] This may take 10-20 minutes (~600 MB compressed)\n";
    std::cout << "[Pipeline] URL: " << url << "\n";

    // For now, provide instructions for manual download
    // Auto-download of large files is complex (compression, progress, etc.)
    std::cerr << "\n";
    std::cerr << "=============================================================\n";
    std::cerr << "MANUAL DOWNLOAD REQUIRED\n";
    std::cerr << "=============================================================\n";
    std::cerr << "Please download Simple Wikipedia manually:\n";
    std::cerr << "1. Go to: https://dumps.wikimedia.org/simplewiki/latest/\n";
    std::cerr << "2. Download: simplewiki-latest-pages-articles.xml.bz2\n";
    std::cerr << "3. Extract the .bz2 file to get the XML\n";
    std::cerr << "4. Place the XML file at: " << output_path << "\n";
    std::cerr << "\n";
    std::cerr << "OR use a simpler text corpus for testing:\n";
    std::cerr << "- Create a .txt file with sample text\n";
    std::cerr << "- Save it to: " << output_path << "\n";
    std::cerr << "=============================================================\n";

    return false;  // Manual download required
}

bool TrainingPipeline::download_from_url(const std::string& url, const std::string& output_path) {
    return download_file(url, output_path);
}

std::string TrainingPipeline::remove_tags(const std::string& text) {
    // Remove HTML/XML tags
    std::regex tag_regex("<[^>]+>");
    return std::regex_replace(text, tag_regex, " ");
}

std::string TrainingPipeline::normalize_whitespace(const std::string& text) {
    // Replace multiple spaces with single space
    std::regex space_regex("\\s+");
    std::string result = std::regex_replace(text, space_regex, " ");

    // Trim leading/trailing spaces
    size_t start = result.find_first_not_of(" \t\n\r");
    size_t end = result.find_last_not_of(" \t\n\r");

    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

std::string TrainingPipeline::clean_text(const std::string& raw_text) {
    std::string text = raw_text;

    // Remove tags
    text = remove_tags(text);

    // Remove special wiki markup (simplified)
    text = std::regex_replace(text, std::regex("\\{\\{[^}]+\\}\\}"), " ");
    text = std::regex_replace(text, std::regex("\\[\\[[^\\]]+\\|([^\\]]+)\\]\\]"), "$1");
    text = std::regex_replace(text, std::regex("\\[\\[([^\\]]+)\\]\\]"), "$1");

    // Remove URLs
    text = std::regex_replace(text, std::regex("https?://\\S+"), " ");

    // Normalize whitespace
    text = normalize_whitespace(text);

    return text;
}

std::vector<std::string> TrainingPipeline::split_sentences(const std::string& text) {
    std::vector<std::string> sentences;

    // Split on sentence terminators
    std::regex sentence_regex("[.!?]+\\s+");
    std::sregex_token_iterator iter(text.begin(), text.end(), sentence_regex, -1);
    std::sregex_token_iterator end;

    for (; iter != end; ++iter) {
        std::string sentence = iter->str();
        sentence = normalize_whitespace(sentence);

        // Only keep sentences with reasonable length
        if (sentence.length() >= 10 && sentence.length() <= 1000) {
            sentences.push_back(sentence);
            stats.total_sentences++;

            // Count words (rough approximation)
            size_t word_count = std::count(sentence.begin(), sentence.end(), ' ') + 1;
            stats.total_words += word_count;
        }
    }

    return sentences;
}

std::string TrainingPipeline::extract_wikipedia_text(const std::string& xml_file) {
    std::cout << "[Pipeline] Extracting text from Wikipedia XML...\n";

    std::ifstream file(xml_file);
    if (!file) {
        std::cerr << "[Pipeline] Error: Cannot open XML file\n";
        return "";
    }

    std::stringstream result;
    std::string line;
    bool in_text = false;

    while (std::getline(file, line)) {
        // Simple extraction: look for <text> tags
        if (line.find("<text") != std::string::npos) {
            in_text = true;
        }

        if (in_text) {
            result << line << " ";
        }

        if (line.find("</text>") != std::string::npos) {
            in_text = false;
        }
    }

    std::string text = result.str();
    std::cout << "[Pipeline] Extracted " << (text.length() / (1024.0 * 1024.0)) << " MB of text\n";

    return clean_text(text);
}

void TrainingPipeline::process_corpus(
    const std::string& input_file,
    std::function<void(const std::string&)> callback
) {
    auto start_time = std::chrono::high_resolution_clock::now();

    std::cout << "[Pipeline] Processing corpus: " << input_file << "\n";

    std::ifstream file(input_file);
    if (!file) {
        std::cerr << "[Pipeline] Error: Cannot open file\n";
        return;
    }

    size_t file_size = get_file_size(input_file);
    size_t bytes_processed = 0;
    int last_progress = 0;

    std::string line;
    while (std::getline(file, line)) {
        bytes_processed += line.length() + 1;
        stats.total_bytes_processed = bytes_processed;

        // Clean and split line into sentences
        std::string cleaned = clean_text(line);
        if (cleaned.empty()) continue;

        auto sentences = split_sentences(cleaned);
        for (const auto& sentence : sentences) {
            callback(sentence);
        }

        // Progress reporting
        if (file_size > 0 && progress_callback) {
            int progress = (bytes_processed * 100) / file_size;
            if (progress > last_progress && progress % 10 == 0) {
                progress_callback(progress);
                last_progress = progress;
            }
        }
    }

    auto end_time = std::chrono::high_resolution_clock::now();
    stats.processing_time_sec = std::chrono::duration<double>(end_time - start_time).count();

    std::cout << "[Pipeline] Processed " << (stats.total_bytes_processed / (1024.0 * 1024.0))
              << " MB in " << stats.processing_time_sec << " seconds\n";
    std::cout << "[Pipeline] Total sentences: " << stats.total_sentences << "\n";
    std::cout << "[Pipeline] Total words: " << stats.total_words << "\n";
}

std::vector<std::string> TrainingPipeline::load_corpus(const std::string& input_file) {
    std::vector<std::string> sentences;

    process_corpus(input_file, [&sentences](const std::string& sentence) {
        sentences.push_back(sentence);
    });

    return sentences;
}

void TrainingPipeline::set_progress_callback(std::function<void(int)> callback) {
    progress_callback = callback;
}
