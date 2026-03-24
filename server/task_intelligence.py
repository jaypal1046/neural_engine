import hashlib
import json
import os
import re
import subprocess
import time
from pathlib import Path
from typing import Any, Optional

from command_discovery import discover_workspace_commands


PROJECT_KEYWORDS = {
    "code", "project", "repo", "repository", "file", "files", "folder", "function", "class",
    "method", "module", "component", "server", "desktop", "react", "python", "c++", "cpp",
    "ollama", "build", "compile", "error", "bug", "stack", "flow", "architecture", "review",
    "refactor", "debug", "fix", "change", "modify", "implement", "workspace",
}
DEEP_ACTION_KEYWORDS = {
    "write", "generate", "implement", "fix", "modify", "edit", "patch", "refactor", "create",
    "delete", "rename", "apply", "run command", "execute", "change",
}
CODE_GENERATION_KEYWORDS = {
    "write", "generate", "create", "make", "show", "give", "draft",
}
LANGUAGE_HINT_KEYWORDS = {
    "python", "py", "javascript", "js", "typescript", "ts", "react",
    "cpp", "c++", "java", "html", "css", "json", "sql",
}
EXPLAIN_KEYWORDS = {
    "explain", "review", "understand", "flow", "architecture", "where", "how", "why", "what does",
}
WEBAPP_TESTING_KEYWORDS = {
    "open browser", "brouser", "screenshot", "screen shot", "navigate", "page", "browser", "website", "playwright",
}
GENERAL_LOOKUP_PREFIXES = (
    "what is ",
    "who is ",
    "when did ",
    "where is ",
    "define ",
)
DESIGN_UI_KEYWORDS = {
    "design", "ui", "ux", "beautify", "style", "css", "layout", "aesthetic", "look", "component", "frontend",
}
CHART_VIZ_KEYWORDS = {
    "chart", "visualize", "plot", "graph", "stats", "statistics", "distribution", "trend", "comparison",
}
DEEP_RESEARCH_KEYWORDS = {
    "research", "investigate", "compare", "deep dive", "thorough", "comprehensive",
}
DATA_ANALYSIS_KEYWORDS = {
    "excel", "csv", "duckdb", "sql query", "pivot", "statistics", "aggregation", "group by", "dataset",
}
CONSULTING_KEYWORDS = {
    "strategy", "roadmap", "trade-off", "feasibility", "business", "competitive", "market", "decision",
}
WEB_DESIGN_KEYWORDS = {
    "guidelines", "accessibility", "responsive", "typography", "colors", "premium ui", "design system",
}
SKILL_CREATOR_KEYWORDS = {
    "new skill", "author skill", "custom persona", "define skill", "create persona",
}
GITHUB_RESEARCH_KEYWORDS = {
    "github", "repository", "open source", "repo analysis", "trending", "stars",
}



