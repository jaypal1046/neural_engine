import sys
import tempfile
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from symbol_graph import SymbolGraph  # noqa: E402


class SymbolGraphTests(unittest.TestCase):
    def test_describes_symbol_definitions_and_references(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            server_dir = workspace / "server"
            server_dir.mkdir(parents=True, exist_ok=True)

            (server_dir / "main.py").write_text(
                "def run_context_chat(message):\n"
                "    return helper(message)\n"
                "\n"
                "def helper(value):\n"
                "    return value\n",
                encoding="utf-8",
            )
            (server_dir / "worker.py").write_text(
                "from server.main import run_context_chat\n"
                "\n"
                "def handle_request():\n"
                "    return run_context_chat('hi')\n",
                encoding="utf-8",
            )

            graph = SymbolGraph(str(workspace))
            summary = graph.describe_symbol("run_context_chat")

        self.assertIsNotNone(summary)
        self.assertEqual(summary["definitions"][0]["path"], "server/main.py")
        self.assertTrue(any(item["path"] == "server/worker.py" for item in summary["references"]))
        self.assertTrue(any(item["kind"] == "call" for item in summary["callers"]))

    def test_describes_file_dependencies_and_dependents(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            workspace = Path(temp_dir)
            server_dir = workspace / "server"
            desktop_dir = workspace / "desktop_app" / "src"
            server_dir.mkdir(parents=True, exist_ok=True)
            desktop_dir.mkdir(parents=True, exist_ok=True)

            (server_dir / "helper.py").write_text(
                "def helper(value):\n"
                "    return value\n",
                encoding="utf-8",
            )
            (server_dir / "main.py").write_text(
                "from helper import helper\n"
                "\n"
                "def run_context_chat(message):\n"
                "    return helper(message)\n",
                encoding="utf-8",
            )
            (server_dir / "worker.py").write_text(
                "from main import run_context_chat\n"
                "\n"
                "def handle_request():\n"
                "    return run_context_chat('hi')\n",
                encoding="utf-8",
            )

            graph = SymbolGraph(str(workspace))
            summary = graph.describe_file("server/main.py")

        self.assertIsNotNone(summary)
        self.assertTrue(any(item["path"] == "server/helper.py" for item in summary["dependencies"]))
        self.assertTrue(any(item["path"] == "server/worker.py" for item in summary["dependents"]))


if __name__ == "__main__":
    unittest.main()
