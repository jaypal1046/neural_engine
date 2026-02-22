# Project Handover Notes / Status Report

## Current State: V10 — Neural Brain + Compression AI

We built our **own AI** — no borrowed LLMs, no external APIs. The Neural Brain learns from scratch, stores everything compressed, and gets smarter over time. "Why adopt someone else's child when you can raise your own."

### The Philosophy

Compression IS intelligence. Shannon proved in 1948 that a perfect compressor must perfectly predict data. Our CMIX engine with 1,046 neural advisors already "understands" data at the byte level — it predicts what comes next using gradient descent. The Neural Brain extends this: it learns from text (user input + web scraping), indexes with TF-IDF, stores everything compressed, and retrieves to answer questions. It starts basic and gradually becomes smarter with every interaction.

### What V10 Delivers:

1. **Neural Brain (Our Own AI)**
   - `learn [topic]: [content]` — Teach it anything, it tokenizes, indexes, and compresses
   - `learn_url [topic] [url]` — Scrapes web pages, extracts text, compresses and stores
   - `ask [question]` — Retrieves relevant knowledge using TF-IDF matching
   - All knowledge stored compressed via CMIX (1,046 advisors) — takes minimal space
   - Vocabulary grows organically — the brain builds its own word index
   - Conversations are remembered
   - Unknown questions prompt the user to teach → the brain grows

2. **1,046-Advisor Neural Compression Engine**
   - Order 0-7 context models (128-192 advisors each)
   - Bit-level sub-byte pattern models (112-148 advisors)
   - Word boundary and skip/long-range models
   - AI-powered file analysis (entropy, byte distribution, pattern detection)
   - Auto-recommends best algorithm per file

3. **Neural Vault (Store & Access)**
   - `store [file]` → Compress and vault with metadata
   - `access [key]` → Decompress and retrieve
   - Full index with compression stats

4. **Desktop App ↔ Server Auto-Connect**
   - Electron auto-starts the Python server on app launch
   - Auto-kills server on app close
   - Live health monitoring in header
   - Server logs forwarded to Electron console

### Architecture:

```
desktop_app/ (Electron + React + Vite)
    ├── src/components/
    │   ├── HelperView.tsx    — AI Copilot with Neural Brain integration
    │   ├── CompressView.tsx  — Compression with AI pre-analysis
    │   ├── DecompressView.tsx — Mirror-mode decompression
    │   ├── Header.tsx        — Live server connection + model count
    │   └── Sidebar.tsx       — V10 branding, AI Copilot primary
    └── electron/
        ├── main.ts           — Auto-starts Python server
        └── preload.ts        — IPC bridge

server/ (FastAPI, Python)
    ├── main.py               — API server (compression + vault + brain endpoints)
    └── neural_brain.py       — The Neural Brain module
        ├── Text processing   — Tokenization, keyword extraction, TF-IDF
        ├── Knowledge storage — Compress & index learned content
        ├── Retrieval         — Find relevant knowledge for queries
        ├── Response gen      — Build answers from compressed knowledge
        ├── Web learning      — Scrape & learn from URLs
        └── Memory            — Remember conversations

brain/ (Auto-created at runtime)
    ├── brain_index.json      — Master index of all knowledge
    ├── vocabulary.json       — TF-IDF vocabulary (grows organically)
    ├── knowledge/            — Compressed knowledge files (.myzip)
    └── memory/               — Conversation history

vault/ (Auto-created at runtime)
    ├── vault_index.json      — File storage index
    └── *.myzip               — Compressed stored files

bin/
    └── myzip.exe             — C++ compression engine

src/ (C++)
    ├── main.cpp, compressor.cpp, cmix.cpp, bwt.cpp, ppm.cpp, etc.
```

### API Endpoints:

**Brain (Our Own AI)**
- `POST /api/brain/learn` — Teach knowledge (topic + content)
- `POST /api/brain/learn_url` — Learn from web page
- `POST /api/brain/ask` — Ask a question
- `POST /api/brain/search` — Search knowledge base
- `GET  /api/brain/stats` — Brain statistics
- `GET  /api/brain/knowledge/{id}` — Get full text of entry

**Compression**
- `POST /api/analyze` — Deep file analysis with AI insights
- `POST /api/compress_stream` — Streaming compression
- `POST /api/decompress_stream` — Streaming decompression
- `GET  /api/health` — Server status

**Vault**
- `POST /api/vault/store` — Compress & store file
- `POST /api/vault/access` — Retrieve & decompress
- `GET  /api/vault/list` — List stored files

**Utility**
- `POST /api/neural/handle` — Intelligent task routing
- `POST /api/cmd` — Terminal command execution

### Run Instructions:

```powershell
# Start the desktop app (auto-starts server)
cd desktop_app
npm run dev

# Or start server manually
cd server
python main.py
```

### Growth Roadmap (Our Own AI):

The brain starts basic and grows gradually:

1. **Phase 1 (Current):** TF-IDF retrieval, keyword matching, sentence extraction
2. **Phase 2:** N-gram language model — predict next words from compressed patterns
3. **Phase 3:** Word embeddings — learn word relationships from accumulated text
4. **Phase 4:** Attention mechanism — focus on relevant parts of knowledge
5. **Phase 5:** Neural response generation — produce coherent multi-sentence answers
6. **Phase 6:** Cross-topic reasoning — connect knowledge across different topics
7. **Phase 7:** Self-improvement — analyze its own errors and adjust weights

Each phase builds on the compressed knowledge already stored. The more you teach it, the smarter each upgrade becomes. The compression engine's neural network IS the foundation — same gradient descent, same pattern learning, just applied to language instead of bytes.

> **Our brain, our rules.** No API keys, no rate limits, no data leaving your machine. Every byte of knowledge stays local, compressed, and private.
