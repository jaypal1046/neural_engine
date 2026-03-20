# server/ast_engine.py
# TREE-SITTER INTEGRATION — DEEP CODE PARSING FOR THE BRAIN
# ==========================================================
import os
import sys
from pathlib import Path
from typing import Any, List, Dict, Tuple, Optional

try:
    import tree_sitter
    from tree_sitter_languages import get_parser as ts_get_parser
except ImportError:
    print("[ERROR] tree-sitter or tree-sitter-languages not found. Please install them.")
    sys.exit(1)

# Language Map for Parsers
LANGUAGES = {
    ".py": "python",
    ".ts": "typescript",
    ".tsx": "tsx",
    ".js": "javascript",
    ".jsx": "javascript",
    ".cpp": "cpp",
    ".cc": "cpp",
    ".cxx": "cpp",
    ".h": "cpp",
    ".hpp": "cpp",
}

class ASTEngine:
    def __init__(self):
        self._parsers: Dict[str, Any] = {}

    def get_parser(self, extension: str) -> Optional[Any]:
        lang_name = LANGUAGES.get(extension.lower())
        if not lang_name:
            return None
        
        if lang_name not in self._parsers:
            try:
                from tree_sitter_languages import get_language
                lang = get_language(lang_name)
                parser = tree_sitter.Parser()
                parser.set_language(lang)
                self._parsers[lang_name] = parser
            except Exception as e:
                print(f"[AST] Failed to load language {lang_name}: {e}")
                return None
        
        return self._parsers.get(lang_name)

    def parse_code(self, code: str, extension: str) -> Optional[Any]:
        parser = self.get_parser(extension)
        if not parser:
            return None
        
        try:
            tree = parser.parse(bytes(code, "utf8"))
            return tree
        except Exception as e:
            print(f"[AST] Parsing failed for {extension}: {e}")
            return None

    def _get_node_name(self, node: Any, field_name: str) -> Optional[str]:
        target = node.child_by_field_name(field_name)
        if target:
            if hasattr(target, 'text'):
                return target.text.decode("utf8")
        return None

    def extract_symbols(self, tree: Any, content: str, extension: str) -> List[Dict[str, Any]]:
        if not tree:
            return []
        
        symbols = []
        root_node = tree.root_node
        lang_name = LANGUAGES.get(extension.lower())
        
        # Traverse the tree for definitions
        def traverse(node):
            # Check for generic definition types
            # Note: node.type varies by language, so we check for common patterns
            is_definition = False
            kind = "unknown"
            name = None

            if "definition" in node.type or "declaration" in node.type:
                is_definition = True
                kind = node.type.split("_")[0] # basic guess: 'function', 'class', etc.
                # Try common field names for names
                name = self._get_node_name(node, "name") or self._get_node_name(node, "declarator") or self._get_node_name(node, "identifier")

            if is_definition and name:
                symbols.append({
                    "name": name,
                    "kind": kind,
                    "line_start": node.start_point[0] + 1,
                    "line_end": node.end_point[0] + 1,
                    "definition_line": content.splitlines()[node.start_point[0]] if node.start_point[0] < len(content.splitlines()) else ""
                })

            for child in node.children:
                traverse(child)

        traverse(root_node)
        return symbols

    def extract_references(self, tree: Any, content: str, extension: str) -> List[Dict[str, Any]]:
        if not tree:
            return []
        
        references = []
        root_node = tree.root_node
        lang_name = LANGUAGES.get(extension.lower())
        
        def traverse(node):
            # General patterns for calls and identifiers
            is_call = False
            name = None

            if node.type in ("call", "call_expression", "function_call"):
                is_call = True
                # In many languages, the "function" field or the first child is the target
                target = node.child_by_field_name("function") or node.child_by_field_name("callee") or node.children[0]
                if target:
                    name = target.text.decode("utf8") if hasattr(target, 'text') else None
            
            # Also catch generic identifiers that might be variable accesses
            elif node.type in ("identifier", "variable_name", "type_identifier"):
                name = node.text.decode("utf8") if hasattr(node, 'text') else None

            if name:
                references.append({
                    "name": name,
                    "kind": "call" if is_call else "reference",
                    "line_start": node.start_point[0] + 1,
                    "snippet": content.splitlines()[node.start_point[0]] if node.start_point[0] < len(content.splitlines()) else ""
                })

            for child in node.children:
                traverse(child)

        traverse(root_node)
        return references
