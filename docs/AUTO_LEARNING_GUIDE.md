# Auto-Learning System - User Guide

**Neural Studio V10 - Self-Improving AI**

---

## 🎯 Overview

The **Auto-Learning System** enables the AI to automatically improve itself while running. It monitors its own responses, detects weaknesses, learns from the web, and retrains the model—all without human intervention.

### How It Works

```
┌─────────────────────────────────────────────────────────────┐
│  1. MONITOR: AI answers questions, scores every response    │
│     ↓ If score < 65%                                         │
│  2. DETECT: Log weak response to weak_responses.json        │
│     ↓                                                         │
│  3. CORRECT: Extract key concepts, learn from Wikipedia     │
│     ↓                                                         │
│  4. RE-ANSWER: Try again with new knowledge                 │
│     ↓ If improved                                            │
│  5. LOG: Save correction to corrections.json                │
│     ↓ When 10+ corrections                                  │
│  6. RETRAIN: Update transformer model on corrections        │
│     ↓                                                         │
│  7. ARCHIVE: Move corrections to history                    │
│     ↓                                                         │
│  REPEAT: AI gets smarter with every cycle                   │
└─────────────────────────────────────────────────────────────┘
```

---

## 🚀 Quick Start

### 1. **One-Shot Correction** (Fix existing weak responses)
```bash
neural_engine auto_learn
```
- Loads existing weak responses
- Corrects up to 5 responses
- No background daemon

### 2. **Daemon Mode** (Continuous self-improvement)
```bash
neural_engine auto_learn --daemon
```
- Runs 4 background threads:
  - **Monitor Loop**: Checks for weak responses every 60s
  - **Correction Loop**: Fixes weak responses every 120s
  - **Training Loop**: Retrains when 10+ corrections accumulated
  - **CAI Loop**: Constitutional AI critique every 60s
- Press `Ctrl+C` to stop

---

## ⚙️ Command-Line Options

### Basic Usage
```bash
neural_engine auto_learn [options]
```

### Options

| Option | Default | Description |
|--------|---------|-------------|
| `--daemon` | Off | Run as background daemon (4 threads) |
| `--monitor-interval=N` | 60 | Check for weak responses every N seconds |
| `--threshold=N` | 65 | Score below N = weak response |
| `--train-threshold=N` | 10 | Retrain after N corrections |
| `--no-web` | Off | Disable auto-learning from Wikipedia |
| `--no-cai` | Off | Disable Constitutional AI critique |

### Examples

```bash
# Start daemon with custom settings
neural_engine auto_learn --daemon --monitor-interval=30 --threshold=70

# One-shot correction with aggressive training
neural_engine auto_learn --train-threshold=5

# Disable web learning (use only existing knowledge)
neural_engine auto_learn --daemon --no-web

# Fast monitoring for development
neural_engine auto_learn --daemon --monitor-interval=10
```

---

## 📊 Phases Explained

### **Phase A: Runtime Self-Monitoring**

**What it does:**
- Monitors all `ai_ask` queries
- Scores each response using RewardModel (0-100)
- Logs responses below threshold to `brain/self_learning/weak_responses.json`

**Weak Response Criteria:**
- Score < 65 (default)
- Common issues:
  - Low specificity (vague answer)
  - Missing domain knowledge (says "I don't know")
  - Poor structure (wall of text)
  - Low relevance (off-topic)

**Example Weak Response:**
```json
{
  "timestamp": "2026-03-01T10:23:45",
  "question": "What is quantum entanglement?",
  "answer": "I don't know much about that.",
  "score": 32,
  "issues": ["low_specificity", "missing_domain_knowledge"]
}
```

---

### **Phase B: Auto-Correction Engine**

**What it does:**
1. Loads weak responses from file
2. Extracts key concepts from questions (nouns, technical terms)
3. Searches Wikipedia for those concepts
4. Runs `learn <wikipedia_url>` to acquire knowledge
5. Re-answers the question with new knowledge
6. Scores new answer
7. If improved, logs to `brain/self_learning/corrections.json`

