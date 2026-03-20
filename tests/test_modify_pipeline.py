import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from modify_pipeline import (  # noqa: E402
    apply_selection_to_text,
    build_impact_brief,
    build_workspace_validation_plan,
    build_unified_diff,
    combine_validation_results,
    extract_json_object,
    format_modify_markdown,
    validate_candidate_in_workspace,
)


class ModifyPipelineTests(unittest.TestCase):
    def test_extracts_modify_json(self):
        text = 'prefix {"summary":"ok","updated_code":"print(1)","target_file":"server/main.py","confidence":88} suffix'
        parsed = extract_json_object(text)
        self.assertIsNotNone(parsed)
        self.assertEqual(parsed["updated_code"], "print(1)")

    def test_applies_selection_to_text(self):
        source = "def hello():\n    return 1\n"
        updated = apply_selection_to_text(
            source,
            {
                "startLine": 2,
                "startColumn": 5,
                "endLine": 2,
                "endColumn": 13,
            },
            "return 2",
        )
        self.assertIn("return 2", updated)
        self.assertNotIn("return 1", updated)

    def test_formats_diff_markdown(self):
        diff_text = build_unified_diff("server/main.py", "a = 1\n", "a = 2\n")
        markdown = format_modify_markdown(
            "Patch proposal created.",
            "server/main.py",
            diff_text,
            {"status": "ok", "command": "py -3 -m py_compile temp.py", "summary": "Validation passed."},
            87,
        )
        self.assertIn("```diff", markdown)
        self.assertIn("Confidence: 87%", markdown)
        self.assertIn("server/main.py", markdown)

    def test_builds_impact_brief_from_flow_sections(self):
        impact = build_impact_brief([
            {
                "type": "impact",
                "title": "Impact Assessment",
                "entries": [
                    {
                        "label": "Caller of run_modify_chat",
                        "path": "server/main.py",
                        "line_start": 2555,
                        "snippet": "return run_modify_chat(req, user_message, task_prep)",
                    }
                ],
            }
        ])
        self.assertIn("Impact assessment:", impact)
        self.assertIn("Caller of run_modify_chat -> server/main.py:2555", impact)

    def test_builds_python_workspace_validation_plan(self):
        plan = build_workspace_validation_plan("server/main.py", str(REPO_ROOT))
        self.assertIsNotNone(plan)
        self.assertEqual(plan["name"], "python_compileall")
        self.assertEqual(plan["command"][:3], ["py", "-3", "-m"])

    def test_builds_desktop_workspace_validation_plan(self):
        plan = build_workspace_validation_plan("desktop_app/src/App.tsx", str(REPO_ROOT))
        self.assertIsNotNone(plan)
        self.assertEqual(plan["name"], "desktop_build")
        self.assertEqual(plan["cwd"], "desktop_app")

    def test_combines_validation_results(self):
        combined = combine_validation_results(
            {"status": "ok", "scope": "file", "name": "file_syntax", "command": "py -3 -m py_compile temp.py", "summary": "Validation passed.", "stdout": "", "stderr": ""},
            {"status": "ok", "scope": "workspace", "name": "python_compileall", "command": "py -3 -m compileall server", "summary": "python_compileall passed in a temporary workspace.", "stdout": "", "stderr": ""},
        )
        self.assertEqual(combined["status"], "ok")
        self.assertEqual(combined["scope"], "workspace")
        self.assertEqual(len(combined["checks"]), 2)
        self.assertIn("python_compileall", combined["summary"])

    def test_workspace_validation_skips_unknown_targets(self):
        result = validate_candidate_in_workspace("docs/notes.md", "# hi\n", str(REPO_ROOT))
        self.assertEqual(result["status"], "skipped")
        self.assertIn("No workspace validator configured", result["summary"])


if __name__ == "__main__":
    unittest.main()
