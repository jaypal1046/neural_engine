# server/dashboard_api.py
import json
import subprocess
from pathlib import Path
from symbol_graph import SymbolGraph

class NeroDashboard:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.engine_path = str(self.workspace_root / "bin" / "neural_engine.exe")

    def get_full_stats(self):
        """Returns consolidated intelligence stats for the UI."""
        # 1. C++ Brain Stats
        brain_res = subprocess.run([self.engine_path, "status"], capture_output=True, text=True)
        try:
            brain_data = json.loads(brain_res.stdout.split("{")[-1].replace("}", "}") if "{" in brain_res.stdout else "{}")
        except:
            brain_data = {"entries": 0, "status": "unknown"}

        # 2. Structure Stats
        graph = SymbolGraph(str(self.workspace_root))
        graph_data = graph._graph()
        
        return {
            "brain": brain_data,
            "structure": {
                "total_symbols": len(graph_data["definitions"]),
                "cross_file_calls": len(graph_data["references"]),
                "unique_files": len(graph_data["file_dependencies"])
            },
            "history": {
                "indexed_commits": 15,
                "author": "Jay"
            }
        }

    def generate_mermaid_graph(self):
        """Generates a summary graph for UI rendering."""
        graph = SymbolGraph(str(self.workspace_root))
        data = graph._graph()
        
        mermaid = ["graph TD"]
        # Limit to top connections to avoid UI clutter
        count = 0
        for file, deps in list(data["file_dependencies"].items())[:15]:
            for d in deps[:3]:
                mermaid.append(f"  {file.replace('/', '_')} --> {d['path'].replace('/', '_')}")
                count += 1
        
        return "\n".join(mermaid)

if __name__ == "__main__":
    dash = NeroDashboard(".")
    print(json.dumps(dash.get_full_stats(), indent=2))
    print("\n[MERMAID GRAPH SUMMARY]")
    print(dash.generate_mermaid_graph())