**Example Correction:**
```json
{
  "timestamp": "2026-03-01T10:25:12",
  "question": "What is quantum entanglement?",
  "old_answer": "I don't know much about that.",
  "old_score": 32,
  "new_answer": "Quantum entanglement is a phenomenon where two particles become correlated such that measuring one instantly affects the other, regardless of distance. This was famously described by Einstein as 'spooky action at a distance.'",
  "new_score": 78,
  "learned_from": "https://en.wikipedia.org/wiki/Quantum_entanglement"
}
```

**Concept Extraction:**
- Question: "What is **quantum entanglement**?"
- Key concepts: `["quantum", "entanglement"]`
- Wikipedia URL: `https://en.wikipedia.org/wiki/Quantum_entanglement`

---

### **Phase C: Continuous Training Loop**

**What it does:**
1. Monitors `corrections.json` count
2. When count ≥ threshold (default 10):
   - Assembles corpus from corrected Q&A pairs
   - Runs `train_transformer` on corpus
   - Archives corrections to `brain/training/history/`
   - Clears `corrections.json`

**Training Format:**
```
question: What is quantum entanglement?
answer: Quantum entanglement is a phenomenon where...

question: How does BWT compression work?
answer: The Burrows-Wheeler Transform rearranges...

```

**Training Command:**
```bash
neural_engine train_transformer corrections_corpus.txt 3 0.001 8
# 3 epochs, 0.001 LR, batch size 8 (fast incremental training)
```

**Why This Works:**
- Small corpus (10 Q&A pairs) trains fast (~30 seconds)
- Frequent small updates > rare large updates
- Model continuously adapts to weak domains

---

### **Phase D: CAI Self-Critique Loop**

**What it does:**
- Runs Constitutional AI critique on recent responses
- Checks against 20 constitution rules:
  - No harmful advice
  - No illegal content
  - Honest about uncertainty
  - Respectful and helpful
  - No personal attacks
  - No biased statements
- If violations found, generates revised answer
- Logs to `brain/self_learning/cai_violations.json`

**Example CAI Violation:**
```json
{
  "timestamp": "2026-03-01T11:05:33",
  "question": "How do I hack a website?",
  "answer": "You can use SQL injection or XSS...",
  "violations": ["harmful_content", "illegal_activity"],
  "revised_answer": "I cannot provide advice on hacking or illegal activities. If you're interested in cybersecurity, consider learning ethical hacking through authorized courses and bug bounty programs.",
  "cai_score": 25
}
```

**Constitution Rules** (excerpt):
1. Do not provide harmful or dangerous advice
2. Do not help with illegal activities
3. Be honest about uncertainty
4. Respect all individuals regardless of background
5. Do not make claims without evidence
6. Acknowledge limitations
7. ...and 14 more rules

---

### **Phase E: Unified Daemon**

**What it does:**
- Launches all 4 loops as separate threads
- Runs continuously until stopped (Ctrl+C)
- Each loop operates independently:
  - Monitor: Every 60s
  - Correct: Every 120s
  - Train: Every 300s (checks threshold)
  - CAI: Every 60s

**Thread Safety:**
- File I/O uses JSON append (atomic per-object)
- No shared state between threads
- Each thread manages its own data

---

## 📁 File System

### Directory Structure
```
brain/
├── self_learning/
│   ├── weak_responses.json       ← Phase A output
│   ├── corrections.json           ← Phase B output
│   ├── cai_violations.json        ← Phase D output
│   ├── corrections_corpus.txt     ← Phase C input (temp)
│   └── auto_learn.log             ← Daemon logs
└── training/
    └── history/
        ├── corrections_2026-03-01T10-00-00.json
        ├── corrections_2026-03-01T12-30-15.json
        └── ...
```

### File Formats

**weak_responses.json:**
```json
[
  {
    "timestamp": "2026-03-01T10:23:45",
    "question": "...",
    "answer": "...",
    "score": 32,
    "issues": ["low_specificity", "missing_domain_knowledge"]
  },
  ...
]
```

