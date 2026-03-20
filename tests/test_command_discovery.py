import json
import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from command_discovery import discover_workspace_commands, match_commands_for_file  # noqa: E402


class CommandDiscoveryTests(unittest.TestCase):
    def test_discovers_manifest_and_fallback_commands(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            (workspace / "server").mkdir()
            (workspace / "tests").mkdir()
            (workspace / "tests" / "test_sample.py").write_text("import unittest\n", encoding="utf-8")
            (workspace / "desktop_app").mkdir()
            (workspace / "desktop_app" / "package.json").write_text(json.dumps({
                "scripts": {
                    "build": "vite build",
                    "lint": "eslint .",
                }
            }), encoding="utf-8")
            (workspace / "build_neural_engine.bat").write_text("@echo off\r\necho build\r\n", encoding="utf-8")

            discovery = discover_workspace_commands(str(workspace), str(workspace))

        command_names = {command["name"] for command in discovery["commands"]}
        self.assertIn("python_compileall", command_names)
        self.assertIn("python_unittest", command_names)
        self.assertIn("desktop_build", command_names)
        self.assertIn("desktop_lint", command_names)
        self.assertIn("cpp_build", command_names)

    def test_matches_commands_for_file_by_prefix(self):
        discovery = discover_workspace_commands(str(REPO_ROOT))
        python_matches = match_commands_for_file(discovery, "server/main.py")
        desktop_matches = match_commands_for_file(discovery, "desktop_app/src/App.tsx")

        self.assertTrue(python_matches)
        self.assertEqual(python_matches[0]["name"], "python_compileall")
        self.assertTrue(desktop_matches)
        self.assertEqual(desktop_matches[0]["name"], "desktop_build")


if __name__ == "__main__":
    unittest.main()
