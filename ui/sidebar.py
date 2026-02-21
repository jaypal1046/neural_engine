"""
sidebar.py — Left navigation panel, Claude-style.
"""
import customtkinter as ctk
from theme import (
    SURFACE, CARD, CARD_HOVER, BORDER, BORDER_DARK,
    ACCENT, ACCENT_SOFT,
    TEXT, TEXT_SUB, TEXT_FAINT, F
)

NAV_ITEMS = [
    ("compress",   "Compress",     "↓"),
    ("decompress", "Decompress",   "↑"),
    ("search",     "Fast Search",  "🔍"),
    ("browser",    "Web Bridge",   "🌐"),
    ("scripts",    "Scripts",      "⚡"),
    ("helper",     "AI Assistant", "✦"),
]


class Sidebar(ctk.CTkFrame):
    def __init__(self, master, on_select, **kw):
        kw["fg_color"]      = SURFACE
        kw["corner_radius"] = 0
        kw["width"]         = 230
        super().__init__(master, **kw)
        self.on_select = on_select
        self._btns: dict[str, ctk.CTkButton] = {}
        self._active: str | None = None
        self._build()

    def _build(self):
        self.grid_propagate(False)
        self.grid_columnconfigure(0, weight=1)
        self.grid_rowconfigure(5, weight=1)   # pushes settings to bottom

        # ── Logo
        logo = ctk.CTkFrame(self, fg_color="transparent")
        logo.grid(row=0, column=0, padx=22, pady=(28, 6), sticky="w")

        ctk.CTkLabel(logo, text="Neural",
                     font=F["heading"], text_color=TEXT).pack(side="left")
        ctk.CTkLabel(logo, text=" Studio",
                     font=F["heading"], text_color=ACCENT).pack(side="left")

        ctk.CTkLabel(self, text="Your personal compression & AI tool",
                     font=F["tiny"], text_color=TEXT_FAINT, wraplength=190, justify="left",
                     ).grid(row=1, column=0, padx=22, pady=(0, 20), sticky="w")

        # ── Divider
        ctk.CTkFrame(self, height=1, fg_color=BORDER, corner_radius=0
                     ).grid(row=2, column=0, sticky="ew", padx=16, pady=(0, 16))

        # ── Nav buttons
        nav_frame = ctk.CTkFrame(self, fg_color="transparent")
        nav_frame.grid(row=3, column=0, sticky="ew")
        nav_frame.grid_columnconfigure(0, weight=1)

        for i, (key, label, icon) in enumerate(NAV_ITEMS):
            btn = ctk.CTkButton(
                nav_frame,
                text=f"  {icon}   {label}",
                anchor="w",
                fg_color="transparent",
                hover_color=CARD_HOVER,
                text_color=TEXT_SUB,
                font=F["nav"],
                height=44,
                corner_radius=10,
                command=lambda k=key: self.select(k)
            )
            btn.grid(row=i, column=0, padx=10, pady=2, sticky="ew")
            self._btns[key] = btn

        # ── Spacer handled by row weight

        # ── Bottom settings section
        bottom = ctk.CTkFrame(self, fg_color="transparent")
        bottom.grid(row=6, column=0, sticky="ew", padx=0, pady=(0, 0))
        bottom.grid_columnconfigure(0, weight=1)

        ctk.CTkFrame(bottom, height=1, fg_color=BORDER, corner_radius=0
                     ).grid(row=0, column=0, sticky="ew", padx=16, pady=(0, 14))

        ctk.CTkLabel(bottom, text="APPEARANCE",
                     font=F["tiny"], text_color=TEXT_FAINT
                     ).grid(row=1, column=0, padx=22, pady=(0, 6), sticky="w")

        ctk.CTkOptionMenu(
            bottom, values=["Light", "Dark", "System"],
            fg_color=CARD, button_color=BORDER_DARK,
            button_hover_color=BORDER_DARK,
            dropdown_fg_color=CARD,
            dropdown_hover_color=CARD_HOVER,
            text_color=TEXT_SUB,
            dropdown_text_color=TEXT,
            font=F["small"],
            dropdown_font=F["small"],
            command=ctk.set_appearance_mode,
            height=34, corner_radius=8
        ).grid(row=2, column=0, padx=14, pady=(0, 6), sticky="ew")

        ctk.CTkLabel(bottom, text="v2.0  ·  Neural Studio",
                     font=F["tiny"], text_color=TEXT_FAINT
                     ).grid(row=3, column=0, padx=22, pady=(4, 20), sticky="w")

    def select(self, key: str):
        # Deactivate previous
        if self._active and self._active in self._btns:
            self._btns[self._active].configure(
                fg_color="transparent", text_color=TEXT_SUB)

        # Activate new
        self._active = key
        self._btns[key].configure(fg_color=ACCENT_SOFT, text_color=ACCENT)
        self.on_select(key)
