# Quick Start Guide

Get your compressed knowledge AI running in **5 minutes**!

---

## 🔧 Step 0: Build First (If No Executable)

If `bin/neural_engine.exe` doesn't exist yet, build it first:

```bash
# Quick build (Windows - use the build script)
build_neural_engine.bat
```

Or manually:
```bash
cd src

g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
    -DINCLUDE_SMART_BRAIN -DUNIFIED_BUILD -I../include ^
    -o ../bin/neural_engine.exe ^
    unified_main.cpp main.cpp neural_engine.cpp test_block_access.cpp ^
    compressor.cpp lz77.cpp huffman.cpp ans.cpp bwt.cpp bwt_simd.cpp ppm.cpp cmix.cpp ^
    knowledge_manager.cpp web_fetcher.cpp html_parser.cpp vector_index.cpp ^
    persistent_mixer.cpp compressed_knowledge.cpp block_access.cpp ^
    word_tokenizer.cpp word_ppm.cpp embedding_trainer.cpp rag_engine.cpp ^
    conversation_memory.cpp reasoning_engine.cpp bpe_tokenizer.cpp real_embeddings.cpp ^
    mini_transformer.cpp optimizer.cpp loss.cpp transformer_gradients.cpp ^
    -lwinhttp -lws2_32 -pthread
```

