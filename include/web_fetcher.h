#pragma once
#include <string>

// =============================================================================
// Web Fetcher - HTTP client for downloading web content
// =============================================================================

struct HttpResponse {
    int status_code;           // HTTP status (200 = OK, 404 = Not Found, etc.)
    std::string body;          // Response body (HTML, JSON, etc.)
    std::string content_type;  // MIME type (text/html, application/json, etc.)
    size_t content_length;     // Size in bytes
};

// Fetch URL and return response
// Automatically follows redirects, 30s timeout
HttpResponse fetch_url(const std::string& url);

// Fetch and save directly to file (for large downloads)
bool download_to_file(const std::string& url, const std::string& output_path);
