# Nero Brain: Local AI Coding Assistant

Build a private AI brain for software development that understands the full project locally and helps developers code, review, and navigate with confidence.

## 🚀 Vision
This project is a local AI development assistant integrated into a VS Code-like editor. It connects to **Ollama** and uses locally installed models to analyze the codebase, explain architecture, answer project questions, assist with coding, and perform self-code reviews—all without sending source code to external services.

## 🧠 Why Nero Brain?
Many developers build features, but later struggle to understand the full project, file relationships, logic flow, or why specific decisions were made. **Nero Brain** solves this by becoming the project’s memory and explanation layer.

## ✨ Key Features
- **Local-First Q&A:** Ask questions about your project knowing your data is private.
- **Autonomous Tools:** The AI can read files, list directories, and run commands to investigate the codebase.
- **Architectural Insights:** Understand file relationships and high-level structure instantly.
- **Self-Independent:** No API keys required when using local Ollama models.

## 🛠️ Architecture
The system consists of three layers:
1.  **React Frontend:** A modern, VS Code-inspired desktop editor.
2.  **C++ Neural Engine:** The high-performance core that handles agent tasks, reasoning, and tool execution.
3.  **FastAPI Backend:** A lightweight bridge connecting the UI to the C++ engine.

## ⚡ Quick Start
1.  **Build the Engine:**
    ```powershell
    .\build_neural_engine.bat
    ```
2.  **Run the Server:**
    ```powershell
    cd server
    python main.py
    ```
3.  **Launch the Editor:**
    ```powershell
    cd desktop_app
    npm run dev
    ```

---
*Built with 1,046 Neural Advisors.*
