import requests
import json

url = "http://127.0.0.1:8001/api/chat"
payload = {
    "message": "/modify add a comment to this file",
    "workspace_root": r"c:\Jay\_Plugin\compress",
    "editor_context": {
        "activeFilePath": r"c:\Jay\_Plugin\compress\desktop_app\src\components\MonacoEditor.tsx",
        "relativePath": "desktop_app/src/components/MonacoEditor.tsx",
        "language": "tsx",
        "selection": None,
        "selectedText": ""
    }
}

try:
    response = requests.post(url, json=payload)
    print(f"Status: {response.status_code}")
    data = response.json()
    print("Full Response:")
    print(json.dumps(data, indent=2))
except Exception as e:
    print(f"Error: {e}")
