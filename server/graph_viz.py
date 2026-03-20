# server/graph_viz.py
from pathlib import Path
from symbol_graph import SymbolGraph

def visualize_project(root: str):
    graph = SymbolGraph(root)
    # Trigger graph build
    data = graph._graph()
    
    print("\n" + "="*50)
    print(" NERO BRAIN - PROJECT TOPOLOGY MAP")
    print("="*50 + "\n")

    # 1. File Dependencies (imports)
    print("📂 ARCHITECTURAL DEPENDENCIES (IMPORTS)")
    for file, deps in data["file_dependencies"].items():
        if deps:
            print(f"  {file}")
            for dep in deps:
                print(f"    └──> {dep['path']}")
    
    print("\n" + "-"*50 + "\n")

    # 2. Key Symbols and their Callers
    print("🧠 CROSS-FILE CALL GRAPH (KEY SYMBOLS)")
    for name, defs in data["definitions"].items():
        # Only show symbols called from other files
        refs = data["references"].get(name, [])
        external_calls = [r for r in refs if r["path"] != defs[0]["path"]]
        
        if external_calls:
            print(f"  {name} (defined in {defs[0]['path']})")
            for call in external_calls:
                print(f"    <── CALLED BY: {call['path']}:L{call['line_start']}")

if __name__ == "__main__":
    visualize_project(".")
