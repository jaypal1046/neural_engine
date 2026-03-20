# server/task_router.py
import re
from typing import Dict, Any

class TaskRouter:
    @staticmethod
    def classify(query: str) -> str:
        """
        Classifies the user intent to trigger specialized local analysis.
        """
        query = query.lower()
        
        # 1. MODIFY - Intent to change code
        if any(w in query for w in ["change", "fix", "modify", "update", "refactor", "add", "implement"]):
            return "MODIFY"
            
        # 2. REVIEW - Intent to critique or check code
        if any(w in query for w in ["review", "critique", "check", "security", "lint", "debug"]):
            return "REVIEW"
            
        # 3. EXPLAIN - Intent to understand code
        if any(w in query for w in ["explain", "how", "what", "where", "find", "understand", "flow"]):
            return "EXPLAIN"
            
        # 4. CHAT - General discussion
        return "CHAT"

    @staticmethod
    def get_pre_analysis_plan(intent: str, query: str) -> list:
        """
        Returns a list of local tools to run BEFORE asking the LLM.
        Directly implements your 'run relevant local analysis first' rule.
        """
        if intent == "EXPLAIN":
            return ["PROJECT_SEARCH", "GET_STRUCTURE"]
        elif intent == "REVIEW":
            return ["NEURAL_ANALYZE", "GET_STRUCTURE"]
        elif intent == "MODIFY":
            return ["PROJECT_SEARCH", "READ_FILE"]
        return []

if __name__ == "__main__":
    test_queries = [
        "How is the ANS compressor initialized?",
        "Review the security of my main.cpp",
        "Refactor the BWT logic to use SIMD",
        "Tell me a joke"
    ]
    for q in test_queries:
        intent = TaskRouter.classify(q)
        print(f"Query: '{q}' -> Intent: {intent}")
