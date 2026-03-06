# Get Training Data - Complete Guide

**Goal**: Get data to train your AI so it becomes intelligent!

---

## 🎯 Three Options (Choose Your Path)

### Option 1: Quick Test (READY NOW! ✅)

**What**: Use existing sample data (9,000 lines, 639 KB)

**Time**: 0 minutes (already created!)

**Quality**: Basic (good for testing system)

```bash
# File already created: wiki_training.txt
# Just start training NOW:
./bin/train_llm.exe --corpus wiki_training.txt --epochs 10 --batch 4

# Takes: ~10-15 minutes
# Result: Model becomes slightly smarter
```

**Pros**: Instant start, tests system works
**Cons**: Not enough for real intelligence

---

### Option 2: Sample Wikipedia (RECOMMENDED)

**What**: Download Simple English Wikipedia

**Time**: 10-30 minutes download + 5 minutes process

**Quality**: Good (enough for basic intelligence)

**Size**:
- Download: ~100 MB compressed
- Expanded: ~400 MB text
- Training time: 2-4 hours

```bash
# 1. Download (10-30 minutes)
python download_wikipedia.py small

# 2. Preprocess (5 minutes)
python preprocess_wiki.py simplewiki-latest-pages-articles.xml wiki_large.txt

# 3. Train (2-4 hours)
./bin/train_llm.exe --corpus wiki_large.txt --epochs 15 --batch 8

# Result: Decent intelligence!
```

---

### Option 3: Full Wikipedia (BEST QUALITY)

**What**: Download full English Wikipedia

**Time**: 2-4 hours download + 30 minutes process

**Quality**: Excellent (production quality)

**Size**:
- Download: ~20 GB compressed
- Expanded: ~80 GB text
- Training time: 2-3 days

```bash
# 1. Download (2-4 hours)
python download_wikipedia.py large

# WARNING: This is 20 GB! Make sure you have space!

# 2. Preprocess (30 minutes)
python preprocess_wiki.py enwiki-latest-pages-articles.xml wiki_full.txt

# 3. Train (2-3 days)
./bin/train_llm.exe --corpus wiki_full.txt --epochs 20 --batch 16

# Result: Excellent intelligence!
```

---

## 🚀 Recommended Path

### Day 1: Quick Test (NOW)

```bash
# Test with existing data (10 minutes)
./bin/train_llm.exe --corpus wiki_training.txt --epochs 5 --batch 4

# Verify:
# - System works ✅
# - Loss decreases ✅
# - No errors ✅
```

### Day 2: Download Small Wikipedia (Tonight)

```bash
# Download while you do other things (30 min)
python download_wikipedia.py small

# Preprocess when done (5 min)
python preprocess_wiki.py simplewiki-*.xml wiki_medium.txt

# Start training before bed (2-4 hours)
./bin/train_llm.exe --corpus wiki_medium.txt --epochs 15 --batch 8

# Wake up to smarter model!
```

### Day 3: Test Trained Model

```bash
# Create test script (we'll build this)
./bin/test_model model_trained.bin "What is AI?"

# Expected: Intelligent response! ✅
```

---

## 📊 Data Size Comparison

| Option | Size | Lines | Training Time | Intelligence Level |
|--------|------|-------|---------------|-------------------|
| **Quick Test** | 639 KB | 9K | 10 min | Basic (10%) |
| **Small Wiki** | 400 MB | 2M | 2-4 hours | Good (60%) |
| **Medium Wiki** | 4 GB | 20M | 12-24 hours | Very Good (80%) |
| **Large Wiki** | 80 GB | 400M | 2-3 days | Excellent (95%) |

---

## 💡 What Each Option Gets You

### Quick Test (wiki_training.txt):

```
Input: "What is AI?"
Output: "Artificial intelligence simulates human..." ✅ Basic
```

- Memorizes training data
- Basic sentence completion
- Good for testing system

### Small Wikipedia:

```
Input: "What is machine learning?"
Output: "Machine learning is a method of data analysis that automates
         analytical model building. It uses algorithms that learn from
         data to make predictions..." ✅ Good
```

- Understands common topics
- Coherent responses
- Production-ready for basic tasks

### Full Wikipedia:

```
Input: "Explain quantum computing"
Output: "Quantum computing uses quantum-mechanical phenomena such as
         superposition and entanglement to perform calculations. Unlike
         classical computers that use bits, quantum computers use qubits
         which can exist in multiple states simultaneously..." ✅ Excellent
```

- Deep understanding
- Complex reasoning
- Production-ready for advanced tasks

---

## 🔧 Troubleshooting

### Problem: Download Too Slow

**Solution**: Use smaller version first
```bash
python download_wikipedia.py small  # Instead of large
```

### Problem: Not Enough Disk Space

**Check space needed**:
```bash
# Small: 1 GB total
# Medium: 10 GB total
# Large: 100 GB total
```

**Solution**: Clean up space or use smaller version

### Problem: Preprocess Takes Forever

**Solution**: Process fewer articles
```bash
python preprocess_wiki.py input.xml output.txt --max-articles 100000
```

### Problem: Can't Download Wikipedia

