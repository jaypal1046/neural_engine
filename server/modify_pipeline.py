import difflib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Any, Optional

from command_discovery import discover_workspace_commands, match_commands_for_file


def modify_system_prompt(summary: str, task_summary: str) -> str:
    return (
        "You are Nero modify mode inside Neural Studio. "
        "Generate a precise code change for the user's local project. "
        "Return strict JSON with keys summary, updated_code, target_file, and confidence. "
        "updated_code must contain only the replacement code for the selected target. "
        "Do not use markdown fences. Do not invent files. "
        f"Workspace summary: {summary}\n"
        f"Task preparation: {task_summary}"
    )


def build_impact_brief(flow_sections: list[dict[str, Any]] | None) -> str:
    if not flow_sections:
        return ""

    lines = ["Impact assessment:"]
    added = 0

    preferred_sections = [section for section in flow_sections if section.get("type") == "impact"]
    fallback_sections = [section for section in flow_sections if section.get("type") in {"symbol", "file"}]
    sections = preferred_sections or fallback_sections

    for section in sections:
        title = str(section.get("title") or "Flow").strip()
        entries = section.get("entries") or []
        if not isinstance(entries, list):
            continue
        if preferred_sections and title:
            lines.append(f"- {title}:")
        for entry in entries:
            if not isinstance(entry, dict):
                continue
            path = str(entry.get("path") or "").strip()
            if not path:
                continue
            label = str(entry.get("label") or "Affected code").strip()
            line_start = entry.get("line_start")
            line_text = f":{line_start}" if line_start else ""
            snippet = str(entry.get("snippet") or "").strip()
            snippet_preview = f" {snippet[:120]}" if snippet else ""
            lines.append(f"  {label} -> {path}{line_text}{snippet_preview}")
            added += 1
            if added >= 6:
                return "\n".join(lines)

    return "\n".join(lines) if added else ""


def extract_json_object(text: str) -> Optional[dict[str, Any]]:
    if not text:
        return None

    candidates = []
    stripped = text.strip()
    if stripped.startswith("{") and stripped.endswith("}"):
        candidates.append(stripped)

    depth = 0
    start = None
    for index, char in enumerate(text):
        if char == "{":
            if depth == 0:
                start = index
            depth += 1
        elif char == "}":
            if depth:
                depth -= 1
                if depth == 0 and start is not None:
                    candidates.append(text[start:index + 1])
                    start = None

    for candidate in candidates:
        try:
            parsed = json.loads(candidate)
            if isinstance(parsed, dict):
                return parsed
        except json.JSONDecodeError:
            continue
    return None


def apply_selection_to_text(full_text: str, selection: dict[str, Any], replacement: str) -> str:
    lines = full_text.splitlines(keepends=True)
    if not lines:
        lines = [""]

    start_line = max(1, int(selection.get("startLine") or 1))
    start_col = max(1, int(selection.get("startColumn") or 1))
    end_line = max(start_line, int(selection.get("endLine") or start_line))
    end_col = max(1, int(selection.get("endColumn") or start_col))

    # Convert 1-based line/column to absolute character offsets.
    start_offset = 0
    for idx in range(start_line - 1):
        start_offset += len(lines[idx])
    start_offset += start_col - 1

    end_offset = 0
    for idx in range(end_line - 1):
        end_offset += len(lines[idx])
    end_offset += end_col - 1

    source = "".join(lines)
    return source[:start_offset] + replacement + source[end_offset:]


def build_unified_diff(file_path: str, original_text: str, updated_text: str) -> str:
    original_lines = original_text.splitlines()
    updated_lines = updated_text.splitlines()
    diff = difflib.unified_diff(
        original_lines,
        updated_lines,
        fromfile=f"a/{file_path.replace(os.sep, '/')}",
        tofile=f"b/{file_path.replace(os.sep, '/')}",
        lineterm="",
    )
    return "\n".join(diff).strip()


