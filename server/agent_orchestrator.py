import os
import json
import re
import subprocess
from typing import List, Dict, Any, Optional

try:
    from llm_adapter import OllamaAdapter
except ImportError:
    import sys
    sys.path.append(os.path.dirname(__file__))
    from llm_adapter import OllamaAdapter

# Local engine path
BIN_PATH = os.path.join(os.path.dirname(__file__), "..", "bin", "neural_engine.exe")

class NeroAgentOrchestrator:
    def __init__(self, workspace_root: str, model: str = "qwen2.5-coder:7b"):
        self.workspace_root = workspace_root
        self.adapter = OllamaAdapter(model=model)
        self.skills_dir = os.path.join(self.workspace_root, "skills")
        self.recent_files = [] # LRU cache of files touched
        self.skills_registry = {}
        self._discover_skills()

    def _discover_skills(self):
        """Scans the skills directory for all available skills."""
        if not os.path.exists(self.skills_dir):
            return
            
        for skill_dir in os.listdir(self.skills_dir):
            skill_folder = os.path.join(self.skills_dir, skill_dir)
            if os.path.isdir(skill_folder):
                skill_file = os.path.join(skill_folder, "SKILL.md")
                if os.path.exists(skill_file):
                    metadata = self._parse_skill_metadata(skill_file)
                    if metadata:
                        metadata['dir_name'] = skill_dir
                        self.skills_registry[skill_dir] = metadata

    def _parse_skill_metadata(self, path: str) -> Dict:
        """Simple regex-based YAML frontmatter parser."""
        try:
            with open(path, 'r', encoding='utf-8') as f:
                content = f.read()
            match = re.search(r'^---\s*\n(.*?)\n---\s*\n', content, re.DOTALL)
            if not match: return {}
            
            yaml_text = match.group(1)
            metadata = {}
            current_key = None
            for line in yaml_text.split('\n'):
                line = line.strip()
                if not line: continue
                
                if line.startswith('-'):
                    # List item
                    if current_key:
                        if not isinstance(metadata[current_key], list):
                            metadata[current_key] = []
                        metadata[current_key].append(line[1:].strip().strip('"').strip("'"))
                elif ':' in line:
                    key, val = line.split(':', 1)
                    current_key = key.strip()
                    val = val.strip().strip('"').strip("'")
                    metadata[current_key] = val if val else []
            return metadata
        except:
            return {}

    def _get_skill_content(self, skill_name: str) -> str:
        """Loads SKILL.md and its references from the skills directory."""
        skill_path = os.path.join(self.skills_dir, skill_name, "SKILL.md")
        if not os.path.exists(skill_path):
            return ""
        
        with open(skill_path, 'r', encoding='utf-8') as f:
            content = f.read()
        
        # Load all files in references/ if they exist
        refs_dir = os.path.join(self.skills_dir, skill_name, "references")
        if os.path.exists(refs_dir):
            for ref_file in os.listdir(refs_dir):
                if ref_file.endswith(".md"):
                    ref_path = os.path.join(refs_dir, ref_file)
                    with open(ref_path, 'r', encoding='utf-8') as f:
                        content += f"\n\n### REFERENCE: {ref_file.upper()}\n" + f.read()
        
        return content
    def _should_inject_skill(self, skill: Dict, intent: str) -> bool:
        """Determines if a skill should be injected based on intent."""
        intents = skill.get('intents', [])
        if isinstance(intents, list):
            return intent in intents
        return intent == intents

    def _get_injected_skills_prompt(self, intent: str) -> str:
        """Builds the prompt section for all relevant skills."""
        prompt = ""
        for skill_id, skill in self.skills_registry.items():
            if self._should_inject_skill(skill, intent):
                skill_content = self._get_skill_content(skill['dir_name'])
                if skill_content:
                    prompt += f"\n\n### SPECIALIZED SKILL: {skill['name'].upper()}\n"
                    prompt += f"Background: {skill.get('description', '')}\n"
                    prompt += "Follow these rules and modes when this skill is active:\n"
                    prompt += skill_content
        return prompt

    def _build_system_prompt(self, intent: str = "") -> str:
        # Dynamically fetch relevant skills first
        skills_prompt = self._get_injected_skills_prompt(intent)
        
        base_prompt = f"""
You are Nero Brain, a private local AI development assistant.
WORKSPACE_ROOT: {self.workspace_root}

Your goal: help with coding, explain project structure, and perform code reviews while staying local.

### CAPABILITIES & SKILLS
Your core capabilities can be significantly expanded by the **Specialized Skills** listed below. When a skill is active, you MUST follow its rules and utilize its tools, even if they seem to exceed your default "text-only" constraints.
{skills_prompt}

### CORE ACTIONS (Strict Format):
1.  [TOOL: READ_FILE] path       - Returns the text content of a file.
2.  [TOOL: LIST_DIR] path        - Returns a list of files in a directory.
3.  [TOOL: NEURAL_ANALYZE] path  - Uses the C++ Neural Engine for deep code analysis/compression.
4.  [TOOL: PROJECT_SEARCH] query - Performs a hybrid semantic/structural search across the codebase.
5.  [TOOL: GET_STRUCTURE] path   - Returns the AST-based call graph and symbol definitions for a file.
7.  [TOOL: RUN_VALIDATION] cmd - Runs a build/test/lint command and returns success/fail/errors.
8.  [TOOL: PROPOSE_COMMAND] cmd - Suggests a shell command for the user to confirm.
9.  [TOOL: PROPOSE_DIFF] path,diff - Shows a code change for the user to apply.
10. [TOOL: WRITE_FILE] path,content - Creates or overwrites a file with the given content.

CRITICAL RULES:
- If a Specialized Skill is active (see above), its instructions take PRECEDENCE over these rules.
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
        return base_prompt

    def _track_file(self, path: str):
        """Maintains a set of recently used files for context."""
        if path in self.recent_files:
            self.recent_files.remove(path)
        self.recent_files.insert(0, path)
        self.recent_files = self.recent_files[:5] # Keep last 5 files
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

    def process_query(self, user_query: str, history: List[Dict[str, str]] = []) -> List[str]:
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
        
        system_prompt = self._build_system_prompt(intent=intent)
        
        max_steps = 5
        conversation_log = []
        
        for _ in range(max_steps):
            response = self.adapter.chat(messages, system_prompt=system_prompt)
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
            elif tool_name == "WRITE_FILE":
                # Split arg into path and content
                try:
                    path, content = tool_arg.split(",", 1)
                    observation = self.tool_write_file(path.strip(), content.strip())
                except:
                    observation = "Error: Invalid arguments for WRITE_FILE. Use path,content"
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
            from action_executor import ActionExecutor
            exe = ActionExecutor(self.workspace_root)
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

    def tool_write_file(self, path: str, content: str) -> str:
        try:
            abspath = os.path.abspath(os.path.join(self.workspace_root, path))
            if not abspath.startswith(self.workspace_root):
                return "Error: Access denied (outside workspace)"
            os.makedirs(os.path.dirname(abspath), exist_ok=True)
            with open(abspath, 'w', encoding='utf-8') as f:
                f.write(content)
            self._track_file(path)
            return f"Successfully wrote to {path}"
        except Exception as e:
            return f"Error writing file: {str(e)}"

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
