# Windows Training Commands (PowerShell)

## Quick Start - Optimized Training

### ✅ Dataset Already Created!
- `wiki_2k.txt` - 2,000 sequences (READY TO USE)

### Start Training Now (2 hours)

```powershell
# In PowerShell - RECOMMENDED SETTINGS
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32
```

**Expected:**
- Time: ~2 hours
- Loss should DECREASE each epoch (6.0 → 4.5 → 3.5 → 3.0 → 2.5)
- Final perplexity: 20-30 (good quality)

---

## Alternative Dataset Sizes

### Ultra-Fast (30 minutes)
```powershell
# Create 1,000 sequence dataset
Get-Content wiki_training_combined.txt -TotalCount 1000 | Out-File -Encoding UTF8 wiki_1k.txt

# Train (30 minutes)
./bin/neural_engine.exe train_transformer wiki_1k.txt 5 0.002 32
```

### High-Quality (4 hours)
```powershell
# Create 5,000 sequence dataset
Get-Content wiki_training_combined.txt -TotalCount 5000 | Out-File -Encoding UTF8 wiki_5k.txt

# Train (4 hours)
./bin/neural_engine.exe train_transformer wiki_5k.txt 5 0.0005 32
```

---

## Using the PowerShell Script

```powershell
# Create 2,000 sequence dataset
./Create-TrainingDataset.ps1 -Sequences 2000

# Create 1,000 sequence dataset
./Create-TrainingDataset.ps1 -Sequences 1000

# Create 5,000 sequence dataset
./Create-TrainingDataset.ps1 -Sequences 5000
```

---

## What to Watch For

### ✅ Good Training (Loss Decreases)
```
Epoch 1/5: Loss 6.0 → 4.5 ✅
Epoch 2/5: Loss 4.5 → 3.5 ✅
Epoch 3/5: Loss 3.5 → 3.0 ✅
Epoch 4/5: Loss 3.0 → 2.8 ✅
Epoch 5/5: Loss 2.8 → 2.5 ✅
```

### ❌ Bad Training (Loss Increases or Stays Same)
```
Epoch 1/5: Loss 6.0 → 6.1 ❌ (GETTING WORSE!)
Epoch 2/5: Loss 6.1 → 6.2 ❌ (STOP TRAINING)
```

If loss increases:
- Learning rate is wrong
- Gradients are exploding
- **Solution**: Use the recommended settings above

---

## Training Time Estimates

| Dataset | Sequences | Epochs | Batch | Time |
|---------|-----------|--------|-------|------|
| Ultra-fast | 1,000 | 5 | 32 | 30 min ✅ |
| **Recommended** | **2,000** | **5** | **32** | **2 hr** ✅ |
| High-quality | 5,000 | 5 | 32 | 4 hr ✅ |
| Full (NOT recommended) | 19,000 | 10 | 8 | 55 hr ❌ |

---

## Why Previous Settings Failed

### ❌ Old Settings (55 hours, loss increases)
```powershell
./bin/neural_engine.exe train_transformer wiki_training_combined.txt 10 0.0001 8
```
- 19,000 sequences (too many for CPU)
- Learning rate 0.0001 (too low - can't learn)
- Batch size 8 (too small - unstable)
- **Result**: 55 hours, model gets WORSE

### ✅ New Settings (2 hours, loss decreases)
```powershell
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32
```
- 2,000 sequences (plenty of data)
- Learning rate 0.001 (10x higher - can learn)
- Batch size 32 (4x larger - stable)
- **Result**: 2 hours, model gets BETTER

---

## Ready-to-Run Command

```powershell
# Copy and paste this (dataset already created):
./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32
```

**Watch for**: Loss should go from ~6.0 down to ~2.5-3.0 over 5 epochs ✅

---

## Monitoring Progress

Training shows progress every 10 batches:
```
[Batch 10 | 16%] Loss: 4.52 | Perplexity: 91.8 | Sequences: 320/2000
```

Good training looks like:
- Loss: 6.0 → 5.5 → 5.0 → 4.5 → 4.0 → 3.5 → 3.0 (DECREASING ✅)
- Perplexity: 400 → 200 → 100 → 50 → 30 → 20 (DECREASING ✅)

Bad training looks like:
- Loss: 6.0 → 6.1 → 6.2 → 6.3 (INCREASING ❌ - STOP!)

---

## After Training

Test your model:
```powershell
# Ask a question
./bin/neural_engine.exe ai_ask "What is machine learning?"

# Generate text
./bin/neural_engine.exe ai_ask "Explain quantum computing"
```

Model is saved automatically to `models/transformer.bin`

---

**Quick Start**: `./bin/neural_engine.exe train_transformer wiki_2k.txt 5 0.001 32`
**Expected**: 2 hours, loss decreases to ~2.5-3.0 ✅
