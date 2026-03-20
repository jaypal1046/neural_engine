import os
import sys
import json
import urllib.request
import urllib.error

def load_prompt(argv):
    if len(argv) >= 3 and argv[1] == "--prompt-file":
        with open(argv[2], "r", encoding="utf-8") as f:
            return f.read()
    return argv[1] if len(argv) > 1 else ""

def query_claude(prompt):
    api_key = os.environ.get("ANTHROPIC_API_KEY", "")
    if not api_key:
        print("[CLOUD_TEACHER] Error: ANTHROPIC_API_KEY environment variable is missing.")
        print("[CLOUD_TEACHER] In PowerShell, run: $env:ANTHROPIC_API_KEY=\"your_key_here\"")
        return

    url = "https://api.anthropic.com/v1/messages"
    data = json.dumps({
        "model": "claude-3-5-sonnet-latest",
        "max_tokens": 1024,
        "messages": [{"role": "user", "content": prompt}]
    }).encode('utf-8')
    headers = {
        "x-api-key": api_key,
        "anthropic-version": "2023-06-01",
        "content-type": "application/json"
    }
    
    req = urllib.request.Request(url, data=data, headers=headers, method='POST')
    try:
        with urllib.request.urlopen(req) as resp:
            resp_data = json.loads(resp.read().decode('utf-8'))
            print("CLAUDE_ANSWER_START")
            print(resp_data['content'][0]['text'])
            print("CLAUDE_ANSWER_END")
    except Exception as e:
        print(f"[CLOUD_TEACHER] Exception: {e}")

if __name__ == "__main__":
    prompt = load_prompt(sys.argv)
    query_claude(prompt)
