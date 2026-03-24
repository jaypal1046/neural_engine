import requests
import json
import time

def test_live_chat():
    url = "http://127.0.0.1:8001/api/chat"
    payload = {
        "message": "open browser and navigate to google.com",
        "history": [],
        "workspace_root": r"c:\Jay\_Plugin\compress"
    }
    
    print("Sending chat request...")
    try:
        response = requests.post(url, json=payload, timeout=300)
        print(f"Status Code: {response.status_code}")
        data = response.json()
        print("AI Response:")
        # The response structure might be complex, let's print the relevant part
        # Based on main.py, it likely returns a list of messages or a final answer
        print(json.dumps(data, indent=2))
        
    except Exception as e:
        print(f"Error: {e}")

if __name__ == "__main__":
    test_live_chat()
