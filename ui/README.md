# Neural Studio

A clean, Claude-inspired desktop UI for your neural compression & AI assistant tool.

## Structure

```
neural_studio/
├── main.py          ← Entry point — run this
├── theme.py         ← All colors, fonts, design tokens
├── widgets.py       ← Reusable UI components
├── sidebar.py       ← Left navigation panel
└── views/
    ├── compress.py  ← Compression page
    ├── decompress.py← Decompression page
    └── helper.py    ← AI Assistant chat
```

## Setup

```bash
pip install customtkinter
python main.py
```

## Notes

- Place your `myzip.exe` in a `bin/` folder one level above `neural_studio/`
- The font engine initializes after the root window, so no "too early" errors
- Light mode by default; toggle Dark/System from the sidebar
