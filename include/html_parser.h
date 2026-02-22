#pragma once
#include <string>

// =============================================================================
// HTML Parser - Extract clean text from HTML content
// =============================================================================

// Extract pure text from HTML, removing:
// - Scripts, styles, navigation, headers, footers
// - All HTML tags
// - Excessive whitespace
// Returns clean, readable text suitable for compression/learning
std::string extract_text_from_html(const std::string& html);

// Decode HTML entities (&amp; -> &, &lt; -> <, etc.)
std::string decode_html_entities(const std::string& text);

// Remove URLs from text
std::string remove_urls(const std::string& text);

// Clean and normalize whitespace
std::string normalize_whitespace(const std::string& text);
