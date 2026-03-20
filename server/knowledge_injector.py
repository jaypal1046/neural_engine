# server/knowledge_injector.py
import os
import subprocess
import tempfile
from pathlib import Path
from ast_engine import ASTEngine
from symbol_graph import SymbolGraph

class KnowledgeInjector:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.ast_engine = ASTEngine()
        self.engine_path = str(self.workspace_root / "bin" / "neural_engine.exe")

    def index_codebase_semantically(self):
        """
        1. Walk the codebase
        2. Parse with Tree-sitter
        3. Extract semantic chunks (functions/classes)
        4. Group by file
        5. Push into Neural Engine for vector indexing
        """
        print(f"\n[BRAIN] Starting Semantic Code Indexing...")
        
        graph = SymbolGraph(str(self.workspace_root))
        data = graph._graph()
        
        # Group definitions by file
        file_map = {}
        for name, defs in data["definitions"].items():
            for d in defs:
                path = d["path"]
                if path not in file_map:
                    file_map[path] = []
                file_map[path].append((name, d))
        
        total_files = 0
        for path, symbols in file_map.items():
            full_path = self.workspace_root / path
            if not full_path.exists(): continue
            
            lines = full_path.read_text(encoding="utf-8").splitlines()
            
            file_summary = f"FILE: {path}\nSYMBOLS IN THIS FILE:\n"
            for name, d in symbols:
                kind = d["kind"]
                line_start = d["line_start"]
                line_end = d["line_end"]
                
                chunk_lines = lines[line_start-1 : line_end]
                code_content = "\n".join(chunk_lines)
                
                file_summary += f"--- {kind.upper()}: {name} (Lines {line_start}-{line_end}) ---\n"
                file_summary += f"{code_content}\n\n"
            
            # Push the entire file's semantic summary into the brain
            print(f"  Indexing {path} ({len(symbols)} symbols)...")
            self._teach_brain(path, file_summary)
            total_files += 1
                
        print(f"[OK] Indexed components from {total_files} files into Vector DB.")

    def _teach_brain(self, topic, content):
        """Calls C++ learn command."""
        # We'll use a temp file to avoid CLI argument length limits
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as tmp:
            tmp.write(content)
            tmp_path = tmp.name
            
        try:
            # Command: neural_engine learn_file <topic> <path>
            # Based on src/neural_engine.cpp handling
            subprocess.run(
                [self.engine_path, "learn", tmp_path],
                cwd=str(self.workspace_root),
                capture_output=True
            )
        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

if __name__ == "__main__":
    injector = KnowledgeInjector(".")
    injector.index_codebase_semantically()
