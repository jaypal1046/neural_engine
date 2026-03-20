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

def query_chatgpt(prompt, model="gpt-4o"):
    api_key = os.environ.get("OPENAI_API_KEY", "")
    if not api_key:
        print("[CLOUD_TEACHER] Error: OPENAI_API_KEY environment variable is missing.")
        return

    url = "https://api.openai.com/v1/chat/completions"
    data = json.dumps({
        "model": model,
        "messages": [{"role": "user", "content": prompt}]
    }).encode('utf-8')
    headers = {
        "Authorization": f"Bearer {api_key}",
        "Content-Type": "application/json"
    }
    
    req = urllib.request.Request(url, data=data, headers=headers, method='POST')
    try:
        with urllib.request.urlopen(req) as resp:
            resp_data = json.loads(resp.read().decode('utf-8'))
            print("GPT_ANSWER_START")
            print(resp_data['choices'][0]['message']['content'])
            print("GPT_ANSWER_END")
    except Exception as e:
        print(f"[CLOUD_TEACHER] Exception: {e}")

if __name__ == "__main__":
    prompt, model = load_prompt_and_model(sys.argv, "gpt-4o")
    query_chatgpt(prompt, model)
