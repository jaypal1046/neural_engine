# server/brain_watch.py
import time
import os
import subprocess
from pathlib import Path
from knowledge_injector import KnowledgeInjector

class BrainWatcher:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.injector = KnowledgeInjector(str(self.workspace_root))
        self.last_sync = time.time()

    def watch(self, interval=300):
        """ Watches for changes and updates the brain incrementally every 5 mins. """
        print(f"[WATCHER] Nero Brain persistent monitor active. Root: {self.workspace_root}")
        
        while True:
            try:
                # Find files modified since last sync
                modified = []
                for ext in ["*.py", "*.cpp", "*.ts", "*.tsx", "*.h"]:
                    for p in self.workspace_root.rglob(ext):
                        if "node_modules" in str(p) or ".git" in str(p): continue
                        if p.stat().st_mtime > self.last_sync:
                            modified.append(p)
                
                if modified:
                    print(f"\n[WATCHER] Detected {len(modified)} changed files. Syncing Brain...")
                    # We'll re-run a fast sync for these specific files
                    # For now, we'll use the main injector (which can be optimized for delta sync later)
                    self.injector.index_codebase_semantically()
                    self.last_sync = time.time()
                    print(f"[WATCHER] Sync complete. Brain is current.")
                
                time.sleep(interval)
                
            except KeyboardInterrupt:
                break
            except Exception as e:
                print(f"[WATCHER ERROR] {str(e)}")
                time.sleep(60) # Wait before retry

if __name__ == "__main__":
    watcher = BrainWatcher(".")
    watcher.watch(interval=60) # Check every minute for now
