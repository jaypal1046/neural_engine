# server/git_memory.py
import subprocess
import json
import tempfile
import os
from pathlib import Path

class GitMemory:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()
        self.engine_path = str(self.workspace_root / "bin" / "neural_engine.exe")

    def analyze_history(self, max_commits=50):
        """
        Extracts recent git history and 'teaches' the brain why things changed.
        """
        print(f"\n[GIT] Extracting last {max_commits} commits for Brain Memory...")
        
        try:
            # Format: hash|author|date|message
            log_format = "%H|%an|%ad|%s"
            result = subprocess.run(
                ["git", "log", f"-n {max_commits}", f"--pretty=format:{log_format}"],
                cwd=str(self.workspace_root),
                capture_output=True,
                text=True,
                encoding="utf-8"
            )
            
            if result.returncode != 0:
                print(f"[ERROR] Git log failed: {result.stderr}")
                return

            commits = result.stdout.splitlines()
            for line in commits:
                parts = line.split("|")
                if len(parts) < 4: continue
                
                commit_hash, author, date, message = parts
                
                # Fetch the diff summary (files changed)
                diff_res = subprocess.run(
                    ["git", "show", "--name-only", "--oneline", commit_hash],
                    cwd=str(self.workspace_root),
                    capture_output=True,
                    text=True
                )
                files_changed = diff_res.stdout.splitlines()[1:] # skip header
                
                # Create a semantic "Commit Knowledge" blob
                knowledge_blob = (
                    f"COMMIT: {commit_hash}\n"
                    f"AUTHOR: {author}\n"
                    f"DATE: {date}\n"
                    f"DESCRIPTION: {message}\n"
                    f"MODIFIED FILES: {', '.join(files_changed)}\n"
                    f"CONTEXT: This commit represents a historical change in the codebase "
                    f"addressing: '{message}'."
                )
                
                print(f"  Teaching Brain about commit: {commit_hash[:8]} - {message[:40]}...")
                self._teach_brain(f"git_{commit_hash[:8]}", knowledge_blob)
                
            print(f"[OK] Git history successfully integrated into Nero Brain.")

        except Exception as e:
            print(f"[ERROR] Git integration error: {str(e)}")

    def _teach_brain(self, topic, content):
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as tmp:
            tmp.write(content)
            tmp_path = tmp.name
        try:
            subprocess.run(
                [self.engine_path, "learn", tmp_path],
                cwd=str(self.workspace_root),
                capture_output=True
            )
        finally:
            if os.path.exists(tmp_path):
                os.unlink(tmp_path)

if __name__ == "__main__":
    memory = GitMemory(".")
    memory.analyze_history(20) # Start with 20 for speed
