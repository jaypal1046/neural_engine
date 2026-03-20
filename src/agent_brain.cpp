#include "agent_brain.h"
#include "web_fetcher.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <array>
#include <memory>
#include <algorithm>
#include <chrono>
#include <cstdlib>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")
#endif

namespace AgentBrain {

    std::string exec_command(const std::string& cmd);

    namespace {
        std::filesystem::path workspace_root() {
            return std::filesystem::weakly_canonical(std::filesystem::current_path());
        }

        std::string trim_copy(const std::string& value) {
            const auto start = value.find_first_not_of(" \t\r\n");
            if (start == std::string::npos) return "";
            const auto end = value.find_last_not_of(" \t\r\n");
            return value.substr(start, end - start + 1);
        }

        bool is_safe_workspace_path(const std::filesystem::path& path) {
            try {
                const auto canonical_root = workspace_root();
                const auto canonical_path = std::filesystem::weakly_canonical(path);
                const auto root_str = canonical_root.generic_string();
                const auto path_str = canonical_path.generic_string();
                return path_str == root_str || path_str.rfind(root_str + "/", 0) == 0;
            } catch (...) {
                return false;
            }
        }

        std::filesystem::path resolve_workspace_path(const std::string& raw_path) {
            auto candidate = trim_copy(raw_path);
            if (candidate.empty()) return {};

            std::filesystem::path path(candidate);
            if (path.is_relative()) {
                path = workspace_root() / path;
            }
            return path;
        }

        bool command_available(const std::string& command) {
#ifdef _WIN32
            const auto probe = exec_command(command + " --version 2>&1");
            return probe.find("not recognized") == std::string::npos &&
                   probe.find("No installed Python") == std::string::npos;
#else
            const auto probe = exec_command(command + " --version 2>&1");
            return probe.find("not found") == std::string::npos;
#endif
        }

        std::string detect_python_command() {
            const char* env_python = std::getenv("NERO_PYTHON_CMD");
            if (env_python && *env_python) {
                return env_python;
            }
#ifdef _WIN32
            const std::array<std::string, 3> candidates = {"python", "py -3", "py"};
#else
            const std::array<std::string, 2> candidates = {"python3", "python"};
#endif
            for (const auto& candidate : candidates) {
                if (command_available(candidate)) {
                    return candidate;
                }
            }
            return "";
        }

        std::string json_escape(const std::string& value) {
            std::string escaped;
            escaped.reserve(value.size() + 32);
            for (char ch : value) {
                switch (ch) {
                case '\\': escaped += "\\\\"; break;
                case '"': escaped += "\\\""; break;
                case '\n': escaped += "\\n"; break;
                case '\r': escaped += "\\r"; break;
                case '\t': escaped += "\\t"; break;
                default: escaped += ch; break;
                }
            }
            return escaped;
        }

        std::string json_unescape(const std::string& value) {
            std::string unescaped;
            unescaped.reserve(value.size());
            bool escaping = false;
            for (char ch : value) {
                if (!escaping) {
                    if (ch == '\\') escaping = true;
                    else unescaped += ch;
                    continue;
                }

                switch (ch) {
                case 'n': unescaped += '\n'; break;
                case 'r': unescaped += '\r'; break;
                case 't': unescaped += '\t'; break;
                case '\\': unescaped += '\\'; break;
                case '"': unescaped += '"'; break;
                default: unescaped += ch; break;
                }
                escaping = false;
            }
            return unescaped;
        }

        std::string extract_json_string_field(const std::string& body, const std::string& field) {
            const auto marker = "\"" + field + "\":\"";
            const auto start = body.find(marker);
            if (start == std::string::npos) return "";

            std::string value;
            bool escaping = false;
            for (size_t i = start + marker.size(); i < body.size(); ++i) {
                const char ch = body[i];
                if (!escaping && ch == '"') {
                    return json_unescape(value);
                }
                if (!escaping && ch == '\\') {
                    escaping = true;
                    value += ch;
                    continue;
                }
                escaping = false;
                value += ch;
            }
            return "";
        }

