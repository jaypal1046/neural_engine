"""
views/decompress.py — Decompression page view.
"""
import os, re, threading, subprocess
import customtkinter as ctk
import tkinter.filedialog as fd

from theme import BG, BORDER, ACCENT_SOFT, TEXT_FAINT, INFO, INFO_BG, F
from widgets import (
    Card, Divider, SectionTitle, CardTitle, SubText,
    FieldLabel, StatusPill, AccentTag,
    ClaudeEntry, PrimaryBtn, GhostBtn,
    WarmProgressBar, TermLog
)


class DecompressView(ctk.CTkScrollableFrame):
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        kw.setdefault("scrollbar_button_color", BORDER)
        kw.setdefault("scrollbar_button_hover_color", BORDER)
        kw.setdefault("corner_radius", 0)
        super().__init__(master, **kw)
        self.grid_columnconfigure(0, weight=1)
        self._build()

    def _build(self):
        # ── Header
        hdr = ctk.CTkFrame(self, fg_color="transparent")
        hdr.grid(row=0, column=0, padx=36, pady=(36, 24), sticky="ew")
        hdr.grid_columnconfigure(1, weight=1)

        SectionTitle(hdr, "Decompress Archive").grid(row=0, column=0, sticky="w")

        # Info tag in blue tones for decompress
        info_tag = ctk.CTkLabel(hdr, text="  Restore  ",
                                font=F["tiny"], text_color=INFO,
                                fg_color=INFO_BG, corner_radius=20)
        info_tag.grid(row=0, column=2, sticky="e")

        SubText(hdr, text="Restore a .myzip archive back to its original file."
                ).grid(row=1, column=0, columnspan=3, sticky="w", pady=(6, 0))

        Divider(self).grid(row=1, column=0, sticky="ew", padx=36, pady=(0, 28))

        # ── Config card
        cfg = Card(self)
        cfg.grid(row=2, column=0, padx=36, pady=(0, 20), sticky="ew")
        cfg.grid_columnconfigure(1, weight=1)

        CardTitle(cfg, "Configuration").grid(
            row=0, column=0, columnspan=3, padx=24, pady=(22, 18), sticky="w")
        Divider(cfg).grid(row=1, column=0, columnspan=3, sticky="ew")

        FieldLabel(cfg, "Archive (.myzip)").grid(
            row=2, column=0, padx=(24, 16), pady=(18, 4), sticky="nw")
        self.entry_in = ClaudeEntry(cfg, placeholder_text="Select a .myzip archive…")
        self.entry_in.grid(row=2, column=1, padx=(0, 8), pady=(18, 4), sticky="ew")
        GhostBtn(cfg, text="Browse", width=96,
                 command=self.browse_in).grid(row=2, column=2, padx=(0, 24), pady=(18, 4))

        FieldLabel(cfg, "Output File").grid(
            row=3, column=0, padx=(24, 16), pady=(12, 4), sticky="nw")
        self.entry_out = ClaudeEntry(cfg, placeholder_text="Where to save the recovered file…")
        self.entry_out.grid(row=3, column=1, padx=(0, 8), pady=(12, 4), sticky="ew")
        GhostBtn(cfg, text="Browse", width=96,
                 command=self.browse_out).grid(row=3, column=2, padx=(0, 24), pady=(12, 4))

        self.btn_run = PrimaryBtn(cfg, text="Restore File →",
                                  command=self.start_decompress)
        self.btn_run.grid(row=4, column=0, columnspan=3,
                          padx=24, pady=(16, 24), sticky="ew")

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

        Divider(prog).grid(row=1, column=0, sticky="ew", pady=(16, 0))

        bar_row = ctk.CTkFrame(prog, fg_color="transparent")
        bar_row.grid(row=2, column=0, padx=24, pady=(16, 4), sticky="ew")
        bar_row.grid_columnconfigure(0, weight=1)

        self.pbar = WarmProgressBar(bar_row)
        self.pbar.grid(row=0, column=0, sticky="ew")
        self.pbar.set(0)

        self.pct_lbl = ctk.CTkLabel(bar_row, text="0%",
                                    font=F["small"], text_color=TEXT_FAINT)
        self.pct_lbl.grid(row=0, column=1, padx=(12, 0))

        self.log = TermLog(prog, height=200)
        self.log.grid(row=3, column=0, padx=24, pady=(4, 24), sticky="ew")

    # ── Actions ────────────────────────────────────────────────────────────

    def browse_in(self):
        f = fd.askopenfilename(filetypes=[("MyZip Archive", "*.myzip"), ("All Files", "*.*")])
        if f:
            self.entry_in.delete(0, "end"); self.entry_in.insert(0, f)
            out = f.replace(".myzip", "") or f + ".recovered"
            self.entry_out.delete(0, "end"); self.entry_out.insert(0, out)

    def browse_out(self):
        f = fd.asksaveasfilename()
        if f:
            self.entry_out.delete(0, "end"); self.entry_out.insert(0, f)

    def start_decompress(self):
        src = self.entry_in.get().strip()
        dst = self.entry_out.get().strip()
        if not src or not dst:
            self.status_pill.set_state("error", "Missing paths"); return

        base = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
        exe  = os.path.join(base, "bin", "myzip.exe")
        cmd  = [exe, "decompress", src, dst]

        self.btn_run.configure(state="disabled", text="Restoring…")
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
        self.btn_run.configure(state="normal", text="Restore File →")
        if code == 0:
            self.pbar.set(1.0); self.pct_lbl.configure(text="100%")
            self.status_pill.set_state("done", "Restored")
        elif code == -999:
            self.status_pill.set_state("missing", "myzip.exe not found")
        else:
            self.status_pill.set_state("error", f"Exit code {code}")
