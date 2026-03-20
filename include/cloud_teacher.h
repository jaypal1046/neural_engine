#pragma once
#include <string>

namespace CloudTeacher {

    // Ask Claude API (Claude 3.5 Sonnet) and return the high-quality response
    std::string ask_claude(const std::string& prompt);

    // Bootstraps training data by asking Claude an array of distinct topics
    // and saves the knowledge into our corpus
    bool bootstrap_from_claude();
}
