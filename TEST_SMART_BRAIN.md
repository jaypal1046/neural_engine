# 🧪 Smart Brain Testing Guide

Follow these steps to test the complete system.

---

## Step 1: Build the C++ Engine

**Windows:**
```bash
build_smart_brain.bat
```

**Linux/Mac:**
```bash
chmod +x build_smart_brain.sh
./build_smart_brain.sh
```

**Expected output:**
```
========================================
Building Smart Brain C++ Engine
========================================

[1/2] Compiling smart_brain.exe...
[2/2] Compiling myzip.exe...

========================================
SUCCESS! Built:
  bin/smart_brain.exe  (AI Knowledge Engine)
  bin/myzip.exe        (Compression Tool)
========================================
```

---

## Step 2: Test Command-Line Interface

### Test 1: Status (Empty Brain)

```bash
bin/smart_brain status
```

**Expected:**
```json
{
  "entries": 0,
  "total_original_mb": 0,
  "total_compressed_mb": 0,
  "average_ratio": 0,
  "savings_percent": 0,
  "specialized_mixers": 4
}
```

### Test 2: Learn from Web

```bash
bin/smart_brain learn https://en.wikipedia.org/wiki/Data_compression
```

**Expected:**
```
[LEARN] Topic: 'data_compression'
[FETCH] Downloading from web...
[FETCH] Downloaded 127543 bytes
[PARSE] Extracted 23456 bytes of clean text
[EMBED] Computing semantic vector...
[COMPRESS] Compressing with CMIX neural engine...
[SUCCESS] Compressed in 2.3s
  Original   : 23456 bytes
  Compressed : 2341 bytes (90.0% saved)
[BRAIN] Knowledge stored. Total entries: 1
```

### Test 3: Learn from File

Create a test file:
```bash
echo "Data compression is the process of encoding information using fewer bits." > test.txt
bin/smart_brain learn test.txt
```

**Expected:**
```
[LEARN] Topic: 'test'
[LOAD] Reading from file...
[LOAD] Read 78 bytes
[EMBED] Computing semantic vector...
[COMPRESS] Compressing with CMIX neural engine...
[SUCCESS] Compressed in 0.1s
  Original   : 78 bytes
  Compressed : 52 bytes (33.3% saved)
[BRAIN] Knowledge stored. Total entries: 2
```

### Test 4: Ask Question (High Confidence)

```bash
bin/smart_brain ask "What is data compression?"
```

**Expected:**
```json
{
  "confidence": 0.92,
  "source": "data_compression",
  "answer": "Data compression is the process of encoding information...",
  "compressed_size": 2341,
  "original_size": 23456
}
```

### Test 5: Ask Question (Low Confidence)

```bash
bin/smart_brain ask "What is quantum entanglement?"
```

**Expected:**
```json
{
  "confidence": 0.35,
  "action": "learn_from_web",
  "query": "What is quantum entanglement?"
}
```

### Test 6: Smart Compression

```bash
# Create test file
echo "int main() { return 0; }" > test.cpp

bin/smart_brain compress test.cpp
```

**Expected:**
```
[SMART COMPRESS] test.cpp
[COMPRESS] Using CMIX with persistent neural weights...
[SUCCESS] Compressed in 0.05s
  Original   : 26 bytes
  Compressed : 18 bytes (30.8% saved)
  Output     : test.cpp.myzip
```

### Test 7: Status (After Learning)

```bash
bin/smart_brain status
```

**Expected:**
```json
{
  "entries": 2,
  "total_original_mb": 0.023,
  "total_compressed_mb": 0.002,
  "average_ratio": 0.087,
  "savings_percent": 91.3,
  "specialized_mixers": 4
}
```

---

## Step 3: Test Python Server

### Start Server

```bash
cd server
python main_smart_brain.py
```

**Expected:**
```
============================================================
🧠 Smart Brain API Server
============================================================
C++ Engine: C:\Jay\_Plugin\compress\bin\smart_brain.exe
Exists: True
Knowledge entries: 2
Storage saved: 91.3%
============================================================
Ready at: http://127.0.0.1:8001
Docs at:  http://127.0.0.1:8001/docs
============================================================
```

### Test API: Learn

**Request:**
```bash
curl -X POST http://127.0.0.1:8001/api/learn \
  -H "Content-Type: application/json" \
  -d '{"source": "https://en.wikipedia.org/wiki/BWT"}'
```

**Expected Response:**
```json
{
  "status": "ok",
  "source": "https://en.wikipedia.org/wiki/BWT",
  "log": "[LEARN] Topic: 'bwt'\n[FETCH] Downloaded...\n[SUCCESS]..."
}
```

### Test API: Ask

**Request:**
```bash
curl -X POST http://127.0.0.1:8001/api/ask \
  -H "Content-Type: application/json" \
  -d '{"question": "What is data compression?"}'
```