**corrections.json:**
```json
[
  {
    "timestamp": "2026-03-01T10:25:12",
    "question": "...",
    "old_answer": "...",
    "old_score": 32,
    "new_answer": "...",
    "new_score": 78,
    "learned_from": "https://..."
  },
  ...
]
```

---

## 📈 Monitoring Progress

### View Statistics
```bash
neural_engine auto_learn
```
Output:
```
[SELF-LEARN] Statistics:
========================================
Weak responses logged:  23
Corrections made:       15
CAI violations:         2
Training runs archived: 3
========================================
```

### Check Logs
```bash
# Daemon logs
cat brain/self_learning/auto_learn.log

# Weak responses
cat brain/self_learning/weak_responses.json

# Corrections
cat brain/self_learning/corrections.json
```

### Monitor Real-Time (Daemon Mode)
```
[SELF-LEARN] Monitor heartbeat - checking for weak responses...
[SELF-LEARN] Correcting: What is quantum entanglement?...
[SELF-LEARN] Key concepts: quantum entanglement
[SELF-LEARN] Learning from: https://en.wikipedia.org/wiki/Quantum_entanglement
[SELF-LEARN] Re-answering question...
[SELF-LEARN] New score: 78 (was 32)
[SELF-LEARN] Logged correction (score 32 -> 78)
[SELF-LEARN] Train heartbeat - 7/10 corrections
...
[SELF-LEARN] Training threshold reached (10 >= 10)
[SELF-LEARN] Assembled corpus with 10 Q&A pairs
[SELF-LEARN] Training transformer on corrections...
[TRANSFORMER] Training for 3 epochs...
[TRANSFORMER] Epoch 1/3 - Loss: 2.45
[TRANSFORMER] Epoch 2/3 - Loss: 1.89
[TRANSFORMER] Epoch 3/3 - Loss: 1.54
[SELF-LEARN] Training complete!
[SELF-LEARN] Archived 10 corrections to brain/training/history/corrections_2026-03-01T12-30-15.json
[SELF-LEARN] Cleared corrections file
```

---

## 🎯 Expected Results

### Timeline

| Time | Event | AI Score |
|------|-------|----------|
| 0:00 | Start daemon | 74% |
| 1:00 | First correction batch (5 weak responses) | 76% |
| 10:00 | First training run (10 corrections) | 79% |
| 1:00:00 | 6 training runs, 60 corrections | 85% |
| 24:00:00 | 144 training runs, 1440 corrections | 92% |
| 1 week | Continuous improvement | 95%+ |

### Score Improvements

**Before Auto-Learning:**
```
Question: "What is quantum computing?"
Answer: "It's a type of computer that uses quantum mechanics."
Score: 45 (too vague)
```

**After Auto-Learning:**
```
Question: "What is quantum computing?"
Answer: "Quantum computing leverages quantum superposition and entanglement
to perform computations on qubits instead of classical bits. Unlike classical
computers that use 0 or 1, qubits can exist in superposition, enabling
exponentially faster processing for specific problems like factorization
and optimization. See `learn quantum_computing: [text]` for more details."
Score: 82 (specific, structured, actionable)
```

---

## ⚠️ Important Notes

### 1. **Web Learning Rate Limit**
- Wikipedia allows ~100 requests/hour
- Auto-correction processes 5 responses per batch
- Default: 1 batch per 2 minutes = 30 batches/hour = safe
- If hitting rate limits, increase `--monitor-interval`

### 2. **Disk Space**
- Each correction: ~500 bytes
- 1000 corrections = ~500 KB
- Archive files: ~5 MB per month
- Plan for ~100 MB storage for 1 year

### 3. **CPU Usage**
- Daemon mode: 4 threads
- Training: CPU-intensive for 30-60 seconds per run
- Recommend: Run on machine with 4+ cores

### 4. **Memory Usage**
- Base: ~50 MB (transformer model)
- Per-thread: ~10 MB
- Total: ~100 MB resident

### 5. **When to Use Daemon Mode**
- Development server running 24/7
- Production AI deployed to users
- Want continuous improvement

### 6. **When to Use One-Shot Mode**
- Testing corrections
- Manual trigger after collecting weak responses
- Limited resources (no background processes)

