import sys
import os
import json

# Add server directory to path
sys.path.append(r"c:\Jay\_Plugin\compress\server")

from agent_orchestrator import NeroAgentOrchestrator

def debug_chat():
    workspace_root = r"c:\Jay\_Plugin\compress"
    orchestrator = NeroAgentOrchestrator(workspace_root)
    
    user_query = "open browser and navigate to google.com"
    print(f"DEBUGGING QUERY: {user_query}")
    
    responses = orchestrator.process_query(user_query, [])
    
    for i, res in enumerate(responses):
        print(f"\n--- RESPONSE {i+1} ---\n")
        print(res)

if __name__ == "__main__":
    debug_chat()
