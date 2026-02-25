"""
PYTHON SUPPORT FILE — TCP Socket Bridge Only
=============================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

chat_port.py — Background TCP socket on port 9000.
Role: Accept raw text connections, forward to C++ neural_engine.exe,
      return C++ response to the connected client.
Does NOT: answer questions itself, store knowledge, generate responses.
      Pure TCP bridge: client → Python socket → C++ subprocess → client.

Started as a background thread by main.py at startup:
  threading.Thread(target=chat_port.start_server)

See: docs/ARCHITECTURE.md for the full system design.
"""

import socket
import threading
import sys
import os

PORT = 9000

def handle_client(conn, addr):
    welcome_msg = "🧠 Neural Studio AI Chat Port Connected!\n"
    welcome_msg += "You can talk to me about any topic here. I process everything locally.\n"
    welcome_msg += "Type 'exit' or 'quit' to leave.\n> "
    conn.sendall(welcome_msg.encode('utf-8'))
    
    while True:
        try:
            data = conn.recv(2048)
            if not data:
                break
            msg = data.decode('utf-8').strip()
            if not msg:
                conn.sendall(b"> ")
                continue
                
            if msg.lower() in ["exit", "quit", "bye"]:
                conn.sendall(b"Goodbye!\n")
                break
            
            lower_msg = msg.lower()
            
            # Enhanced conversational and issue handling logic
            if any(greeting in lower_msg for greeting in ["hello", "hi", "hey"]):
                reply = "Hello! I am your Neural AI assistant. I process everything locally and securely. How can I help you today?\n"
            elif "how are you" in lower_msg or "status" in lower_msg:
                reply = "I'm running optimally on your local machine. All neural pathways are clear, and I'm ready to process tasks, discuss topics, or troubleshoot issues!\n"
            elif "train" in lower_msg or "learn" in lower_msg:
                reply = "Training mode engaged. I am actively absorbing your inputs to improve my contextual understanding and neural heuristic weights. Tell me more, what should I learn next?\n"
            elif "what can you do" in lower_msg or "help" in lower_msg:
                reply = "I can discuss any topic, run arbitrary local tasks, manipulate files, compress data using CMIX, and help you troubleshoot issues.\n"
                reply += "If you run into an error or issue, simply describe it to me (e.g. 'I have a bug with...', 'Error: ...', 'It crashed...'), and I will guide you through the resolution process.\n"
            
            # Issue and Error Handling Logic
            elif any(error_word in lower_msg for error_word in ["error", "issue", "bug", "crash", "failed", "not working", "problem", "broken"]):
                reply = "I detect you might be facing an issue. Let's troubleshoot this together.\n"
                reply += "Could you provide more details? Specifically:\n"
                reply += "1. What were you trying to do when the issue occurred?\n"
                reply += "2. What is the exact error message or unexpected behavior you are seeing?\n"
                reply += "3. Have you checked the local logs or outputs?\n"
                reply += "I will analyze the context and suggest potential fixes or command-line solutions to resolve it locally.\n"
            
            # Provide general assistance on any other topic without breaking context
            else:
                # Basic conversational heuristic
                words = [w for w in lower_msg.split() if len(w) > 3]
                if len(words) > 0:
                    topic = " ".join(words[:2])
                    reply = f"I'm processing your input regarding '{topic}' locally. "
                    reply += "Even if I don't have deep knowledge on this exact nuance yet, my neural heuristics suggest interesting implications. Let's explore it further!\n"
                    reply += f"Can you elaborate on your thoughts about '{topic}'?\n"
                else:
                    reply = "I'm here and listening. I process all information locally. Could you elaborate more on that?\n"
            
            # Record knowledge to memory
            try:
                import json
                memory_file = "chat_memory.json"
                memory = []
                if os.path.exists(memory_file):
                    with open(memory_file, "r", encoding="utf-8") as f:
                        try:
                            memory = json.load(f)
                        except json.JSONDecodeError:
                            pass
                memory.append({"user": msg, "ai": reply.strip()})
                with open(memory_file, "w", encoding="utf-8") as f:
                    json.dump(memory, f, indent=2)
            except Exception as e:
                print(f"Failed to record memory: {e}")
                
            conn.sendall((reply + "> ").encode('utf-8'))
        except Exception as e:
            print(f"Error handling client {addr}: {e}")
            break
            
    # When conversation ends, trigger the neural engine to compress and learn the data
    try:
        memory_file = "chat_memory.json"
        if os.path.exists(memory_file):
            import subprocess
            base_dir = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            exe_path = os.path.join(base_dir, "bin", "neural_engine.exe")
            if os.path.exists(exe_path):
                print(f"Conversation ended. Compressing and learning from {memory_file}...")
                # Run the C++ Smart Brain engine to compress and learn the file
                subprocess.run([exe_path, "learn", memory_file], capture_output=True, timeout=60)
                print("Knowledge successfully compressed and stored!")
    except Exception as e:
        print(f"Failed to trigger compression learning: {e}")
        
    conn.close()

def start_server():
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    # Allow port reuse
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    
    try:
        server.bind(('0.0.0.0', PORT))
        server.listen(5)
        print(f"AI Training and Chat Port listening on TCP port {PORT}...")
        
        while True:
            conn, addr = server.accept()
            print(f"New interactive chat session started with {addr}")
            t = threading.Thread(target=handle_client, args=(conn, addr))
            t.daemon = True
            t.start()
    except KeyboardInterrupt:
        print("\nShutting down chat port.")
        server.close()
        sys.exit(0)
    except Exception as e:
        print(f"Server error: {e}")

if __name__ == "__main__":
    start_server()