**Alternative**: Use text files you already have!
```bash
# Any .txt files work! (books, articles, docs)
cat my_text1.txt my_text2.txt > training_corpus.txt

./bin/train_llm.exe --corpus training_corpus.txt --epochs 10
```

---

## 📋 Step-by-Step: Complete Workflow

### Step 1: Quick Test (Do This NOW!)

```bash
# Already have data: wiki_training.txt ✅

# Start training:
./bin/train_llm.exe --corpus wiki_training.txt --epochs 5 --batch 4

# Wait 10 minutes

# Check result:
# - Loss decreased? ✅
# - Checkpoints saved? ✅
# - No errors? ✅

# If all yes → System works! Continue to Step 2
```

### Step 2: Get Real Data (Do Tonight)

```bash
# Download Wikipedia (choose size):
python download_wikipedia.py small    # 30 min, good quality
# OR
python download_wikipedia.py medium   # 2 hours, better quality
# OR
python download_wikipedia.py large    # 4 hours, best quality

# Script will:
# 1. Download automatically
# 2. Extract automatically
# 3. Tell you next steps
```

### Step 3: Preprocess (After Download)

```bash
# Clean Wikipedia XML to text:
python preprocess_wiki.py <downloaded.xml> wiki_clean.txt

# This creates clean text file ready for training

# Check size:
ls -lh wiki_clean.txt
# Should see: 100 MB - 10 GB depending on what you downloaded
```

### Step 4: Train Overnight

```bash
# Start training before bed:
./bin/train_llm.exe --corpus wiki_clean.txt --epochs 20 --batch 8

# Go to sleep 😴

# Next morning: Model is SMART! ✅
```

### Step 5: Test Intelligence

```bash
# Load trained model
# Test responses
# Celebrate! 🎉
```

---

## ✅ Verification Checklist

### After Quick Test:

- [ ] `wiki_training.txt` exists (639 KB)
- [ ] Training ran without errors
- [ ] Loss decreased (e.g., 5.0 → 3.5)
- [ ] Checkpoint files created
- [ ] System works! ✅

### After Real Training:

- [ ] Wikipedia downloaded (100 MB - 20 GB)
- [ ] Preprocessed to clean text
- [ ] Training completed (2-48 hours)
- [ ] Final loss < 3.0
- [ ] Model gives intelligent responses
- [ ] SUCCESS! 🎉

---

## 🎯 Quick Commands Reference

```bash
# Create sample data (DONE ✅)
python preprocess_wiki.py --sample --sample-size 2000

# Download Wikipedia
python download_wikipedia.py small     # Recommended
python download_wikipedia.py medium    # Better
python download_wikipedia.py large     # Best

# Preprocess
python preprocess_wiki.py input.xml output.txt

# Train (quick test)
./bin/train_llm.exe --corpus wiki_training.txt --epochs 5 --batch 4

# Train (production)
./bin/train_llm.exe --corpus wiki_clean.txt --epochs 20 --batch 8

# Monitor training
# Watch for: Loss decreasing, no errors, checkpoints saving
```

---

## 💡 Pro Tips

### Tip 1: Start Small

Always test with small data first!
```bash
# Test → Small Wiki → Full Wiki
# Don't jump straight to full Wikipedia!
```

### Tip 2: Download Overnight

Large downloads take time:
```bash
# Start download before bed
python download_wikipedia.py large

# Wake up → data ready!
```

### Tip 3: Train on Weekend

Long training takes time:
```bash
# Friday night: Start training
# Monday morning: Smart model!
```

### Tip 4: Monitor Progress

Check training periodically:
```bash
# Loss should decrease:
# Epoch 1: 5.0
# Epoch 5: 3.5 ✅ Good!
# Epoch 10: 2.5 ✅ Excellent!
```

---

## 🎉 Success Criteria

### You Have Good Training Data When:

✅ File is > 10 MB (bigger = better)
✅ Contains varied topics
✅ Clean text (no HTML/XML)
✅ Many sentences (1000+)

### Training is Working When:

✅ Loss decreases each epoch
✅ No NaN or inf values
✅ Checkpoints save successfully
✅ Takes reasonable time

### Model is Smart When:

✅ Generates coherent sentences
✅ Answers questions reasonably
✅ No gibberish output
✅ Makes sense in context

---

## 📊 Timeline

| Task | Time | When to Do |
|------|------|------------|
| Quick test | 10 min | NOW! |
| Download small | 30 min | Tonight |
| Preprocess | 5 min | After download |
| Train small | 2-4 hours | Overnight |
| Test model | 10 min | Next morning |
| Download large | 4 hours | Weekend (optional) |
| Train large | 2-3 days | Weekend (optional) |

---

## 🚀 START NOW!

```bash
# You have data ready! Just run this:
./bin/train_llm.exe --corpus wiki_training.txt --epochs 5 --batch 4

# Takes 10 minutes
# Verifies system works
# Then get bigger data for better results!
```

---

**Bottom Line**:
1. Test now with `wiki_training.txt` (10 min)
2. Download Wikipedia tonight (30 min)
3. Train overnight (2-4 hours)
4. Wake up to SMART AI! 🎉
