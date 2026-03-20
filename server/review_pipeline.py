import json
import re
from typing import Any, Optional


SEVERITY_ORDER = {
    "high": 0,
    "medium": 1,
    "low": 2,
    "info": 3,
}


def is_review_request(message: str) -> bool:
    lowered = message.lower()
    review_terms = (
        "review",
        "code review",
        "analyze risk",
        "find issues",
        "find bugs",
        "audit",
        "check this code",
    )
    return any(term in lowered for term in review_terms)


def review_system_prompt(summary: str, task_summary: str) -> str:
    return (
        "You are Nero review mode inside Neural Studio. "
        "Review the provided local code context like a senior engineer. "
        "Prioritize concrete bugs, behavioral regressions, hidden risks, and missing tests. "
        "Return strict JSON with keys summary, findings, test_gaps, and confidence. "
        "Each finding must include title, severity, file, line_start, line_end, body, and confidence. "
        "Do not invent files or lines. If context is incomplete, say so in summary. "
        f"Workspace summary: {summary}\n"
        f"Task preparation: {task_summary}"
    )


def patch_review_system_prompt(summary: str, task_summary: str) -> str:
    return (
        "You are Nero patch review mode inside Neural Studio. "
        "Review a proposed local code patch that has already been staged in a temporary workspace and validated locally. "
        "Prioritize regressions, incomplete fixes, scope creep, missing edge cases, and missing tests. "
        "Return strict JSON with keys summary, findings, test_gaps, and confidence. "
        "Each finding must include title, severity, file, line_start, line_end, body, and confidence. "
        "If the patch looks safe from the provided diff and validation data, return an empty findings list and explain that briefly in summary. "
        f"Workspace summary: {summary}\n"
        f"Task preparation: {task_summary}"
    )


def _extract_json(text: str) -> Optional[dict[str, Any]]:
    if not text:
        return None

    candidates = []
    stripped = text.strip()
    if stripped.startswith("{") and stripped.endswith("}"):
        candidates.append(stripped)

    for match in re.finditer(r"\{[\s\S]*\}", text):
        candidates.append(match.group(0))

    for candidate in candidates:
        try:
            parsed = json.loads(candidate)
            if isinstance(parsed, dict):
                return parsed
        except json.JSONDecodeError:
            continue
    return None


def _normalize_finding(raw: dict[str, Any]) -> dict[str, Any]:
    severity = str(raw.get("severity") or "medium").lower()
    if severity not in SEVERITY_ORDER:
        severity = "medium"
    title = str(raw.get("title") or "Review finding").strip() or "Review finding"
    file_path = str(raw.get("file") or raw.get("path") or "unknown").strip() or "unknown"
    line_start = raw.get("line_start") or raw.get("lineStart") or 1
    line_end = raw.get("line_end") or raw.get("lineEnd") or line_start
    try:
        line_start = int(line_start)
    except (TypeError, ValueError):
        line_start = 1
    try:
        line_end = int(line_end)
    except (TypeError, ValueError):
        line_end = line_start
    confidence = raw.get("confidence", 0.7)
    try:
        confidence = float(confidence)
    except (TypeError, ValueError):
        confidence = 0.7

    return {
        "title": title,
        "severity": severity,
        "file": file_path,
        "line_start": line_start,
        "line_end": max(line_start, line_end),
        "body": str(raw.get("body") or raw.get("description") or "").strip(),
        "confidence": max(0.0, min(1.0, confidence)),
    }


def normalize_review_response(raw_text: str, context: dict[str, Any]) -> dict[str, Any]:
    parsed = _extract_json(raw_text)
    if parsed:
        findings = [_normalize_finding(item) for item in parsed.get("findings", []) if isinstance(item, dict)]
        findings.sort(key=lambda item: (SEVERITY_ORDER.get(item["severity"], 9), item["file"], item["line_start"]))
        return {
            "summary": str(parsed.get("summary") or "").strip(),
            "findings": findings,
            "test_gaps": [str(item).strip() for item in parsed.get("test_gaps", []) if str(item).strip()],
            "confidence": int(parsed.get("confidence", 85)) if str(parsed.get("confidence", "")).strip() else 85,
        }

    return build_review_fallback(context)