def validate_candidate_code(file_path: str, candidate_text: str, workspace_root: str) -> dict[str, Any]:
    suffix = Path(file_path).suffix.lower()
    result = {
        "status": "skipped",
        "scope": "file",
        "name": "file_syntax",
        "command": "",
        "summary": "No local validator configured for this file type.",
        "stdout": "",
        "stderr": "",
    }

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir) / Path(file_path).name
        temp_path.write_text(candidate_text, encoding="utf-8")

        if suffix == ".py":
            cmd = ["py", "-3", "-m", "py_compile", str(temp_path)]
        elif suffix == ".js":
            cmd = ["node", "--check", str(temp_path)]
        elif suffix in {".ts", ".tsx"}:
            cmd = [
                "npx.cmd" if os.name == "nt" else "npx",
                "tsc",
                "--noEmit",
                "--pretty",
                "false",
                "--target",
                "ES2020",
                "--module",
                "esnext",
                "--jsx",
                "react-jsx",
                str(temp_path),
            ]
        else:
            return result

        result["command"] = " ".join(cmd)
        try:
            proc = subprocess.run(
                cmd,
                cwd=workspace_root,
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="ignore",
                timeout=45,
                shell=False,
            )
            result["stdout"] = proc.stdout.strip()
            result["stderr"] = proc.stderr.strip()
            result["status"] = "ok" if proc.returncode == 0 else "error"
            result["summary"] = "Validation passed." if proc.returncode == 0 else (proc.stderr.strip() or proc.stdout.strip() or "Validation failed.")
        except Exception as exc:
            result["status"] = "error"
            result["summary"] = str(exc)

    return result


def build_workspace_validation_plan(file_path: str, workspace_root: str) -> Optional[dict[str, Any]]:
    relative_path = file_path.replace("\\", "/").lstrip("./")
    suffix = Path(relative_path).suffix.lower()
    workspace = Path(workspace_root)
    discovery = discover_workspace_commands(str(workspace))
    discovered_matches = match_commands_for_file(discovery, relative_path, ("validate", "build"))

    if discovered_matches:
        command = discovered_matches[0]
        return {
            "name": str(command.get("name") or "workspace_validation"),
            "scope": "workspace",
            "cwd": str(command.get("cwd") or "."),
            "command": list(command.get("command") or []),
            "timeout": int(command.get("timeout") or 120),
            "copy_paths": list(command.get("copy_paths") or []),
            "link_paths": list(command.get("link_paths") or []),
            "ensure_dirs": list(command.get("ensure_dirs") or []),
            "ignore_patterns": dict(command.get("ignore_patterns") or {}),
        }

    if relative_path.startswith("server/") or (suffix == ".py" and (workspace / "server").exists()):
        return {
            "name": "python_compileall",
            "scope": "workspace",
            "cwd": ".",
            "command": ["py", "-3", "-m", "compileall", "server"],
            "timeout": 60,
            "copy_paths": ["server"],
            "link_paths": [],
            "ensure_dirs": [],
            "ignore_patterns": {},
        }

    if relative_path.startswith("desktop_app/") and (workspace / "desktop_app" / "package.json").exists():
        npm_cmd = "npm.cmd" if os.name == "nt" else "npm"
        return {
            "name": "desktop_build",
            "scope": "workspace",
            "cwd": "desktop_app",
            "command": [npm_cmd, "run", "build"],
            "timeout": 180,
            "copy_paths": ["desktop_app"],
            "link_paths": ["desktop_app/node_modules"],
            "ensure_dirs": [],
            "ignore_patterns": {
                "desktop_app": ["node_modules", "dist", "dist-electron", ".vite", "coverage"],
            },
        }

    cpp_suffixes = {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}
    if (
        relative_path.startswith("src/")
        or relative_path.startswith("include/")
        or suffix in cpp_suffixes
    ) and (workspace / "build_neural_engine.bat").exists():
        return {
            "name": "cpp_build",
            "scope": "workspace",
            "cwd": ".",
            "command": ["cmd", "/c", "build_neural_engine.bat"],
            "timeout": 240,
            "copy_paths": ["src", "include", "build_neural_engine.bat"],
            "link_paths": [],
            "ensure_dirs": ["bin"],
            "ignore_patterns": {},
        }

    return None


