"""
views/helper.py — AI Assistant Command Panel.
"""
import customtkinter as ctk
import websocket
import threading
import json
import time
import os
import subprocess
from theme import (
    BG, SURFACE, CARD, BORDER, BORDER_DARK,
    ACCENT, ACCENT_SOFT, SUCCESS, SUCCESS_BG,
    TEXT, TEXT_FAINT, TEXT_INV, F
)
from widgets import Divider, SendBtn, ClaudeEntry

INIT_MESSAGES = [
    ("system",    "Neural engine initialized — 1046 sub-models in cache."),
    ("assistant", "Hi! I am your Neural Studio AI agent.\n\nType a command to control me:\n- 'navigate [url]'\n- 'click [selector]'\n- 'screenshot'\n- 'compress [file]'\n\nWhat would you like me to do?"),
]

class HelperView(ctk.CTkFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        kw.setdefault("corner_radius", 0)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(1, weight=1)
        self._msg_row = 0
        self._web_on  = False
        self.ws = None
        self._build()

    def _build(self):
        # ── Top bar
        topbar = ctk.CTkFrame(self, fg_color=SURFACE, corner_radius=0, height=58)
        topbar.grid(row=0, column=0, sticky="ew")
        topbar.grid_propagate(False)
        topbar.grid_columnconfigure(1, weight=1)

        left = ctk.CTkFrame(topbar, fg_color="transparent")
        left.grid(row=0, column=0, padx=24, sticky="w")
        ctk.CTkLabel(left, text="Command Panel", font=F["heading"], text_color=TEXT).pack(side="left")

        self.model_badge = ctk.CTkLabel(
            left, text="  1046 models  ",
            font=F["tiny"], text_color=ACCENT,
            fg_color=ACCENT_SOFT, corner_radius=20)
        self.model_badge.pack(side="left", padx=(12, 0))

        right = ctk.CTkFrame(topbar, fg_color="transparent")
        right.grid(row=0, column=2, padx=24, sticky="e")
        ctk.CTkLabel(right, text="Web Bridge", font=F["small"], text_color=TEXT_FAINT).pack(side="left", padx=(0, 8))

        self.web_sw = ctk.CTkSwitch(
            right, text="", progress_color=SUCCESS, button_color=TEXT_FAINT,
            onvalue=True, offvalue=False, command=self._toggle_web, width=40)
        self.web_sw.pack(side="left")

        Divider(self).grid(row=0, column=0, sticky="sew")

        # ── Chat scroll area
        self.chat = ctk.CTkScrollableFrame(
            self, fg_color=BG, corner_radius=0,
            scrollbar_button_color=BORDER, scrollbar_button_hover_color=BORDER_DARK)
        self.chat.grid(row=1, column=0, sticky="nsew")
        self.chat.grid_columnconfigure(0, weight=1)

        for role, text in INIT_MESSAGES:
            self._add_message(role, text)

        # ── Bottom input dock
        dock = ctk.CTkFrame(self, fg_color=SURFACE, corner_radius=0, height=74)
        dock.grid(row=2, column=0, sticky="ew")
        dock.grid_propagate(False)
        dock.grid_columnconfigure(0, weight=1)

        Divider(dock).grid(row=0, column=0, columnspan=2, sticky="ew")
        inner = ctk.CTkFrame(dock, fg_color="transparent")
        inner.grid(row=1, column=0, padx=24, pady=14, sticky="ew")
        inner.grid_columnconfigure(0, weight=1)

        self.entry = ClaudeEntry(inner, placeholder_text="Enter a command (e.g. 'navigate youtube.com')", height=44)
        self.entry.grid(row=0, column=0, sticky="ew", padx=(0, 10))
        self.entry.bind("<Return>", lambda e: self._send())

        SendBtn(inner, text="Execute", command=self._send).grid(row=0, column=1)

    def _add_message(self, role: str, text: str):
        is_user = role == "user"
        is_sys  = role == "system"

        outer = ctk.CTkFrame(self.chat, fg_color="transparent")
        outer.grid(row=self._msg_row, column=0, padx=24, pady=(4, 4), sticky="ew")
        outer.grid_columnconfigure(0, weight=1)
        self._msg_row += 1

        if is_sys:
            ctk.CTkLabel(outer, text=text, font=F["tiny"], text_color=TEXT_FAINT,
                         wraplength=580, justify="center").grid(row=0, column=0, pady=8)
            return

        bubble_bg = ACCENT if is_user else CARD
        bubble_fg = TEXT_INV if is_user else TEXT
        align = "e" if is_user else "w"
        pad = (120, 0) if is_user else (0, 120)

        ctk.CTkLabel(
            outer, text="You" if is_user else "Agent", font=F["tiny"],
            text_color=ACCENT if is_user else TEXT_FAINT
        ).grid(row=0, column=0, sticky=align, padx=(pad[0]+2, pad[1]+2), pady=(0, 2))

        bubble = ctk.CTkLabel(
            outer, text=text, font=F["body"], text_color=bubble_fg, fg_color=bubble_bg,
            corner_radius=16, wraplength=420, justify="left", padx=16, pady=12, anchor="w"
        )
        bubble.grid(row=1, column=0, sticky=align, padx=pad)

    def _send(self):
        msg = self.entry.get().strip()
        if not msg: return

        self._add_message("user", msg)
        self.entry.delete(0, "end")
        self.after(100, lambda: self.chat._parent_canvas.yview_moveto(1.0))
        
        # Parse command intents!
        command = msg.lower()
        if command.startswith("navigate ") or command.startswith("open ") or command.startswith("go to "):
            url = msg.split(" ", 1)[1]
            if not url.startswith("http"): url = "https://" + url
            self.after(200, lambda: self._add_message("assistant", f"Navigating to {url}..."))
            self._send_ws("browserAgent.navigate", {"url": url})
            
        elif command.startswith("click "):
            selector = msg.split(" ", 1)[1]
            self.after(200, lambda: self._add_message("assistant", f"Clicking element: {selector}..."))
            self._send_ws("browserAgent.click", {"selector": selector})

        elif command.startswith("type ") and " into " in command:
            text_to_type = command.split("type ", 1)[1].split(" into ")[0].strip("'\" ")
            selector = command.split(" into ")[1].strip()
            self.after(200, lambda: self._add_message("assistant", f"Typing into {selector}..."))
            self._send_ws("browserAgent.type", {"selector": selector, "text": text_to_type})

        elif "screenshot" in command:
            self.after(200, lambda: self._add_message("assistant", "Capturing browser screenshot..."))
            self._send_ws("browserAgent.screenshot", {"fullPage": False})

        elif command.startswith("compress "):
            file_name = msg.split(" ")[1]
            self.after(200, lambda: self._add_message("assistant", f"Initializing Local Neural Compiler on {file_name}..."))
            base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
            exe = os.path.join(base, "bin", "myzip.exe")
            target_myzip = file_name + ".myzip"
            if os.path.exists(file_name):
                 subprocess.Popen([exe, "compress", file_name, target_myzip, "--cmix"])
                 self.after(500, lambda: self._add_message("assistant", f"Compressing background sub-process launched -> {target_myzip}"))
            else:
                 self.after(500, lambda: self._add_message("assistant", "Error: I cannot find that file locally."))
        else:
            self.after(500, lambda: self._add_message("assistant", "I don't recognize that command intent. I support:\n- navigate [url]\n- click [selector]\n- type 'text' into [selector]\n- screenshot\n- compress [file]"))
        
        self.after(600, lambda: self.chat._parent_canvas.yview_moveto(1.0))

    def _toggle_web(self):
        self._web_on = self.web_sw.get()
        status = "active" if self._web_on else "off"
        color  = SUCCESS if self._web_on else TEXT_FAINT
        self.model_badge.configure(text=f"  Web Bridge: {status}  ", text_color=color, fg_color=SUCCESS_BG if self._web_on else ACCENT_SOFT)
        
        if self._web_on:
            self._add_message("system", "Web API Bridge enabled. Connecting to Relay Server (ws://localhost:8080)...")
            threading.Thread(target=self._ws_listen, daemon=True).start()
        else:
            self._add_message("system", "Web API Bridge disabled. Websocket disconnected.")
            if self.ws: self.ws.close()
            
        self.after(100, lambda: self.chat._parent_canvas.yview_moveto(1.0))

    def _ws_listen(self):
        try:
            self.ws = websocket.WebSocketApp(
                "ws://localhost:8080",
                on_message=self._on_ws_msg, on_open=self._on_ws_open, on_error=self._on_ws_err)
            self.ws.run_forever()
        except Exception as e:
            self.after(0, lambda: self._add_message("system", f"[WS] Failure: {str(e)}"))

    def _on_ws_open(self, ws):
        self.after(0, lambda: self._add_message("system", "Copilot Browser Relay Connected successfully!"))
        ws.send(json.dumps({
            "id": f"conn_{int(time.time()*1000)}", "type": "command", "from": "vscode",
            "command": "browserAgent.connect", "params": {}
        }))

    def _on_ws_msg(self, ws, message):
        try:
            data = json.loads(message)
            msg_type = data.get("type", "unknown")
            if msg_type == "response" and data.get("success", False):
                resp_data = data.get("data", {})
                log_text = json.dumps(resp_data)
                if len(log_text) > 200: log_text = log_text[:200] + "... [data truncated]"
                self.after(0, lambda: self._add_message("assistant", f"Success Result:\n{log_text}"))
            elif msg_type == "response" and not data.get("success", True):
                self.after(0, lambda: self._add_message("assistant", f"Error Result: {data.get('error')}"))
            self.after(100, lambda: self.chat._parent_canvas.yview_moveto(1.0))
        except: pass

    def _on_ws_err(self, ws, error):
        self.after(0, lambda: self._add_message("system", f"[WS] Socket Err: {str(error)}"))

    def _send_ws(self, cmd_name, params):
        if not self._web_on or not self.ws or not self.ws.sock or not self.ws.sock.connected:
            self.after(200, lambda: self._add_message("system", "Error: Web Bridge must be toggled ON to talk to the browser extension!"))
            return
        msg = {
            "id": f"cmd_{int(time.time()*1000)}", "type": "command", "from": "vscode",
            "timestamp": int(time.time()*1000), "command": cmd_name, "params": params
        }
        self.ws.send(json.dumps(msg))
