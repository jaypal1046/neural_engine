import hashlib
import json
import os
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from context_provider import ContextProvider  # noqa: E402


class ContextProviderMemoryTests(unittest.TestCase):
    def test_build_context_reads_separate_workspace_memory(self):
        workspace_root = REPO_ROOT
        workspace_id = hashlib.sha1(str(workspace_root).lower().encode("utf-8")).hexdigest()[:16]

        with tempfile.TemporaryDirectory() as temp_appdata, tempfile.TemporaryDirectory() as temp_local:
            memory_dir = Path(temp_appdata) / "desktop_app" / "project-memory" / workspace_id
            memory_dir.mkdir(parents=True, exist_ok=True)
            local_workspace_dir = Path(temp_local) / "NeuralStudio" / "workspaces" / workspace_id
            local_workspace_dir.mkdir(parents=True, exist_ok=True)

            workspace_state = {
                "lastActivityAt": "2026-03-19T10:15:00Z",
                "eventCounts": {
                    "file_accessed": 4,
                    "command_executed": 2,
                },
                "recentFiles": [
                    {"path": str(workspace_root / "server" / "main.py")},
                    {"path": str(workspace_root / "desktop_app" / "src" / "App.tsx")},
                ],
                "recentCommands": [
                    {"command": "npm run build"},
                    {"command": "bin\\neural_engine.exe agent_task \"hi\" --model ollama"},
                ],
            }
            (memory_dir / "workspace.json").write_text(
                json.dumps(workspace_state),
                encoding="utf-8",
            )
            (memory_dir / "events.jsonl").write_text(
                json.dumps({
                    "type": "file_accessed",
                    "timestamp": "2026-03-19T10:14:00Z",
                    "filePath": str(workspace_root / "server" / "main.py"),
                }) + "\n",
                encoding="utf-8",
            )
            (local_workspace_dir / "editor_context.json").write_text(
                json.dumps({
                    "workspaceRoot": str(workspace_root),
                    "activeFilePath": str(workspace_root / "server" / "main.py"),
                    "relativePath": "server/main.py",
                    "cursorLine": 2401,
                    "cursorColumn": 1,
                    "currentSymbolName": "run_context_chat",
                    "selection": {
                        "startLine": 2401,
                        "endLine": 2402,
                    },
                    "selectedText": "def run_context_chat(req, user_message):\n    if OllamaAdapter is None:",
                }),
                encoding="utf-8",
            )

            original_appdata = os.environ.get("APPDATA")
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["APPDATA"] = temp_appdata
                os.environ["LOCALAPPDATA"] = temp_local
                provider = ContextProvider(str(workspace_root))
                result = provider.build_context("explain this file and recent commands", max_files=2)
            finally:
                if original_appdata is None:
                    os.environ.pop("APPDATA", None)
                else:
                    os.environ["APPDATA"] = original_appdata
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertIn("Recently executed terminal commands", result["workspace_summary"])
        self.assertIn("Active editor target", result["workspace_summary"])
        self.assertIn("PROJECT MEMORY:", result["context_text"])
        self.assertIn("ACTIVE EDITOR SELECTION:", result["context_text"])
        self.assertTrue(any(source["path"] == "server/main.py" for source in result["sources"]))

    def test_symbol_query_prefers_symbol_definition(self):
        provider = ContextProvider(str(REPO_ROOT))
        result = provider.build_context("Explain run_context_chat function in server/main.py", max_files=2)

        self.assertTrue(any(source["path"] == "server/main.py" for source in result["sources"]))
        self.assertTrue(any("symbol:run_context_chat" in source.get("reason", "") for source in result["sources"]))

    def test_symbol_query_includes_symbol_flow_summary(self):
        provider = ContextProvider(str(REPO_ROOT))
        result = provider.build_context("Explain how run_modify_chat is used and where it flows", max_files=3)

        self.assertIn("SYMBOL FLOW: run_modify_chat", result["context_text"])
        self.assertTrue(any("symbol-flow-definition:run_modify_chat" in source.get("reason", "") for source in result["sources"]))
        self.assertTrue(any(section.get("type") == "symbol" and section.get("title") == "run_modify_chat" for section in result.get("flow_sections", [])))

    def test_file_flow_query_includes_dependency_summary(self):
        provider = ContextProvider(str(REPO_ROOT))
        result = provider.build_context("Explain the architecture flow of server/main.py", max_files=3)

        self.assertIn("FILE FLOW: server/main.py", result["context_text"])
        self.assertTrue(any("file-flow-" in source.get("reason", "") for source in result["sources"]))
        self.assertTrue(any(section.get("type") == "file" and section.get("title") == "server/main.py" for section in result.get("flow_sections", [])))

    def test_modify_query_includes_impact_assessment(self):
        provider = ContextProvider(str(REPO_ROOT))
        result = provider.build_context("If I change run_modify_chat, what will it impact?", max_files=3)

        self.assertIn("IMPACT SUMMARY:", result["context_text"])
        self.assertTrue(any(source.get("reason") == "impact-assessment" for source in result["sources"]))
        self.assertTrue(any(section.get("type") == "impact" for section in result.get("flow_sections", [])))


if __name__ == "__main__":
    unittest.main()
