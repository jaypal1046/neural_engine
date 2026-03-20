# server/action_executor.py
import subprocess
import os
from pathlib import Path

class ActionExecutor:
    def __init__(self, workspace_root: str):
        self.workspace_root = Path(workspace_root).resolve()

    def run_tests(self, test_command="npm test"):
        """ Runs local tests and captures results. """
        print(f"[ACTION] Running validation: {test_command}")
        try:
            result = subprocess.run(
                test_command,
                shell=True,
                cwd=str(self.workspace_root),
                capture_output=True,
                text=True,
                timeout=120
            )
            return {
                "success": result.returncode == 0,
                "stdout": result.stdout[-2000:], # keep last 2k
                "stderr": result.stderr[-2000:]
            }
        except Exception as e:
            return {"success": False, "error": str(e)}

    def run_lint(self, file_path):
        """ Runs a linter on a specific file. """
        # Custom logic based on file extension
        if file_path.endswith(".py"):
            cmd = f"ruff check {file_path}"
        elif file_path.endswith((".ts", ".tsx")):
            cmd = f"npx eslint {file_path}"
        else:
            return {"success": True, "message": "No linter defined for this type"}

        print(f"[ACTION] Running lint: {cmd}")
        result = subprocess.run(cmd, shell=True, cwd=str(self.workspace_root), capture_output=True, text=True)
        return {
            "success": result.returncode == 0,
            "output": result.stdout or result.stderr
        }

if __name__ == "__main__":
    executor = ActionExecutor(".")
    # Dummy test
    print(executor.run_lint("server/agent_orchestrator.py"))
