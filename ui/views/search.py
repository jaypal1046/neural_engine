"""
views/search.py — Accelerated Local C++ Search
"""
import customtkinter as ctk
from theme import BG, TEXT, F
from widgets import SectionTitle, SubText, ClaudeEntry, PrimaryBtn, TermLog, Card, Divider

class SearchView(ctk.CTkScrollableFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self._build()

    def _build(self):
        hdr = ctk.CTkFrame(self, fg_color="transparent")
        hdr.grid(row=0, column=0, padx=36, pady=(36, 24), sticky="ew")
        
        SectionTitle(hdr, "Accelerated Search").grid(row=0, column=0, sticky="w")
        SubText(hdr, text="Access desktop files via Neural Studio C++ core search.").grid(row=1, column=0, pady=(6, 0), sticky="w")
        Divider(self).grid(row=1, column=0, sticky="ew", padx=36, pady=(0, 20))

        cfg = Card(self)
        cfg.grid(row=2, column=0, padx=36, pady=20, sticky="ew")
        cfg.grid_columnconfigure(1, weight=1)

        ctk.CTkLabel(cfg, text="Search Query", font=F["label"]).grid(row=0, column=0, padx=20, pady=20, sticky="w")
        self.entry_q = ClaudeEntry(cfg, placeholder_text="Pattern or text...")
        self.entry_q.grid(row=0, column=1, padx=(0, 20), pady=20, sticky="ew")
        
        self.btn_run = PrimaryBtn(cfg, text="Search C++ Core", command=self.do_search)
        self.btn_run.grid(row=1, column=0, columnspan=2, padx=20, pady=(0, 20), sticky="ew")

        prog = Card(self)
        prog.grid(row=3, column=0, padx=36, pady=(0, 40), sticky="ew")
        prog.grid_columnconfigure(0, weight=1)
        ctk.CTkLabel(prog, text="Local Results", font=F["label"]).grid(row=0, column=0, padx=24, pady=20, sticky="w")
        
        self.log = TermLog(prog, height=300)
        self.log.grid(row=1, column=0, padx=24, pady=(0, 24), sticky="ew")

    def do_search(self):
        query = self.entry_q.get()
        if not query: return
        self.log.clear()
        self.log.append(f"Initiating High-Speed Neural Traverse for: {query}...\n")
        self.log.append("Indexing local nodes...\n")
        self.after(800, lambda: self.log.append("Search architecture routing pending C++ hook.\n"))
