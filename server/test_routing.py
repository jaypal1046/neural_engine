import sys
import os
import json

# Add server directory to path
sys.path.append(r"c:\Jay\_Plugin\compress\server")

from task_intelligence import LocalTaskIntelligence

def test_routing():
    ti = LocalTaskIntelligence(r"c:\Jay\_Plugin\compress")
    
    queries = [
        "open browser and take a screenshot",
        "navigate to google.com",
        "how to fix this bug?",
        "explain the source code",
        "brouser test",
        "take screen shot of the website",
        "playwright automation"
    ]
    
    results = []
    for query in queries:
        res = ti.classify_intent(query, allow_web=True)
        results.append({
            "query": query,
            "intent": res['intent'],
            "route": res['route']
        })
    
    with open(r"c:\Jay\_Plugin\compress\server\test_results.json", "w", encoding="utf-8") as f:
        json.dump(results, f, indent=2)
    
    print("Results written to test_results.json")

if __name__ == "__main__":
    test_routing()
