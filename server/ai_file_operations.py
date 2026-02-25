"""
PYTHON SUPPORT FILE — File System Operations Only
==================================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

ai_file_operations.py — File search, read, and listing support.
Role: Provide file system operations to the React UI via main.py API.
      Search files, read file content, list folders, analyze code structure.
Does NOT: answer questions, store knowledge, generate AI responses.
      These are pure file I/O helper functions.

API endpoints that use this file (in main.py):
  POST /api/ai/search_files    → cmd_search_files(query)
  POST /api/ai/list_by_type    → cmd_list_by_type(query)
  POST /api/ai/list_folder     → cmd_list_folder(query)
  POST /api/ai/read_file       → cmd_read_file(path)
  POST /api/ai/analyze_file    → cmd_analyze_file(path)
  POST /api/ai/find_text       → cmd_find_text(text)
  GET  /api/ai/project_stats   → cmd_project_stats()

See: docs/ARCHITECTURE.md for the full system design.
"""

import os
import json
import subprocess
from pathlib import Path

BASE_DIR = Path(__file__).parent.parent
NEURAL_ENGINE = BASE_DIR / "bin" / "neural_engine.exe"
PROJECT_INDEX = BASE_DIR / "project_index.json"

class AIFileOperations:
    """AI-powered file operations using project index"""

    def __init__(self):
        self.load_index()

    def load_index(self):
        """Load project index"""
        if PROJECT_INDEX.exists():
            with open(PROJECT_INDEX) as f:
                self.index = json.load(f)
        else:
            self.index = {"folders": {}, "file_types": {}, "indexed_files": 0}

    def search_files(self, query):
        """Search for files matching query"""
        query_lower = query.lower()
        results = []

        for folder, info in self.index.get("folders", {}).items():
            for file_info in info.get("files", []):
                if (query_lower in file_info["name"].lower() or
                    query_lower in file_info["path"].lower()):
                    results.append({
                        "name": file_info["name"],
                        "path": file_info["path"],
                        "size": file_info["size"],
                        "folder": folder,
                    })

        return results

    def list_files_by_type(self, extension):
        """List all files with given extension"""
        if not extension.startswith("."):
            extension = f".{extension}"

        results = []
        for folder, info in self.index.get("folders", {}).items():
            for file_info in info.get("files", []):
                if file_info.get("extension") == extension:
                    results.append({
                        "name": file_info["name"],
                        "path": file_info["path"],
                        "size": file_info["size"],
                        "folder": folder,
                    })

        return results

    def list_files_in_folder(self, folder_name):
        """List all files in a specific folder"""
        folder_data = self.index.get("folders", {}).get(folder_name, {})
        return folder_data.get("files", [])

    def read_file(self, file_path):
        """Read file contents"""
        full_path = BASE_DIR / file_path
        if full_path.exists() and full_path.stat().st_size < 1024 * 1024:  # < 1MB
            try:
                with open(full_path, "r", encoding="utf-8") as f:
                    return f.read()
            except:
                return None
        return None

    def get_file_info(self, file_path):
        """Get detailed file information"""
        full_path = BASE_DIR / file_path
        if not full_path.exists():
            return None

        stat = full_path.stat()
        return {
            "path": str(file_path),
            "name": full_path.name,
            "size": stat.st_size,
            "extension": full_path.suffix,
            "exists": True,
        }

    def analyze_code_file(self, file_path):
        """Analyze code file (count lines, functions, etc.)"""
        content = self.read_file(file_path)
        if not content:
            return None

        lines = content.split("\n")
        analysis = {
            "total_lines": len(lines),
            "non_empty_lines": len([l for l in lines if l.strip()]),
            "comment_lines": 0,
            "function_count": 0,
        }

        ext = Path(file_path).suffix

        # Count comments and functions based on file type
        if ext in [".cpp", ".c", ".h", ".hpp"]:
            analysis["comment_lines"] = len([l for l in lines if l.strip().startswith("//")])
            analysis["function_count"] = content.count("(")  # Rough estimate

        elif ext == ".py":
            analysis["comment_lines"] = len([l for l in lines if l.strip().startswith("#")])
            analysis["function_count"] = content.count("def ")

        elif ext in [".js", ".ts", ".jsx", ".tsx"]:
            analysis["comment_lines"] = len([l for l in lines if l.strip().startswith("//")])
            analysis["function_count"] = content.count("function ") + content.count("=> ")

        return analysis

    def find_text_in_files(self, search_text):
        """Search for text across all indexed files"""
        results = []

        for folder, info in self.index.get("folders", {}).items():
            for file_info in info.get("files", []):
                content = self.read_file(file_info["path"])
                if content and search_text in content:
                    # Count occurrences
                    count = content.count(search_text)
                    # Find first occurrence line
                    lines = content.split("\n")
                    line_num = 0
                    for i, line in enumerate(lines):
                        if search_text in line:
                            line_num = i + 1
                            break

                    results.append({
                        "file": file_info["path"],
                        "occurrences": count,
                        "first_line": line_num,
                    })

        return results

