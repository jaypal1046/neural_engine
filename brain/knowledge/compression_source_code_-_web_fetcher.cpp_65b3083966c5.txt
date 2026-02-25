#include "web_fetcher.h"
#include <iostream>
#include <fstream>

// Cross-platform HTTP implementation
// Windows: use WinHTTP, Linux/Mac: use libcurl
// For simplicity, we'll use a fallback that works everywhere

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

HttpResponse fetch_url(const std::string& url) {
    HttpResponse resp;
    resp.status_code = -1;

    // Parse URL
    std::wstring wide_url(url.begin(), url.end());

    URL_COMPONENTS urlComp = {0};
    urlComp.dwStructSize = sizeof(urlComp);
    wchar_t hostName[256] = {0};
    wchar_t urlPath[2048] = {0};
    urlComp.lpszHostName = hostName;
    urlComp.dwHostNameLength = 256;
    urlComp.lpszUrlPath = urlPath;
    urlComp.dwUrlPathLength = 2048;

    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &urlComp)) {
        resp.body = "Error: Invalid URL";
        return resp;
    }

    // Initialize WinHTTP
    HINTERNET hSession = WinHttpOpen(
        L"SmartBrain/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME,
        WINHTTP_NO_PROXY_BYPASS,
        0
    );

    if (!hSession) {
        resp.body = "Error: Cannot initialize HTTP session";
        return resp;
    }

    // Connect
    HINTERNET hConnect = WinHttpConnect(
        hSession,
        hostName,
        urlComp.nPort,
        0
    );

    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        resp.body = "Error: Cannot connect to host";
        return resp;
    }

    // Open request
    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(
        hConnect,
        L"GET",
        urlPath,
        NULL,
        WINHTTP_NO_REFERER,
        WINHTTP_DEFAULT_ACCEPT_TYPES,
        flags
    );

    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        resp.body = "Error: Cannot create request";
        return resp;
    }

    // Send request
    BOOL bResults = WinHttpSendRequest(
        hRequest,
        WINHTTP_NO_ADDITIONAL_HEADERS,
        0,
        WINHTTP_NO_REQUEST_DATA,
        0,
        0,
        0
    );

    if (bResults) {
        bResults = WinHttpReceiveResponse(hRequest, NULL);
    }

    if (bResults) {
        // Get status code
        DWORD statusCode = 0;
        DWORD dwSize = sizeof(statusCode);
        WinHttpQueryHeaders(
            hRequest,
            WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
            WINHTTP_HEADER_NAME_BY_INDEX,
            &statusCode,
            &dwSize,
            WINHTTP_NO_HEADER_INDEX
        );
        resp.status_code = statusCode;

        // Read data
        std::string body;
        DWORD dwAvailable = 0;
        do {
            dwAvailable = 0;
            if (!WinHttpQueryDataAvailable(hRequest, &dwAvailable)) break;

            if (dwAvailable > 0) {
                char* buffer = new char[dwAvailable + 1];
                DWORD dwRead = 0;

                if (WinHttpReadData(hRequest, buffer, dwAvailable, &dwRead)) {
                    buffer[dwRead] = 0;
                    body.append(buffer, dwRead);
                }

                delete[] buffer;
            }
        } while (dwAvailable > 0);

        resp.body = body;
        resp.content_length = body.size();
    } else {
        resp.body = "Error: Request failed";
    }

    // Cleanup
    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);

    return resp;
}

bool download_to_file(const std::string& url, const std::string& output_path) {
    auto resp = fetch_url(url);
    if (resp.status_code != 200) return false;

    std::ofstream f(output_path, std::ios::binary);
    if (!f) return false;

    f.write(resp.body.data(), resp.body.size());
    return true;
}

#else
// Unix/Linux/Mac: Use system curl command as fallback
#include <cstdlib>

HttpResponse fetch_url(const std::string& url) {
    HttpResponse resp;
    resp.status_code = -1;

    // Use curl command-line tool
    std::string temp_file = "/tmp/smart_brain_download.tmp";
    std::string cmd = "curl -L -s -o " + temp_file + " -w \"%{http_code}\" \"" + url + "\"";

    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) {
        resp.body = "Error: Cannot execute curl";
        return resp;
    }

    char buffer[128];
    std::string status_str;
    while (fgets(buffer, sizeof(buffer), pipe)) {
        status_str += buffer;
    }
    pclose(pipe);

    // Parse status code
    try {
        resp.status_code = std::stoi(status_str);
    } catch (...) {
        resp.status_code = -1;
    }

    // Read downloaded file
    std::ifstream f(temp_file, std::ios::binary);
    if (f) {
        resp.body.assign(
            (std::istreambuf_iterator<char>(f)),
            std::istreambuf_iterator<char>()
        );
        resp.content_length = resp.body.size();
    }

    // Cleanup
    std::remove(temp_file.c_str());

    return resp;
}

bool download_to_file(const std::string& url, const std::string& output_path) {
    std::string cmd = "curl -L -s -o \"" + output_path + "\" \"" + url + "\"";
    return system(cmd.c_str()) == 0;
}

#endif
