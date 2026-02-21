"""
main.py — Entry point for Neural Studio.

Run:
    python main.py

Requirements:
    pip install customtkinter
"""
import sys, os

# Make sure sibling imports work when run from any directory
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

import customtkinter as ctk

import theme
from sidebar import Sidebar
from views.compress   import CompressView
from views.decompress import DecompressView
from views.helper     import HelperView
from views.search     import SearchView
from views.browser    import BrowserView
from views.scripts    import ScriptsView


ctk.set_appearance_mode("Light")
ctk.set_default_color_theme("blue")


class NeuralStudio(ctk.CTk):
    def __init__(self):
        super().__init__()

        # Fonts MUST be initialized after the root window exists
        theme.init_fonts()

        # ── Window setup
        self.title("Neural Studio")
        self.geometry("1120x720")
        self.minsize(900, 600)
        self.configure(fg_color=theme.BG)

        # ── Layout: sidebar (col 0) + content (col 1)
        self.grid_rowconfigure(0, weight=1)
        self.grid_columnconfigure(1, weight=1)

        # ── Sidebar
        self.sidebar = Sidebar(self, on_select=self._show)
        self.sidebar.grid(row=0, column=0, sticky="nsew")

        # ── Views — all created up front, shown/hidden via grid
        self._views: dict[str, ctk.CTkBaseClass] = {
            "compress":   CompressView(self),
            "decompress": DecompressView(self),
            "search":     SearchView(self),
            "browser":    BrowserView(self),
            "scripts":    ScriptsView(self),
            "helper":     HelperView(self),
        }
        for view in self._views.values():
            view.grid(row=0, column=1, sticky="nsew")

        # ── Default view
        self.sidebar.select("compress")

    def _show(self, name: str):
        for key, view in self._views.items():
            if key == name:
                view.grid(row=0, column=1, sticky="nsew")
            else:
                view.grid_forget()


if __name__ == "__main__":
    app = NeuralStudio()
    app.mainloop()
