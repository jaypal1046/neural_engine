import json
import os
from pathlib import Path
from typing import Any

try:
    import project_indexer
    from symbol_graph import SymbolGraph
except ImportError:
    from server import project_indexer
    from server.symbol_graph import SymbolGraph


class CodeGraphEngine:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.symbol_graph = SymbolGraph(str(self.workspace_root))

    def _scan_index_quietly(self) -> dict[str, Any]:
        index = {
            "total_files": 0,
            "indexed_files": 0,
            "folders": {},
            "file_types": {},
            "total_size": 0,
        }

        for folder in project_indexer.FOLDERS_TO_INDEX:
            folder_path = self.workspace_root / folder
            if not folder_path.exists():
                continue

            folder_files: list[dict[str, Any]] = []
            for root, dirs, names in os.walk(folder_path):
                dirs[:] = [d for d in dirs if d not in project_indexer.SKIP_FOLDERS]
                for name in names:
                    file_path = Path(root) / name
                    index["total_files"] += 1
                    if not project_indexer.should_index_file(file_path):
                        continue

                    rel_path = str(file_path.relative_to(self.workspace_root)).replace("\\", "/")
                    size = file_path.stat().st_size
                    folder_files.append({
                        "path": rel_path,
                        "name": name,
                        "size": size,
                        "extension": file_path.suffix.lower(),
                    })
                    index["indexed_files"] += 1
                    index["total_size"] += size
                    ext = file_path.suffix.lower()
                    index["file_types"][ext] = index["file_types"].get(ext, 0) + 1

            index["folders"][folder] = {
                "file_count": len(folder_files),
                "files": folder_files,
            }

        return index

    def _load_index(self) -> dict[str, Any]:
        index_file = self.workspace_root / "project_index.json"
        if index_file.exists():
            try:
                return json.loads(index_file.read_text(encoding="utf-8"))
            except (OSError, json.JSONDecodeError):
                pass
        return self._scan_index_quietly()

    def architecture_overview(self) -> dict[str, Any]:
        index = self._load_index()
        folders = index.get("folders", {}) or {}
        ranked = sorted(
            (
                {
                    "folder": name,
                    "file_count": info.get("file_count", 0),
                    "sample_files": [f.get("path") for f in (info.get("files") or [])[:4] if f.get("path")],
                }
                for name, info in folders.items()
                if info.get("file_count", 0) > 0
            ),
            key=lambda item: (-item["file_count"], item["folder"]),
        )

        narrative: list[str] = []
        folder_names = [item["folder"] for item in ranked[:6]]
        if folder_names:
            narrative.append(f"Main active folders: {', '.join(folder_names)}.")
        if "server" in folders:
            narrative.append("`server/` is the Python connector and orchestration layer.")
        if "src" in folders:
            narrative.append("`src/` contains the C++ brain and native engine implementation.")
        if "include" in folders:
            narrative.append("`include/` contains shared C++ interfaces and headers.")
        if "desktop_app/src" in folders:
            narrative.append("`desktop_app/src` contains the React/Electron desktop UI.")
        if "tests" in folders:
            narrative.append("`tests/` contains local regression and behavior checks.")

        return {
            "workspace_root": str(self.workspace_root),
            "indexed_files": index.get("indexed_files", 0),
            "folders": ranked,
            "narrative": narrative,
        }

    def describe_symbol(self, symbol_name: str) -> dict[str, Any]:
        summary = self.symbol_graph.describe_symbol(symbol_name)
        if not summary:
            return {
                "found": False,
                "symbol": symbol_name,
                "message": f"No known symbol named `{symbol_name}` was found in the indexed workspace.",
            }

        return {
            "found": True,
            "symbol": summary["name"],
            "definitions": summary.get("definitions", []),
            "references": summary.get("references", []),
            "callers": summary.get("callers", []),
        }

    def describe_file(self, rel_path: str) -> dict[str, Any]:
        summary = self.symbol_graph.describe_file(rel_path)
        if not summary:
            return {
                "found": False,
                "path": rel_path,
                "message": f"No graph information was found for `{rel_path}`.",
            }

        return {
            "found": True,
            "path": summary["path"],
            "dependencies": summary.get("dependencies", []),
            "dependents": summary.get("dependents", []),
        }

    def impact_analysis(self, *, symbol: str | None = None, rel_path: str | None = None) -> dict[str, Any]:
        symbol_result = self.describe_symbol(symbol) if symbol else None
        file_result = self.describe_file(rel_path) if rel_path else None

        impacts: list[dict[str, Any]] = []
        if symbol_result and symbol_result.get("found"):
            for caller in (symbol_result.get("callers") or [])[:8]:
                impacts.append({
                    "type": "symbol_caller",
                    "label": f"Caller of {symbol_result['symbol']}",
                    **caller,
                })
        if file_result and file_result.get("found"):
            for dependent in (file_result.get("dependents") or [])[:8]:
                impacts.append({
                    "type": "file_dependent",
                    "label": f"Dependent of {file_result['path']}",
                    **dependent,
                })

        return {
            "symbol": symbol_result,
            "file": file_result,
            "impacts": impacts,
        }
