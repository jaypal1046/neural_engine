import requests
import json
import os
from typing import List, Dict, Any, Optional

class OllamaAdapter:
    def __init__(self, model: str = "qwen2.5-coder:7b", base_url: str = None):
        self.model = model
        self.base_url = (base_url or os.environ.get("OLLAMA_HOST") or "http://127.0.0.1:11434").rstrip("/")
        if "://" not in self.base_url:
            self.base_url = f"http://{self.base_url}"

    def chat(
        self,
        messages: List[Dict[str, str]],
        system_prompt: Optional[str] = None,
        *,
        temperature: float = 0.4,
        num_ctx: int = 8192,
        timeout: Optional[int] = None,  # No timeout for local Ollama
        num_predict: int = 512,
        stream: bool = False,
        format: Optional[str] = None,
    ):
        """
        Sends a conversation history to Ollama.
        """
        chat_messages = []
        if system_prompt:
            chat_messages.append({"role": "system", "content": system_prompt})
        
        chat_messages.extend(messages)

        payload = {
            "model": self.model,
            "messages": chat_messages,
            "stream": stream,
            "format": format,
            "options": {
                "temperature": temperature,
                "num_ctx": num_ctx,
                "num_predict": num_predict,
            },
            "keep_alive": "30m",
        }

        try:
            response = requests.post(
                f"{self.base_url}/api/chat",
                json=payload,
                timeout=timeout,
                stream=stream
            )
            response.raise_for_status()
            
            if stream:
                def gen():
                    for line in response.iter_lines():
                        if line:
                            try:
                                chunk = json.loads(line)
                                msg = chunk.get("message", {}).get("content", "")
                                if msg: 
                                    yield msg
                            except Exception:
                                pass
                return gen()

            data = response.json()
            return data.get("message", {}).get("content", "")
        except Exception as e:
            error_msg = str(e)
            if stream:
                def err_gen():
                    yield f"Error contacting Ollama: {error_msg}"
                return err_gen()
            return f"Error contacting Ollama: {error_msg}"

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
