# server/project_bootstrapper.py
import sys
import subprocess
from pathlib import Path
from knowledge_injector import KnowledgeInjector
from git_memory import GitMemory
from graph_viz import visualize_project

def bootstrap_project():
    """ Runs the complete intelligence pipeline for the project. """
    print("\n" + "🚀"*10)
    print(" NERO BRAIN PROJECT BOOTSTRAP ")
    print("🚀"*10 + "\n")

    root = "."
    
    # 1. Structural Understanding (Tree-sitter)
    # This builds the call graph internal data
    print("🌳 Step 1: Building Structural Call Graph...")
    visualize_project(root)

    # 2. Semantic Memory (Vector DB)
    # This parses all files and pushes them into the C++ brain
    print("\n🧠 Step 2: Injecting Semantic Code Knowledge...")
    injector = KnowledgeInjector(root)
    injector.index_codebase_semantically()

    # 3. Historical Memory (Git)
    # This teaches the brain the 'why' behind the code
    print("\n📜 Step 3: Integrating Git History Memory...")
    memory = GitMemory(root)
    memory.analyze_history(max_commits=15)

    print("\n" + "✅"*10)
    print(" PROJECT FULLY BRAIN-READY ")
    print("✅"*10 + "\n")

if __name__ == "__main__":
    bootstrap_project()
