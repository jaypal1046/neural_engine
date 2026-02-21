"""
theme.py — Design tokens for Neural Studio
Inspired by Claude's warm, minimal, human-centered aesthetic.

Palette: warm off-white background, cream cards, soft warm grays,
terracotta/copper accent (like Claude's orange-brown brand color).
"""
import customtkinter as ctk

# ── Colors ─────────────────────────────────────────────────────────────────

# Backgrounds
BG          = ("#F5F0EB", "#212121")   # warm off-white (main bg) / Dark minimal
SURFACE     = ("#EFEAD4", "#181818")   # slightly warmer sidebar/panel / Darker sidebar
CARD        = ("#FDFAF7", "#2B2B2B")   # near-white card / Dark card
CARD_HOVER  = ("#F7F3EE", "#383838")

# Borders & dividers
BORDER      = ("#E2DAD0", "#424242")
BORDER_DARK = ("#C9BFB2", "#5E5E5E")

# Brand accent — warm terracotta, like Claude's coral/orange
ACCENT      = ("#C96442", "#DA7555")   # terracotta
ACCENT_DARK = ("#A84F33", "#E3896D")
ACCENT_SOFT = ("#F2E4DC", "#3A2A25")   # tinted bg for accent areas

# Semantic
SUCCESS     = ("#2D8A5E", "#3BB37A")
SUCCESS_BG  = ("#E6F5EE", "#1E362C")
ERROR       = ("#C0392B", "#E34E3F")
ERROR_BG    = ("#FDEDED", "#3F1E1B")
WARNING     = ("#B07D2A", "#D49E3F")
WARNING_BG  = ("#FDF3DC", "#45361B")
INFO        = ("#2B6CB0", "#4A90E2")
INFO_BG     = ("#EBF4FF", "#18324A")

# Text
TEXT        = ("#1A1410", "#F0F0F0")   # near-black warm / Light grey
TEXT_SUB    = ("#6B6059", "#A8A8A8")   # warm medium gray
TEXT_FAINT  = ("#A8998E", "#7A7A7A")   # muted label
TEXT_INV    = ("#FDFAF7", "#1A1410")   # text on dark bg

# Terminal / log
TERM_BG     = ("#1C1814", "#121212")
TERM_TEXT   = ("#C8B89A", "#E6D4B8")   # warm amber log text

# Chat bubbles
BUBBLE_USER = ("#C96442", "#DA7555")   # terracotta user bubble
BUBBLE_AI   = ("#FDFAF7", "#2C2C2C")   # white AI bubble
BUBBLE_SYS  = "transparent"


# ── Fonts (call init_fonts() after CTk root exists) ────────────────────────

F = {}   # populated by init_fonts()

def init_fonts():
    """Must be called after the root CTk window is created."""
    F["title"]   = ctk.CTkFont(family="Georgia",  size=28, weight="bold")
    F["heading"] = ctk.CTkFont(family="Georgia",  size=18, weight="bold")
    F["sub"]     = ctk.CTkFont(family="Georgia",  size=14)
    F["body"]    = ctk.CTkFont(family="Helvetica", size=14)
    F["label"]   = ctk.CTkFont(family="Helvetica", size=13, weight="bold")
    F["small"]   = ctk.CTkFont(family="Helvetica", size=12)
    F["tiny"]    = ctk.CTkFont(family="Helvetica", size=11, weight="bold")
    F["mono"]    = ctk.CTkFont(family="Courier New", size=13)
    F["mono_s"]  = ctk.CTkFont(family="Courier New", size=12)
    F["nav"]     = ctk.CTkFont(family="Helvetica", size=14, weight="bold")
    F["btn"]     = ctk.CTkFont(family="Helvetica", size=14, weight="bold")