**Expected Response:**
```json
{
  "confidence": 0.92,
  "source": "data_compression",
  "answer": "Data compression is the process...",
  "compressed_size": 2341,
  "original_size": 23456
}
```

### Test API: Status

**Request:**
```bash
curl http://127.0.0.1:8001/api/status
```

**Expected Response:**
```json
{
  "entries": 3,
  "total_original_mb": 0.045,
  "total_compressed_mb": 0.004,
  "average_ratio": 0.089,
  "savings_percent": 91.1,
  "specialized_mixers": 4
}
```

---

## Step 4: Test Desktop App Integration

### Update Desktop App to Use New API

Edit `desktop_app/src/components/HelperView.tsx`:

```typescript
// Add new API endpoint
const API = 'http://127.0.0.1:8001';

// Use new endpoints
const handleAsk = async () => {
  const resp = await fetch(`${API}/api/ask`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify({ question: input })
  });

  const data = await resp.json();

  if (data.confidence >= 0.7) {
    // High confidence - show answer
    addMessage('assistant', data.answer);
  } else {
    // Low confidence - learn from web
    await fetch(`${API}/api/learn`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ source: `https://en.wikipedia.org/wiki/${input}` })
    });

    // Retry after learning
    const retry = await fetch(`${API}/api/ask`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ question: input })
    });

    const retryData = await retry.json();
    addMessage('assistant', retryData.answer);
  }
};
```

---

## Step 5: Verify Brain Directory

Check that these files exist:

```
brain/
├── knowledge/
│   ├── data_compression.myzip      ✅
│   ├── test.myzip                  ✅
│   └── bwt.myzip                   ✅
│
├── index.bin                        ✅ (Vector index)
│
└── mixer_*.weights                  ✅ (4 files)
    ├── mixer_general.weights
    ├── mixer_text.weights
    ├── mixer_code.weights
    └── mixer_json.weights
```

---

## Step 6: Test Persistent Learning

### First Compression (Cold Start)

```bash
echo "The quick brown fox jumps over the lazy dog" > file1.txt
bin/smart_brain compress file1.txt
```

**Note the ratio (e.g., 65% saved)**

### Second Compression (Warm Start)

```bash
echo "The lazy cat sleeps under the warm sun" > file2.txt
bin/smart_brain compress file2.txt
```

**Expected: BETTER ratio (e.g., 72% saved)**

The neural network learned from file1 and applies that knowledge to file2!

---

## Step 7: Test File Type Detection

### Text File

```bash
echo "Lorem ipsum dolor sit amet" > test.txt
bin/smart_brain compress test.txt
```

**Expected log:**
```
[MIXER] Loaded 4 pre-trained weight sets
```
(Uses `mixer_text.weights`)

### Code File

```bash
echo "function test() { return 42; }" > test.js
bin/smart_brain compress test.js
```

**Expected:**
Uses `mixer_code.weights` → better ratio for code patterns

### JSON File

```bash
echo '{"name": "test", "value": 123}' > test.json
bin/smart_brain compress test.json
```

**Expected:**
Uses `mixer_json.weights` → better ratio for JSON structure

---

## Troubleshooting

### Build fails with "command not found"

**Solution:** Install GCC/Clang:
- Windows: Install MinGW-w64 or MSYS2
- Linux: `sudo apt install build-essential`
- Mac: `xcode-select --install`

### "Cannot open brain/index.bin"

**Solution:** This is normal on first run. The file will be created automatically.

### Web fetch fails

**Windows:** WinHTTP should work automatically.

**Linux/Mac:** Install curl:
```bash
sudo apt install curl       # Ubuntu/Debian
brew install curl           # Mac
```

### Compression is slow

**Expected:** CMIX is compute-intensive. First file is slowest (cold start).

Speeds:
- Small files (<1 MB): ~1-5s
- Medium files (1-10 MB): ~10-30s
- Large files (>10 MB): Use `--best` instead of `--cmix`

### "similarity: 0.0" for all queries

**Issue:** Dummy embeddings are being used.

**Solution:** Phase B.1 will integrate real neural_engine embeddings.

---

## Success Criteria

✅ Smart Brain builds without errors
✅ Can learn from URLs (Wikipedia works)
✅ Can learn from local files
✅ Queries return >0.5 confidence for learned topics
✅ Queries recommend "learn_from_web" for unknown topics
✅ Compression improves on 2nd file (persistent weights)
✅ File type detection works (text vs code vs JSON)
✅ Python server starts and responds to API calls
✅ Brain directory contains .myzip files and weights
✅ Status command shows correct statistics

---

**If all tests pass: 🎉 Smart Brain is working!**

Next steps:
1. Integrate real embeddings (neural_engine)
2. Add auto-learning to desktop app
3. Deploy and test with real workloads
