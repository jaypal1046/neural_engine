# 📚 What's What - File Purpose Guide

## Confused about which file does what? Here's the breakdown:

---

## 🐍 Python Server Files

### `server/main.py` ⭐ **THE MAIN SERVER** ⭐
**What it does:**
- Main FastAPI server that handles ALL requests
- Runs on `http://127.0.0.1:8001`
- Routes requests to C++ engines

**Key endpoints:**
```python
# File operations
POST /api/analyze           # AI file analysis
POST /api/compress_stream   # Stream compression output
POST /api/fs/list          # List directory
POST /api/fs/read          # Read file

# Neural Vault
POST /api/vault/store      # Store in vault
GET /api/vault/list        # List vault
POST /api/vault/access     # Retrieve from vault

# Smart Brain (C++ Engine) 🆕
POST /api/brain/learn      # Learn from URL → smart_brain.exe
POST /api/brain/ask        # Query knowledge → smart_brain.exe
GET /api/brain/status      # Brain stats → smart_brain.exe

# Old Neural Brain (Python-based, lower priority)
POST /api/brain/think      # Fallback reasoning
POST /api/brain/learn      # Old learn endpoint
POST /api/brain/ask        # Old ask endpoint
```

**When to use:** This is your ONLY server. Run: `python server/main.py`

---

### `server/main_smart_brain.py` ❌ **NOT USED - CAN DELETE**
**What it was:** Ultra-thin server for ONLY Smart Brain endpoints

**Status:** We integrated Smart Brain into `main.py` instead, so this file is **redundant**. You can delete it or keep it as reference.

---

### Other Python files in `server/`
- `neural_brain.py` - Old Python-based brain (used by `/api/brain/think`)
- `neural_reasoning.py` - Old reasoning engine
- `file_converter.py` - PDF/DOCX text extraction
- `train_basics.py` - Old training scripts

**Status:** Still used by old endpoints but lower priority than Smart Brain

---

## 💻 Desktop App Files

### `desktop_app/src/components/HelperView.tsx` ⭐ **THE MAIN CHAT** ⭐
**What it does:**
- The "🧠 AI Copilot" tab you see in the app
- Handles ALL chat messages and commands
- Routes to appropriate API endpoints

**Flow:**
```
User types: "What is BWT?"
    ↓
HelperView detects question pattern
    ↓
Tries Smart Brain FIRST: POST /api/brain/ask
    ↓
If Smart Brain answers → show answer
If Smart Brain fails → try old brain
If both fail → show "I don't understand"
```

**Key features:**
- ✅ Smart Brain integration (NEW!)
- ✅ File operations (ls, read, find)
- ✅ Compression commands
- ✅ Neural Vault
- ✅ Math calculations
- ✅ Old neural brain (fallback)

**When to use:** This is your ONLY chat interface. No other component needed!

---

### `desktop_app/src/components/SmartBrainChat.tsx` ❌ **DELETED**
**What it was:** Separate chat component just for Smart Brain

**Status:** We integrated Smart Brain into `HelperView.tsx` instead, so this file was **deleted**. All Smart Brain features are in `HelperView.tsx` now.

---

### Other Desktop App files
- `App.tsx` - Main app router (routes to HelperView, CompressView, etc.)
- `Sidebar.tsx` - Left sidebar with tabs
- `CompressView.tsx` - Compression tab UI
- `DecompressView.tsx` - Decompression tab UI
- `BrowserView.tsx` - Web bridge tab
- `SearchView.tsx` - Search tab
- `ScriptsView.tsx` - Scripts tab

---

## ⚙️ C++ Engine Files

### `bin/smart_brain.exe` ⭐ **THE SMART BRAIN** ⭐
**What it does:**
- C++ knowledge engine
- Learns from web, compresses, stores, retrieves
- Called by `main.py` endpoints

**Commands:**
```bash
smart_brain.exe learn <url>      # Learn from web
smart_brain.exe ask "<question>" # Query knowledge
smart_brain.exe compress <file>  # Smart compression
smart_brain.exe status           # Show stats
```