def build_review_fallback(context: dict[str, Any]) -> dict[str, Any]:
    findings = []
    sources = context.get("sources", [])

    for source in sources[:4]:
        file_path = source.get("path", "unknown")
        reason = source.get("reason", "relevant local context")
        findings.append({
            "title": "Manual inspection needed",
            "severity": "info",
            "file": file_path,
            "line_start": int(source.get("line_start") or 1),
            "line_end": int(source.get("line_end") or source.get("line_start") or 1),
            "body": f"This file was selected for review because of {reason}, but the local model did not return structured findings.",
            "confidence": 0.45,
        })

    test_gaps = []
    interesting_paths = [source.get("path", "") for source in sources]
    if any(path.startswith(("server/", "src/", "desktop_app/")) for path in interesting_paths):
        test_gaps.append("No focused automated regression test was generated for the reviewed files yet.")

    return {
        "summary": "The review pipeline gathered the most relevant local files, but the model did not return structured findings. Start by checking the files below.",
        "findings": findings,
        "test_gaps": test_gaps,
        "confidence": 55,
    }


def build_patch_review_fallback(
    file_path: str,
    diff_text: str,
    validation: dict[str, Any],
    *,
    line_start: int = 1,
) -> dict[str, Any]:
    findings = []
    test_gaps = []
    validation_status = str(validation.get("status") or "unknown")
    validation_summary = str(validation.get("summary") or "").strip()

    if validation_status == "error":
        findings.append({
            "title": "Staged patch failed validation",
            "severity": "high",
            "file": file_path,
            "line_start": max(1, line_start),
            "line_end": max(1, line_start),
            "body": validation_summary or "The proposed patch did not pass local validation after being staged in a temporary workspace.",
            "confidence": 0.96,
        })
        summary = "The patch was staged in a temporary workspace, but local validation still failed."
    elif not diff_text.strip():
        findings.append({
            "title": "No effective code change",
            "severity": "medium",
            "file": file_path,
            "line_start": max(1, line_start),
            "line_end": max(1, line_start),
            "body": "The generated patch did not produce a visible diff, so the requested change may not have been applied.",
            "confidence": 0.8,
        })
        summary = "The patch staged cleanly, but it did not produce a meaningful diff to review."
    else:
        summary = "The patch was staged in a temporary workspace and no concrete diff-level issues were identified from the available local signals."

    if file_path.startswith(("server/", "src/", "desktop_app/")):
        test_gaps.append("The staged patch should get a focused regression test that exercises the changed path directly.")

    return {
        "summary": summary,
        "findings": findings,
        "test_gaps": test_gaps,
        "confidence": 72 if findings else 78,
    }


def normalize_patch_review_response(
    raw_text: str,
    *,
    file_path: str,
    diff_text: str,
    validation: dict[str, Any],
    line_start: int = 1,
) -> dict[str, Any]:
    parsed = _extract_json(raw_text)
    if parsed:
        findings = [_normalize_finding(item) for item in parsed.get("findings", []) if isinstance(item, dict)]
        findings.sort(key=lambda item: (SEVERITY_ORDER.get(item["severity"], 9), item["file"], item["line_start"]))
        return {
            "summary": str(parsed.get("summary") or "").strip(),
            "findings": findings,
            "test_gaps": [str(item).strip() for item in parsed.get("test_gaps", []) if str(item).strip()],
            "confidence": int(parsed.get("confidence", 85)) if str(parsed.get("confidence", "")).strip() else 85,
        }

    return build_patch_review_fallback(
        file_path,
        diff_text,
        validation,
        line_start=line_start,
    )


def format_review_markdown(review: dict[str, Any]) -> str:
    lines = [review.get("summary", "").strip() or "Review completed."]

    findings = review.get("findings", [])
    if findings:
        lines.extend(["", "Findings:"])
        for finding in findings:
            location = finding["file"]
            if finding.get("line_start"):
                location += f":{finding['line_start']}"
                if finding.get("line_end") and finding["line_end"] != finding["line_start"]:
                    location += f"-{finding['line_end']}"
            lines.append(
                f"- [{finding['severity'].upper()}] {finding['title']} - {location} - {finding['body']}"
            )
    else:
        lines.extend(["", "Findings:", "- No concrete issues were identified from the available context."])

    test_gaps = [gap for gap in review.get("test_gaps", []) if gap]
    if test_gaps:
        lines.extend(["", "Test gaps:"])
        for gap in test_gaps:
            lines.append(f"- {gap}")

    return "\n".join(lines).strip()