        std::string query_ollama_local(const std::string& prompt) {
            const char* env_model = std::getenv("OLLAMA_MODEL");
            const std::string model = (env_model && *env_model) ? env_model : "llama3:latest";
            const std::string request_body =
                "{\"model\":\"" + json_escape(model) +
                "\",\"prompt\":\"" + json_escape(prompt) +
                "\",\"stream\":false}";

#ifdef _WIN32
            HINTERNET session = WinHttpOpen(
                L"NeroBrain/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0
            );
            if (!session) {
                return "Error: Cannot open WinHTTP session for Ollama";
            }

            HINTERNET connection = WinHttpConnect(session, L"127.0.0.1", 11434, 0);
            if (!connection) {
                WinHttpCloseHandle(session);
                return "Error: Cannot connect to local Ollama";
            }

            HINTERNET request = WinHttpOpenRequest(
                connection,
                L"POST",
                L"/api/generate",
                NULL,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                0
            );
            if (!request) {
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return "Error: Cannot create Ollama request";
            }

            const wchar_t* headers = L"Content-Type: application/json\r\n";
            BOOL sent = WinHttpSendRequest(
                request,
                headers,
                -1L,
                (LPVOID)request_body.data(),
                static_cast<DWORD>(request_body.size()),
                static_cast<DWORD>(request_body.size()),
                0
            );
            if (sent) {
                sent = WinHttpReceiveResponse(request, NULL);
            }
            if (!sent) {
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connection);
                WinHttpCloseHandle(session);
                return "Error: Local Ollama request failed";
            }

            DWORD status_code = 0;
            DWORD status_size = sizeof(status_code);
            WinHttpQueryHeaders(
                request,
                WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX,
                &status_code,
                &status_size,
                WINHTTP_NO_HEADER_INDEX
            );

            std::string response_body;
            DWORD available = 0;
            do {
                available = 0;
                if (!WinHttpQueryDataAvailable(request, &available)) break;
                if (available == 0) break;

                std::string chunk(available, '\0');
                DWORD read = 0;
                if (!WinHttpReadData(request, chunk.data(), available, &read)) break;
                chunk.resize(read);
                response_body += chunk;
            } while (available > 0);

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connection);
            WinHttpCloseHandle(session);

            if (status_code != 200) {
                return "Error: Ollama HTTP " + std::to_string(status_code) + " " + response_body;
            }

            const auto response_text = extract_json_string_field(response_body, "response");
            return response_text.empty() ? "Error: Ollama response missing body" : response_text;
#else
            return "Error: Native Ollama bridge is only implemented for Windows builds";
#endif
        }
    }

    // Helper to run a command and capture output
    std::string exec_command(const std::string& cmd) {
        std::array<char, 128> buffer;
        std::string result;
#ifdef _WIN32
        std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd.c_str(), "r"), _pclose);
#else
        std::unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd.c_str(), "r"), pclose);