def _copy_required_path(source: Path, destination: Path, ignore_names: list[str] | None = None) -> None:
    if source.is_dir():
        shutil.copytree(
            source,
            destination,
            ignore=shutil.ignore_patterns(*(ignore_names or [])),
            dirs_exist_ok=True,
        )
        return

    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def _link_or_copy_path(source: Path, destination: Path) -> None:
    if not source.exists():
        return

    destination.parent.mkdir(parents=True, exist_ok=True)

    if destination.exists():
        return

    if os.name == "nt":
        proc = subprocess.run(
            ["cmd", "/c", "mklink", "/J", str(destination), str(source)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="ignore",
            shell=False,
        )
        if proc.returncode == 0 and destination.exists():
            return
    else:
        try:
            os.symlink(source, destination, target_is_directory=source.is_dir())
            return
        except OSError:
            pass

    if source.is_dir():
        shutil.copytree(source, destination, dirs_exist_ok=True)
    else:
        shutil.copy2(source, destination)


def validate_candidate_in_workspace(file_path: str, candidate_text: str, workspace_root: str) -> dict[str, Any]:
    plan = build_workspace_validation_plan(file_path, workspace_root)
    result = {
        "status": "skipped",
        "scope": "workspace",
        "name": "workspace_validation",
        "command": "",
        "summary": "No workspace validator configured for this target yet.",
        "stdout": "",
        "stderr": "",
    }

    if not plan:
        return result

    workspace = Path(workspace_root).resolve()
    result["name"] = str(plan.get("name") or "workspace_validation")
    result["command"] = " ".join(plan.get("command", []))

    try:
        with tempfile.TemporaryDirectory(prefix="nero-workspace-") as temp_dir:
            temp_root = Path(temp_dir)

            for rel_dir in plan.get("ensure_dirs", []):
                (temp_root / rel_dir).mkdir(parents=True, exist_ok=True)

            for rel_path in plan.get("copy_paths", []):
                source = workspace / rel_path
                if not source.exists():
                    raise FileNotFoundError(f"Required workspace path is missing: {rel_path}")
                ignore_names = list((plan.get("ignore_patterns") or {}).get(rel_path, []))
                _copy_required_path(source, temp_root / rel_path, ignore_names)

            for rel_path in plan.get("link_paths", []):
                source = workspace / rel_path
                _link_or_copy_path(source, temp_root / rel_path)

            target_path = temp_root / file_path
            target_path.parent.mkdir(parents=True, exist_ok=True)
            target_path.write_text(candidate_text, encoding="utf-8")

            proc = subprocess.run(
                plan["command"],
                cwd=temp_root / plan.get("cwd", "."),
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="ignore",
                timeout=int(plan.get("timeout", 120)),
                shell=False,
            )
            result["stdout"] = proc.stdout.strip()
            result["stderr"] = proc.stderr.strip()
            result["status"] = "ok" if proc.returncode == 0 else "error"
            result["summary"] = (
                f"{plan['name']} passed in a temporary workspace."
                if proc.returncode == 0
                else (proc.stderr.strip() or proc.stdout.strip() or f"{plan['name']} failed in a temporary workspace.")
            )
    except Exception as exc:
        result["status"] = "error"
        result["summary"] = str(exc)

    return result


def combine_validation_results(*checks: dict[str, Any]) -> dict[str, Any]:
    normalized = [check for check in checks if isinstance(check, dict)]
    if not normalized:
        return {
            "status": "skipped",
            "scope": "file",
            "name": "validation",
            "command": "",
            "summary": "No validation checks were run.",
            "stdout": "",
            "stderr": "",
            "checks": [],
        }

    status = "skipped"
    for check in normalized:
        check_status = str(check.get("status") or "skipped")
        if check_status == "error":
            status = "error"
            break
        if check_status == "ok":
            status = "ok"

    summaries = []
    commands = []
    stdout_parts = []
    stderr_parts = []
    scope = "workspace" if any(check.get("scope") == "workspace" and check.get("status") != "skipped" for check in normalized) else str(normalized[0].get("scope") or "file")

    for check in normalized:
        label = str(check.get("name") or check.get("scope") or "validation")
        summary = str(check.get("summary") or "").strip()
        if summary:
            summaries.append(f"{label}: {summary}")
        command = str(check.get("command") or "").strip()
        if command:
            commands.append(command)
        stdout = str(check.get("stdout") or "").strip()
        if stdout:
            stdout_parts.append(f"[{label}] {stdout}")
        stderr = str(check.get("stderr") or "").strip()
        if stderr:
            stderr_parts.append(f"[{label}] {stderr}")

    return {
        "status": status,
        "scope": scope,
        "name": "combined_validation",
        "command": " | ".join(commands),
        "summary": " ".join(summaries).strip() or "Validation checks completed.",
        "stdout": "\n\n".join(stdout_parts).strip(),
        "stderr": "\n\n".join(stderr_parts).strip(),
        "checks": normalized,
    }


def validate_candidate_change(file_path: str, candidate_text: str, workspace_root: str) -> dict[str, Any]:
    file_validation = validate_candidate_code(file_path, candidate_text, workspace_root)
    workspace_validation = (
        validate_candidate_in_workspace(file_path, candidate_text, workspace_root)
        if file_validation.get("status") != "error"
        else {
            "status": "skipped",
            "scope": "workspace",
            "name": "workspace_validation",
            "command": "",
            "summary": "Workspace validation was skipped because the file-level syntax check failed.",
            "stdout": "",
            "stderr": "",
        }
    )
    return combine_validation_results(file_validation, workspace_validation)


def format_modify_markdown(
    summary: str,
    file_path: str,
    diff_text: str,
    validation: dict[str, Any],
    confidence: int,
    patch_review: Optional[dict[str, Any]] = None,
    impact_summary: str = "",
) -> str:
    lines = [summary.strip() or "Patch proposal created."]
    lines.append("")
    lines.append(f"Target file: `{file_path}`")
    lines.append(f"Confidence: {confidence}%")
    lines.append("")
    if impact_summary.strip():
        lines.append("Impact:")
        for impact_line in impact_summary.strip().splitlines()[1:]:
            cleaned = impact_line.strip()
            if cleaned.startswith("-"):
                lines.append(cleaned)
            else:
                lines.append(f"- {cleaned}")
        lines.append("")
    lines.append("Validation:")
    lines.append(f"- Status: {validation.get('status', 'unknown')}")
    if validation.get("command"):
        lines.append(f"- Command: `{validation['command']}`")
    if validation.get("summary"):
        lines.append(f"- Result: {validation['summary']}")
    for check in validation.get("checks", []):
        check_name = check.get("name") or check.get("scope") or "validation"
        check_status = check.get("status") or "unknown"
        check_summary = check.get("summary") or ""
        lines.append(f"- Check `{check_name}`: {check_status} - {check_summary}")
    if patch_review:
        lines.append("")
        lines.append("Patch review:")
        if patch_review.get("summary"):
            lines.append(f"- Summary: {patch_review['summary']}")
        if patch_review.get("confidence") is not None:
            lines.append(f"- Confidence: {patch_review['confidence']}%")
        for gap in patch_review.get("test_gaps", []):
            lines.append(f"- Test gap: {gap}")
    if diff_text:
        lines.extend(["", "Proposed diff:", "```diff", diff_text, "```"])
    return "\n".join(lines).strip()