class LocalTaskIntelligence:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        local_appdata = Path(os.getenv("LOCALAPPDATA") or (Path.home() / "AppData" / "Local"))
        self.base_dir = local_appdata / "NeuralStudio"
        self.workspace_id = hashlib.sha1(str(self.workspace_root).lower().encode("utf-8")).hexdigest()[:16]
        self.workspace_dir = self.base_dir / "workspaces" / self.workspace_id
        self.router_config_file = self.base_dir / "task_router.json"
        self.workspace_profile_file = self.workspace_dir / "workspace_profile.json"
        self.analysis_cache_file = self.workspace_dir / "analysis_cache.json"
        self.command_discovery_file = self.workspace_dir / "command_discovery.json"
        self.command_stats_file = self.workspace_dir / "command_stats.json"
        self.command_preferences_file = self.workspace_dir / "command_preferences.json"
        self.editor_context_file = self.workspace_dir / "editor_context.json"
        self.task_log_file = self.workspace_dir / "task_log.jsonl"
        self.ensure_storage()

    def ensure_storage(self) -> None:
        self.workspace_dir.mkdir(parents=True, exist_ok=True)

        if not self.router_config_file.exists():
            self.router_config_file.write_text(json.dumps({
                "local_only_default": True,
                "compile_before_local_tasks": True,
                "allow_web_only_for_general_lookup": True,
                "analysis_timeouts_seconds": {
                    "python_compileall": 45,
                    "desktop_build": 90,
                    "cpp_build": 180,
                },
            }, indent=2), encoding="utf-8")

        if not self.workspace_profile_file.exists():
            self.workspace_profile_file.write_text(
                json.dumps(self._detect_workspace_profile(), indent=2),
                encoding="utf-8",
            )

        if not self.analysis_cache_file.exists():
            self.analysis_cache_file.write_text(json.dumps({
                "workspaceRoot": str(self.workspace_root),
                "lastRunAt": None,
                "overallStatus": "not_run",
                "steps": [],
            }, indent=2), encoding="utf-8")

        if not self.command_discovery_file.exists():
            self.command_discovery_file.write_text(json.dumps({
                "workspaceRoot": str(self.workspace_root),
                "discoveredAt": None,
                "signature": {"files": []},
                "commands": [],
            }, indent=2), encoding="utf-8")

        if not self.command_stats_file.exists():
            self.command_stats_file.write_text(json.dumps({
                "workspaceRoot": str(self.workspace_root),
                "updatedAt": None,
                "commands": {},
            }, indent=2), encoding="utf-8")

        if not self.command_preferences_file.exists():
            self.command_preferences_file.write_text(json.dumps({
                "workspaceRoot": str(self.workspace_root),
                "updatedAt": None,
                "intents": {},
            }, indent=2), encoding="utf-8")

        if not self.editor_context_file.exists():
            self.editor_context_file.write_text(json.dumps({
                "workspaceRoot": str(self.workspace_root),
                "activeFilePath": None,
                "selectedText": "",
                "updatedAt": None,
            }, indent=2), encoding="utf-8")

        if not self.task_log_file.exists():
            self.task_log_file.write_text("", encoding="utf-8")

    def _load_json(self, path: Path, fallback: dict[str, Any]) -> dict[str, Any]:
        try:
            return json.loads(path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            return fallback

    def _save_json(self, path: Path, payload: dict[str, Any]) -> None:
        path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def _detect_workspace_profile(self) -> dict[str, Any]:
        roots = {
            "hasCpp": (self.workspace_root / "src").exists() or (self.workspace_root / "include").exists(),
            "hasPython": (self.workspace_root / "server").exists(),
            "hasDesktopApp": (self.workspace_root / "desktop_app").exists(),
            "hasElectron": (self.workspace_root / "desktop_app" / "electron").exists(),
            "hasOllamaBridge": (self.workspace_root / "server" / "llm_adapter.py").exists(),
            "hasCxxBuildScript": (self.workspace_root / "build_neural_engine.bat").exists(),
            "hasDesktopBuild": (self.workspace_root / "desktop_app" / "package.json").exists(),
        }
        stack = []
        if roots["hasCpp"]:
            stack.append("C++ brain")
        if roots["hasPython"]:
            stack.append("Python connector")
        if roots["hasDesktopApp"]:
            stack.append("React/Electron desktop")
        if roots["hasOllamaBridge"]:
            stack.append("local Ollama adapter")

        return {
            "workspaceRoot": str(self.workspace_root),
            "workspaceId": self.workspace_id,
            "detectedAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "stack": stack,
            "capabilities": roots,
        }

    def _load_command_discovery(self) -> dict[str, Any]:
        return discover_workspace_commands(str(self.workspace_root), str(self.workspace_dir))

    def _load_command_stats(self) -> dict[str, Any]:
        return self._load_json(self.command_stats_file, {
            "workspaceRoot": str(self.workspace_root),
            "updatedAt": None,
            "commands": {},
        })

    def _load_command_preferences(self) -> dict[str, Any]:
        return self._load_json(self.command_preferences_file, {
            "workspaceRoot": str(self.workspace_root),
            "updatedAt": None,
            "intents": {},
        })

    def _save_command_preferences(self, payload: dict[str, Any]) -> None:
        payload["updatedAt"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self._save_json(self.command_preferences_file, payload)

    def set_command_preference(self, intent: str, command_name: str) -> dict[str, Any]:
        normalized_intent = str(intent or "").strip().lower()
        normalized_name = str(command_name or "").strip()
        if not normalized_intent or not normalized_name:
            raise ValueError("Both intent and command_name are required.")

        payload = self._load_command_preferences()
        intents = payload.setdefault("intents", {})
        current = intents.get(normalized_intent) or {"preferred_commands": []}
        preferred_commands = [name for name in current.get("preferred_commands", []) if name != normalized_name]
        preferred_commands.insert(0, normalized_name)
        current["preferred_commands"] = preferred_commands[:5]
        intents[normalized_intent] = current
        self._save_command_preferences(payload)
        return payload

    def _update_command_stats(self, step_result: dict[str, Any]) -> None:
        name = str(step_result.get("name") or "").strip()
        if not name:
            return

        payload = self._load_command_stats()
        commands = payload.setdefault("commands", {})
        current = commands.get(name) or {
            "runs": 0,
            "successes": 0,
            "timeouts": 0,
            "lastStatus": None,
            "lastDurationMs": None,
            "avgDurationMs": None,
        }

        runs = int(current.get("runs") or 0) + 1
        duration_ms = int(step_result.get("durationMs") or 0)
        previous_avg = current.get("avgDurationMs")
        if previous_avg is None:
            avg_duration = duration_ms or None
        elif duration_ms:
            avg_duration = int(((float(previous_avg) * (runs - 1)) + duration_ms) / runs)
        else:
            avg_duration = int(previous_avg)

        current.update({
            "runs": runs,
            "successes": int(current.get("successes") or 0) + (1 if step_result.get("status") == "ok" else 0),
            "timeouts": int(current.get("timeouts") or 0) + (1 if step_result.get("status") == "timeout" else 0),
            "lastStatus": step_result.get("status"),
            "lastDurationMs": duration_ms or None,
            "avgDurationMs": avg_duration,
        })
        commands[name] = current
        payload["updatedAt"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        self._save_json(self.command_stats_file, payload)

    def _candidate_steps(self, message: str, task: dict[str, Any]) -> list[dict[str, Any]]:
        config = self._load_json(self.router_config_file, {})
        timeouts = config.get("analysis_timeouts_seconds", {})
        discovery = self._load_command_discovery()
        command_stats = self._load_command_stats().get("commands", {})
        command_preferences = (self._load_command_preferences().get("intents", {}) or {}).get(str(task.get("intent") or ""), {})
        preferred_commands = list(command_preferences.get("preferred_commands", []))
        lowered = message.lower()
        wants_desktop = any(token in lowered for token in ("desktop", "react", "electron", "ui", "frontend", "tsx", "ai chat"))
        wants_cpp = any(token in lowered for token in ("c++", "cpp", "src/", "include/", "neural_engine", "build_neural_engine", "compile", "linker", "engine"))
        wants_python = any(token in lowered for token in ("python", "server", ".py", "fastapi", "connector", "chat routing"))
        wants_tests = any(token in lowered for token in (" test", " tests", "testing", "unittest", "pytest"))
        wants_lint = any(token in lowered for token in ("lint", "eslint", "style", "format"))
        wants_build = any(token in lowered for token in ("build", "compile", "rebuild"))
        target_hints = [str(hint).lower() for hint in task.get("target_hints", [])]
        explicit_checks = wants_tests or wants_lint or wants_build

        if explicit_checks:
            requested_categories = set()
            if wants_build:
                requested_categories.add("build")
            if wants_tests:
                requested_categories.add("test")
            if wants_lint:
                requested_categories.add("lint")
            if task.get("intent") == "modify":
                requested_categories.add("validate")
        elif task.get("intent") == "modify":
            requested_categories = {"validate", "build"}
        elif task.get("intent") == "review":
            requested_categories = {"validate", "lint"}
        else:
            requested_categories = {"validate"}

        max_steps = 2 if explicit_checks else 1
        if task.get("intent") == "modify":
            max_steps = 2

        def _command_matches_target(command: dict[str, Any]) -> bool:
            stack = str(command.get("stack") or "")
            prefixes = [str(prefix).lower() for prefix in (command.get("path_prefixes") or [])]

            if any(prefix and hint.startswith(prefix) for prefix in prefixes for hint in target_hints):
                return True
            if wants_desktop and not wants_python and not wants_cpp:
                return stack == "desktop"
            if wants_cpp and not wants_desktop and not wants_python:
                return stack == "cpp"
            if wants_python and not wants_desktop and not wants_cpp:
                return stack == "python"
            if any(hint.startswith("symbol:") or hint.startswith("function:") for hint in target_hints):
                return stack == "python" and not (wants_desktop or wants_cpp)
            if stack == "desktop" and wants_desktop:
                return True
            if stack == "cpp" and wants_cpp:
                return True
            if stack == "python" and (wants_python or task["intent"] in {"explain_or_review", "modify", "review"}):
                return True
            return False

        scored: list[tuple[int, dict[str, Any]]] = []
        for command in discovery.get("commands", []):
            category = str(command.get("category") or "")
            if category not in requested_categories:
                continue
            if not _command_matches_target(command):
                continue

            preferred_order = ["validate", "lint", "build", "test"]
            if task.get("intent") == "modify":
                preferred_order = ["validate", "build", "test", "lint"]
            elif explicit_checks:
                preferred_order = ["build", "test", "lint", "validate"]

            score = 12 - (preferred_order.index(category) if category in preferred_order else len(preferred_order) + 1)

            stack = str(command.get("stack") or "")
            if stack == "desktop" and wants_desktop:
                score += 4
            if stack == "cpp" and wants_cpp:
                score += 4
            if stack == "python" and wants_python:
                score += 3
            if any(
                prefix and hint.startswith(str(prefix).lower())
                for prefix in (command.get("path_prefixes") or [])
                for hint in target_hints
            ):
                score += 5

            stats = command_stats.get(str(command.get("name") or ""), {})
            avg_duration = int(stats.get("avgDurationMs") or 0)
            timeout_count = int(stats.get("timeouts") or 0)
            successes = int(stats.get("successes") or 0)
            runs = int(stats.get("runs") or 0)

            if avg_duration:
                if avg_duration <= 15000:
                    score += 3
                elif avg_duration <= 45000:
                    score += 1
                elif avg_duration >= 120000:
                    score -= 4
                elif avg_duration >= 60000:
                    score -= 2
            if timeout_count:
                score -= min(5, timeout_count * 2)
            if runs and successes == runs:
                score += 1
            if name := str(command.get("name") or ""):
                if name in preferred_commands:
                    score += 8 - preferred_commands.index(name)

            scored.append((score, command))

        scored.sort(key=lambda item: (-item[0], -(float(item[1].get("confidence") or 0)), item[1].get("name") or ""))

        selected: list[dict[str, Any]] = []
        seen_names: set[str] = set()
        for _score, command in scored:
            name = str(command.get("name") or "")
            if not name or name in seen_names:
                continue
            seen_names.add(name)
            selected.append({
                "name": name,
                "cwd": str(self.workspace_root / str(command.get("cwd") or ".")),
                "command": list(command.get("command") or []),
                "timeout": int(timeouts.get(name, command.get("timeout", 60))),
                "category": command.get("category"),
                "reason": command.get("reason"),
                "estimatedDurationMs": (command_stats.get(name) or {}).get("avgDurationMs"),
            })
            if len(selected) >= max_steps:
                break

        return selected

    def _run_step(self, step: dict[str, Any]) -> dict[str, Any]:
        started_at = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
        started_perf = time.perf_counter()
        try:
            result = subprocess.run(
                step["command"],
                cwd=step["cwd"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="ignore",
                timeout=step["timeout"],
                shell=False,
            )
            stdout_lines = [line for line in result.stdout.splitlines() if line.strip()]
            stderr_lines = [line for line in result.stderr.splitlines() if line.strip()]
            summary_lines = self._summarize_step_output(
                str(step.get("name") or ""),
                result.returncode,
                stdout_lines,
                stderr_lines,
            )
            payload = {
                "name": step["name"],
                "startedAt": started_at,
                "category": step.get("category"),
                "reason": step.get("reason"),
                "command": " ".join(step.get("command") or []),
                "durationMs": int((time.perf_counter() - started_perf) * 1000),
                "status": "ok" if result.returncode == 0 else "error",
                "returnCode": result.returncode,
                "summary": summary_lines,
            }
            self._update_command_stats(payload)
            return payload
        except subprocess.TimeoutExpired:
            payload = {
                "name": step["name"],
                "startedAt": started_at,
                "category": step.get("category"),
                "reason": step.get("reason"),
                "command": " ".join(step.get("command") or []),
                "durationMs": int((time.perf_counter() - started_perf) * 1000),
                "status": "timeout",
                "returnCode": None,
                "summary": [f"Timed out after {step['timeout']} seconds."],
            }
            self._update_command_stats(payload)
            return payload
        except Exception as exc:
            payload = {
                "name": step["name"],
                "startedAt": started_at,
                "category": step.get("category"),
                "reason": step.get("reason"),
                "command": " ".join(step.get("command") or []),
                "durationMs": int((time.perf_counter() - started_perf) * 1000),
                "status": "error",
                "returnCode": None,
                "summary": [str(exc)],
            }
            self._update_command_stats(payload)
            return payload

    def _summarize_step_output(
        self,
        step_name: str,
        return_code: int,
        stdout_lines: list[str],
        stderr_lines: list[str],
    ) -> list[str]:
        combined = [line.strip() for line in (stdout_lines + stderr_lines) if line.strip()]

        if step_name == "python_compileall":
            error_lines = [
                line for line in combined
                if any(token in line.lower() for token in ("error", "traceback", "syntaxerror", "indentationerror"))
            ]
            if return_code == 0:
                compiled_count = sum(1 for line in combined if "Compiling " in line)
                listing_count = sum(1 for line in combined if line.startswith("Listing "))
                summary = "Python validation passed."
                if compiled_count:
                    summary += f" Compiled {compiled_count} file(s)."
                elif listing_count:
                    summary += " Source tree scan completed."
                return [summary]
            if error_lines:
                return error_lines[-4:]

        if step_name.startswith("desktop_") and return_code == 0:
            useful = [
                line for line in combined
                if not line.startswith((">", "npm", "vite v", "transforming...", "rendering chunks...", "computing gzip size..."))
            ]
            if useful:
                return useful[-4:]

        return combined[-8:] if combined else (["Completed successfully."] if return_code == 0 else ["Command failed without output."])

    def _load_editor_context(self) -> dict[str, Any]:
        return self._load_json(self.editor_context_file, {
            "workspaceRoot": str(self.workspace_root),
            "activeFilePath": None,
            "selectedText": "",
            "updatedAt": None,
        })

    def _contains_keyword_phrase(self, text: str, phrases: set[str]) -> bool:
        for phrase in phrases:
            escaped = re.escape(phrase)
            pattern = r"(?<![A-Za-z0-9_])" + escaped.replace(r"\ ", r"\s+") + r"(?![A-Za-z0-9_])"
            if re.search(pattern, text, re.IGNORECASE):
                return True
        return False

    def classify_intent(self, message: str, allow_web: bool, editor_context: dict[str, Any] | None = None) -> dict[str, Any]:
        lowered = message.strip().lower()
        mentions_project = any(keyword in lowered for keyword in PROJECT_KEYWORDS)
        wants_deep_action = self._contains_keyword_phrase(lowered, DEEP_ACTION_KEYWORDS)
        wants_code_generation = self._contains_keyword_phrase(lowered, CODE_GENERATION_KEYWORDS)
        wants_explain = any(keyword in lowered for keyword in EXPLAIN_KEYWORDS)
        wants_flow_explanation = (
            any(keyword in lowered for keyword in ("flow", "path", "route", "pipeline", "journey"))
            and (
                lowered.startswith(("flow ", "path ", "route "))
                or any(keyword in lowered for keyword in ("explain", "tell me", "show", "how", "what happens"))
            )
        )
        mentions_language = any(re.search(rf"(?<![A-Za-z0-9_]){re.escape(keyword)}(?![A-Za-z0-9_])", lowered) for keyword in LANGUAGE_HINT_KEYWORDS)
        wants_webapp_testing = any(keyword in lowered for keyword in WEBAPP_TESTING_KEYWORDS)
        explicit_path = bool(re.search(r"[\w./\\-]+\.(cpp|h|hpp|py|ts|tsx|js|jsx|json|md)\b", message, re.IGNORECASE))
        deictic_request = any(token in lowered.split() for token in ("this", "here", "current", "selected"))
        function_name = re.search(r"\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)", message, re.IGNORECASE)
        active_file_path = (editor_context or {}).get("activeFilePath") or ""
        active_relative_path = (editor_context or {}).get("relativePath") or ""
        current_symbol_name = str((editor_context or {}).get("currentSymbolName") or "").strip()
        has_editor_target = bool(active_file_path)

        target_hints = []
        if explicit_path:
            target_hints.extend(re.findall(r"[\w./\\-]+\.(?:cpp|h|hpp|py|ts|tsx|js|jsx|json|md)\b", message, re.IGNORECASE))
        
        # Filter deictic (this/here) targets to avoid server-side internal files
        if has_editor_target and (deictic_request or wants_explain):
            is_internal = any(active_relative_path.startswith(p) for p in ("server/", "knowledge_sample/"))
            if not is_internal or explicit_path:
                if active_relative_path not in target_hints:
                    target_hints.append(active_relative_path or active_file_path)
        
        if current_symbol_name and (deictic_request or wants_explain):
            target_hints.append(f"symbol:{current_symbol_name}")
        
        if function_name:
            target_hints.append(f"function:{function_name.group(1)}")


        generic_generation_request = (
            wants_code_generation
            and mentions_language
            and not explicit_path
            and not deictic_request
            and not has_editor_target
        )

        route = "fast_local_chat"
        intent = "general_chat"

        if wants_webapp_testing:
            route = "webapp_testing"
            intent = "WEBAPP_TESTING"
        elif generic_generation_request:
            route = "generate_chat"
            intent = "generate"
        elif wants_flow_explanation:
            route = "context_chat"
            intent = "explain_or_review"
        elif wants_deep_action and (explicit_path or deictic_request or has_editor_target):
            route = "modify_chat"
            intent = "modify"
        elif wants_deep_action and mentions_project:
            route = "modify_chat"
            intent = "modify"
        elif "review" in lowered or "code review" in lowered or "audit" in lowered:
            route = "review_chat"
            intent = "review"
        elif wants_explain or explicit_path or mentions_project or (deictic_request and has_editor_target):
            route = "context_chat"
            intent = "explain_or_review"
        elif allow_web and lowered.startswith(GENERAL_LOOKUP_PREFIXES) and not mentions_project:
            route = "web_lookup"
            intent = "general_lookup"
        elif any(k in lowered for k in DESIGN_UI_KEYWORDS) and ("make" in lowered or "create" in lowered or "improve" in lowered or "design" in lowered):
            route = "design_chat"
            intent = "DESIGN_UI"
        elif any(k in lowered for k in CHART_VIZ_KEYWORDS) and mentions_project:
            route = "visualize_chat"
            intent = "VISUALIZE_DATA"
        elif any(k in lowered for k in DEEP_RESEARCH_KEYWORDS):
            route = "research_chat"
            intent = "DEEP_RESEARCH"
        elif any(k in lowered for k in DATA_ANALYSIS_KEYWORDS):
            route = "data_analysis_chat"
            intent = "DATA_ANALYSIS"
        elif any(k in lowered for k in CONSULTING_KEYWORDS):
            route = "consulting_chat"
            intent = "CONSULTING"
        elif any(k in lowered for k in WEB_DESIGN_KEYWORDS):
            route = "web_guidelines_chat"
            intent = "WEB_DESIGN"
        elif any(k in lowered for k in SKILL_CREATOR_KEYWORDS):
            route = "skill_creator_chat"
            intent = "SKILL_CREATOR"
        elif any(k in lowered for k in GITHUB_RESEARCH_KEYWORDS):
            route = "github_research_chat"
            intent = "GITHUB_RESEARCH"
        
        # Upgrade to deep research if general lookup but "deep" mentioned
        if intent == "general_lookup" and "deep" in lowered:
            route = "research_chat"
            intent = "DEEP_RESEARCH"


        return {
            "intent": intent,
            "route": route,
            "requires_local_analysis": route in {"context_chat", "modify_chat", "review_chat", "generate_chat", "deep_agent", "webapp_testing"},
            "allow_web_lookup": route == "web_lookup",
            "target_hints": target_hints,
        }

    def _run_analysis(self, message: str, task: dict[str, Any]) -> dict[str, Any]:
        steps = [self._run_step(step) for step in self._candidate_steps(message, task)]
        overall_status = "ok"
        if any(step["status"] == "error" for step in steps):
            overall_status = "error"
        elif any(step["status"] == "timeout" for step in steps):
            overall_status = "warning"

        payload = {
            "workspaceRoot": str(self.workspace_root),
            "lastRunAt": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "overallStatus": overall_status,
            "steps": steps,
        }
        self._save_json(self.analysis_cache_file, payload)
        return payload

    def _build_analysis_summary(
        self,
        task: dict[str, Any],
        profile: dict[str, Any],
        analysis: dict[str, Any],
        editor_context: dict[str, Any],
        discovery: dict[str, Any],
        preferences: dict[str, Any],
    ) -> str:
        lines = [
            f"Detected task intent: {task['intent']}.",
            "Use local workspace information first. Do not browse unless the task is clearly external/general knowledge.",
        ]

        stack = profile.get("stack") or []
        if stack:
            lines.append(f"Workspace stack: {', '.join(stack)}.")

        if task.get("target_hints"):
            lines.append(f"Target hints from the user request: {', '.join(task['target_hints'])}.")

        active_path = editor_context.get("relativePath") or editor_context.get("activeFilePath")
        if active_path:
            line_info = ""
            if editor_context.get("cursorLine"):
                line_info = f" at line {editor_context['cursorLine']}"
                if editor_context.get("cursorColumn"):
                    line_info += f", column {editor_context['cursorColumn']}"
            lines.append(f"Active editor file: {active_path}{line_info}.")

        if editor_context.get("selectedText"):
            preview = str(editor_context["selectedText"]).strip().replace("\n", " ")
            if len(preview) > 180:
                preview = preview[:180].rsplit(" ", 1)[0] + "..."
            lines.append(f"Current selected code preview: {preview}")
        elif editor_context.get("currentSymbolName"):
            lines.append(f"Current editor symbol: {editor_context['currentSymbolName']}")

        discovered_commands = discovery.get("commands") or []
        if discovered_commands:
            by_category: dict[str, list[str]] = {}
            for command in discovered_commands:
                category = str(command.get("category") or "other")
                by_category.setdefault(category, []).append(str(command.get("name") or "command"))
            command_summary = [
                f"{category}: {', '.join(sorted(names)[:4])}"
                for category, names in sorted(by_category.items())
            ]
            lines.append(f"Discovered local commands: {'; '.join(command_summary)}.")

        preferred_commands = ((preferences.get("intents") or {}).get(task.get("intent") or "", {}) or {}).get("preferred_commands") or []
        if preferred_commands:
            lines.append(f"Preferred checks for {task['intent']}: {', '.join(preferred_commands[:3])}.")

        steps = analysis.get("steps") or []
        if steps:
            lines.append(f"Local analysis status: {analysis.get('overallStatus', 'unknown')} at {analysis.get('lastRunAt', 'unknown time')}.")
            for step in steps:
                lines.append(f"- {step['name']} ({step.get('category', 'analysis')}): {step['status']}.")
                if step.get("reason"):
                    lines.append(f"  Why selected: {step['reason']}")
                if step.get("durationMs"):
                    lines.append(f"  Duration: {step['durationMs']} ms")
                if step.get("summary"):
                    lines.append(f"  Last output: {step['summary'][-1]}")

        return "\n".join(lines)

    def prepare_task(self, message: str, allow_web: bool, explicit_context: Optional[dict[str, Any]] = None) -> dict[str, Any]:
        self.ensure_storage()
        profile = self._load_json(self.workspace_profile_file, self._detect_workspace_profile())
        editor_context = explicit_context or self._load_editor_context()
        discovery = self._load_command_discovery()
        preferences = self._load_command_preferences()
        task = self.classify_intent(message, allow_web, editor_context)
        analysis = self._load_json(self.analysis_cache_file, {
            "workspaceRoot": str(self.workspace_root),
            "lastRunAt": None,
            "overallStatus": "not_run",
            "steps": [],
        })

        if task["requires_local_analysis"]:
            analysis = self._run_analysis(message, task)

        payload = {
            **task,
            "workspace_root": str(self.workspace_root),
            "local_storage_root": str(self.base_dir),
            "workspace_storage_dir": str(self.workspace_dir),
            "workspace_profile": profile,
            "analysis": analysis,
            "command_discovery": discovery,
            "command_preferences": preferences,
            "editor_context": editor_context,
        }
        payload["analysis_summary"] = self._build_analysis_summary(task, profile, analysis, editor_context, discovery, preferences)

        with self.task_log_file.open("a", encoding="utf-8") as handle:
            handle.write(json.dumps({
                "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
                "message": message,
                "intent": task["intent"],
                "route": task["route"],
                "analysisStatus": analysis.get("overallStatus"),
            }) + "\n")

        return payload
