import os
import sys

# Ensure we can import from server folder
sys.path.append(os.path.abspath('c:/Jay/_Plugin/compress/server'))

from agent_orchestrator import NeroAgentOrchestrator
from task_router import TaskRouter

def debug_prompt():
    orchestrator = NeroAgentOrchestrator(workspace_root='c:/Jay/_Plugin/compress')
    query = "open brouser and teke screen shot of google.com"
    intent = TaskRouter.classify(query)
    print(f"DEBUG: Intent detected: {intent}")
    
    # Manually check if skill is discovered
    print(f"DEBUG: Discovered skills: {list(orchestrator.skills_registry.keys())}")
    
    system_prompt = orchestrator._build_system_prompt(intent=intent)
    
    if "WEBAPP_TESTING" in system_prompt or "Web Application Testing" in system_prompt:
        print("DEBUG: Skill successfully found in system prompt!")
        # Print the first 20 lines
        print("--- FIRST 20 LINES OF PROMPT ---")
        print("\n".join(system_prompt.splitlines()[:20]))
    else:
        print("DEBUG: Skill NOT found in system prompt.")

if __name__ == "__main__":
    debug_prompt()
