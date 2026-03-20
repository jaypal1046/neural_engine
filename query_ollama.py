import os
import sys
import json
import urllib.request
import urllib.error

def load_prompt_and_model(argv, default_model):
    prompt = ""
    model = default_model

    if len(argv) >= 3 and argv[1] == "--prompt-file":
        with open(argv[2], "r", encoding="utf-8") as f:
            prompt = f.read()
        if len(argv) >= 4:
            model = argv[3]
    else:
        prompt = argv[1] if len(argv) > 1 else ""
        if len(argv) > 2:
            model = argv[2]

    return prompt, model

def query_ollama(prompt, model="llama3"):
    url = "http://localhost:11434/api/generate"
    data = json.dumps({
        "model": model,
        "prompt": prompt,
        "stream": False
    }).encode('utf-8')
    
    headers = {
        "Content-Type": "application/json"
    }
    
    req = urllib.request.Request(url, data=data, headers=headers, method='POST')
    try:
        with urllib.request.urlopen(req) as resp:
            resp_data = json.loads(resp.read().decode('utf-8'))
            print("OLLAMA_ANSWER_START")
            print(resp_data['response'])
            print("OLLAMA_ANSWER_END")
    except Exception as e:
        print(f"[LOCAL_TEACHER] Exception: {e}")
        print("Make sure Ollama is running (ollama serve)")

if __name__ == "__main__":
    prompt, model = load_prompt_and_model(sys.argv, "llama3")
    query_ollama(prompt, model)
