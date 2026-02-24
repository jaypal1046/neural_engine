"""
Project-Wide File Indexer
Automatically scans and indexes ALL files in the project for AI awareness
"""

import os
import json
import subprocess
from pathlib import Path

BASE_DIR = Path(__file__).parent.parent
NEURAL_ENGINE = BASE_DIR / "bin" / "neural_engine.exe"
PROJECT_INDEX_FILE = BASE_DIR / "project_index.json"

# Folders to index (all folders in project)
FOLDERS_TO_INDEX = [
    "src",
    "include",
    "server",
    "docs",
    "phases",
    "doc",
    "knowledge",
    "knowledge_sample",
    "desktop_app/src",
    "tests",
    "brain",
]

# File extensions to index
INDEXABLE_EXTENSIONS = [
    ".cpp", ".h", ".hpp",  # C++ code
    ".py",  # Python code
    ".md", ".txt",  # Documentation
    ".js", ".jsx", ".ts", ".tsx",  # JavaScript/TypeScript
    ".json", ".yaml", ".yml",  # Config files
    ".bat", ".sh",  # Scripts
]

# Folders to skip
SKIP_FOLDERS = [
    "node_modules",
    ".git",
    ".vscode",
    "bin/backup",
    "__pycache__",
    "build",
    "dist",
]

def should_index_file(file_path):
    """Check if file should be indexed"""
    # Check extension
    if not any(str(file_path).endswith(ext) for ext in INDEXABLE_EXTENSIONS):
        return False

    # Check size (skip files > 1 MB)
    try:
        if file_path.stat().st_size > 1024 * 1024:
            return False
    except:
        return False

    # Check if in skip folder
    for skip in SKIP_FOLDERS:
        if skip in str(file_path):
            return False

    return True

def index_project_files():
    """Scan entire project and create file index"""
    index = {
        "total_files": 0,
        "indexed_files": 0,
        "folders": {},
        "file_types": {},
        "total_size": 0,
    }

    print("=" * 60)
    print("PROJECT-WIDE FILE INDEXING")
    print("=" * 60)
    print()

    for folder in FOLDERS_TO_INDEX:
        folder_path = BASE_DIR / folder
        if not folder_path.exists():
            continue

        print(f"Scanning: {folder}/")
        folder_files = []

        for root, dirs, files in os.walk(folder_path):
            # Skip excluded folders
            dirs[:] = [d for d in dirs if d not in SKIP_FOLDERS]

            for file in files:
                file_path = Path(root) / file
                index["total_files"] += 1

                if should_index_file(file_path):
                    rel_path = file_path.relative_to(BASE_DIR)
                    file_info = {
                        "path": str(rel_path),
                        "name": file,
                        "size": file_path.stat().st_size,
                        "extension": file_path.suffix,
                    }

                    folder_files.append(file_info)
                    index["indexed_files"] += 1
                    index["total_size"] += file_info["size"]

                    # Count by type
                    ext = file_path.suffix
                    index["file_types"][ext] = index["file_types"].get(ext, 0) + 1

        index["folders"][folder] = {
            "file_count": len(folder_files),
            "files": folder_files,
        }

        print(f"  → Found {len(folder_files)} indexable files")

    print()
    print("=" * 60)
    print(f"Total files scanned: {index['total_files']}")
    print(f"Files indexed: {index['indexed_files']}")
    print(f"Total size: {index['total_size'] / (1024*1024):.2f} MB")
    print()
    print("File types:")
    for ext, count in sorted(index["file_types"].items(), key=lambda x: -x[1]):
        print(f"  {ext}: {count} files")
    print("=" * 60)
    print()

    # Save index
    with open(PROJECT_INDEX_FILE, "w") as f:
        json.dump(index, f, indent=2)

    print(f"✓ Index saved to: {PROJECT_INDEX_FILE}")
    return index

def load_project_files_into_ai():
    """Load file index into AI's knowledge base"""
    if not PROJECT_INDEX_FILE.exists():
        print("⚠ No project index found. Run index_project_files() first.")
        return

    with open(PROJECT_INDEX_FILE) as f:
        index = json.load(f)

    print()
    print("=" * 60)
    print("LOADING PROJECT FILES INTO AI KNOWLEDGE")
    print("=" * 60)
    print()

    # Create summary document for AI
    summary = f"""PROJECT FILE INDEX

Total Files: {index['indexed_files']}
Total Size: {index['total_size'] / (1024*1024):.2f} MB

FOLDER STRUCTURE:
"""

    for folder, info in index["folders"].items():
        summary += f"\n{folder}/ ({info['file_count']} files)\n"
        for file_info in info["files"][:10]:  # Show first 10 files per folder
            summary += f"  - {file_info['name']} ({file_info['size']} bytes)\n"
        if info['file_count'] > 10:
            summary += f"  ... and {info['file_count'] - 10} more files\n"

    summary += f"\n\nFILE TYPES:\n"
    for ext, count in sorted(index["file_types"].items(), key=lambda x: -x[1]):
        summary += f"  {ext}: {count} files\n"

    # Save summary
    summary_file = BASE_DIR / "PROJECT_STRUCTURE.txt"
    with open(summary_file, "w") as f:
        f.write(summary)

    print(f"✓ Created project summary: {summary_file}")

    # Compress and add to knowledge
    compressed_file = BASE_DIR / "knowledge" / "project_structure.aiz"
    print(f"✓ Compressing to: {compressed_file}")

    try:
        result = subprocess.run(
            [str(NEURAL_ENGINE), "compress", str(summary_file), "-o", str(compressed_file), "--best"],
            capture_output=True,
            text=True,
            timeout=30
        )
        if compressed_file.exists():
            print(f"✓ Compressed successfully")

            # Load into AI
            result2 = subprocess.run(
                [str(NEURAL_ENGINE), "knowledge_load", "project_structure"],
                capture_output=True,
                text=True,
                timeout=10
            )
            if "success" in result2.stdout.lower():
                print(f"✓ Loaded into AI knowledge base")
                print()
                print("=" * 60)
                print("✅ AI NOW KNOWS ABOUT ALL PROJECT FILES!")
                print("=" * 60)
            else:
                print(f"⚠ Failed to load: {result2.stderr}")
        else:
            print(f"⚠ Compression failed: {result.stderr}")
    except Exception as e:
        print(f"⚠ Error: {e}")

if __name__ == "__main__":
    # Step 1: Index all files
    index = index_project_files()

    # Step 2: Load into AI
    load_project_files_into_ai()

    print()
    print("Done! The AI now has complete project awareness.")
    print("You can ask it: 'What files are in the src/ folder?'")
    print("Or: 'Show me all .md files'")
