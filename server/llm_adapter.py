import requests
import json
import os
from typing import List, Dict, Any, Optional

class OllamaAdapter:
    def __init__(self, model: str = "llama3", base_url: str = "http://localhost:11434"):
        self.model = model
        self.base_url = base_url

    def chat(
        self,
        messages: List[Dict[str, str]],
        system_prompt: Optional[str] = None,
        *,
        temperature: float = 0.4,
        num_ctx: int = 2048,
        timeout: int = 45,
        num_predict: int = 256,
    ) -> str:
        """
        Sends a conversation history to Ollama.
        """
        payload = {
            "model": self.model,
            "messages": messages,
            "stream": False,
            "options": {
                "temperature": temperature,
                "num_ctx": num_ctx,
                "num_predict": num_predict,
            },
            "keep_alive": "30m",
        }
        
        if system_prompt:
            payload["system"] = system_prompt

        try:
            response = requests.post(
                f"{self.base_url}/api/chat",
                json=payload,
                timeout=timeout
            )
            response.raise_for_status()
            data = response.json()
            return data.get("message", {}).get("content", "")
        except Exception as e:
            return f"Error contacting Ollama: {str(e)}"

    def generate_streaming(self, prompt: str):
        """Streaming version for better UI response."""
        payload = {
            "model": self.model,
            "prompt": prompt,
            "stream": True
        }
        try:
            response = requests.post(
                f"{self.base_url}/api/generate",
                json=payload,
                stream=True
            )
            for line in response.iter_lines():
                if line:
                    chunk = json.loads(line)
                    yield chunk.get("response", "")
                    if chunk.get("done"):
                        break
        except Exception as e:
            yield f"\n[Error: {str(e)}]"
