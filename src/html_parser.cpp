#include "html_parser.h"
#include <regex>
#include <algorithm>
#include <cctype>

std::string decode_html_entities(const std::string& text) {
    std::string result = text;

    // Common HTML entities
    static const struct { const char* entity; const char* replacement; } entities[] = {
        {"&nbsp;", " "}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"},
        {"&quot;", "\""}, {"&#39;", "'"}, {"&apos;", "'"},
        {"&ndash;", "-"}, {"&mdash;", "--"}, {"&hellip;", "..."},
        {"&copy;", "(c)"}, {"&reg;", "(R)"}, {"&trade;", "(TM)"},
        {nullptr, nullptr}
    };

    for (int i = 0; entities[i].entity; i++) {
        size_t pos = 0;
        while ((pos = result.find(entities[i].entity, pos)) != std::string::npos) {
            result.replace(pos, strlen(entities[i].entity), entities[i].replacement);
            pos += strlen(entities[i].replacement);
        }
    }

    // Numeric entities &#NNN; and &#xHH;
    std::regex numeric_entity(R"(&#(\d+);)");
    std::regex hex_entity(R"(&#x([0-9A-Fa-f]+);)");

    result = std::regex_replace(result, numeric_entity, " ");
    result = std::regex_replace(result, hex_entity, " ");

    return result;
}

std::string remove_urls(const std::string& text) {
    std::regex url_pattern(R"((https?://|www\.)[^\s]+)");
    return std::regex_replace(text, url_pattern, "");
}

std::string normalize_whitespace(const std::string& text) {
    std::string result = text;

    // Multiple spaces -> one space
    result = std::regex_replace(result, std::regex(R"([ \t]+)"), " ");

    // Leading/trailing space on each line
    result = std::regex_replace(result, std::regex(R"(\n[ \t]+)"), "\n");
    result = std::regex_replace(result, std::regex(R"([ \t]+\n)"), "\n");

    // Multiple newlines -> max 2 newlines
    result = std::regex_replace(result, std::regex(R"(\n{3,})"), "\n\n");

    // Trim start and end
    size_t start = result.find_first_not_of(" \n\t\r");
    size_t end = result.find_last_not_of(" \n\t\r");

    if (start == std::string::npos) return "";
    return result.substr(start, end - start + 1);
}

std::string extract_text_from_html(const std::string& html) {
    std::string text = html;

    // 1. Remove script tags and content
    text = std::regex_replace(text,
        std::regex(R"(<script[^>]*>.*?</script>)", std::regex::icase), "");

    // 2. Remove style tags and content
    text = std::regex_replace(text,
        std::regex(R"(<style[^>]*>.*?</style>)", std::regex::icase), "");

    // 3. Remove navigation, header, footer, aside
    text = std::regex_replace(text,
        std::regex(R"(<nav[^>]*>.*?</nav>)", std::regex::icase), "");
    text = std::regex_replace(text,
        std::regex(R"(<header[^>]*>.*?</header>)", std::regex::icase), "");
    text = std::regex_replace(text,
        std::regex(R"(<footer[^>]*>.*?</footer>)", std::regex::icase), "");
    text = std::regex_replace(text,
        std::regex(R"(<aside[^>]*>.*?</aside>)", std::regex::icase), "");

    // 4. Remove forms (usually noise)
    text = std::regex_replace(text,
        std::regex(R"(<form[^>]*>.*?</form>)", std::regex::icase), "");

    // 5. Replace block-level closing tags with newlines
    text = std::regex_replace(text,
        std::regex(R"(</(div|p|h[1-6]|li|tr|td|section|article|blockquote)>)",
        std::regex::icase), "\n");

    // 6. Replace <br> with newline
    text = std::regex_replace(text,
        std::regex(R"(<br\s*/?>)", std::regex::icase), "\n");

    // 7. Remove ALL remaining HTML tags
    text = std::regex_replace(text, std::regex(R"(<[^>]*>)"), "");

    // 8. Decode HTML entities
    text = decode_html_entities(text);

    // 9. Remove URLs
    text = remove_urls(text);

    // 10. Normalize whitespace
    text = normalize_whitespace(text);

    return text;
}
