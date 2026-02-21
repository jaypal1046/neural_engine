"""
views/browser.py — Web Automator Bridge
"""
import customtkinter as ctk
import websocket
import threading
import json
import time
from theme import BG, TEXT, F, ACCENT_SOFT, ACCENT
from widgets import SectionTitle, SubText, ClaudeEntry, PrimaryBtn, TermLog, Card, Divider, AccentTag

class BrowserView(ctk.CTkScrollableFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self.ws = None
        self._build()

    def _build(self):
        hdr = ctk.CTkFrame(self, fg_color="transparent")
        hdr.grid(row=0, column=0, padx=36, pady=(36, 24), sticky="ew")
        hdr.grid_columnconfigure(1, weight=1)
        
        SectionTitle(hdr, "Web Automator").grid(row=0, column=0, sticky="w")
        AccentTag(hdr, "Copilot Bridge").grid(row=0, column=2, sticky="e")
        SubText(hdr, text="Control and extract real-time knowledge via external websocket relay.").grid(row=1, column=0, columnspan=3, pady=(6, 0), sticky="w")
        
        Divider(self).grid(row=1, column=0, sticky="ew", padx=36, pady=(0, 20))

        cfg = Card(self)
        cfg.grid(row=2, column=0, padx=36, pady=20, sticky="ew")
        cfg.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(cfg, text="Target URL", font=F["label"]).grid(row=0, column=0, padx=24, pady=20, sticky="nw")
        self.entry_url = ClaudeEntry(cfg, placeholder_text="localhost / github / etc...")
        self.entry_url.grid(row=0, column=1, padx=(0, 24), pady=20, sticky="ew")
        
        btn_bar = ctk.CTkFrame(cfg, fg_color="transparent")
        btn_bar.grid(row=1, column=0, columnspan=2, padx=24, pady=(0, 20), sticky="ew")
        btn_bar.grid_columnconfigure((0, 1, 2), weight=1)
        
        PrimaryBtn(btn_bar, text="Launch Web Bridge", command=self.do_connect).grid(row=0, column=0, padx=5, sticky="ew")
        PrimaryBtn(btn_bar, text="Extract DOM", command=self.do_dom).grid(row=0, column=1, padx=5, sticky="ew")
        PrimaryBtn(btn_bar, text="Take Screenshot", command=self.do_shot).grid(row=0, column=2, padx=5, sticky="ew")

        prog = Card(self)
        prog.grid(row=3, column=0, padx=36, pady=(0, 40), sticky="ew")
        prog.grid_columnconfigure(0, weight=1)
        
        ctk.CTkLabel(prog, text="Browser Telemetry", font=F["label"]).grid(row=0, column=0, padx=24, pady=20, sticky="w")
        self.log = TermLog(prog, height=200)
        self.log.grid(row=1, column=0, padx=24, pady=(0, 24), sticky="ew")

    def do_connect(self):
        url = self.entry_url.get()
        if not url: return
        self.log.clear()
        
        if self.ws and self.ws.sock and self.ws.sock.connected:
            self.log.append("[WS] Already connected to Relay server.\n")
            self._send_command("browserAgent.navigate", {"url": url})
            return

        self.log.append("[WS] Initializing Websocket hook to localhost:8080...\n")
        threading.Thread(target=self._ws_listen, daemon=True).start()
        
        # Wait for connection, then navigate
        self.after(1000, lambda: self._send_command("browserAgent.navigate", {"url": url}))

    def _ws_listen(self):
        try:
            self.ws = websocket.WebSocketApp(
                "ws://localhost:8080",
                on_message=self._on_ws_msg,
                on_error=self._on_ws_err,
                on_close=self._on_ws_close
            )
            self.ws.on_open = self._on_ws_open
            self.ws.run_forever()
        except Exception as e:
            self.after(0, lambda: self.log.append(f"[WS] Failure: {str(e)}\n"))

    def _on_ws_open(self, ws):
        self.after(0, lambda: self.log.append("[WS] Copilot Relay Connected!\n"))
        # Register ourselves as VS_CODE so the relay server knows we are commanding the browser
        handshake = {
            "id": f"conn_{int(time.time()*1000)}",
            "type": "command",
            "from": "vscode",
            "command": "browserAgent.connect",
            "params": {}
        }
        ws.send(json.dumps(handshake))

    def _on_ws_msg(self, ws, message):
        try:
            data = json.loads(message)
            msg_type = data.get("type", "unknown")
            if msg_type == "response":
                success = data.get("success", False)
                if success:
                    resp_data = data.get("data", {})
                    # Truncate response data if it's massive (like DOM or Screenshot base64)
                    log_text = json.dumps(resp_data)
                    if len(log_text) > 200: log_text = log_text[:200] + "... [truncated]"
                    self.after(0, lambda: self.log.append(f"[Browser] SUCCESS: {log_text}\n"))
                else:
                    err = data.get("error", "Unknown error")
                    self.after(0, lambda: self.log.append(f"[Browser] ERROR: {err}\n"))
        except Exception as e:
             self.after(0, lambda: self.log.append(f"[WS] Message Parse Error: {e}\n"))

    def _on_ws_err(self, ws, error):
        self.after(0, lambda: self.log.append(f"[WS] Socket Err: {str(error)}\n"))

    def _on_ws_close(self, ws, close_status_code, close_msg):
        self.after(0, lambda: self.log.append("[WS] Connection to Relay Server terminated.\n"))

    def _send_command(self, cmd_name, params):
        if not self.ws or not self.ws.sock or not self.ws.sock.connected:
            self.log.append("[WS] ERROR: Not connected to Relay Server. Is it running?\n")
            return
            
        self.log.append(f"[API] Sending Command: {cmd_name}\n")
        msg = {
            "id": f"cmd_{int(time.time()*1000)}",
            "type": "command",
            "from": "vscode",
            "timestamp": int(time.time()*1000),
            "command": cmd_name,
            "params": params
        }
        self.ws.send(json.dumps(msg))

    def do_dom(self):
        self._send_command("browserAgent.getDOM", {"selector": "body"})
        
    def do_shot(self):
        self._send_command("browserAgent.screenshot", {"fullPage": False})