# Create global instance
ai_files = AIFileOperations()

# API-style functions for AI commands

def cmd_search_files(query):
    """AI Command: search_files <query>"""
    results = ai_files.search_files(query)
    return {
        "command": "search_files",
        "query": query,
        "found": len(results),
        "results": results[:20],  # Limit to 20 results
    }

def cmd_list_by_type(extension):
    """AI Command: list_files_by_type <extension>"""
    results = ai_files.list_files_by_type(extension)
    return {
        "command": "list_by_type",
        "extension": extension,
        "found": len(results),
        "results": results,
    }

def cmd_list_folder(folder):
    """AI Command: list_folder <folder_name>"""
    results = ai_files.list_files_in_folder(folder)
    return {
        "command": "list_folder",
        "folder": folder,
        "found": len(results),
        "files": results,
    }

def cmd_read_file(file_path):
    """AI Command: read_file <path>"""
    content = ai_files.read_file(file_path)
    if content is None:
        return {"error": f"Cannot read file: {file_path}"}

    return {
        "command": "read_file",
        "file": file_path,
        "size": len(content),
        "content": content[:5000],  # First 5000 chars
        "truncated": len(content) > 5000,
    }

def cmd_analyze_file(file_path):
    """AI Command: analyze_file <path>"""
    analysis = ai_files.analyze_code_file(file_path)
    if analysis is None:
        return {"error": f"Cannot analyze file: {file_path}"}

    return {
        "command": "analyze_file",
        "file": file_path,
        **analysis
    }

def cmd_find_text(search_text):
    """AI Command: find_text <text>"""
    results = ai_files.find_text_in_files(search_text)
    return {
        "command": "find_text",
        "search": search_text,
        "found_in": len(results),
        "results": results[:10],  # Limit to 10 files
    }

def cmd_project_stats():
    """AI Command: project_stats"""
    ai_files.load_index()  # Reload latest
    return {
        "command": "project_stats",
        "total_files": ai_files.index.get("indexed_files", 0),
        "total_size_mb": ai_files.index.get("total_size", 0) / (1024*1024),
        "file_types": ai_files.index.get("file_types", {}),
        "folders": {k: v.get("file_count", 0) for k, v in ai_files.index.get("folders", {}).items()},
    }

if __name__ == "__main__":
    import sys

    if len(sys.argv) < 2:
        print("Usage: python ai_file_operations.py <command> [args]")
        print("\nCommands:")
        print("  search_files <query>")
        print("  list_by_type <extension>")
        print("  list_folder <folder>")
        print("  read_file <path>")
        print("  analyze_file <path>")
        print("  find_text <text>")
        print("  project_stats")
        sys.exit(1)

    cmd = sys.argv[1]

    if cmd == "search_files" and len(sys.argv) > 2:
        print(json.dumps(cmd_search_files(sys.argv[2]), indent=2))

    elif cmd == "list_by_type" and len(sys.argv) > 2:
        print(json.dumps(cmd_list_by_type(sys.argv[2]), indent=2))

    elif cmd == "list_folder" and len(sys.argv) > 2:
        print(json.dumps(cmd_list_folder(sys.argv[2]), indent=2))

    elif cmd == "read_file" and len(sys.argv) > 2:
        print(json.dumps(cmd_read_file(sys.argv[2]), indent=2))

    elif cmd == "analyze_file" and len(sys.argv) > 2:
        print(json.dumps(cmd_analyze_file(sys.argv[2]), indent=2))

    elif cmd == "find_text" and len(sys.argv) > 2:
        print(json.dumps(cmd_find_text(sys.argv[2]), indent=2))

    elif cmd == "project_stats":
        print(json.dumps(cmd_project_stats(), indent=2))

    else:
        print(f"Unknown command: {cmd}")
        sys.exit(1)
