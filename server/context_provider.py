import json
import os
import re
import hashlib
from pathlib import Path
from typing import Any

try:
    import project_indexer
except ImportError:
    from server import project_indexer
try:
    from symbol_graph import SymbolGraph
except ImportError:
    from server.symbol_graph import SymbolGraph

BASE_DIR = Path(__file__).parent.parent
PROJECT_INDEX_FILE = BASE_DIR / "project_index.json"
STOPWORDS = {
    "a", "an", "and", "are", "as", "at", "be", "by", "do", "for", "from", "how",
    "i", "in", "is", "it", "me", "of", "on", "or", "show", "tell", "that", "the",
    "this", "to", "what", "where", "which", "who", "why", "with", "you",
}
CONTEXTUAL_QUERY_HINTS = {
    "this", "that", "current", "opened", "recent", "latest", "here", "file", "folder",
    "workspace", "project", "screen", "editor", "tab",
}
FILE_FLOW_HINTS = {
    "flow", "architecture", "connected", "connection", "dependency", "dependencies",
    "import", "imports", "include", "used", "impact", "break", "breaks",
}
IMPACT_HINTS = {
    "change", "modify", "fix", "edit", "patch", "refactor", "implement", "update",
    "rewrite", "optimize", "rename", "remove", "delete",
}
PROJECT_OVERVIEW_HINTS = {
    "project", "workspace", "repo", "repository", "codebase", "architecture", "structure",
}


