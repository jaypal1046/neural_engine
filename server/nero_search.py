# server/nero_search.py
import json
import subprocess
from pathlib import Path
from symbol_graph import SymbolGraph

class NeroSearch:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.engine_path = str(self.workspace_root / "bin" / "neural_engine.exe")

    def semantic_query(self, query):
        """Asks the C++ Brain for semantic matches (The 'Why' and 'Where')"""
        print(f"[SEARCH] Semantic Query: '{query}'...")
        
        # Command: neural_engine ask "<question>"
        result = subprocess.run(
            [self.engine_path, "ask", query],
            cwd=str(self.workspace_root),
            capture_output=True,
            text=True
        )
        
        try:
            # Parse the JSON output from neural_engine.cpp:374
            json_start = result.stdout.find("{")
            if json_start != -1:
                return json.loads(result.stdout[json_start:])
        except:
            return {"error": "Failed to parse brain response", "raw": result.stdout}
        
        return {"error": "No match found"}

    def structural_find(self, symbol_name):
        """Finds exact symbols using Tree-sitter (The 'What' and 'How')"""
        graph = SymbolGraph(str(self.workspace_root))
        data = graph._graph()
        
        matches = []
        # Case-insensitive partial match
        for name, defs in data["definitions"].items():
            if symbol_name.lower() in name.lower():
                for d in defs:
                    matches.append({
                        "name": name,
                        "kind": d["kind"],
                        "path": d["path"],
                        "line": d["line_start"]
                    })
        return matches

    def hybrid_search(self, query):
        """Combines both worlds."""
        semantic = self.semantic_query(query)
        structural = self.structural_find(query)
        
        return {
            "query": query,
            "semantic_match": semantic,
            "structural_matches": structural[:10] # Top 10
        }

if __name__ == "__main__":
    import sys
    query = sys.argv[1] if len(sys.argv) > 1 else "compression"
    searcher = NeroSearch(".")
    results = searcher.hybrid_search(query)
    print(json.dumps(results, indent=2))
