# 🧠 Smart Brain - Commands Cheat Sheet

## Quick Reference

### Build & Run

```bash
# 1. Build C++ engine (once)
build_smart_brain.bat

# 2. Start server
cd server && python main.py

# 3. Start app
cd desktop_app && npm run dev
```

---

## Chat Commands (AI Copilot Tab)

### Smart Brain

| Command | What it does |
|---------|--------------|
| `brain status` | Show knowledge entries and compression stats |
| `learn https://...` | Learn from any Wikipedia URL |
| `What is X?` | Ask question (auto Smart Brain) |
| `How does X work?` | Ask question (auto Smart Brain) |
| `Explain X` | Ask question (auto Smart Brain) |

### Existing Features (Still Work!)

| Command | What it does |
|---------|--------------|
| `help` | Show help |
| `ls` or `ls C:\path` | List directory |
| `read C:\file.txt` | Read file |
| `find *.txt` | Find files |
| `analyze C:\file.txt` | AI file analysis |
| `compress C:\file.txt` | Compress with CMIX |
| `store C:\file.txt` | Store in neural vault |
| `vault list` | Show vault contents |
| `access [key]` | Retrieve from vault |
| `calc 2 + 3 * 4` | Math calculation |
| `run echo hello` | Run shell command |

---

## Example Workflows

### Learn About Compression

```
learn https://en.wikipedia.org/wiki/Data_compression
learn https://en.wikipedia.org/wiki/Huffman_coding
learn https://en.wikipedia.org/wiki/BWT
learn https://en.wikipedia.org/wiki/LZ77

What is data compression?
How does Huffman coding work?
Explain BWT algorithm
```

### File Operations

```
ls C:\data
read C:\data\file.txt
analyze C:\data\file.txt
compress C:\data\file.txt
brain status
```

### Build Knowledge Base

```
learn https://en.wikipedia.org/wiki/Artificial_intelligence
learn https://en.wikipedia.org/wiki/Machine_learning
learn https://en.wikipedia.org/wiki/Neural_network

What is AI?
How does machine learning work?
Explain neural networks
```

---

## API Endpoints (For Reference)

### Smart Brain (NEW)

```
POST /api/brain/learn
Body: { "source": "https://..." }

POST /api/brain/ask
Body: { "question": "What is X?" }

GET /api/brain/status
```

### Existing

```
POST /api/analyze
POST /api/compress_stream
POST /api/vault/store
GET /api/vault/list
POST /api/fs/list
POST /api/fs/read
```

---

## Keyboard Shortcuts (In Chat)

- `Enter` - Send message
- `Shift+Enter` - New line
- Click 🧠 - Go to AI Copilot

---

## Files You Can Learn From

Smart Brain supports any URL, especially:

- ✅ Wikipedia articles
- ✅ Documentation sites
- ✅ Blog posts
- ✅ News articles
- ✅ Any HTML page

---

## Tips

1. **Start with Wikipedia** - Clean, well-formatted content
2. **Use specific questions** - "What is X?" works better than "X"
3. **Check confidence** - If < 70%, consider learning more
4. **Learn related topics** - Builds better understanding
5. **Check brain status** - Monitor growth and savings

---

## Quick Start Flow

```bash
# 1. Build
build_smart_brain.bat

# 2. Start server
cd server
python main.py

# 3. Open app (new terminal)
cd desktop_app
npm run dev

# 4. In browser (http://localhost:5173)
Click "🧠 AI Copilot" tab

# 5. Try these:
brain status
learn https://en.wikipedia.org/wiki/Data_compression
What is data compression?
```

---

**That's it! Smart Brain is ready to learn and answer! 🚀**
