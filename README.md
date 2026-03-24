# Nero Brain: Private AI Coding Studio

Build a high-performance, private AI brain for software development that understands your full project locally. Nero Brain helps developers code, review, and navigate with absolute confidence and zero data leakage.

## 🚀 Vision
Nero Brain is a unified AI development environment integrated into a modern, Monaco-based editor. It connects to **Ollama** and leverages locally installed models (Llama 3, Phi-3, etc.) to provide deep codebase analysis, autonomous command execution, and professional-grade code review—all processed strictly on your hardware.

## ✨ Advanced Features

### 🛠️ Integrated AI Development Environment
- **Unified AI Popup:** Use `Ctrl+I` to modify code and `Ctrl+Shift+E` to explain selections directly in the editor.
- **Context-Aware Chat:** Real-time analysis of your active files and project structure.
- **Smart Symbol Navigation:** Instantly locate and understand definitions across the entire repository.

### 🧠 Specialized Skill Suite (New!)
Nero Brain now includes a modular skill system for high-level specialized tasks:
- **Data Analysis:** Process structured data and identify patterns locally.
- **Web Design Guidelines:** Apply modern UI/UX best practices and glassmorphism standards.
- **Consulting Analysis:** Generate professional strategic insights from your codebase.
- **Skill Creator:** Design and deploy new AI capabilities dynamically.

### ⚡ Seamless Setup & Performance
- **Automated Setup Wizard:** One-click environment check and automated installation of Ollama and required models.
- **C++ Neural Engine:** High-speed token processing and tool execution for near-instant responses.
- **Local-First RAG:** Optimized project indexing that respects your privacy.

## 🛠️ Architecture
1.  **Monaco Editor (Frontend):** Modern, VS Code-inspired UI with rich syntax highlighting and inline AI features.
2.  **C++ Neural Engine (Core):** The high-performance "brain" that manages background agents and CMIX compression.
3.  **FastAPI Bridge (Backend):** Orchestrates communication between the UI, the C++ engine, and local LLMs.

## ⚡ Quick Start

### 1. Build the Neural Engine
```powershell
.\build_neural_engine.bat
```

### 2. Configure Local Environment
Run the desktop application and follow the **Setup Wizard** to automatically install Ollama and your preferred models.

### 3. Launch Development Server
```powershell
cd server
python main.py
```

### 4. Run the Desktop Studio
```powershell
cd desktop_app
npm run dev
```

## ⚖️ License & Terms
- Read the full [MIT License](LICENSE)
- Read our [Terms & Conditions](TERMS.md)

---
*Built by Developers, for Developers. Unleash the power of Private Intelligence.*
