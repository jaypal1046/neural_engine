# Neural Studio V10 — Architecture Reference

## THE GOLDEN RULE

> **Python = Support Layer only. C++ = The ONE Brain.**
>
> Python files NEVER answer questions, NEVER generate responses, NEVER store knowledge.
> Python only: serves HTTP, reads files, parses formats, bridges UI → C++.
> C++ does ALL intelligence: learning, reasoning, memory, training, compression.

This rule must NEVER be broken. If you are about to add AI logic to a Python file — stop. Port it to C++ instead.

---

## System Overview

```
┌─────────────────────────────────────────────────────────────────┐
│  React + Electron Desktop App  (desktop_app/)                   │
│  - UI: CompressView, DecompressView, HelperView, BrowserView    │
│  - Electron IPC: file dialogs, server management               │
└────────────────────────┬────────────────────────────────────────┘
                         │ HTTP REST API (port 8001)
┌────────────────────────▼────────────────────────────────────────┐
│  Python FastAPI Server  (server/main.py)           SUPPORT ONLY │
│  Role: HTTP routing, file I/O, PDF/DOCX parsing, subprocess     │
│  Does NOT: answer questions, generate AI responses, store data  │
└────────────────────────┬────────────────────────────────────────┘
                         │ subprocess calls
┌────────────────────────▼────────────────────────────────────────┐
│  C++ Neural Engine  (bin/neural_engine.exe)        THE ONE BRAIN│
│                                                                 │
│  Intelligence:                                                  │
│  ├─ ai_ask        → reason + RAG + memory (best Q&A)           │
│  ├─ ask           → direct knowledge query                      │
│  ├─ rag_ask       → RAG engine direct                           │
│  ├─ learn <url>   → fetch, parse, compress, index              │
│  ├─ learn <file>  → read, compress, index                      │
│  ├─ memory_record → store conversation turn                     │
│  ├─ reason        → chain-of-thought reasoning                  │
│  └─ verify        → fact verification                           │
│                                                                 │
│  Training:                                                      │
│  ├─ train_transformer <corpus> [epochs] [lr] [batch]           │
│  └─ (RLHF: SFT + reward model + PPO — next C++ phase)          │
│                                                                 │
│  Compression:                                                   │
│  ├─ compress <file> [-o output] [--cmix|--best|--ultra]        │
│  ├─ decompress <archive> [-o output]                           │
│  └─ benchmark <file>                                           │
└─────────────────────────────────────────────────────────────────┘
```

---

## Python Files — Support Roles Only

### `server/main.py`
**Role: FastAPI HTTP server and request router.**
- Receives HTTP requests from React frontend
- Validates inputs, checks file existence
- Calls `neural_engine.exe` via subprocess for ALL AI operations
- Returns JSON responses to frontend
- Does NOT: answer questions, store knowledge, generate text

### `server/neural_brain.py`
**Role: Legacy Python TF-IDF brain — DEPRECATED for Q&A.**
- Functions still used: `load_index()`, `load_vocab()` (for stats display only)
- `generate_response()` is NO LONGER CALLED anywhere in main.py
- `store_knowledge()` is NO LONGER CALLED anywhere in main.py
- Do NOT call these for intelligence — use C++ `ai_ask` instead
- This file exists only to provide `load_index()` / `load_vocab()` for the `/api/brain/stats` and `/api/brain/status` endpoints

### `server/file_converter.py`
**Role: Document format extraction (PDF, DOCX, XLSX → plain text).**
- Reads binary document formats that C++ cannot natively parse
- Extracts plain text content
- Returns text to `main.py` which writes it to a temp file for C++ to learn
- Does NOT store, index, or learn anything itself

### `server/ai_file_operations.py`
**Role: File system operations for the AI helper.**
- Searches files, reads file contents, lists folders
- Pure file I/O — no AI logic
- Results returned to React via main.py

### `server/project_indexer.py`
**Role: Project file discovery and loading into C++ brain on startup.**
- Scans all project source files
- Calls `neural_engine.exe learn <file>` to index them into C++
- Runs ONCE on server startup
- Support only — triggers C++ learning, does not learn itself

### `server/dynamic_indexer.py`
**Role: Watch for new/changed files and trigger C++ reindexing.**
- File system watcher
- When files change, calls `neural_engine.exe learn <file>`
- Support only — triggers C++ learning

### `server/chat_port.py`
**Role: Background TCP socket on port 9000.**
- Accepts raw text connections
- Forwards messages to C++ engine via subprocess
- Support/bridge only

### `server/rlhf_trainer.py`
**Role: DEPRECATED — Python RLHF training scripts.**
- Was used for Python TF-IDF brain training
- NO LONGER CALLED by main.py (brain_train now uses C++ train_transformer)
- FUTURE: Full RLHF (SFT + reward model + PPO) must be implemented in C++
- See: RLHF_IN_CPP section below for the porting plan

### `server/deep_trainer.py`
**Role: DEPRECATED — Python deep training scripts.**
- Same as rlhf_trainer.py — was for Python brain
- Not called by main.py anymore
- Future: Port to C++

### `server/self_improve.py`
**Role: DEPRECATED — Python self-assessment.**
- Was for scoring Python brain responses
- Not called by main.py (brain_assess still calls it but Python brain is deprecated)
- Future: C++ should handle self-assessment via `verify` command

### `server/neural_reasoning.py`
**Role: Deprecated Python reasoning helper.**
- Was supplementary reasoning logic for Python brain
- Not used in current C++ brain path

