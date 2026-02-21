"""
widgets.py — Reusable UI components styled to the Claude-inspired theme.
"""
import customtkinter as ctk
from theme import (
    BG, SURFACE, CARD, CARD_HOVER, BORDER, BORDER_DARK,
    ACCENT, ACCENT_DARK, ACCENT_SOFT,
    SUCCESS, SUCCESS_BG, ERROR, ERROR_BG, WARNING, WARNING_BG,
    TERM_BG, TERM_TEXT,
    TEXT, TEXT_SUB, TEXT_FAINT, TEXT_INV,
    F
)


# ── Layout Helpers ─────────────────────────────────────────────────────────

class Divider(ctk.CTkFrame):
    """1px horizontal separator."""
    def __init__(self, master, color=BORDER, **kw):
        kw.setdefault("height", 1)
        kw.setdefault("fg_color", color)
        kw.setdefault("corner_radius", 0)
        super().__init__(master, **kw)


class Card(ctk.CTkFrame):
    """Warm white card with soft border and rounded corners."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", CARD)
        kw.setdefault("corner_radius", 14)
        kw.setdefault("border_width", 1)
        kw.setdefault("border_color", BORDER)
        super().__init__(master, **kw)


class SectionTitle(ctk.CTkLabel):
    """Page-level title in Georgia serif."""
    def __init__(self, master, text, **kw):
        kw.setdefault("font", F["title"])
        kw.setdefault("text_color", TEXT)
        super().__init__(master, text=text, **kw)


class CardTitle(ctk.CTkLabel):
    """Card-level heading."""
    def __init__(self, master, text, **kw):
        kw.setdefault("font", F["heading"])
        kw.setdefault("text_color", TEXT)
        super().__init__(master, text=text, **kw)


class SubText(ctk.CTkLabel):
    """Muted subtitle/description text."""
    def __init__(self, master, text, **kw):
        kw.setdefault("font", F["body"])
        kw.setdefault("text_color", TEXT_SUB)
        super().__init__(master, text=text, **kw)


class FieldLabel(ctk.CTkLabel):
    """Small bold label above an input field."""
    def __init__(self, master, text, **kw):
        kw.setdefault("font", F["label"])
        kw.setdefault("text_color", TEXT_SUB)
        super().__init__(master, text=text, **kw)


class StatusPill(ctk.CTkLabel):
    """Colored status indicator pill."""
    PRESETS = {
        "idle":     (TEXT_FAINT,  CARD,       "Idle"),
        "running":  (WARNING,     WARNING_BG, "Running…"),
        "done":     (SUCCESS,     SUCCESS_BG, "Complete"),
        "error":    (ERROR,       ERROR_BG,   "Failed"),
        "missing":  (ERROR,       ERROR_BG,   "Not Found"),
    }

    def __init__(self, master, state="idle", **kw):
        color, bg, label = self.PRESETS[state]
        super().__init__(master,
                         text=f"  {label}  ",
                         font=F["small"],
                         text_color=color,
                         fg_color=bg,
                         corner_radius=20,
                         **kw)

    def set_state(self, state, custom_text=None):
        color, bg, label = self.PRESETS.get(state, (TEXT_FAINT, CARD, state))
        self.configure(text=f"  {custom_text or label}  ",
                       text_color=color, fg_color=bg)


class AccentTag(ctk.CTkLabel):
    """Small accent-colored tag/badge."""
    def __init__(self, master, text, **kw):
        super().__init__(master, text=f"  {text}  ",
                         font=F["tiny"],
                         text_color=ACCENT,
                         fg_color=ACCENT_SOFT,
                         corner_radius=20, **kw)


# ── Inputs ─────────────────────────────────────────────────────────────────

class ClaudeEntry(ctk.CTkEntry):
    """Clean warm-styled text entry."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        kw.setdefault("border_color", BORDER_DARK)
        kw.setdefault("border_width", 1)
        kw.setdefault("text_color", TEXT)
        kw.setdefault("placeholder_text_color", TEXT_FAINT)
        kw.setdefault("font", F["body"])
        kw.setdefault("height", 40)
        kw.setdefault("corner_radius", 8)
        super().__init__(master, **kw)


class ClaudeOptionMenu(ctk.CTkOptionMenu):
    """Warm-styled dropdown."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BG)
        kw.setdefault("button_color", BORDER_DARK)
        kw.setdefault("button_hover_color", BORDER_DARK)
        kw.setdefault("dropdown_fg_color", CARD)
        kw.setdefault("dropdown_hover_color", CARD_HOVER)
        kw.setdefault("dropdown_text_color", TEXT)
        kw.setdefault("text_color", TEXT)
        kw.setdefault("font", F["body"])
        kw.setdefault("dropdown_font", F["body"])
        kw.setdefault("height", 40)
        kw.setdefault("corner_radius", 8)
        super().__init__(master, **kw)


# ── Buttons ────────────────────────────────────────────────────────────────

class PrimaryBtn(ctk.CTkButton):
    """Main CTA button — terracotta filled."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", ACCENT)
        kw.setdefault("hover_color", ACCENT_DARK)
        kw.setdefault("text_color", TEXT_INV)
        kw.setdefault("font", F["btn"])
        kw.setdefault("height", 44)
        kw.setdefault("corner_radius", 10)
        super().__init__(master, **kw)


class GhostBtn(ctk.CTkButton):
    """Secondary ghost button."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", "transparent")
        kw.setdefault("hover_color", CARD_HOVER)
        kw.setdefault("border_width", 1)
        kw.setdefault("border_color", BORDER_DARK)
        kw.setdefault("text_color", TEXT_SUB)
        kw.setdefault("font", F["label"])
        kw.setdefault("height", 40)
        kw.setdefault("corner_radius", 8)
        super().__init__(master, **kw)


class SendBtn(ctk.CTkButton):
    """Chat send button."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", ACCENT)
        kw.setdefault("hover_color", ACCENT_DARK)
        kw.setdefault("text_color", TEXT_INV)
        kw.setdefault("font", F["btn"])
        kw.setdefault("height", 42)
        kw.setdefault("width", 90)
        kw.setdefault("corner_radius", 10)
        super().__init__(master, **kw)


# ── Progress ───────────────────────────────────────────────────────────────

class WarmProgressBar(ctk.CTkProgressBar):
    """Slim warm-tinted progress bar."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", BORDER)
        kw.setdefault("progress_color", ACCENT)
        kw.setdefault("height", 4)
        kw.setdefault("corner_radius", 2)
        super().__init__(master, **kw)


# ── Terminal Log ───────────────────────────────────────────────────────────

class TermLog(ctk.CTkTextbox):
    """Dark terminal-style log box with warm amber text."""
    def __init__(self, master, **kw):
        kw.setdefault("fg_color", TERM_BG)
        kw.setdefault("border_color", BORDER)
        kw.setdefault("border_width", 1)
        kw.setdefault("text_color", TERM_TEXT)
        kw.setdefault("font", F["mono"])
        kw.setdefault("corner_radius", 10)
        super().__init__(master, **kw)

    def append(self, text):
        self.insert("end", text)
        self.see("end")

    def clear(self):
        self.delete("1.0", "end")
