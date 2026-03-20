import sys
import unittest
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parent.parent
SERVER_DIR = REPO_ROOT / "server"

if str(SERVER_DIR) not in sys.path:
    sys.path.insert(0, str(SERVER_DIR))

from review_pipeline import (  # noqa: E402
    build_patch_review_fallback,
    build_review_fallback,
    format_review_markdown,
    is_review_request,
    normalize_patch_review_response,
    normalize_review_response,
    patch_review_system_prompt,
)


class ReviewPipelineTests(unittest.TestCase):
    def test_detects_review_request(self):
        self.assertTrue(is_review_request("Review this function for bugs"))
        self.assertTrue(is_review_request("Can you audit server/main.py"))
        self.assertFalse(is_review_request("Explain this function"))

    def test_normalizes_json_review_response(self):
        raw = """
        {
          "summary": "Found one bug risk.",
          "findings": [
            {
              "title": "Potential null branch",
              "severity": "high",
              "file": "server/main.py",
              "line_start": 2401,
              "line_end": 2405,
              "body": "The adapter path can fail before context is checked.",
              "confidence": 0.91
            }
          ],
          "test_gaps": ["No regression test covers the failing adapter path."],
          "confidence": 93
        }
        """
        context = {"sources": [{"path": "server/main.py", "line_start": 2401, "line_end": 2405, "reason": "symbol match"}]}
        review = normalize_review_response(raw, context)
        self.assertEqual(review["summary"], "Found one bug risk.")
        self.assertEqual(review["findings"][0]["severity"], "high")
        self.assertEqual(review["findings"][0]["file"], "server/main.py")
        self.assertEqual(review["confidence"], 93)

    def test_fallback_review_has_findings_and_markdown(self):
        context = {"sources": [{"path": "server/main.py", "line_start": 2401, "line_end": 2405, "reason": "symbol match"}]}
        review = build_review_fallback(context)
        markdown = format_review_markdown(review)
        self.assertTrue(review["findings"])
        self.assertIn("Findings:", markdown)
        self.assertIn("server/main.py:2401-2405", markdown)

    def test_patch_review_prompt_mentions_temp_workspace(self):
        prompt = patch_review_system_prompt("workspace summary", "task summary")
        self.assertIn("temporary workspace", prompt)
        self.assertIn("strict JSON", prompt)

    def test_patch_review_fallback_reports_validation_failure(self):
        review = build_patch_review_fallback(
            "server/main.py",
            "@@ -1 +1 @@\n-a = 1\n+a = 2",
            {"status": "error", "summary": "compileall failed"},
            line_start=10,
        )
        self.assertEqual(review["findings"][0]["severity"], "high")
        self.assertIn("failed", review["summary"].lower())

    def test_normalizes_patch_review_json_response(self):
        raw = """
        {
          "summary": "Patch looks safe.",
          "findings": [],
          "test_gaps": ["Add a regression test for the modified branch."],
          "confidence": 89
        }
        """
        review = normalize_patch_review_response(
            raw,
            file_path="server/main.py",
            diff_text="@@ -1 +1 @@",
            validation={"status": "ok", "summary": "passed"},
            line_start=12,
        )
        self.assertEqual(review["summary"], "Patch looks safe.")
        self.assertEqual(review["confidence"], 89)
        self.assertEqual(review["test_gaps"][0], "Add a regression test for the modified branch.")


if __name__ == "__main__":
    unittest.main()
