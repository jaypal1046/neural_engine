# test_ast.py
import sys
import os
from pathlib import Path

# Add current directory to path
sys.path.append(os.getcwd())

from server.ast_engine import ASTEngine

def test():
    engine = ASTEngine()
    test_file = Path("server/symbol_graph.py")
    if not test_file.exists():
        print(f"File not found: {test_file}")
        return

    content = test_file.read_text(encoding="utf-8")
    tree = engine.parse_code(content, ".py")
    if tree:
        symbols = engine.extract_symbols(tree, content, ".py")
        print(f"\n--- Symbols in {test_file} ---\n")
        for sym in symbols:
            print(f"[{sym['kind']:10}] {sym['name']:25} | L{sym['line_start']}-L{sym['line_end']}")
    else:
        print("Failed to parse tree.")

if __name__ == "__main__":
    test()
