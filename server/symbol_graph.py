import os
import re
from pathlib import Path
from typing import Any

try:
    import project_indexer
    from ast_engine import ASTEngine
except ImportError:
    import project_indexer
    from ast_engine import ASTEngine


SUPPORTED_EXTENSIONS = {".py", ".ts", ".tsx", ".js", ".jsx", ".cpp", ".cc", ".cxx", ".h", ".hpp"}
SYMBOL_FOLDERS = [
    "src",
    "include",
    "server",
    "desktop_app/src",
    "tests",
]


class SymbolGraph:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self._cache: dict[str, Any] | None = None
        self._signature: tuple[tuple[str, float, int], ...] | None = None
        self.ast_engine = ASTEngine()

    def _iter_indexable_files(self) -> list[Path]:
        files: list[Path] = []
        for folder in SYMBOL_FOLDERS:
            folder_path = self.workspace_root / folder
            if not folder_path.exists():
                continue
            for root, dirs, names in os.walk(folder_path):
                dirs[:] = [d for d in dirs if d not in project_indexer.SKIP_FOLDERS]
                for name in names:
                    file_path = Path(root) / name
                    if not project_indexer.should_index_file(file_path):
                        continue
                    if file_path.suffix.lower() not in SUPPORTED_EXTENSIONS:
                        continue
                    if "brain\\knowledge" in str(file_path) or "brain/knowledge" in str(file_path):
                        continue
                    files.append(file_path)
        return files

    def _compute_signature(self) -> tuple[tuple[str, float, int], ...]:
        rows: list[tuple[str, float, int]] = []
        for file_path in self._iter_indexable_files():
            stat = file_path.stat()
            rel_path = str(file_path.relative_to(self.workspace_root)).replace("\\", "/")
            rows.append((rel_path, stat.st_mtime, stat.st_size))
        rows.sort()
        return tuple(rows)

    def _read_text(self, file_path: Path) -> str:
        try:
            return file_path.read_text(encoding="utf-8", errors="replace")
        except OSError:
            return ""

    def _normalize_rel_path(self, rel_path: str) -> str:
        return rel_path.replace("\\", "/").lstrip("./")

    def _patterns_for_extension(self, ext: str) -> list[tuple[re.Pattern[str], str, int]]:
        if ext == ".py":
            return [
                (re.compile(r"^\s*def\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("), "function", 1),
                (re.compile(r"^\s*class\s+([A-Za-z_][A-Za-z0-9_]*)\s*[:(]"), "class", 1),
            ]
        if ext in {".ts", ".tsx", ".js", ".jsx"}:
            return [
                (re.compile(r"^\s*(?:export\s+)?(?:async\s+)?function\s+([A-Za-z_][A-Za-z0-9_]*)\s*\("), "function", 1),
                (re.compile(r"^\s*(?:export\s+)?class\s+([A-Za-z_][A-Za-z0-9_]*)\b"), "class", 1),
                (re.compile(r"^\s*(?:export\s+)?interface\s+([A-Za-z_][A-Za-z0-9_]*)\b"), "interface", 1),
                (re.compile(r"^\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(?:async\s*)?\("), "function", 1),
                (re.compile(r"^\s*(?:export\s+)?(?:const|let|var)\s+([A-Za-z_][A-Za-z0-9_]*)\s*="), "variable", 1),
            ]
        if ext in {".cpp", ".cc", ".cxx", ".h", ".hpp"}:
            return [
                (re.compile(r"^\s*(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\b"), "class", 1),
                (re.compile(r"^\s*(?:[\w:<>~*&]+\s+)+([A-Za-z_][A-Za-z0-9_:]*)\s*\([^;]*\)\s*\{"), "function", 1),
            ]
        return []

    def _extract_symbols(self, rel_path: str, content: str) -> list[dict[str, Any]]:
        ext = Path(rel_path).suffix.lower()
        tree = self.ast_engine.parse_code(content, ext)
        if not tree:
            return []
        
        symbols = self.ast_engine.extract_symbols(tree, content, ext)
        for sym in symbols:
            sym["path"] = rel_path
        return symbols

    def _extract_references(
        self,
        rel_path: str,
        content: str,
        symbol_names: set[str],
        definition_lines: set[tuple[str, int]],
    ) -> list[dict[str, Any]]:
        ext = Path(rel_path).suffix.lower()
        tree = self.ast_engine.parse_code(content, ext)
        if not tree:
            return []
        
        raw_refs = self.ast_engine.extract_references(tree, content, ext)
        
        references: list[dict[str, Any]] = []
        for ref in raw_refs:
            # Only keep references to symbols we actually know about in the codebase
            if ref["name"] in symbol_names:
                # Skip if it's the definition itself
                if (rel_path, ref["line_start"]) in definition_lines:
                    continue
                
                references.append({
                    "name": ref["name"],
                    "path": rel_path,
                    "line_start": ref["line_start"],
                    "line_end": ref["line_start"],
                    "kind": ref["kind"],
                    "snippet": ref["snippet"],
                })
        return references

    def _resolve_python_module(self, module_name: str) -> str | None:
        module_rel = module_name.replace(".", "/")
        candidates = [
            Path(f"{module_rel}.py"),
            Path(module_rel) / "__init__.py",
            Path("server") / f"{module_rel}.py",
            Path("server") / module_rel / "__init__.py",
        ]
        for candidate in candidates:
            full_path = self.workspace_root / candidate
            if full_path.exists():
                return self._normalize_rel_path(str(candidate))
        return None

    def _resolve_relative_module(self, rel_path: str, module_name: str) -> str | None:
        if not module_name.startswith("."):
            return self._resolve_python_module(module_name)

        current_dir = Path(rel_path).parent
        dot_count = len(module_name) - len(module_name.lstrip("."))
        module_suffix = module_name[dot_count:].replace(".", "/")
        base_dir = current_dir
        for _ in range(max(0, dot_count - 1)):
            base_dir = base_dir.parent

        candidates = []
        if module_suffix:
            candidates.append(base_dir / f"{module_suffix}.py")
            candidates.append(base_dir / module_suffix / "__init__.py")
        else:
            candidates.append(base_dir / "__init__.py")

        for candidate in candidates:
            full_path = self.workspace_root / candidate
            if full_path.exists():
                return self._normalize_rel_path(str(candidate))
        return None

    def _resolve_js_import(self, rel_path: str, import_path: str) -> str | None:
        if import_path.startswith("."):
            base = Path(rel_path).parent / import_path
            candidates = [
                base,
                Path(f"{base}.ts"),
                Path(f"{base}.tsx"),
                Path(f"{base}.js"),
                Path(f"{base}.jsx"),
                base / "index.ts",
                base / "index.tsx",
                base / "index.js",
                base / "index.jsx",
            ]
        else:
            module_path = Path(import_path.replace("@/", "desktop_app/src/"))
            candidates = [
                module_path,
                Path(f"{module_path}.ts"),
                Path(f"{module_path}.tsx"),
                Path(f"{module_path}.js"),
                Path(f"{module_path}.jsx"),
            ]

        for candidate in candidates:
            full_path = self.workspace_root / candidate
            if full_path.exists():
                return self._normalize_rel_path(str(candidate))
        return None

    def _resolve_cpp_include(self, rel_path: str, include_path: str) -> str | None:
        candidates = [
            Path(rel_path).parent / include_path,
            Path("include") / include_path,
            Path("src") / include_path,
        ]
        for candidate in candidates:
            full_path = self.workspace_root / candidate
            if full_path.exists():
                return self._normalize_rel_path(str(candidate))
        return None

    def _extract_file_dependencies(self, rel_path: str, content: str) -> list[dict[str, Any]]:
        dependencies: list[dict[str, Any]] = []
        ext = Path(rel_path).suffix.lower()
        lines = content.splitlines()

        for index, line in enumerate(lines):
            line_no = index + 1
            if ext == ".py":
                import_match = re.match(r"^\s*import\s+([A-Za-z0-9_.,\s]+)", line)
                from_match = re.match(r"^\s*from\s+([A-Za-z0-9_\.]+)\s+import\s+", line)
                modules: list[str] = []
                if import_match:
                    modules = [part.strip().split(" as ")[0] for part in import_match.group(1).split(",") if part.strip()]
                elif from_match:
                    modules = [from_match.group(1).strip()]

                for module_name in modules:
                    target = self._resolve_relative_module(rel_path, module_name)
                    if target:
                        dependencies.append({
                            "path": target,
                            "line_start": line_no,
                            "line_end": line_no,
                            "kind": "import",
                            "snippet": line.strip()[:240],
                        })
            elif ext in {".ts", ".tsx", ".js", ".jsx"}:
                for import_path in re.findall(r"""from\s+['"]([^'"]+)['"]|import\s+['"]([^'"]+)['"]""", line):
                    raw = next((item for item in import_path if item), "")
                    if not raw:
                        continue
                    target = self._resolve_js_import(rel_path, raw)
                    if target:
                        dependencies.append({
                            "path": target,
                            "line_start": line_no,
                            "line_end": line_no,
                            "kind": "import",
                            "snippet": line.strip()[:240],
                        })
            elif ext in {".cpp", ".cc", ".cxx", ".h", ".hpp"}:
                include_match = re.match(r'^\s*#include\s+"([^"]+)"', line)
                if include_match:
                    target = self._resolve_cpp_include(rel_path, include_match.group(1))
                    if target:
                        dependencies.append({
                            "path": target,
                            "line_start": line_no,
                            "line_end": line_no,
                            "kind": "include",
                            "snippet": line.strip()[:240],
                        })

        deduped: list[dict[str, Any]] = []
        seen = set()
        for item in dependencies:
            key = (item["path"], item["line_start"], item["kind"])
            if key in seen:
                continue
            seen.add(key)
            deduped.append(item)
        return deduped

    def _build_graph(self) -> dict[str, Any]:
        definitions: dict[str, list[dict[str, Any]]] = {}
        definition_lines: set[tuple[str, int]] = set()
        file_contents: dict[str, str] = {}
        file_dependencies: dict[str, list[dict[str, Any]]] = {}

        for file_path in self._iter_indexable_files():
            rel_path = str(file_path.relative_to(self.workspace_root)).replace("\\", "/")
            content = self._read_text(file_path)
            file_contents[rel_path] = content
            file_dependencies[rel_path] = self._extract_file_dependencies(rel_path, content)
            for symbol in self._extract_symbols(rel_path, content):
                definitions.setdefault(symbol["name"], []).append(symbol)
                definition_lines.add((rel_path, symbol["line_start"]))

        known_symbols = set(definitions)
        references: dict[str, list[dict[str, Any]]] = {name: [] for name in known_symbols}
        for rel_path, content in file_contents.items():
            for reference in self._extract_references(rel_path, content, known_symbols, definition_lines):
                references.setdefault(reference["name"], []).append(reference)

        reverse_dependencies: dict[str, list[dict[str, Any]]] = {}
        for source_path, deps in file_dependencies.items():
            for dep in deps:
                reverse_dependencies.setdefault(dep["path"], []).append({
                    "path": source_path,
                    "line_start": dep["line_start"],
                    "line_end": dep["line_end"],
                    "kind": dep["kind"],
                    "snippet": dep["snippet"],
                })

        return {
            "definitions": definitions,
            "references": references,
            "file_dependencies": file_dependencies,
            "reverse_dependencies": reverse_dependencies,
        }

    def _graph(self) -> dict[str, Any]:
        signature = self._compute_signature()
        if self._cache is not None and self._signature == signature:
            return self._cache

        self._cache = self._build_graph()
        self._signature = signature
        return self._cache

    def describe_symbol(self, symbol_name: str) -> dict[str, Any] | None:
        graph = self._graph()
        definitions = graph["definitions"].get(symbol_name) or []
        if not definitions:
            lowered = symbol_name.lower()
            for candidate_name, candidate_defs in graph["definitions"].items():
                if candidate_name.lower() == lowered:
                    symbol_name = candidate_name
                    definitions = candidate_defs
                    break

        if not definitions:
            return None

        references = graph["references"].get(symbol_name) or []
        callers = [reference for reference in references if reference["kind"] == "call"]

        return {
            "name": symbol_name,
            "definitions": sorted(definitions, key=lambda item: (item["path"], item["line_start"])),
            "references": sorted(references, key=lambda item: (item["path"], item["line_start"]))[:8],
            "callers": sorted(callers, key=lambda item: (item["path"], item["line_start"]))[:6],
        }

    def describe_symbols(self, symbol_names: list[str]) -> list[dict[str, Any]]:
        described: list[dict[str, Any]] = []
        seen: set[str] = set()
        for symbol_name in symbol_names:
            if not symbol_name or symbol_name.lower() in seen:
                continue
            summary = self.describe_symbol(symbol_name)
            if summary:
                described.append(summary)
                seen.add(summary["name"].lower())
        return described

    def describe_file(self, rel_path: str) -> dict[str, Any] | None:
        graph = self._graph()
        normalized = self._normalize_rel_path(rel_path)
        dependencies = graph["file_dependencies"].get(normalized)
        reverse_dependencies = graph["reverse_dependencies"].get(normalized, [])

        if dependencies is None and reverse_dependencies == []:
            return None

        return {
            "path": normalized,
            "dependencies": sorted(dependencies or [], key=lambda item: (item["path"], item["line_start"]))[:8],
            "dependents": sorted(reverse_dependencies, key=lambda item: (item["path"], item["line_start"]))[:8],
        }
