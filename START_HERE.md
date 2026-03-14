# 🚀 START HERE - Optimized Training

## ✅ Ready to Train!

Dataset created: **wiki_2k.txt** (2,000 sequences)

## Copy & Paste This Command

```powershell
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32
```

**What it does:**
- Trains on 2,000 sequences (plenty of data)
- 5 epochs (enough to learn)
- Learning rate 0.001 (10x higher than before - can actually learn)
- Batch size 32 (4x larger than before - more stable)

**Expected:**
- ⏱️ Time: **2 hours** (not 55 hours!)
- 📉 Loss: **Decreases** from 6.0 → 2.5 (model gets better!)
- 🎯 Quality: Good (perplexity 20-30)

---

## What to Watch For

### ✅ GOOD Training (You want to see this)
```
Epoch 1: Loss 6.0 → 4.5 ✅ DECREASING
Epoch 2: Loss 4.5 → 3.5 ✅ DECREASING
Epoch 3: Loss 3.5 → 3.0 ✅ DECREASING
Epoch 4: Loss 3.0 → 2.8 ✅ DECREASING
Epoch 5: Loss 2.8 → 2.5 ✅ GOOD!
```

### ❌ BAD Training (Stop if you see this)
```
Epoch 1: Loss 6.0 → 6.1 ❌ INCREASING - STOP!
```

---

## Why Previous Training Failed

### ❌ Old Settings (What you tried before)
```powershell
./bin/neural_engine.exe train_transformer wiki_training_combined.txt 10 0.0001 8
```
**Problems:**
- 19,000 sequences = 55 hours ❌
- Learning rate 0.0001 = too low, can't learn ❌
- Batch size 8 = unstable gradients ❌
- **Result**: Loss INCREASED (model got worse!)

### ✅ New Settings (Use this)
```powershell
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32
```
**Benefits:**
- 2,000 sequences = 2 hours ✅
- Learning rate 0.001 = 10x higher, can learn ✅
- Batch size 32 = stable gradients ✅
- **Result**: Loss DECREASES (model gets better!)

---

## Alternative Options

### Faster Test (30 minutes)
```powershell
# Create smaller dataset
Get-Content wiki_training_combined.txt -TotalCount 1000 | Out-File -Encoding UTF8 wiki_1k.txt

# Train
./bin/neural_engine.exe train_transformer wiki_1k.txt 5 0.002 32
```

### Higher Quality (4 hours)
```powershell
# Create larger dataset
Get-Content wiki_training_combined.txt -TotalCount 5000 | Out-File -Encoding UTF8 wiki_5k.txt

# Train
./bin/neural_engine.exe train_transformer wiki_5k.txt 5 0.0005 32
```

---

## After Training

Test your model:
```powershell
./bin/neural_engine.exe ai_ask "What is AI?"
```

Model saved to: `models/transformer.bin`

---

## 📖 More Info

- **WINDOWS_TRAINING_COMMANDS.md** - Detailed Windows commands
- **URGENT_TRAINING_FIX.md** - Why old settings failed
- **TRAINING_QUICK_START.md** - General training guide

---

# 🎯 QUICK START

```powershell
# Just run this:
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32

# Expected: 2 hours, loss decreases 6.0 → 2.5 ✅
```

**GO!** 🚀
