import os
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from task_intelligence import LocalTaskIntelligence  # noqa: E402


class LocalTaskIntelligenceTests(unittest.TestCase):
    def test_storage_files_are_created_under_local_appdata(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                created = (
                    intelligence.router_config_file.exists(),
                    intelligence.workspace_profile_file.exists(),
                    intelligence.analysis_cache_file.exists(),
                    intelligence.command_discovery_file.exists(),
                    intelligence.command_stats_file.exists(),
                    intelligence.command_preferences_file.exists(),
                    intelligence.task_log_file.exists(),
                )
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(created, (True, True, True, True, True, True, True))

    def test_project_explain_request_routes_to_context_chat(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence._candidate_steps = lambda _message, _task: []
                result = intelligence.prepare_task("Explain server/main.py and the chat function flow", allow_web=False)
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["route"], "context_chat")
        self.assertTrue(result["requires_local_analysis"])
        self.assertIn("server/main.py", result["analysis_summary"])
        self.assertIn("Discovered local commands", result["analysis_summary"])

    def test_deictic_request_uses_active_editor_context(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence.editor_context_file.write_text(
                    '{"workspaceRoot": "C:\\\\Jay\\\\_Plugin\\\\compress", "activeFilePath": "C:\\\\Jay\\\\_Plugin\\\\compress\\\\server\\\\main.py", "relativePath": "server/main.py", "cursorLine": 2401, "currentSymbolName": "run_context_chat", "selectedText": "def run_context_chat(req, user_message):"}',
                    encoding="utf-8",
                )
                intelligence._candidate_steps = lambda _message, _task: []
                result = intelligence.prepare_task("Explain this function", allow_web=False)
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["route"], "context_chat")
        self.assertIn("server/main.py", result["analysis_summary"])
        self.assertIn("Current selected code preview", result["analysis_summary"])
        self.assertIn("symbol:run_context_chat", result["target_hints"])

    def test_review_request_routes_to_review_chat(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence._candidate_steps = lambda _message, _task: []
                result = intelligence.prepare_task("Review server/main.py for bugs and missing tests", allow_web=False)
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["route"], "review_chat")
        self.assertEqual(result["intent"], "review")

    def test_modify_request_routes_to_modify_chat(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence._candidate_steps = lambda _message, _task: []
                result = intelligence.prepare_task("Fix this function and update the logic", allow_web=False)
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["route"], "modify_chat")
        self.assertEqual(result["intent"], "modify")

    def test_flow_explanation_request_does_not_route_to_modify_chat(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence._candidate_steps = lambda _message, _task: []
                result = intelligence.prepare_task("flow from editor selection to modify patch", allow_web=False)
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["route"], "context_chat")
        self.assertEqual(result["intent"], "explain_or_review")

    def test_desktop_target_prefers_desktop_analysis_steps(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                steps = intelligence._candidate_steps("Review desktop_app/src/App.tsx and run lint", {
                    "intent": "review",
                    "target_hints": ["desktop_app/src/App.tsx"],
                })
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        step_names = {step["name"] for step in steps}
        self.assertIn("desktop_lint", step_names)
        self.assertNotIn("python_compileall", step_names)
        self.assertEqual(len(steps), 1)

    def test_explain_request_prefers_single_fast_validation_step(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                steps = intelligence._candidate_steps("Explain server/main.py and chat flow", {
                    "intent": "explain_or_review",
                    "target_hints": ["server/main.py"],
                })
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(len(steps), 1)
        self.assertEqual(steps[0]["name"], "python_compileall")

    def test_command_stats_are_updated_after_run(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                result = intelligence._run_step({
                    "name": "quick_python_version",
                    "cwd": str(REPO_ROOT),
                    "command": ["py", "-3", "--version"],
                    "timeout": 10,
                    "category": "validate",
                    "reason": "Test probe",
                })
                stats = intelligence._load_json(intelligence.command_stats_file, {})
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertEqual(result["status"], "ok")
        self.assertIn("quick_python_version", stats.get("commands", {}))
        self.assertGreaterEqual(stats["commands"]["quick_python_version"]["runs"], 1)

    def test_command_preference_moves_preferred_check_to_front(self):
        with tempfile.TemporaryDirectory() as temp_local:
            original_local = os.environ.get("LOCALAPPDATA")
            try:
                os.environ["LOCALAPPDATA"] = temp_local
                intelligence = LocalTaskIntelligence(str(REPO_ROOT))
                intelligence.set_command_preference("review", "desktop_build")
                steps = intelligence._candidate_steps("Review desktop_app/src/App.tsx and run build lint", {
                    "intent": "review",
                    "target_hints": ["desktop_app/src/App.tsx"],
                })
                prefs = intelligence._load_json(intelligence.command_preferences_file, {})
            finally:
                if original_local is None:
                    os.environ.pop("LOCALAPPDATA", None)
                else:
                    os.environ["LOCALAPPDATA"] = original_local

        self.assertTrue(steps)
        self.assertEqual(steps[0]["name"], "desktop_build")
        self.assertEqual(prefs["intents"]["review"]["preferred_commands"][0], "desktop_build")


if __name__ == "__main__":
    unittest.main()
