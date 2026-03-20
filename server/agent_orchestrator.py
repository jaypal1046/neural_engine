import os
import json
import re
import subprocess
from typing import List, Dict, Any, Optional

try:
    from llm_adapter import OllamaAdapter
except ImportError:
    # Final fallback since we are in the same folder
    import sys
    sys.path.append(os.path.dirname(__file__))
    from llm_adapter import OllamaAdapter

BIN_PATH = os.path.join(os.path.dirname(__file__), "..", "bin", "neural_engine.exe")

class NeroAgentOrchestrator:
    def __init__(self, workspace_root: str, model: str = "llama3"):
        self.workspace_root = workspace_root
        self.adapter = OllamaAdapter(model=model)
        self.system_prompt = self._build_system_prompt()
        self.recent_files = [] # LRU cache of files touched

    def _track_file(self, path: str):
        """Maintains a set of recently used files for context."""
        # Get last 5 recently accessed files for context
        recent = self.recent_files[:5]
        if path in self.recent_files:
            self.recent_files.remove(path)
        self.recent_files.insert(0, path)
        self.recent_files = self.recent_files[:5] # Keep last 5 files

    def _build_system_prompt(self) -> str:
        return f"""
You are Nero Brain, a private local AI development assistant.
WORKSPACE_ROOT: {self.workspace_root}

Your goal: help with coding, explain project structure, and perform code reviews while staying local.

Available Actions (Strict Format):
1.  [TOOL: READ_FILE] path       - Returns the text content of a file.
2.  [TOOL: LIST_DIR] path        - Returns a list of files in a directory.
3.  [TOOL: NEURAL_ANALYZE] path  - Uses the C++ Neural Engine for deep code analysis/compression.
4.  [TOOL: PROJECT_SEARCH] query - Performs a hybrid semantic/structural search across the codebase.
5.  [TOOL: GET_STRUCTURE] path   - Returns the AST-based call graph and symbol definitions for a file.
7.  [TOOL: RUN_VALIDATION] cmd - Runs a build/test/lint command and returns success/fail/errors.
8.  [TOOL: PROPOSE_COMMAND] cmd - Suggests a shell command for the user to confirm.
9.  [TOOL: PROPOSE_DIFF] path,diff - Shows a code change for the user to apply.

CRITICAL RULES:
- You CANNOT execute shell commands or write files directly. You MUST propose them.
- If you propose a diff or fix, you MUST run [TOOL: RUN_VALIDATION] first to ensure it builds/tests if possible.
- Provide errors back to your [THOUGHT] for iterative refinement.
- Only access files within the WORKSPACE_ROOT.
- Provide a summary and reasoning for your findings.
- Use the structural tools (PROJECT_SEARCH/GET_STRUCTURE) first to avoid reading too much code.

Response Format:
[THOUGHT] Your internal reasoning step.
[ACTION: TOOL_NAME] arg1, arg2...
OR if done:
[FINAL_ANSWER] Your response to the user.
"""

    def get_dashboard_summary(self):
        """Called by the UI to show the 'Brain Dashboard'."""
        try:
            from dashboard_api import NeroDashboard
            dash = NeroDashboard(self.workspace_root)
            return dash.get_full_stats()
        except:
            return {"status": "error", "message": "Dashboard API not found"}

    def call_neural_engine(self, command: str, arg: str) -> str:
        """Invokes the C++ Neural Engine for specialized analysis."""
        try:
            cmd = [BIN_PATH, command, arg]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=60, cwd=self.workspace_root)
            return result.stdout or result.stderr
        except Exception as e:
            return f"Error calling C++ engine: {str(e)}"

    def process_query(self, user_query: str, history: List[Dict[str, str]] = None) -> List[str]:
        """
        Main orchestration loop: Router -> Pre-Analysis -> Think -> Action -> Observation -> Final Answer.
        """
        from task_router import TaskRouter
        intent = TaskRouter.classify(user_query)
        pre_plans = TaskRouter.get_pre_analysis_plan(intent, user_query)
        
        obs_log = []
        # Pre-execution: Run relevant local analysis first
        for tool in pre_plans:
            if tool == "PROJECT_SEARCH":
                obs_log.append(f"[PRE-ANALYSIS: SEARCH] {self.tool_project_search(user_query)}")
            elif tool == "GET_STRUCTURE" and self.recent_files:
                obs_log.append(f"[PRE-ANALYSIS: STRUCTURE] {self.tool_get_structure(self.recent_files[0])}")
            elif tool == "NEURAL_ANALYZE" and self.recent_files:
                obs_log.append(f"[PRE-ANALYSIS: NEURAL] {self.call_neural_engine('analyze', self.recent_files[0])}")

        messages = history or []
        context_str = "\n".join(obs_log)
        messages.append({"role": "user", "content": f"INTENT: {intent}\nCONTEXT:\n{context_str}\n\nQUERY: {user_query}"})
        
        max_steps = 5
        conversation_log = []
        
        for _ in range(max_steps):
            response = self.adapter.chat(messages, system_prompt=self.system_prompt)
            conversation_log.append(response)
            
            # Look for [ACTION: TOOL_NAME] arg
            action_match = re.search(r'\[ACTION:\s*(\w+)\]\s*(.*)', response, re.IGNORECASE)
            
            if not action_match:
                # No more actions, assume final answer
                break
                
            tool_name = action_match.group(1).upper()
            tool_arg = action_match.group(2).strip()
            
            observation = ""
            if tool_name == "READ_FILE":
                observation = self.tool_read_file(tool_arg)
            elif tool_name == "LIST_DIR":
                observation = self.tool_list_dir(tool_arg)
            elif tool_name == "NEURAL_ANALYZE":
                observation = self.call_neural_engine("analyze", tool_arg)
            elif tool_name == "PROJECT_SEARCH":
                observation = self.tool_project_search(tool_arg)
            elif tool_name == "GET_STRUCTURE":
                observation = self.tool_get_structure(tool_arg)
            elif tool_name == "RUN_VALIDATION":
                observation = self.tool_run_validation(tool_arg)
            else:
                # If it's a proposal (COMMAND or DIFF), we stop and return it for the UI
                break
                
            messages.append({"role": "assistant", "content": response})
            messages.append({"role": "user", "content": f"[OBSERVATION] {observation}"})
        
        return conversation_log

    def tool_run_validation(self, cmd: str) -> str:
        try:
            # from action_executor import ActionExecutor # Original line
            exe = ActionExecutor(self.workspace_root) # Changed to use imported ActionExecutor
            res = exe.run_tests(cmd)
            return json.dumps(res, indent=2)
        except Exception as e:
            return f"Error running validation: {str(e)}"

    def tool_project_search(self, query: str) -> str:
        try:
            from nero_search import NeroSearch
            searcher = NeroSearch(self.workspace_root)
            results = searcher.hybrid_search(query)
            return json.dumps(results, indent=2)
        except Exception as e:
            return f"Error in hybrid search: {str(e)}"

    def tool_get_structure(self, path: str) -> str:
        try:
            from symbol_graph import SymbolGraph
            graph = SymbolGraph(self.workspace_root)
            # Find definitions related to this path
            data = graph._graph()
            file_defs = {name: d for name, d in data["definitions"].items() if any(x["path"] == path for x in d)}
            return json.dumps(file_defs, indent=2)
        except Exception as e:
            return f"Error getting structure: {str(e)}"

    def tool_read_file(self, path: str) -> str:
        try:
            abspath = os.path.abspath(os.path.join(self.workspace_root, path))
            if not abspath.startswith(self.workspace_root):
                return "Error: Access denied (outside workspace)"
            with open(abspath, 'r', encoding='utf-8') as f:
                content = f.read()
                self._track_file(path)
                return content[:2000] # Limit context
        except Exception as e:
            return f"Error reading file: {str(e)}"

    def tool_list_dir(self, path: str) -> str:
        try:
            target = os.path.abspath(os.path.join(self.workspace_root, path or "."))
            if not target.startswith(self.workspace_root):
                return "Error: Access denied"
            return ", ".join(os.listdir(target))
        except Exception as e:
            msg = str(e)
            return f"Search error: {msg[:100]}"

    def validate_action(self, action_str: str):
        """Phase 2: Add validation/human-in-the-loop logic here."""
        pass
