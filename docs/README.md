# AIZip Documentation

> **One unified AI system: world-class compression + full AI intelligence, 100% offline, $0 cost**

---

## 📚 Documentation Index

### Getting Started
| File | Description |
|------|-------------|
| [QUICKSTART.md](QUICKSTART.md) | **Start here** — build, run, first query in 5 minutes |
| [COMPLETE_SYSTEM.md](COMPLETE_SYSTEM.md) | Full system reference — all features, architecture, build |
| [AI_CAPABILITIES.md](AI_CAPABILITIES.md) | All 40+ AI commands and 20+ REST API endpoints |
| [AI_SELF_AWARENESS.md](AI_SELF_AWARENESS.md) | How the AI knows its own capabilities |

### Reference
| File | Description |
|------|-------------|
| [HANDOVER.md](HANDOVER.md) | Project handover notes and design decisions |
| [v8_BWT.md](v8_BWT.md) | BWT algorithm details (.aiz v8 format) |

### Phase Documentation
Located in [../phases/](../phases/):
| File | Description |
|------|-------------|
| [PHASE13_MULTITHREADING.md](../phases/PHASE13_MULTITHREADING.md) | 8-thread parallel compression |
| [PHASE14_BLOCK_ARCHITECTURE.md](../phases/PHASE14_BLOCK_ARCHITECTURE.md) | Block-based random access |
| [PHASE15_INTEGRATION_COMPLETE.md](../phases/PHASE15_INTEGRATION_COMPLETE.md) | Knowledge integration |
| [PHASE16_SIMD_OPTIMIZATION.md](../phases/PHASE16_SIMD_OPTIMIZATION.md) | SSE2/AVX2 SIMD optimization |

---

## 🔧 Build in 60 Seconds

```bash
# Windows — use the build script
build_neural_engine.bat

# Or manually (from project root)
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

> See [QUICKSTART.md](QUICKSTART.md) for step-by-step guide, [COMPLETE_SYSTEM.md](COMPLETE_SYSTEM.md) for full details.

---

## ⚡ Quick Usage

```bash
# Compress
bin\neural_engine.exe compress file.txt -o file.aiz --best

# Decompress
bin\neural_engine.exe decompress file.aiz -o file.txt

# Ask AI
bin\neural_engine.exe knowledge_query capabilities "What can you do?"

# Start server (auto-loads all knowledge)
python server\main.py
```

---

## 🎯 Key Stats

| Metric | Value |
|--------|-------|
| Compression ratio | 85-99% saved |
| Decompression speed | 7.11 MB/s (SIMD) |
| Query speed | < 50ms |
| AI commands | 40+ |
| REST endpoints | 20+ |
| Executable size | 4.4 MB (one file) |
| Operating cost | $0 |

---

## 🌟 What Makes This Special

- **One binary**: `neural_engine.exe` does everything — compress, AI, knowledge, training
- **Self-aware**: AI knows its own capabilities automatically
- **Dynamic**: AI indexes and knows all your project files in real-time
- **Offline**: No internet, no API keys, no privacy concerns, ever
- **Fast**: SIMD + multi-threading + block access = maximum speed
- **`.aiz` format**: Unique AI Zip format, purposefully built for this system

---

**Status**: Production Ready ✅ | **Version**: .aiz v8 | **Updated**: 2026-02-25
