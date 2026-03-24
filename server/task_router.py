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
        
        # 4. MAP_SOURCE - Explicit mapping request
        if any(w in query for w in ["diagram", "source map", "map source", "relationship", "architecture", "flowchart"]):
            return "MAP_SOURCE"
            
        # 5. WRITING - Content creation, articles, newsletters
        if any(w in query for w in ["write", "article", "blog", "newsletter", "outline", "hook", "citation", "research for my", "case study", "tutorial"]):
            return "WRITING"
            
        # 6. ART_GENERATION - Generative/algorithmic art
        if any(w in query for w in ["generative art", "algorithmic art", "make art", "p5.js", "visual piece", "creative coding"]):
            return "ART_GENERATION"
            
        # 7. WEBAPP_TESTING - Browser automation and testing
        if any(w in query for w in ["test", "browser", "brouser", "playwright", "automation", "click", "navigate", "screenshot", "screen shot", "open google", "open website"]):
            return "WEBAPP_TESTING"
            
        # 8. CHAT - General discussion
        return "CHAT"

    @staticmethod
    def get_pre_analysis_plan(intent: str, query: str) -> list:
        """
        Returns a list of local tools to run BEFORE asking the LLM.
        Directly implements your 'run relevant local analysis first' rule.
        """
        if intent == "EXPLAIN" or intent == "MAP_SOURCE":
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