**When it's used:**
- When you type: `learn https://...` → calls this
- When you ask: "What is X?" → calls this
- When you run: `brain status` → calls this

**Built by:** `build_smart_brain.bat`

---

### `bin/myzip.exe` ⭐ **THE COMPRESSOR** ⭐
**What it does:**
- C++ compression tool
- CMIX, BWT, PPM modes
- Called by `main.py` for compression

**Commands:**
```bash
myzip.exe compress <file>           # Default (v7 LZ77)
myzip.exe compress <file> --best    # BWT mode (v8)
myzip.exe compress <file> --ultra   # PPM mode (v9)
myzip.exe compress <file> --cmix    # CMIX mode (v10)
myzip.exe decompress <file>         # Restore
```

**When it's used:**
- When you type: `compress C:\file.txt` → calls this
- When you type: `store C:\file.txt` → calls this (vault)

**Built by:** Included in `build_smart_brain.bat`

---

## 📊 Quick Reference

### What runs where:

| Component | File | Port | Purpose |
|-----------|------|------|---------|
| Python Server | `server/main.py` | 8001 | Routes ALL requests |
| Desktop App | `desktop_app/` | 5173 | UI (Vite dev server) |
| Smart Brain | `bin/smart_brain.exe` | N/A | Called by server |
| Compressor | `bin/myzip.exe` | N/A | Called by server |

### User journey for "What is BWT?":

```
1. User types in HelperView.tsx: "What is BWT?"
2. HelperView.tsx → POST /api/brain/ask
3. main.py → spawns: smart_brain.exe ask "What is BWT?"
4. smart_brain.exe → searches brain/index.bin
5. smart_brain.exe → decompresses brain/knowledge/bwt.myzip
6. smart_brain.exe → returns JSON answer
7. main.py → returns JSON to HelperView
8. HelperView.tsx → shows answer with confidence
```

---

## 🎯 To Start Everything:

```bash
# 1. Build C++ engines (once)
build_smart_brain.bat

# 2. Start Python server (Terminal 1)
cd server
python main.py

# 3. Start desktop app (Terminal 2)
cd desktop_app
npm run dev

# 4. Open browser
http://localhost:5173

# 5. Click "🧠 AI Copilot" tab

# 6. Try commands:
brain status
learn https://en.wikipedia.org/wiki/Data_compression
What is data compression?
```

---

## 🗑️ Files You Can Delete (Optional)

These are redundant now:

- ❌ `server/main_smart_brain.py` - Smart Brain already in main.py
- ❌ `desktop_app/src/components/SmartBrainChat.tsx` - Already deleted
- ❌ Any `.pyc` files or `__pycache__` folders

---

## ✅ Files You Must Keep

**Python:**
- ✅ `server/main.py` - THE MAIN SERVER
- ✅ `server/neural_brain.py` - Used by old brain (fallback)
- ✅ `server/neural_reasoning.py` - Used by old brain
- ✅ `server/file_converter.py` - PDF/DOCX conversion

**Desktop:**
- ✅ `desktop_app/src/components/HelperView.tsx` - THE MAIN CHAT
- ✅ `desktop_app/src/App.tsx` - App router
- ✅ `desktop_app/src/components/Sidebar.tsx` - Sidebar
- ✅ All other `.tsx` components

**C++:**
- ✅ Everything in `src/` and `include/`
- ✅ `bin/smart_brain.exe`
- ✅ `bin/myzip.exe`

---

## 🔄 Priority Order

When you ask a question like "What is neural?":

1. **First:** Smart Brain C++ (`/api/brain/ask` → `smart_brain.exe`)
2. **Second:** Old Python brain (`/api/brain/think` → `neural_brain.py`)
3. **Last:** Fallback ("I don't understand")

Smart Brain is **higher priority** now!

---

## 🎉 Summary

**One server (`main.py`), one chat (`HelperView.tsx`), two C++ engines (`smart_brain.exe` + `myzip.exe`).**

Everything else is just supporting files or old redundant code you can delete.

Simple! 🚀
