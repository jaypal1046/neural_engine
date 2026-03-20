#pragma once
#include <string>
#include <vector>
#include <map>
#include <functional>

namespace AgentBrain {

    struct Tool {
        std::string name;
        std::string description;
        std::string parameters; // JSON-like description
        std::function<std::string(const std::string&)> execute;
    };

    class Agent {
    public:
        Agent(const std::string& model_type = "ollama"); // "claude", "gpt", "ollama"
        
        // Main loop for task execution
        std::string perform_task(const std::string& task);

        // Tool management
        void register_tool(const Tool& tool);
        
        // System Prompt Initialization
        std::string get_system_prompt();

    private:
        std::string m_model_type;
        std::vector<Tool> m_tools;
        std::string m_context;

        // Model Interaction
        std::string query_model(const std::string& prompt);
        
        // Action Dispatcher
        std::string handle_agent_action(const std::string& model_output);
    };

    // Standard Tools Initialization
    void register_standard_tools(Agent& agent);

    // Command Line Entry Point
    int run_agentic_brain(int argc, char** argv);

}
