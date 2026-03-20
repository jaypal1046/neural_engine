import os
import json
import urllib.request
import subprocess

# 1. READ LOCAL FILE (Direct project access)
print("[LOCAL_AGENT] Reading src/agent_brain.cpp...")
with open("src/agent_brain.cpp", "r") as f:
    code = f.read()

# 2. ASK LOCAL OLLAMA (No API Key)
prompt = f"Summarize this C++ code in 2 sentences:\n\n{code[:2000]}"
print("[LOCAL_AGENT] Asking Ollama (llama3)...")

url = "http://localhost:11434/api/generate"
data = json.dumps({
    "model": "llama3",
    "prompt": prompt,
    "stream": False
}).encode('utf-8')
headers = {"Content-Type": "application/json"}

try:
    req = urllib.request.Request(url, data=data, headers=headers, method='POST')
    with urllib.request.urlopen(req) as resp:
        resp_data = json.loads(resp.read().decode('utf-8'))
        print("\nOLLAMA_RESPONSE:")
        print(resp_data['response'])
except Exception as e:
    print(f"\n[ERROR] Could not connect to Ollama: {e}")
    print("Please make sure 'ollama serve' is running and you have 'llama3' downloaded.")
