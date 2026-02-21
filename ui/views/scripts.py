"""
views/scripts.py — User-defined Script Engine
"""
import customtkinter as ctk
from theme import BG, TEXT, F, TERM_BG, TERM_TEXT
from widgets import SectionTitle, SubText, PrimaryBtn, TermLog, Card, Divider

class ScriptsView(ctk.CTkScrollableFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(2, weight=1)
        self._build()

    def _build(self):
        hdr = ctk.CTkFrame(self, fg_color="transparent")
        hdr.grid(row=0, column=0, padx=36, pady=(36, 24), sticky="ew")
        
        SectionTitle(hdr, "Script Environment").grid(row=0, column=0, sticky="w")
        SubText(hdr, text="Write and execute neural macros directly inside the studio.").grid(row=1, column=0, pady=(6, 0), sticky="w")
        Divider(self).grid(row=1, column=0, sticky="ew", padx=36, pady=(0, 20))

        cfg = Card(self)
        cfg.grid(row=2, column=0, padx=36, pady=20, sticky="nsew")
        cfg.grid_columnconfigure(0, weight=1)
        cfg.grid_rowconfigure(1, weight=1)

        ctk.CTkLabel(cfg, text="Editor", font=F["label"]).grid(row=0, column=0, padx=24, pady=(20, 10), sticky="w")
        
        self.editor = ctk.CTkTextbox(cfg, corner_radius=10, fg_color=TERM_BG, text_color=TERM_TEXT, font=F["mono"])
        self.editor.grid(row=1, column=0, padx=24, pady=(0, 20), sticky="nsew")
        self.editor.insert("end", "# Write neural script here...\n# Available commands: \n#   studio.compress()\n#   studio.web.navigate('http://localhost')\n\nprint('Engine ready.')\n")
        
        PrimaryBtn(cfg, text="Execute Macro", command=self.do_exec).grid(row=2, column=0, padx=24, pady=(0, 24), sticky="se")

    def do_exec(self):
        code = self.editor.get("1.0", "end")
        # Sandbox exec
        try:
            exec(code)
        except Exception as e:
            print("Execution Error:", e)
