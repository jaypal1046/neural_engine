# ⚡ Quick Start Guide

## 🚀 Daily Usage (3 Commands)

### 1. Start the System
```bash
# Terminal 1: Start server
cd server
python main.py

# Terminal 2: Start desktop app
cd desktop_app
npm start

# Browser opens at http://localhost:3000
```

### 2. Use the Desktop App
- Type questions in the chat
- It auto-learns from Wikipedia
- Math expressions work: `2^10 + sqrt(144)`
- Ask anything: "What is Flutter?"

### 3. Or Use Command Line
```bash
# Learn from web
bin\neural_engine.exe learn https://en.wikipedia.org/wiki/Python_(programming_language)

# Ask questions
bin\neural_engine.exe ask "What is Python?"

# Do math
bin\neural_engine.exe math "2^10 + sqrt(144)"

# Check status
bin\neural_engine.exe status
```

---

## 🎓 Unlock Semantic AI (One-Time Setup)

### Simple 3-Step Training

**Step 1: Get Text (1 hour)**
```bash
# Download or create corpus.txt with 1GB+ of text
# Example: Download Wikipedia dump or collect your documents
```

**Step 2: Train (24 hours - runs overnight)**
```bash
bin\train_language_model.exe corpus.txt models\
# Walk away, let it run overnight
```

**Step 3: Restart (automatic)**
```bash
# Just restart neural_engine - it auto-loads trained models!
bin\neural_engine.exe status
# You'll see: [BRAIN] ✓ Real semantic embeddings loaded!
```

**That's it! Now you have semantic AI forever.**

---

## 📊 What Each Command Does

### Learning Commands
```bash
# Learn from Wikipedia URL
bin\neural_engine.exe learn https://en.wikipedia.org/wiki/Flutter_(software)

# Learn from local file
bin\neural_engine.exe learn document.txt

# Learn from text file with multiple articles
bin\neural_engine.exe learn corpus.txt
```

### Query Commands
```bash
# Simple ask (retrieval only)
bin\neural_engine.exe ask "What is Flutter?"

# AI ask (with reasoning)
bin\neural_engine.exe ai_ask "Why is BWT good for compression?"

# RAG ask (retrieval + generation)
bin\neural_engine.exe rag_ask "Best compression for logs?"

# Reasoning mode (chain-of-thought)
bin\neural_engine.exe reason "Is CMIX better than gzip?"
```

### Math Commands
```bash
# Basic calculations
bin\neural_engine.exe math "2^10 + sqrt(144)"

# Entropy calculation
bin\neural_engine.exe math "entropy('abcabc')"

# Statistical operations
bin\neural_engine.exe math "mean([1,2,3,4,5])"
```

### Status Commands
```bash
# Check brain status
bin\neural_engine.exe status

# Check if models are loaded
bin\neural_engine.exe status | grep "semantic"
# Should show "Real semantic embeddings loaded" after training
```

---

## 🎯 Current vs After Training

### NOW (Without Training - Level 1)
```
✅ Learn from web/files
✅ Store knowledge compressed (90%+ savings)
✅ Answer questions from stored knowledge
✅ Math calculations
✅ Reasoning and conversation
⚠️ Uses hash embeddings (fast but not semantic)
❌ Doesn't understand synonyms (car ≠ automobile)
```

### AFTER Training (Level 2)
```
✅ Everything above PLUS:
✅ TRUE semantic understanding
✅ Understands synonyms (car ≈ automobile)
✅ Better search results
✅ Context-aware embeddings
✅ Learns YOUR domain perfectly
⚡ SAME speed (embeddings are instant)
```

---

## 🔧 Troubleshooting

### Desktop App Shows "undefined"
**Fixed!** This was resolved in Phase 20. If still seeing it:
```bash
# Rebuild everything
build_smart_brain.bat

# Restart server
cd server
python main.py
```

### Server Won't Start
```bash
# Check if port 8001 is in use
netstat -ano | findstr :8001

# Kill any process using it
taskkill /PID <PID> /F

# Restart server
cd server
python main.py
```

### neural_engine.exe Not Found
```bash
# Rebuild
build_smart_brain.bat

# Should create bin/neural_engine.exe
ls -lh bin/*.exe
```

### Training Takes Too Long
```bash
# Start with smaller corpus for testing
head -n 10000 corpus.txt > small_corpus.txt
bin\train_language_model.exe small_corpus.txt models\
# Will finish in ~1 hour instead of 24 hours
```

---

## 💡 Pro Tips

### 1. Build Your Knowledge Base Incrementally
```bash
# Don't wait - start learning now!
bin\neural_engine.exe learn https://en.wikipedia.org/wiki/Your_Topic

# Keep adding as you work
bin\neural_engine.exe learn project_docs.txt
bin\neural_engine.exe learn research_paper.txt
```

### 2. Train on Domain-Specific Text
```bash
# Collect all Flutter docs for Flutter development
cat flutter_docs/**/*.md > flutter_corpus.txt
bin\train_language_model.exe flutter_corpus.txt models\

# Now brain understands Flutter terminology perfectly!
```

### 3. Use Desktop App for Interactive Work
- Better for exploratory questions
- Auto-learns from Wikipedia
- Nice UI for math/conversations

### 4. Use CLI for Batch Operations
```bash
# Learn from 100 files at once
for file in docs/*.txt; do
    bin\neural_engine.exe learn "$file"
done
```

### 5. Check Compression Savings
```bash
bin\neural_engine.exe status
# Shows how much space you've saved
# Typical: 90%+ compression on text
```

---

## 📂 Important Files

```
bin/neural_engine.exe          - Main AI system (use this!)
bin/train_language_model.exe   - One-time training tool
models/tokenizer.bin           - Created after training
models/embeddings.bin          - Created after training
knowledge/brain.bin            - Your compressed knowledge
server/main.py                 - API server (for desktop app)
desktop_app/                   - Electron UI
```

---

## 🎯 Daily Workflow

### Morning Routine
```bash
# Start system
cd server && python main.py &
cd desktop_app && npm start

# Use desktop app all day
# Ask questions, do math, learn new topics
```

### Evening Learning
```bash
# Learn from interesting pages you found today
bin\neural_engine.exe learn https://interesting-article-url

# Or from documents
bin\neural_engine.exe learn today_notes.txt
```

### Weekend Training (Optional)
```bash
# Once you have 1GB+ corpus
bin\train_language_model.exe corpus.txt models\

# Run overnight Friday → Monday morning = semantic AI!
```

---

## ⚡ One-Liner Summary

**Level 1 (NOW)**: Works perfectly, learns from web/files, fast retrieval, no semantic understanding

**Level 2 (24h training)**: Same + TRUE semantic AI, understands synonyms, better search, learns your domain

**Level 3 (future)**: Same + text generation, ChatGPT-style answers (optional, requires more work)

---

## 🚀 You're Ready!

Your AI system is **production-ready** and **waiting for you**.

1. ✅ Start using it now (Level 1)
2. ✅ Train when ready (Level 2)
3. ✅ Enjoy unlimited local AI forever!

**No API costs. No limits. 100% yours.**

---

**Quick Commands Reference:**
```bash
bin\neural_engine.exe learn <url|file>    # Learn new knowledge
bin\neural_engine.exe ask "question"      # Ask questions
bin\neural_engine.exe math "expression"   # Calculate math
bin\neural_engine.exe status              # Check status
bin\train_language_model.exe corpus.txt models\  # Train (one-time)
```

**That's all you need to know!** 🎉