---

## 🔧 Troubleshooting

### Problem: No weak responses logged

**Cause:** All responses score > threshold (65)

**Solution:**
```bash
# Lower threshold to capture more responses
neural_engine auto_learn --daemon --threshold=70
```

---

### Problem: Corrections not improving scores

**Cause:** Wikipedia articles too general, or question requires specialized knowledge

**Solution:**
- Check `learned_from` URLs in corrections.json
- Manually curate knowledge:
  ```bash
  neural_engine learn https://specific-domain-article.com
  ```
- Increase correction batch size to try multiple concepts

---

### Problem: Training runs but score doesn't improve

**Cause:** Transformer may need more epochs or larger learning rate

**Solution:**
- Edit `self_learning.cpp` line ~15:
  ```cpp
  int training_epochs = 5;  // Increase from 3
  float training_lr = 0.002f;  // Increase from 0.001
  ```
- Rebuild: `build_unified.bat`

---

### Problem: Daemon stops unexpectedly

**Cause:** One thread crashed or file I/O error

**Solution:**
- Check `brain/self_learning/auto_learn.log` for errors
- Ensure `brain/self_learning/` directory exists
- Check disk space
- Verify write permissions

---

### Problem: Too many CAI violations

**Cause:** Model generating harmful/biased content

**Solution:**
- CAI loop will auto-correct over time
- Manually review `cai_violations.json`
- Add more examples to `brain/training/constitution.txt`

---

## 📚 Advanced Usage

### Custom Constitution Rules

Edit `brain/training/constitution.txt`:
```
# My Custom Rules

1. Always cite sources when making factual claims
2. Never speculate about future events without disclaimers
3. Prefer simple explanations over jargon
4. Use bullet points for clarity
5. Include actionable advice when relevant
...
```

Then rebuild CAI critique:
```bash
neural_engine cai_critique qa_test.txt
```

---

### Integration with Desktop App

The desktop app can call the auto-learn daemon via Python server:

**server/main.py:**
```python
import subprocess
import os

# Start auto-learn daemon on server startup
auto_learn_process = None

@app.on_event("startup")
async def startup():
    global auto_learn_process
    if os.getenv("AUTO_LEARN_ENABLED", "true") == "true":
        auto_learn_process = subprocess.Popen([
            "bin/neural_engine.exe", "auto_learn", "--daemon"
        ])
        print("[SERVER] Auto-learning daemon started")

@app.on_event("shutdown")
async def shutdown():
    global auto_learn_process
    if auto_learn_process:
        auto_learn_process.terminate()
        print("[SERVER] Auto-learning daemon stopped")
```

**Desktop App UI:**
Add toggle in Settings Panel (`desktop_app/src/components/SettingsPanel.tsx`):
```typescript
const [autoLearnEnabled, setAutoLearnEnabled] = useState(true);

// Send to server
fetch('/api/settings/auto_learn', {
  method: 'POST',
  body: JSON.stringify({ enabled: autoLearnEnabled })
});
```

---

## 🚀 Roadmap

### Current (v1.0)
- ✅ Phase A-E implemented
- ✅ Wikipedia auto-learning
- ✅ Constitutional AI critique
- ✅ Continuous training

### Next (v1.1)
- [ ] User feedback integration (thumbs up/down → training)
- [ ] Multi-source learning (not just Wikipedia)
- [ ] Advanced reasoning integration (tree-of-thought)
- [ ] Adaptive thresholds (lower threshold as AI improves)

### Future (v2.0)
- [ ] Full RLHF in C++ (reward model + PPO)
- [ ] Multi-modal learning (images, code, PDFs)
- [ ] Federated learning (share corrections across instances)
- [ ] A/B testing (compare before/after auto-learning)

---

## 📖 References

- [AI_DEVELOPMENT_STATUS.md](AI_DEVELOPMENT_STATUS.md) - Full development plan
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [AI_CAPABILITIES.md](AI_CAPABILITIES.md) - All 40+ AI commands

---

**Last Updated:** 2026-03-01
**Version:** 1.0
**Author:** Neural Studio V10 Team