class ContextProvider:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root)
        self._index_cache: dict[str, Any] | None = None
        self._index_mtime: float | None = None
        self._base_workspace_root = BASE_DIR.resolve()
        self._shared_index_enabled = self.workspace_root.resolve() == self._base_workspace_root
        self._symbol_graph = SymbolGraph(str(self.workspace_root))

    def _load_index(self) -> dict[str, Any]:
        if self._shared_index_enabled and PROJECT_INDEX_FILE.exists():
            mtime = PROJECT_INDEX_FILE.stat().st_mtime
            if self._index_cache is not None and self._index_mtime == mtime:
                return self._index_cache

            with open(PROJECT_INDEX_FILE, "r", encoding="utf-8") as f:
                self._index_cache = json.load(f)
            self._index_mtime = mtime
            return self._index_cache

        generated = self._scan_workspace()
        self._index_cache = generated
        self._index_mtime = None
        return generated

    def _scan_workspace(self) -> dict[str, Any]:
        index: dict[str, Any] = {"folders": {}, "indexed_files": 0, "total_size": 0, "file_types": {}}

        folders_to_index = [
            folder
            for folder in project_indexer.FOLDERS_TO_INDEX
            if (self.workspace_root / folder).exists()
        ]

        if not folders_to_index:
            folders_to_index = ["."]

        for folder in folders_to_index:
            folder_path = self.workspace_root / folder
            if not folder_path.exists():
                continue

            files: list[dict[str, Any]] = []
            for root, dirs, names in os.walk(folder_path):
                dirs[:] = [d for d in dirs if d not in project_indexer.SKIP_FOLDERS]
                for name in names:
                    file_path = Path(root) / name
                    if not project_indexer.should_index_file(file_path):
                        continue

                    rel_path = file_path.relative_to(self.workspace_root)
                    size = file_path.stat().st_size
                    files.append({
                        "path": str(rel_path).replace("\\", "/"),
                        "name": name,
                        "size": size,
                        "extension": file_path.suffix.lower(),
                    })
                    index["indexed_files"] += 1
                    index["total_size"] += size
                    ext = file_path.suffix.lower()
                    index["file_types"][ext] = index["file_types"].get(ext, 0) + 1

            index["folders"][folder] = {"file_count": len(files), "files": files}

        return index

    def _all_files(self) -> list[dict[str, Any]]:
        index = self._load_index()
        files: list[dict[str, Any]] = []
        for folder_info in index.get("folders", {}).values():
            for file_info in folder_info.get("files", []):
                rel_path = file_info.get("path", "").replace("\\", "/")
                if rel_path.startswith("brain/knowledge/"):
                    continue
                files.append(file_info)
        return files

    def _workspace_memory_dir(self) -> Path | None:
        workspace_id = hashlib.sha1(str(self.workspace_root).lower().encode("utf-8")).hexdigest()[:16]
        candidates = []

        appdata = os.getenv("APPDATA")
        if appdata:
            candidates.append(Path(appdata) / "desktop_app" / "project-memory" / workspace_id)

        home = Path.home()
        candidates.append(home / "AppData" / "Roaming" / "desktop_app" / "project-memory" / workspace_id)

        for candidate in candidates:
            if candidate.exists():
                return candidate
        return candidates[0] if candidates else None

    def _local_workspace_dir(self) -> Path | None:
        workspace_id = hashlib.sha1(str(self.workspace_root).lower().encode("utf-8")).hexdigest()[:16]
        candidates = []

        local_appdata = os.getenv("LOCALAPPDATA")
        if local_appdata:
            candidates.append(Path(local_appdata) / "NeuralStudio" / "workspaces" / workspace_id)

        home = Path.home()
        candidates.append(home / "AppData" / "Local" / "NeuralStudio" / "workspaces" / workspace_id)

        for candidate in candidates:
            if candidate.exists():
                return candidate
        return candidates[0] if candidates else None

    def _load_workspace_memory(self) -> dict[str, Any]:
        memory_dir = self._workspace_memory_dir()
        if not memory_dir:
            return {"state": {}, "events": []}

        state_file = memory_dir / "workspace.json"
        events_file = memory_dir / "events.jsonl"

        try:
            state = json.loads(state_file.read_text(encoding="utf-8")) if state_file.exists() else {}
        except (OSError, json.JSONDecodeError):
            state = {}

        events: list[dict[str, Any]] = []
        if events_file.exists():
            try:
                lines = events_file.read_text(encoding="utf-8").splitlines()
                for line in lines[-40:]:
                    try:
                        events.append(json.loads(line))
                    except json.JSONDecodeError:
                        continue
            except OSError:
                pass

        return {
            "memory_dir": str(memory_dir),
            "state": state,
            "events": events,
        }

    def _load_editor_context(self) -> dict[str, Any]:
        workspace_dir = self._local_workspace_dir()
        if not workspace_dir:
            return {}

        editor_context_file = workspace_dir / "editor_context.json"
        try:
            return json.loads(editor_context_file.read_text(encoding="utf-8")) if editor_context_file.exists() else {}
        except (OSError, json.JSONDecodeError):
            return {}

    def _recent_files(self) -> list[str]:
        state = self._load_workspace_memory().get("state", {})
        recent = []
        for item in state.get("recentFiles", [])[:8]:
            file_path = str(item.get("path", "")).strip()
            if not file_path:
                continue
            try:
                rel_path = Path(file_path).resolve().relative_to(self.workspace_root.resolve())
                rel_text = str(rel_path).replace("\\", "/")
                if not rel_text.startswith("brain/knowledge/"):
                    recent.append(rel_text)
            except ValueError:
                continue
        return recent

    def _is_project_overview_query(self, query: str, file_targets: list[str]) -> bool:
        lowered = query.lower()
        if file_targets:
            return False
        return any(hint in lowered.split() for hint in PROJECT_OVERVIEW_HINTS) or any(
            phrase in lowered for phrase in ("tell me about project", "explain this project", "about this project")
        )

    def _build_project_overview(self) -> tuple[str, list[dict[str, Any]]]:
        index = self._load_index()
        folders = index.get("folders", {}) or {}
        ordered = sorted(
            (
                (name, info.get("file_count", 0), info.get("files", []))
                for name, info in folders.items()
                if info.get("file_count", 0) > 0
            ),
            key=lambda item: (-item[1], item[0]),
        )

        if not ordered:
            return "", []

        lines = ["PROJECT OVERVIEW:"]
        source_entries: list[dict[str, Any]] = []

        top_folders = ordered[:6]
        folder_names = [name for name, _, _ in top_folders]
        lines.append(f"- Main active folders: {', '.join(folder_names)}.")

        if "server" in folders:
            lines.append("- `server/` is the Python connector and routing layer.")
        if "src" in folders:
            lines.append("- `src/` contains the core C++ brain and engine implementation.")
        if "include" in folders:
            lines.append("- `include/` contains C++ headers and shared interfaces.")
        if "desktop_app" in folders:
            lines.append("- `desktop_app/` is the React/Electron desktop client.")
        if "knowledge" in folders or "knowledge_sample" in folders:
            lines.append("- `knowledge/` and related sample knowledge folders store local AI knowledge and examples.")

        lines.append("- Representative files:")
        for folder_name, _count, files in top_folders[:4]:
            if not files:
                continue
            file_info = files[0]
            rel_path = str(file_info.get("path", "")).replace("\\", "/")
            if not rel_path:
                continue
            lines.append(f"  - {rel_path}")
            source_entries.append({
                "path": rel_path,
                "line_start": 1,
                "line_end": 5,
                "reason": f"project-overview:{folder_name}",
            })

        return "\n".join(lines), source_entries

    def _normalize_target_path(self, rel_path: str) -> str:
        normalized = rel_path.replace("\\", "/").lstrip("./")
        if normalized.startswith(self.workspace_root.name.lower() + "/"):
            normalized = normalized.split("/", 1)[1]
        return normalized

    def _matching_file_infos(self, targets: list[str]) -> list[dict[str, Any]]:
        if not targets:
            return []

        all_files = self._all_files()
        path_map = {str(file_info.get("path", "")).replace("\\", "/").lower(): file_info for file_info in all_files}
        matches: list[dict[str, Any]] = []
        seen: set[str] = set()

        for target in targets:
            normalized = self._normalize_target_path(target).lower()
            match = path_map.get(normalized)
            if match:
                match_path = str(match.get("path", "")).replace("\\", "/").lower()
                if match_path not in seen:
                    seen.add(match_path)
                    matches.append(match)

        return matches

    def _active_editor_file(self) -> str | None:
        editor_context = self._load_editor_context()
        relative_path = str(editor_context.get("relativePath", "")).strip().replace("\\", "/")
        if relative_path and not relative_path.startswith("brain/knowledge/"):
            return relative_path

        file_path = str(editor_context.get("activeFilePath", "")).strip()
        if not file_path:
            return None
        try:
            rel_path = Path(file_path).resolve().relative_to(self.workspace_root.resolve())
            rel_text = str(rel_path).replace("\\", "/")
            if not rel_text.startswith("brain/knowledge/"):
                return rel_text
        except ValueError:
            return None
        return None

    def _build_memory_summary(self) -> str:
        memory = self._load_workspace_memory()
        editor_context = self._load_editor_context()
        state = memory.get("state", {})
        recent_files = state.get("recentFiles", [])[:5]
        recent_commands = state.get("recentCommands", [])[:5]
        event_counts = state.get("eventCounts", {})

        parts: list[str] = []

        if state.get("lastActivityAt"):
            parts.append(f"Last tracked workspace activity: {state['lastActivityAt']}.")

        active_path = str(editor_context.get("relativePath") or editor_context.get("activeFilePath") or "").replace("\\", "/")
        if active_path:
            cursor_line = editor_context.get("cursorLine")
            cursor_col = editor_context.get("cursorColumn")
            location = ""
            if cursor_line:
                location = f" at line {cursor_line}"
                if cursor_col:
                    location += f", column {cursor_col}"
            parts.append(f"Active editor target: {active_path}{location}.")

        selected_text = str(editor_context.get("selectedText", "")).strip()
        if selected_text:
            preview = selected_text.replace("\n", " ")
            if len(preview) > 200:
                preview = preview[:200].rsplit(" ", 1)[0] + "..."
            parts.append(f"Selected code preview: {preview}")

        if event_counts:
            summarized = ", ".join(
                f"{event_type.replace('_', ' ')}={count}"
                for event_type, count in sorted(event_counts.items())
            )
            parts.append(f"Tracked activity counts: {summarized}.")

        if recent_files:
            file_lines = []
            for item in recent_files:
                file_path = str(item.get("path", "")).replace("\\", "/")
                if file_path:
                    file_lines.append(f"- {file_path}")
            if file_lines:
                parts.append("Recently opened files:\n" + "\n".join(file_lines))

        if recent_commands:
            command_lines = []
            for item in recent_commands:
                command = str(item.get("command", "")).strip()
                if command:
                    command_lines.append(f"- {command}")
            if command_lines:
                parts.append("Recently executed terminal commands:\n" + "\n".join(command_lines))

        return "\n\n".join(parts).strip()

    def _tokenize(self, text: str) -> list[str]:
        tokens = re.findall(r"[a-zA-Z0-9_./-]+", text.lower())
        return [t for t in tokens if len(t) >= 2 and t not in STOPWORDS]

    def _read_text(self, rel_path: str) -> str:
        full_path = self.workspace_root / rel_path
        try:
            with open(full_path, "r", encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    def _extract_symbol_targets(self, query: str) -> list[str]:
        editor_context = self._load_editor_context()
        targets: list[str] = []

        for token in re.findall(r"\b[A-Za-z_][A-Za-z0-9_]{2,}\b", query):
            lowered = token.lower()
            if (
                token not in targets
                and lowered not in STOPWORDS
                and ( "_" in token or any(ch.isupper() for ch in token[1:]) )
            ):
                targets.append(token)

        for pattern in (
            r"\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"\bmethod\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"\bclass\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"`([A-Za-z_][A-Za-z0-9_]*)`",
            r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(",
        ):
            for match in re.findall(pattern, query):
                if match not in targets and match.lower() not in STOPWORDS:
                    targets.append(match)

        current_symbol = str(editor_context.get("currentSymbolName", "")).strip()
        if current_symbol and current_symbol not in targets:
            targets.append(current_symbol)

        selected_text = str(editor_context.get("selectedText", "")).strip()
        if selected_text:
            first_lines = "\n".join(selected_text.splitlines()[:3])
            for pattern in (
                r"\bdef\s+([A-Za-z_][A-Za-z0-9_]*)",
                r"\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)",
                r"\bclass\s+([A-Za-z_][A-Za-z0-9_]*)",
            ):
                match = re.search(pattern, first_lines)
                if match:
                    name = match.group(1)
                    if name not in targets:
                        targets.append(name)

        return targets[:6]

    def _extract_file_targets(self, query: str) -> list[str]:
        editor_context = self._load_editor_context()
        targets: list[str] = []

        for match in re.findall(r"[\w./\\-]+\.(?:cpp|h|hpp|py|ts|tsx|js|jsx|json|md)\b", query, re.IGNORECASE):
            normalized = match.replace("\\", "/").lstrip("./")
            if normalized not in targets and "/" in normalized:
                targets.append(normalized)

        relative_path = str(editor_context.get("relativePath", "")).strip().replace("\\", "/")
        active_file_path = str(editor_context.get("activeFilePath", "")).strip()
        lowered_tokens = set(query.lower().split())
        contextual_file_tokens = CONTEXTUAL_QUERY_HINTS - {"project", "workspace"}
        if any(token in lowered_tokens for token in contextual_file_tokens):
            if relative_path and relative_path not in targets:
                targets.append(relative_path)
            elif active_file_path:
                try:
                    rel_path = Path(active_file_path).resolve().relative_to(self.workspace_root.resolve())
                    normalized = str(rel_path).replace("\\", "/")
                    if normalized not in targets:
                        targets.append(normalized)
                except ValueError:
                    pass

        return targets[:4]

    def _extract_symbols_from_content(self, rel_path: str, content: str) -> list[dict[str, Any]]:
        ext = Path(rel_path).suffix.lower()
        if ext in {".py"}:
            patterns = [
                (re.compile(r"^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("), "function"),
                (re.compile(r"^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)\s*[:(]"), "class"),
            ]
        elif ext in {".ts", ".tsx", ".js", ".jsx"}:
            patterns = [
                (re.compile(r"^\s*(?:export\s+)?(?:async\s+)?function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("), "function"),
                (re.compile(r"^\s*(?:export\s+)?class\s+([A-Za-z_][A-Za-z0-9_]*)\b"), "class"),
                (re.compile(r"^\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?:async\s*)?\("), "function"),
                (re.compile(r"^\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_][A-Za-z0-9_]*)\s*="), "variable"),
            ]
        elif ext in {".cpp", ".cc", ".cxx", ".h", ".hpp"}:
            patterns = [
                (re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\b"), "class"),
                (re.compile(r"^\s*(?:[\w:<>~*&]+\s+)+([A-Za-z_][A-Za-z0-9_:]*)\s*\([^;]*\)\s*\{"), "function"),
            ]
        else:
            patterns = []

        if not patterns:
            return []

        lines = content.splitlines()
        raw_matches: list[dict[str, Any]] = []
        for index, line in enumerate(lines):
            for regex, kind in patterns:
                match = regex.search(line)
                if match:
                    name = match.group(1).split("::")[-1]
                    raw_matches.append({
                        "name": name,
                        "kind": kind,
                        "line_start": index + 1,
                    })

        for idx, item in enumerate(raw_matches):
            next_start = raw_matches[idx + 1]["line_start"] if idx + 1 < len(raw_matches) else len(lines) + 1
            item["line_end"] = min(len(lines), max(item["line_start"], next_start - 1, item["line_start"] + 24))
            start = max(0, item["line_start"] - 1)
            end = min(len(lines), item["line_end"])
            item["snippet"] = "\n".join(lines[start:end]).strip()[:1800]

        return raw_matches

    def _find_symbol_match(self, rel_path: str, content: str, symbol_targets: list[str]) -> dict[str, Any] | None:
        if not symbol_targets:
            return None

        symbols = self._extract_symbols_from_content(rel_path, content)
        if not symbols:
            return None

        normalized_targets = [target.lower() for target in symbol_targets]
        for target in normalized_targets:
            for symbol in symbols:
                if symbol["name"].lower() == target:
                    return symbol
        for target in normalized_targets:
            for symbol in symbols:
                if target in symbol["name"].lower():
                    return symbol
        return None

    def _build_symbol_flow_blocks(self, symbol_targets: list[str]) -> tuple[list[str], list[dict[str, Any]], list[dict[str, Any]]]:
        if not symbol_targets:
            return [], [], []

        blocks: list[str] = []
        sources: list[dict[str, Any]] = []
        sections: list[dict[str, Any]] = []
        for summary in self._symbol_graph.describe_symbols(symbol_targets[:3]):
            definitions = summary.get("definitions", [])
            primary = definitions[0] if definitions else None
            if not primary:
                continue

            lines = [
                f"SYMBOL FLOW: {summary['name']}",
                f"DEFINED IN: {primary['path']}:{primary['line_start']}-{primary['line_end']}",
                f"KIND: {primary['kind']}",
            ]
            section_entries = [{
                "label": "Definition",
                "path": primary["path"],
                "line_start": primary["line_start"],
                "line_end": primary["line_end"],
                "kind": primary["kind"],
                "snippet": primary.get("definition_line", ""),
            }]
            sources.append({
                "path": primary["path"],
                "line_start": primary["line_start"],
                "line_end": primary["line_end"],
                "reason": f"symbol-flow-definition:{summary['name']}",
            })

            callers = summary.get("callers", [])
            if callers:
                lines.append("CALLED OR REFERENCED BY:")
                for caller in callers[:4]:
                    lines.append(
                        f"- {caller['path']}:{caller['line_start']} ({caller['kind']}) {caller['snippet']}"
                    )
                    section_entries.append({
                        "label": "Caller",
                        "path": caller["path"],
                        "line_start": caller["line_start"],
                        "line_end": caller["line_end"],
                        "kind": caller["kind"],
                        "snippet": caller["snippet"],
                    })
                    sources.append({
                        "path": caller["path"],
                        "line_start": caller["line_start"],
                        "line_end": caller["line_end"],
                        "reason": f"symbol-flow-reference:{summary['name']}",
                    })
            elif summary.get("references"):
                lines.append("REFERENCED BY:")
                for reference in summary["references"][:4]:
                    lines.append(
                        f"- {reference['path']}:{reference['line_start']} ({reference['kind']}) {reference['snippet']}"
                    )
                    section_entries.append({
                        "label": "Reference",
                        "path": reference["path"],
                        "line_start": reference["line_start"],
                        "line_end": reference["line_end"],
                        "kind": reference["kind"],
                        "snippet": reference["snippet"],
                    })
                    sources.append({
                        "path": reference["path"],
                        "line_start": reference["line_start"],
                        "line_end": reference["line_end"],
                        "reason": f"symbol-flow-reference:{summary['name']}",
                    })

            blocks.append("\n".join(lines))
            sections.append({
                "type": "symbol",
                "title": summary["name"],
                "summary": f"Definition and local callers/references for {summary['name']}.",
                "entries": section_entries,
            })

        deduped_sources: list[dict[str, Any]] = []
        seen = set()
        for source in sources:
            key = (source["path"], source["line_start"], source["line_end"], source["reason"])
            if key in seen:
                continue
            seen.add(key)
            deduped_sources.append(source)
        return blocks, deduped_sources, sections

    def _build_file_flow_blocks(self, file_targets: list[str]) -> tuple[list[str], list[dict[str, Any]], list[dict[str, Any]]]:
        if not file_targets:
            return [], [], []

        blocks: list[str] = []
        sources: list[dict[str, Any]] = []
        sections: list[dict[str, Any]] = []
        for file_target in file_targets:
            summary = self._symbol_graph.describe_file(file_target)
            if not summary:
                continue

            lines = [f"FILE FLOW: {summary['path']}"]
            section_entries: list[dict[str, Any]] = []
            dependencies = summary.get("dependencies", [])
            dependents = summary.get("dependents", [])

            if dependencies:
                lines.append("DEPENDS ON:")
                for dependency in dependencies[:4]:
                    lines.append(
                        f"- {dependency['path']}:{dependency['line_start']} ({dependency['kind']}) {dependency['snippet']}"
                    )
                    section_entries.append({
                        "label": "Depends on",
                        "path": dependency["path"],
                        "line_start": dependency["line_start"],
                        "line_end": dependency["line_end"],
                        "kind": dependency["kind"],
                        "snippet": dependency["snippet"],
                    })
                    sources.append({
                        "path": dependency["path"],
                        "line_start": dependency["line_start"],
                        "line_end": dependency["line_end"],
                        "reason": f"file-flow-dependency:{summary['path']}",
                    })

            if dependents:
                lines.append("USED BY:")
                for dependent in dependents[:4]:
                    lines.append(
                        f"- {dependent['path']}:{dependent['line_start']} ({dependent['kind']}) {dependent['snippet']}"
                    )
                    section_entries.append({
                        "label": "Used by",
                        "path": dependent["path"],
                        "line_start": dependent["line_start"],
                        "line_end": dependent["line_end"],
                        "kind": dependent["kind"],
                        "snippet": dependent["snippet"],
                    })
                    sources.append({
                        "path": dependent["path"],
                        "line_start": dependent["line_start"],
                        "line_end": dependent["line_end"],
                        "reason": f"file-flow-dependent:{summary['path']}",
                    })

            if len(lines) > 1:
                blocks.append("\n".join(lines))
                sections.append({
                    "type": "file",
                    "title": summary["path"],
                    "summary": f"Local dependency and dependent file flow for {summary['path']}.",
                    "entries": section_entries,
                })

        deduped_sources: list[dict[str, Any]] = []
        seen = set()
        for source in sources:
            key = (source["path"], source["line_start"], source["line_end"], source["reason"])
            if key in seen:
                continue
            seen.add(key)
            deduped_sources.append(source)
        return blocks, deduped_sources, sections

    def _build_impact_assessment(
        self,
        query: str,
        symbol_targets: list[str],
        file_targets: list[str],
    ) -> tuple[str, list[dict[str, Any]]]:
        lowered_tokens = set(query.lower().split())
        if not any(token in lowered_tokens for token in IMPACT_HINTS):
            return "", []

        entries: list[dict[str, Any]] = []
        summary_lines = ["IMPACT SUMMARY:"]

        for summary in self._symbol_graph.describe_symbols(symbol_targets[:2]):
            callers = summary.get("callers", [])
            if callers:
                summary_lines.append(
                    f"- Changing symbol `{summary['name']}` may affect {len(callers)} local caller(s)."
                )
                for caller in callers[:4]:
                    entries.append({
                        "label": f"Caller of {summary['name']}",
                        "path": caller["path"],
                        "line_start": caller["line_start"],
                        "line_end": caller["line_end"],
                        "kind": caller["kind"],
                        "snippet": caller["snippet"],
                    })

        for file_target in file_targets[:2]:
            summary = self._symbol_graph.describe_file(file_target)
            if not summary:
                continue
            dependents = summary.get("dependents", [])
            if dependents:
                summary_lines.append(
                    f"- Changing file `{summary['path']}` may affect {len(dependents)} dependent file(s)."
                )
                for dependent in dependents[:4]:
                    entries.append({
                        "label": f"Dependent of {summary['path']}",
                        "path": dependent["path"],
                        "line_start": dependent["line_start"],
                        "line_end": dependent["line_end"],
                        "kind": dependent["kind"],
                        "snippet": dependent["snippet"],
                    })

        deduped_entries: list[dict[str, Any]] = []
        seen = set()
        for entry in entries:
            key = (entry["label"], entry["path"], entry["line_start"], entry["line_end"])
            if key in seen:
                continue
            seen.add(key)
            deduped_entries.append(entry)

        if len(summary_lines) == 1 and not deduped_entries:
            return "", []

        if len(summary_lines) == 1:
            summary_lines.append("- No strong local callers or dependent files were detected from the indexed workspace.")

        return "\n".join(summary_lines), deduped_entries[:8]

    def _score_file(
        self,
        query: str,
        tokens: list[str],
        file_info: dict[str, Any],
        recent_file_paths: set[str] | None = None,
        recency_bias: bool = False,
        symbol_targets: list[str] | None = None,
    ) -> tuple[int, dict[str, Any] | None]:
        path_text = file_info["path"].lower()
        name_text = file_info["name"].lower()
        score = 0
        reasons: list[str] = []

        if query and query in path_text:
            score += 20
            reasons.append("path match")

        for token in tokens:
            if token in name_text:
                score += 16
                reasons.append(f"name:{token}")
            elif token in path_text:
                score += 8
                reasons.append(f"path:{token}")

        if recent_file_paths and file_info["path"] in recent_file_paths:
            score += 6
            reasons.append("recently opened")
            if recency_bias:
                score += 18
                reasons.append("context follow-up")

        if score == 0 and not any(token in path_text for token in tokens):
            return 0, None

        content = self._read_text(file_info["path"])
        if not content:
            return score, {
                "file": file_info["path"],
                "score": score,
                "reason": ", ".join(dict.fromkeys(reasons)) or "path similarity",
                "snippet": "",
                "line_start": 1,
                "line_end": 1,
            }

        symbol_match = None
        if symbol_targets and any(target.lower() in content.lower() for target in symbol_targets):
            symbol_match = self._find_symbol_match(file_info["path"], content, symbol_targets)
            if symbol_match:
                score += 40
                reasons.append(f"symbol:{symbol_match['name']}")

        lines = content.splitlines()
        best_line = 0
        best_line_score = 0
        for idx, line in enumerate(lines):
            line_lower = line.lower()
            line_score = 0
            if query and query in line_lower:
                line_score += 30
            for token in tokens:
                if token in line_lower:
                    line_score += 4
            if line_score > best_line_score:
                best_line_score = line_score
                best_line = idx

        score += best_line_score
        if best_line_score:
            reasons.append("content match")

        if symbol_match:
            start = max(0, symbol_match["line_start"] - 1)
            end = min(len(lines), symbol_match["line_end"])
            snippet = symbol_match["snippet"]
            line_start = symbol_match["line_start"]
            line_end = symbol_match["line_end"]
        else:
            start = max(0, best_line - 4)
            end = min(len(lines), best_line + 5)
            snippet = "\n".join(lines[start:end]).strip()
            line_start = start + 1
            line_end = max(start + 1, end)

        return score, {
            "file": file_info["path"],
            "score": score,
            "reason": ", ".join(dict.fromkeys(reasons)) or "related content",
            "snippet": snippet[:1800],
            "line_start": line_start,
            "line_end": line_end,
        }

    def build_context(self, query: str, max_files: int = 4, max_chars: int = 7000) -> dict[str, Any]:
        cleaned_query = query.strip().lower()
        tokens = self._tokenize(query)
        symbol_targets = self._extract_symbol_targets(query)
        file_targets = self._extract_file_targets(query)
        explicit_target_files = self._matching_file_infos(file_targets)
        recency_bias = any(hint in cleaned_query.split() for hint in CONTEXTUAL_QUERY_HINTS)
        recent_file_paths = set() if explicit_target_files else set(self._recent_files())
        active_file = self._active_editor_file()
        if active_file and not explicit_target_files:
            recent_file_paths.add(active_file)
        candidates: list[dict[str, Any]] = []

        search_space = explicit_target_files or self._all_files()
        for file_info in search_space:
            score, candidate = self._score_file(
                cleaned_query,
                tokens,
                file_info,
                recent_file_paths=recent_file_paths,
                recency_bias=recency_bias,
                symbol_targets=symbol_targets,
            )
            if candidate and score > 0:
                candidates.append(candidate)

        candidates.sort(key=lambda item: item["score"], reverse=True)
        selected = candidates[:max_files]

        context_parts: list[str] = []
        used_chars = 0
        sources: list[dict[str, Any]] = []
        flow_sections: list[dict[str, Any]] = []
        project_overview = ""
        if self._is_project_overview_query(query, file_targets):
            project_overview, overview_sources = self._build_project_overview()
            if project_overview:
                context_parts.append(project_overview)
                used_chars += len(project_overview)
                sources.extend(overview_sources)
        symbol_flow_blocks, symbol_flow_sources, symbol_flow_sections = self._build_symbol_flow_blocks(symbol_targets)
        for block in symbol_flow_blocks:
            if used_chars + len(block) > max_chars and context_parts:
                break
            context_parts.append(block)
            used_chars += len(block)
        sources.extend(symbol_flow_sources)
        flow_sections.extend(symbol_flow_sections)
        if file_targets and any(hint in cleaned_query for hint in FILE_FLOW_HINTS):
            file_flow_blocks, file_flow_sources, file_flow_sections = self._build_file_flow_blocks(file_targets)
            for block in file_flow_blocks:
                if used_chars + len(block) > max_chars and context_parts:
                    break
                context_parts.append(block)
                used_chars += len(block)
            sources.extend(file_flow_sources)
            flow_sections.extend(file_flow_sections)
        impact_summary, impact_entries = self._build_impact_assessment(query, symbol_targets, file_targets)
        if impact_summary:
            if used_chars + len(impact_summary) <= max_chars or not context_parts:
                context_parts.append(impact_summary)
                used_chars += len(impact_summary)
            flow_sections.append({
                "type": "impact",
                "title": "Impact Assessment",
                "summary": "Local callers and dependent files that may be affected by the requested change.",
                "entries": impact_entries,
            })
            for entry in impact_entries:
                sources.append({
                    "path": entry["path"],
                    "line_start": entry.get("line_start") or 1,
                    "line_end": entry.get("line_end") or entry.get("line_start") or 1,
                    "reason": "impact-assessment",
                })

        for item in selected:
            snippet = item["snippet"] or "(file matched by name/path, snippet unavailable)"
            block = (
                f"FILE: {item['file']}\n"
                f"LINES: {item['line_start']}-{item['line_end']}\n"
                f"WHY: {item['reason']}\n"
                f"SNIPPET:\n{snippet}\n"
            )
            if used_chars + len(block) > max_chars and context_parts:
                break
            context_parts.append(block)
            used_chars += len(block)
            sources.append({
                "path": item["file"],
                "line_start": item["line_start"],
                "line_end": item["line_end"],
                "reason": item["reason"],
            })

        index = self._load_index()
        memory_summary = self._build_memory_summary()
        workspace_summary = (
            f"Workspace has {index.get('indexed_files', 0)} indexed files across "
            f"{len(index.get('folders', {}))} folders."
        )
        if memory_summary:
            workspace_summary = f"{workspace_summary}\n{memory_summary}"

        if memory_summary:
            memory_block = f"PROJECT MEMORY:\n{memory_summary}"
            if context_parts:
                context_parts.insert(0, memory_block)
            else:
                context_parts.append(memory_block)

        editor_context = self._load_editor_context()
        selected_text = str(editor_context.get("selectedText", "")).strip()
        active_path = str(editor_context.get("relativePath") or editor_context.get("activeFilePath") or "").replace("\\", "/")
        if active_path and selected_text:
            selection_start = (editor_context.get("selection") or {}).get("startLine") or editor_context.get("cursorLine") or 1
            selection_end = (editor_context.get("selection") or {}).get("endLine") or selection_start
            selected_block = (
                f"ACTIVE EDITOR SELECTION:\n"
                f"FILE: {active_path}\n"
                f"LINES: {selection_start}-{selection_end}\n"
                f"SNIPPET:\n{selected_text[:1800]}\n"
            )
            context_parts.insert(0, selected_block)
            sources.insert(0, {
                "path": active_path,
                "line_start": selection_start,
                "line_end": selection_end,
                "reason": "active editor selection",
            })

        return {
            "workspace_summary": workspace_summary,
            "context_text": "\n\n".join(context_parts).strip(),
            "sources": sources,
            "flow_sections": flow_sections,
            "project_overview": project_overview,
        }