#endif
        if (!pipe) {
            return "Error: popen() failed!";
        }
        while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
            result += buffer.data();
        }
        return result;
    }

    Agent::Agent(const std::string& model_type) : m_model_type(model_type) {
        if (m_model_type.empty()) m_model_type = "ollama"; // USE OLLAMA BY DEFAULT (LOCAL, NO API KEY)
        register_standard_tools(*this);
    }

    void Agent::register_tool(const Tool& tool) {
        m_tools.push_back(tool);
    }

    std::string Agent::get_system_prompt() {
        std::string prompt = "You are a private AI development brain integrated into the Nero brain system.\n";
        prompt += "Vision: Build a private AI brain for software development that understands the full project locally and helps developers code, review, and navigate with confidence.\n";
        prompt += "Goal: You are a local AI development assistant using locally installed models (Ollama/Claude) to analyze the codebase, explain architecture, answer project questions, assist with coding, and perform self-code reviews, all without sending source code externally.\n";
        prompt += "Role: You act as the project memory and explanation layer, helping developers understand file relationships, logic flow, and architectural decisions.\n\n";
        prompt += "Tools available to you:\n";
        for (const auto& tool : m_tools) {
            prompt += "- " + tool.name + ": " + tool.description + "\n";
            prompt += "  Parameters: " + tool.parameters + "\n";
        }
        prompt += "\nResponse format:\n";
        prompt += "THOUGHT: <your reasoning>\n";
        prompt += "ACTION: <tool_name>(<parameters>)\n";
        prompt += "WAIT: <observation>\n";
        prompt += "FINAL_ANSWER: <your final answer>\n\n";
        prompt += "You can use only one tool at a time. After ACTION, stop and wait for observation.\n";
        return prompt;
    }

    std::string Agent::query_model(const std::string& prompt) {
        std::string ollama_error;
        if (m_model_type == "ollama") {
            const auto direct_response = query_ollama_local(prompt);
            if (direct_response.rfind("Error:", 0) != 0) {
                return direct_response;
            }
            ollama_error = direct_response;
        }

        const auto python_command = detect_python_command();
        if (python_command.empty()) {
            if (!ollama_error.empty()) {
                return ollama_error + " | Python fallback unavailable";
            }
            return "Error: No Python runtime available for model bridge";
        }

        const auto temp_name = "nero_prompt_" + std::to_string(
            std::chrono::steady_clock::now().time_since_epoch().count()
        ) + ".txt";
        const auto prompt_file = std::filesystem::temp_directory_path() / temp_name;
        {
            std::ofstream prompt_stream(prompt_file, std::ios::binary);
            if (!prompt_stream.is_open()) {
                return "Error: Could not create temporary prompt file";
            }
            prompt_stream << prompt;
        }

        std::string cmd;
        if (m_model_type == "claude") {
            cmd = python_command + " query_claude.py --prompt-file \"" + prompt_file.string() + "\"";
        } else if (m_model_type == "gpt") {
            cmd = python_command + " query_chatgpt.py --prompt-file \"" + prompt_file.string() + "\"";
        } else if (m_model_type == "ollama") {
            cmd = python_command + " query_ollama.py --prompt-file \"" + prompt_file.string() + "\"";
        } else {
            std::error_code ec;
            std::filesystem::remove(prompt_file, ec);
            return "Error: Unknown model type";
        }

        std::string output = exec_command(cmd);
        std::error_code ec;
        std::filesystem::remove(prompt_file, ec);
        
        // Extract content between markers
        std::string start_marker, end_marker;
        if (m_model_type == "claude") { start_marker = "CLAUDE_ANSWER_START"; end_marker = "CLAUDE_ANSWER_END"; }
        else if (m_model_type == "gpt") { start_marker = "GPT_ANSWER_START"; end_marker = "GPT_ANSWER_END"; }
        else { start_marker = "OLLAMA_ANSWER_START"; end_marker = "OLLAMA_ANSWER_END"; }

        size_t start = output.find(start_marker);
        size_t end = output.find(end_marker);
        if (start != std::string::npos && end != std::string::npos) {
            return output.substr(start + start_marker.length() + 1, end - start - start_marker.length() - 2);
        }
        return output;
    }

    std::string Agent::handle_agent_action(const std::string& model_output) {
        size_t action_pos = model_output.find("ACTION: ");
        if (action_pos == std::string::npos) return "";

        std::string action_line = model_output.substr(action_pos + 8);
        size_t paren_pos = action_line.find("(");
        if (paren_pos == std::string::npos) return "Error: Invalid action format";

        std::string tool_name = trim_copy(action_line.substr(0, paren_pos));
        std::string params = trim_copy(action_line.substr(paren_pos + 1));
        if (!params.empty() && params.back() == ')') params.pop_back();
        params = trim_copy(params);

        for (const auto& tool : m_tools) {
            if (tool.name == tool_name) {
                std::cout << "[AGENT] Executing " << tool_name << "(" << params << ")..." << std::endl;
                return tool.execute(params);
            }
        }

        return "Error: Tool not found";
    }

    std::string Agent::perform_task(const std::string& task) {
        m_context = "TASK: " + task + "\n\n";
        bool completed = false;
        int steps = 0;
        int max_steps = 10;

        while (!completed && steps < max_steps) {
            std::string full_prompt = get_system_prompt() + m_context;
            std::string model_output = query_model(full_prompt);
            
            std::cout << "[AGENT] Thought: " << model_output << std::endl;
            m_context += model_output + "\n";

            if (model_output.find("FINAL_ANSWER: ") != std::string::npos) {
                completed = true;
                break;
            }

            std::string observation = handle_agent_action(model_output);
            if (!observation.empty()) {
                m_context += "OBSERVATION: " + observation + "\n";
            } else {
                completed = true; // No action found, might be an error or just plain text
            }
            steps++;
        }

        return m_context;
    }

    void register_standard_tools(Agent& agent) {
        // Read File Tool
        agent.register_tool({
            "ReadFile",
            "Read content of a file",
            "path: string",
            [](const std::string& path) -> std::string {
                const auto resolved = resolve_workspace_path(path);
                if (resolved.empty() || !is_safe_workspace_path(resolved)) {
                    return "Error: Access denied outside workspace";
                }
                std::ifstream f(resolved, std::ios::binary);
                if (!f.is_open()) return "Error: Could not open " + resolved.string();
                std::stringstream buffer;
                buffer << f.rdbuf();
                auto content = buffer.str();
                if (content.size() > 200000) {
                    content.resize(200000);
                    content += "\n...[truncated]";
                }
                return content;
            }
        });

        // List Directory Tool
        agent.register_tool({
            "ListDir",
            "List items in a directory",
            "path: string",
            [](const std::string& path) -> std::string {
                std::string result;
                try {
                    const auto resolved = path.empty() ? workspace_root() : resolve_workspace_path(path);
                    if (resolved.empty() || !is_safe_workspace_path(resolved)) {
                        return "Error: Access denied outside workspace";
                    }
                    for (const auto& entry : std::filesystem::directory_iterator(resolved)) {
                        result += entry.path().filename().string() + (entry.is_directory() ? "/" : "") + "\n";
                    }
                } catch (...) { return "Error listing directory"; }
                return result;
            }
        });

        // Fetch Web Tool
        agent.register_tool({
            "FetchWeb",
            "Download content from a URL",
            "url: string",
            [](const std::string& url) -> std::string {
                HttpResponse resp = fetch_url(url);
                if (resp.status_code != 200) return "Error: HTTP " + std::to_string(resp.status_code);
                return resp.body.substr(0, 5000); // Limit size
            }
        });

    }

    int run_agentic_brain(int argc, char** argv) {
        if (argc < 2) {
            std::cout << "Usage: neural_engine agent_task <task> [--model claude|gpt|ollama]\n";
            return 1;
        }

        std::string task = argv[1];
        std::string model = ""; // Let Agent constructor handle default

        for (int i = 2; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--model" && i + 1 < argc) {
                model = argv[++i];
            }
        }

        std::cout << "\n--- INITIALIZING LOCAL AGENTIC BRAIN ---\n";
        Agent agent(model);
        std::string result = agent.perform_task(task);
        
        std::cout << "\n--- TASK COMPLETED ---\n";
        // Final result is already in the context, but let's print a clean summary if possible
        size_t final_pos = result.rfind("\nFINAL_ANSWER: ");
        if (final_pos != std::string::npos) {
            std::cout << "\nRESULT: " << result.substr(final_pos + 15) << "\n";
        }

        return 0;
    }

}
