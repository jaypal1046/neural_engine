#pragma once
#include <string>
#include <vector>

namespace TeacherBrain {

    // Check if the small teacher brain (stories15M.bin, 58MB) exists locally.
    // Download it exclusively if absent. Doesn't access internet otherwise!
    bool ensure_teacher_model_exists();

    // Generate a single block of high-quality synthetic text completely offline
    std::string generate_synthetic_text(float temperature = 0.8f, int max_tokens = 256);

    // Bootstrap: Continuous offline synthetic generation loop -> feeding directly into MiniTransformer training
    bool run_offline_bootstrap(int num_sentences);

}
