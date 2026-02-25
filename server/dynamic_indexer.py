"""
PYTHON SUPPORT FILE — File System Watcher Only
===============================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

dynamic_indexer.py — Watch for file changes and re-trigger C++ learning.
Role: Monitor project directory for new/modified/deleted files.
      When a file changes: call neural_engine.exe learn <file>
      so C++ re-indexes the updated content automatically.
Does NOT: learn anything itself, store knowledge, answer questions.
      Python only watches the filesystem and calls C++ when needed.

Runs as a background daemon thread started by main.py at startup:
  threading.Thread(target=dynamic_indexer.start_dynamic_indexing)

See: docs/ARCHITECTURE.md for the full system design.
"""

import os
import time
import threading
from pathlib import Path
from watchdog.observers import Observer
from watchdog.events import FileSystemEventHandler
import project_indexer

class ProjectFileWatcher(FileSystemEventHandler):
    """Watches project files and triggers re-indexing on changes"""

    def __init__(self):
        self.last_index_time = 0
        self.index_delay = 5  # Wait 5 seconds after last change before re-indexing

    def on_any_event(self, event):
        """Triggered on any file system event"""
        if event.is_directory:
            return

        # Skip temporary files and excluded folders
        path_str = str(event.src_path)
        if any(skip in path_str for skip in project_indexer.SKIP_FOLDERS):
            return

        # Skip non-indexable files
        if not any(path_str.endswith(ext) for ext in project_indexer.INDEXABLE_EXTENSIONS):
            return

        # Debounce: only index after delay
        self.last_index_time = time.time()

    def start_debounced_indexer(self):
        """Run indexer after delay (debounced)"""
        while True:
            time.sleep(1)
            if self.last_index_time > 0 and (time.time() - self.last_index_time) >= self.index_delay:
                print("\n>> File changes detected, re-indexing project...")
                try:
                    project_indexer.index_project_files()
                    project_indexer.load_project_files_into_ai()
                    print(">> ✓ AI knowledge updated with latest project state")
                except Exception as e:
                    print(f">> ⚠ Re-indexing failed: {e}")
                self.last_index_time = 0  # Reset

def start_dynamic_indexing():
    """Start dynamic file watching and indexing"""
    print("=" * 60)
    print("STARTING DYNAMIC PROJECT INDEXING")
    print("=" * 60)
    print()

    # Initial index
    print(">> Performing initial project scan...")
    try:
        project_indexer.index_project_files()
        project_indexer.load_project_files_into_ai()
    except Exception as e:
        print(f">> ⚠ Initial indexing failed: {e}")
        return

    # Start file watcher
    event_handler = ProjectFileWatcher()
    observer = Observer()

    # Watch all indexed folders
    base_dir = Path(__file__).parent.parent
    for folder in project_indexer.FOLDERS_TO_INDEX:
        folder_path = base_dir / folder
        if folder_path.exists():
            observer.schedule(event_handler, str(folder_path), recursive=True)
            print(f">> Watching: {folder}/")

    observer.start()

    # Start debounced indexer thread
    indexer_thread = threading.Thread(target=event_handler.start_debounced_indexer, daemon=True)
    indexer_thread.start()

    print()
    print("=" * 60)
    print("✅ DYNAMIC INDEXING ACTIVE")
    print("=" * 60)
    print("AI will automatically update when project files change!")
    print()

if __name__ == "__main__":
    start_dynamic_indexing()

    # Keep running
    try:
        while True:
            time.sleep(1)
    except KeyboardInterrupt:
        print("\nStopping dynamic indexer...")
