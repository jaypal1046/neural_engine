"""
views/compress.py — Compression page view.
"""
import os, re, threading, subprocess
import customtkinter as ctk
import tkinter.filedialog as fd

from theme import BG, CARD, BORDER, ACCENT_SOFT, TEXT, TEXT_SUB, TEXT_FAINT, F
from widgets import (
    Card, Divider, SectionTitle, CardTitle, SubText,
    FieldLabel, StatusPill, AccentTag,
    ClaudeEntry, ClaudeOptionMenu,
    PrimaryBtn, GhostBtn,
    WarmProgressBar, TermLog
)

ALGORITHMS = [
    "CMIX — Neural Genetic",
    "DEFAULT — LZ77 + Delta",
    "BEST — BWT + rANS",
    "ULTRA — PPM Order-8",
]
FLAG_MAP = {
    "CMIX — Neural Genetic":  "--cmix",
    "DEFAULT — LZ77 + Delta": "",
    "BEST — BWT + rANS":      "--best",
    "ULTRA — PPM Order-8":    "--ultra",
}


class CompressView(ctk.CTkScrollableFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        kw.setdefault("scrollbar_button_color", BORDER)
        kw.setdefault("scrollbar_button_hover_color", BORDER)
        kw.setdefault("corner_radius", 0)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self._build()

    def _build(self):
        # ── Page header
        hdr = ctk.CTkFrame(self, fg_color="transparent")
        hdr.grid(row=0, column=0, padx=36, pady=(36, 24), sticky="ew")
        hdr.grid_columnconfigure(1, weight=1)

        SectionTitle(hdr, "Compress a File").grid(row=0, column=0, sticky="w")
        AccentTag(hdr, "Neural Engine").grid(row=0, column=2, sticky="e")
        SubText(hdr, text="Shrink any file using neural and entropy-based algorithms."
                ).grid(row=1, column=0, columnspan=3, sticky="w", pady=(6, 0))

        Divider(self).grid(row=1, column=0, sticky="ew", padx=36, pady=(0, 28))

        # ── Config card
        cfg = Card(self)
        cfg.grid(row=2, column=0, padx=36, pady=(0, 20), sticky="ew")
        cfg.grid_columnconfigure(1, weight=1)

        CardTitle(cfg, "Configuration").grid(
            row=0, column=0, columnspan=3, padx=24, pady=(22, 18), sticky="w")

        Divider(cfg).grid(row=1, column=0, columnspan=3, sticky="ew", padx=0)

        # Input file
        FieldLabel(cfg, "Input File").grid(
            row=2, column=0, padx=(24, 16), pady=(18, 4), sticky="nw")
        self.entry_in = ClaudeEntry(cfg, placeholder_text="Click Browse or paste a file path…")
        self.entry_in.grid(row=2, column=1, padx=(0, 8), pady=(18, 4), sticky="ew")
        GhostBtn(cfg, text="Browse", width=96,
                 command=self.browse_in).grid(row=2, column=2, padx=(0, 24), pady=(18, 4))

        # Output file
        FieldLabel(cfg, "Save Archive As").grid(
            row=3, column=0, padx=(24, 16), pady=(12, 4), sticky="nw")
        self.entry_out = ClaudeEntry(cfg, placeholder_text="Destination .myzip path…")
        self.entry_out.grid(row=3, column=1, padx=(0, 8), pady=(12, 4), sticky="ew")
        GhostBtn(cfg, text="Browse", width=96,
                 command=self.browse_out).grid(row=3, column=2, padx=(0, 24), pady=(12, 4))

        # Algorithm
        FieldLabel(cfg, "Algorithm").grid(
            row=4, column=0, padx=(24, 16), pady=(12, 4), sticky="nw")
        self.algo_var = ctk.StringVar(value=ALGORITHMS[0])
        ClaudeOptionMenu(cfg, variable=self.algo_var, values=ALGORITHMS, width=280
                         ).grid(row=4, column=1, padx=(0, 8), pady=(12, 4), sticky="w")

        # Algo info pills
        pills = ctk.CTkFrame(cfg, fg_color="transparent")
        pills.grid(row=5, column=0, columnspan=3, padx=24, pady=(10, 22), sticky="w")
        info = [
            ("Best ratio",   "CMIX"),
            ("Fastest",      "DEFAULT"),
            ("Balanced",     "BEST"),
            ("Max compress", "ULTRA"),
        ]
        for desc, tag in info:
            chip = ctk.CTkFrame(pills, fg_color=ACCENT_SOFT, corner_radius=20)
            chip.pack(side="left", padx=(0, 8))
            ctk.CTkLabel(chip, text=tag, font=F["tiny"],
                         text_color="#8B3A20").pack(side="left", padx=10, pady=3)
            ctk.CTkLabel(chip, text=desc, font=F["tiny"],
                         text_color=TEXT_SUB).pack(side="left", padx=(0, 10), pady=3)

        # CTA button
        self.btn_run = PrimaryBtn(cfg, text="Start Compression →",
                                  command=self.start_compress)
        self.btn_run.grid(row=6, column=0, columnspan=3,
                          padx=24, pady=(4, 24), sticky="ew")

        # ── Progress card
        prog = Card(self)
        prog.grid(row=3, column=0, padx=36, pady=(0, 40), sticky="ew")
        prog.grid_columnconfigure(0, weight=1)

        prog_hdr = ctk.CTkFrame(prog, fg_color="transparent")
        prog_hdr.grid(row=0, column=0, padx=24, pady=(22, 0), sticky="ew")
        prog_hdr.grid_columnconfigure(1, weight=1)

        CardTitle(prog_hdr, "Output").grid(row=0, column=0, sticky="w")
        self.status_pill = StatusPill(prog_hdr, state="idle")
        self.status_pill.grid(row=0, column=2, sticky="e")

        Divider(prog).grid(row=1, column=0, sticky="ew", padx=0, pady=(16, 0))

        # Progress bar + pct
        bar_row = ctk.CTkFrame(prog, fg_color="transparent")
        bar_row.grid(row=2, column=0, padx=24, pady=(16, 4), sticky="ew")
        bar_row.grid_columnconfigure(0, weight=1)

        self.pbar = WarmProgressBar(bar_row)
        self.pbar.grid(row=0, column=0, sticky="ew")
        self.pbar.set(0)

        self.pct_lbl = ctk.CTkLabel(bar_row, text="0%",
                                    font=F["small"], text_color=TEXT_FAINT)
        self.pct_lbl.grid(row=0, column=1, padx=(12, 0))

        # Log
        self.log = TermLog(prog, height=200)
        self.log.grid(row=3, column=0, padx=24, pady=(4, 24), sticky="ew")

    # ── Actions ────────────────────────────────────────────────────────────

    def browse_in(self):
        f = fd.askopenfilename()
        if f:
            self.entry_in.delete(0, "end"); self.entry_in.insert(0, f)
            self.entry_out.delete(0, "end"); self.entry_out.insert(0, f + ".myzip")

    def browse_out(self):
        f = fd.asksaveasfilename(defaultextension=".myzip",
                                 filetypes=[("MyZip Archive", "*.myzip")])
        if f:
            self.entry_out.delete(0, "end"); self.entry_out.insert(0, f)

    def start_compress(self):
        src = self.entry_in.get().strip()
        dst = self.entry_out.get().strip()
        if not src or not dst:
            self.status_pill.set_state("error", "Missing paths"); return

        flag = FLAG_MAP.get(self.algo_var.get(), "")
        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        exe  = os.path.join(base, "bin", "myzip.exe")
        cmd  = [exe, "compress", src, dst] + ([flag] if flag else [])

        self.btn_run.configure(state="disabled", text="Compressing…")
        self.log.clear()
        self.pbar.set(0); self.pct_lbl.configure(text="0%")
        self.status_pill.set_state("running")
        threading.Thread(target=self._worker, args=(cmd,), daemon=True).start()

    def _worker(self, cmd):
        pct_re = re.compile(r"(\d+(?:\.\d+)?)%")
        try:
            proc = subprocess.Popen(
                cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, bufsize=1)
        except FileNotFoundError:
            self.after(0, lambda: self._done(-999)); return

        for ch in iter(lambda: proc.stdout.read(1), ""):
            if ch == "\r":
                line = ""
                for nc in iter(lambda: proc.stdout.read(1), ""):
                    if nc in ("\r", "\n"): break
                    line += nc
                m = pct_re.search(line)
                if m:
                    p = float(m.group(1)) / 100
                    self.after(0, lambda p=p, l=line: self._update(p, l))
                else:
                    self.after(0, lambda l=line: self.log.append(l + "\n"))
            else:
                line = ch
                for nc in iter(lambda: proc.stdout.read(1), ""):
                    if nc == "\n": break
                    if nc != "\r": line += nc
                self.after(0, lambda l=line: self.log.append(l + "\n"))

        proc.wait()
        self.after(0, lambda: self._done(proc.returncode))

    def _update(self, pct, line):
        self.pbar.set(pct)
        self.pct_lbl.configure(text=f"{pct*100:.0f}%")
        self.log.append(line + "\n")

    def _done(self, code):
        self.btn_run.configure(state="normal", text="Start Compression →")
        if code == 0:
            self.pbar.set(1.0); self.pct_lbl.configure(text="100%")
            self.status_pill.set_state("done")
        elif code == -999:
            self.status_pill.set_state("missing", "myzip.exe not found")
        else:
            self.status_pill.set_state("error", f"Exit code {code}")