### `train_from_anthropic.py`
**Role: Support script — parses Anthropic .md files, sends to C++.**
- Python parses the .md files into sections (Python is good at text parsing)
- Writes each section to a temp .txt file
- Calls `neural_engine.exe learn <temp_file>` for each section
- C++ does the actual learning, compression, indexing
- Run: `python train_from_anthropic.py` from project root

### `create_training_corpus.py`
**Role: Support script — assembles corpus from knowledge files for C++ training.**
- Collects all .txt files from brain/knowledge/
- Combines into single corpus.txt
- Caller then runs: `neural_engine.exe train_transformer corpus.txt`

---

## C++ Commands Reference

### Intelligence Commands (replace ALL Python brain calls)

| Command | Usage | What it does |
|---------|-------|--------------|
| `ai_ask` | `neural_engine.exe ai_ask "question"` | Best answer: reason + RAG + memory. Returns JSON `{answer, confidence, reasoning_steps, sources}` |
| `ask` | `neural_engine.exe ask "question"` | Direct knowledge lookup. Returns JSON `{answer, confidence}` |
| `rag_ask` | `neural_engine.exe rag_ask "question"` | RAG engine only. Fast retrieval |
| `learn` | `neural_engine.exe learn <url_or_file>` | Fetch/read, compress with CMIX, store in brain/knowledge/, index for RAG |
| `memory_record` | `neural_engine.exe memory_record "text"` | Store conversation turn |
| `reason` | `neural_engine.exe reason "problem"` | Chain-of-thought reasoning |
| `verify` | `neural_engine.exe verify "claim"` | Fact check a statement |
| `chain_of_thought` | `neural_engine.exe chain_of_thought "q"` | Multi-step reasoning |

### Training Commands

| Command | Usage | What it does |
|---------|-------|--------------|
| `train_transformer` | `neural_engine.exe train_transformer corpus.txt 7 0.002 16` | Train on corpus file. Args: corpus, epochs, lr, batch_size |
| `knowledge_load` | `neural_engine.exe knowledge_load <module>` | Load .aiz knowledge module into memory |
| `knowledge_query` | `neural_engine.exe knowledge_query <topic> "query"` | Query specific knowledge module |

### Compression Commands

| Command | Usage |
|---------|-------|
| `compress` | `neural_engine.exe compress <file> -o <output.aiz> [--cmix\|--best\|--ultra]` |
| `decompress` | `neural_engine.exe decompress <archive.aiz> -o <output>` |
| `benchmark` | `neural_engine.exe benchmark <file>` |

---

## RLHF in C++ — The Plan

Anthropic trains Claude in 3 phases. Here is how to implement each in C++:

### Phase 1: Pretraining (DONE)
- `train_transformer corpus.txt epochs lr batch` already exists
- Trains on next-token prediction
- Use `brain/training_corpus.txt` (assembled from all knowledge files)

### Phase 2: SFT — Supervised Fine-Tuning (NEXT)
- Need: `brain/training/` folder with `{prompt, ideal_response}` pairs in JSON
- Need: C++ command `sft <training_pairs.json>` that fine-tunes the transformer
- Implementation: Load pairs, run forward pass, compute cross-entropy loss on response tokens only, backprop

### Phase 3: RLHF — Reward Model + PPO (FUTURE)
- Need: `brain/training/` folder with `{prompt, response_a, response_b, preferred}` comparisons
- Need: C++ `train_reward_model <comparisons.json>` command
- Need: C++ `rlhf_ppo <reward_model> <corpus.json>` command
- This is the phase that makes the AI significantly more helpful and aligned

### Constitutional AI (CAI) — Anthropic's Key Innovation
- Generate response → critique it against a "constitution" (rules.txt) → revise → train on revised
- Need: C++ `cai_improve <constitution.txt> <response>` command
- The revised responses become training data for RLHF

---

## How All Python API Endpoints Map to C++

| Python Endpoint | C++ Command Called |
|-----------------|--------------------|
| `POST /api/brain/think` | `neural_engine.exe ai_ask <message>` |
| `POST /api/brain/ask` | `neural_engine.exe ai_ask <question>` |
| `POST /api/brain/learn` | `neural_engine.exe learn <url>` |
| `POST /api/brain/learn_url` | `neural_engine.exe learn <url>` |
| `POST /api/brain/learn_file` | Python reads PDF/DOCX → temp.txt → `neural_engine.exe learn temp.txt` |
| `POST /api/brain/train` | Assembles corpus.txt → `neural_engine.exe train_transformer corpus.txt` |
| `POST /api/compress` | `neural_engine.exe compress <file> -o <output> <algo>` |
| `POST /api/decompress` | `neural_engine.exe decompress <archive> -o <output>` |
| `POST /api/brain/learn` (C++) | `neural_engine.exe learn <source>` |
| `POST /api/math/process` | `neural_engine.exe math <query>` |

---

## File Output: What C++ Creates

All C++ learning output goes to `brain/knowledge/`:
```
brain/knowledge/<topic>_<hash>.txt    ← original extracted text
brain/knowledge/<topic>_<hash>.aiz    ← CMIX-compressed version
brain/brain_index.json                ← index of all knowledge items
brain/vocabulary.json                 ← word vocabulary
brain/training_corpus.txt             ← assembled corpus for train_transformer
```

---

## Quick Build Command

```batch
cd src && g++ -O3 -std=c++17 -Wall -march=native -msse2 -mavx2 ^
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

---

*Last updated: 2026-02-25 | Version: Neural Studio V10*