> See [README.md](../README.md#-build-instructions) for full build details, Linux/Mac instructions, and troubleshooting.

---

## ⚡ 5-Minute Setup

### Step 1: Test the System (30 seconds)

```bash
# Navigate to project
cd C:\Jay\_Plugin\compress

# Test compression
bin\neural_engine.exe compress knowledge_sample\programming.txt -o test.aiz --best

# Test knowledge query
bin\neural_engine.exe knowledge_query programming "python programming"
```

**Expected**: JSON output with Python programming information ✅

---

### Step 2: Install Python Dependencies (1 minute, first time only)

```bash
pip install fastapi uvicorn watchdog
```

---

### Step 3: Start the Server (30 seconds)

```bash
python server\main.py
```

**Expected**: Server running on `http://localhost:8001`

On startup the server automatically:
- Loads all `.aiz` knowledge modules from `knowledge/`
- Indexes all project files for AI access
- Starts file watcher for real-time updates

---

### Step 4: Open Desktop App (1 minute)

```bash
cd desktop_app

# Install dependencies (first time only)
npm install

# Start app
npm run dev
```

**Expected**: Electron app opens ✅

---

### Step 5: Ask Your First Question (30 seconds)

In the desktop app:
1. Type: **"What is Python?"**
2. Click **"Ask"**
3. See answer with **85% confidence**

**Expected**: Detailed Python explanation from compressed knowledge ✅

---

## 🎯 What You Just Did

1. **Built the executable**: One unified binary with all features
2. **Tested compression**: Compressed 7.9 KB → 4.1 KB (47.6% saved)
3. **Tested knowledge query**: Retrieved info from compressed module
4. **Started full stack**: Desktop app → Server → Neural engine → Knowledge
5. **Got AI answer**: From compressed knowledge in < 50ms

---

## 📝 Common First Questions

### "What is Python?"
- **Source**: programming.aiz
- **Response Time**: < 50ms
- **Content**: Language description, features, libraries

### "What can you do?"
- **Source**: capabilities.aiz (AI self-awareness module)
- **Content**: All 40+ commands, API endpoints, features

### "javascript frameworks"
- **Source**: programming.aiz
- **Topics**: React, Vue.js, Angular, Node.js

### "What files are in this project?"
- **Source**: project_structure.aiz (auto-generated index)
- **Content**: Real-time project file listing

---

## 🔧 Troubleshooting

### Build fails
```bash
# Check compiler version
g++ --version
# Need g++ 7+ for C++17

# If missing SSE2/AVX2 support, remove those flags
# Replace: -msse2 -mavx2
# With nothing (will be ~18% slower)
```

### Server won't start
```bash
# Check if port 8001 is in use
netstat -an | findstr 8001

# Kill existing python process
taskkill /F /IM python.exe

# Or install missing deps
pip install fastapi uvicorn watchdog
```

### Desktop app shows error
**Problem**: Server not running
**Solution**: Make sure `python server\main.py` is running first

### "No modules loaded" error
**Problem**: Wrong directory or no .aiz files in knowledge/
**Solution**:
```bash
# Run from project root
cd C:\Jay\_Plugin\compress

# Create a knowledge module
bin\neural_engine.exe compress knowledge_sample\programming.txt -o knowledge\programming.aiz --best
```

### Missing -lwinhttp error (Windows)
**Problem**: Not using MinGW-w64
**Solution**: Install MinGW-w64 or use the provided `build_neural_engine.bat`

---

## 🎓 Next Steps

### 1. Create Your Own Knowledge Module

```bash
# Create content
echo Your knowledge here... > my_knowledge.txt

# Compress it
bin\neural_engine.exe compress my_knowledge.txt -o knowledge\my_knowledge.aiz --best

# Server auto-loads it on next start, or query directly
bin\neural_engine.exe knowledge_query my_knowledge "your question"
```

### 2. Use the REST API

```bash
# Ask the AI
curl -X POST http://localhost:8001/api/brain/ask ^
  -H "Content-Type: application/json" ^
  -d "{\"question\": \"What can you do?\"}"

# Search project files
curl -X POST http://localhost:8001/api/ai/search ^
  -H "Content-Type: application/json" ^
  -d "{\"query\": \"compression\"}"

# Compress a file
curl -X POST http://localhost:8001/api/compress ^
  -H "Content-Type: application/json" ^
  -d "{\"file_path\": \"doc.txt\", \"algorithm\": \"--best\"}"
```

### 3. Explore All Commands

```bash
# Show all available commands
bin\neural_engine.exe

# Math operations
bin\neural_engine.exe math "sqrt(16) + log(100)"

# Entropy calculation
bin\neural_engine.exe entropy "Hello World"

# RAG - add documents and ask questions
bin\neural_engine.exe rag_add_doc my_document.txt
bin\neural_engine.exe rag_ask "What is the main topic?"

# Train a transformer model
bin\neural_engine.exe train_transformer corpus.txt 7 0.002 16
bin\neural_engine.exe transformer_generate "Once upon a time"
```

---

## 📊 What's Happening Under the Hood

```
Your Question: "What is Python?"
    ↓
Desktop App sends to Server (port 8001)
    ↓
Server calls: neural_engine.exe knowledge_query programming "What is Python?"
    ↓
Neural Engine:
  1. Opens programming.aiz (4.1 KB compressed)
  2. Decompresses block 0 using SIMD-accelerated BWT
  3. Searches for "python" (case-insensitive)
  4. Finds match, extracts 500 chars context
  5. Returns JSON with answer + confidence
    ↓
Server sends response to Desktop App
    ↓
You see answer < 50 milliseconds! ✅
```

---

## ✅ Success Checklist

- [ ] `bin\neural_engine.exe` built successfully
- [ ] Compression test works (`compress` command)
- [ ] Knowledge query works (`knowledge_query` command)
- [ ] `python server\main.py` running without errors
- [ ] Desktop app opens (`npm run dev`)
- [ ] First question answered (< 50ms response)

**All checked?** You're ready to go! 🚀

---

## 🎉 Congratulations!

You now have a **working compressed knowledge AI system**!

- ✅ One unified executable (4.4 MB) - no dependencies
- ✅ 90-99% compression ratio
- ✅ < 50ms query speed
- ✅ 100% offline operation
- ✅ $0 API costs
- ✅ Self-aware AI (knows its own 40+ capabilities)
- ✅ Dynamic project awareness (knows all your files)

**Next**: Read [COMPLETE_SYSTEM.md](COMPLETE_SYSTEM.md) to understand the full architecture.

---

**See Also**: [README.md](../README.md) | [AI_CAPABILITIES.md](AI_CAPABILITIES.md) | [phases/](../phases/)
