# Phase K: Scaling to Perfect LLM

**Current Status**: ✅ Phase J Complete - All 10 Tests Pass
**Current Quality**: Working but outputs gibberish (3M params, 400 lines training)
**Goal**: Transform from prototype → production-quality code LLM

---

## 🎯 What "Perfect LLM" Means

### Current State (Phase J Complete)
- ✅ Infrastructure works 100%
- ✅ Training pipeline functional
- ✅ Generation working
- ✅ Backend routing correct
- ⚠️ Output quality: Gibberish (expected)

### Target State (Phase K Goal)
- ✅ Generates valid Python code
- ✅ Handles 50+ common patterns
- ✅ Basic bug fixing capability
- ✅ Reasonable syntax accuracy
- ✅ Self-improves from feedback

**Realistic Timeline**: 2-4 weeks

---

## 📊 The Gap Analysis

### Why Current Output is Gibberish

| Aspect | Current | Needed | Gap |
|--------|---------|--------|-----|
| **Model Size** | 3M params | 50-100M params | **17-33x too small** |
| **Training Data** | 400 lines | 5,000-10,000 lines | **12-25x too little** |
| **Training Time** | 15 min | 2-4 hours | **8-16x too short** |
| **Vocab Size** | 282 tokens | 8,000-16,000 tokens | **28-57x too limited** |
| **Perplexity** | 16.5 | <5.0 | **3x too high** |

**Bottom Line**: You need ~20-50x more scale to get from "working" to "good"

---

## 🚀 Phase K: 4-Week Implementation Plan

### Week 1: Scale Training Data (Most Important!)

#### Task 1.1: Expand Code Corpus to 5,000+ Lines

**Current**: `brain/training/code_corpus.txt` (400 lines)
**Target**: `brain/training/large_code_corpus.txt` (5,000+ lines)

**Sources**:
1. **Python Standard Library Examples** (1,000 lines)
   - Copy from Python docs tutorials
   - `os`, `sys`, `collections`, `itertools` examples

2. **LeetCode/Algorithm Solutions** (1,500 lines)
   - Easy: 50 problems (array, string, math)
   - Medium: 50 problems (DP, trees, graphs)
   - Download from GitHub repositories

3. **Real GitHub Projects** (1,500 lines)
   - Flask hello world examples
   - Django snippets
   - Utility scripts (file processing, data parsing)

4. **Common Patterns** (1,000 lines)
   - Classes and OOP
   - Decorators
   - Context managers
   - Generators
   - Async/await
   - Error handling (try/except)
   - List comprehensions

**Action**:
```bash
# Start collecting code examples
mkdir brain/training/sources

# Create large corpus
type brain\training\sources\*.txt > brain\training\large_code_corpus.txt
```

#### Task 1.2: Format as Instruction Pairs

**Problem**: Model doesn't understand "Write a function..."

**Solution**: Train on instruction-response pairs

**Format**:
```
<|user|>Write a function to calculate fibonacci<|assistant|>
def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)
<|end|>

<|user|>Fix this code: def add(a,b) return a - b<|assistant|>
def add(a, b):
    return a + b
<|end|>

<|user|>Implement binary search<|assistant|>
def binary_search(arr, target):
    left, right = 0, len(arr) - 1
    while left <= right:
        mid = (left + right) // 2
        if arr[mid] == target:
            return mid
        elif arr[mid] < target:
            left = mid + 1
        else:
            right = mid - 1
    return -1
<|end|>
```

**Create**: `brain/training/instruction_corpus.txt`

---

### Week 2: Scale Model Architecture

#### Task 2.1: Increase Model Size to 50M Parameters

**File**: `src/neural_engine.cpp` (lines 1793-1806)

**Current Config**:
```cpp
config.vocab_size = 282;
config.embedding_dim = 256;
config.num_layers = 4;
config.num_heads = 4;
config.ff_dim = 1024;
config.max_seq_length = 128;
// Total: ~3M parameters
```

**New Config**:
```cpp
config.vocab_size = 8192;      // 29x larger vocab
config.embedding_dim = 512;    // 2x larger embeddings
config.num_layers = 8;         // 2x more layers
config.num_heads = 8;          // 2x more heads
config.ff_dim = 2048;          // 2x larger FF
config.max_seq_length = 256;   // 2x longer context
// Total: ~50M parameters
```

**Expected**:
- Memory: ~200 MB (still fits in RAM)
- Training time: 2-4 hours (longer but manageable)
- Quality: 10-20x better than current

#### Task 2.2: Retrain BPE Tokenizer

**Problem**: Current tokenizer only has 282 tokens (too small)

**Solution**: Retrain on larger code corpus

```bash
# Delete old tokenizer
del models\tokenizer.bin

# Train will create new tokenizer automatically
bin\neural_engine.exe train_transformer brain\training\large_code_corpus.txt 30 0.001 4
```

**Expected**: 8,000-10,000 tokens (covers more code patterns)

---

### Week 3: Intensive Training

#### Task 3.1: Long Training Run

**Command**:
```bash
bin\neural_engine.exe train_transformer brain\training\instruction_corpus.txt 50 0.0005 4
```

**Parameters**:
- Epochs: 50 (was 15)
- Learning rate: 0.0005 (smaller for stability)
- Batch size: 4 (larger model needs smaller batch)

**Expected**:
- Training time: 2-4 hours
- Final perplexity: <8.0 (target <5.0)
- Memory usage: 2-4 GB

**Monitor Progress**:
```bash
# Watch training in terminal
# Look for:
# - Perplexity decreasing
# - Loss decreasing
# - No NaN or Inf values
```

#### Task 3.2: Test Generation Quality

After every 10 epochs, test:
```bash
bin\neural_engine.exe transformer_generate "Write a fibonacci function"
```

**Quality Milestones**:
- Epoch 10: Should produce valid Python syntax
- Epoch 20: Should have correct indentation
- Epoch 30: Should handle basic functions correctly
- Epoch 40: Should fix simple bugs
- Epoch 50: Production quality for simple tasks

---

### Week 4: Fine-Tuning & Integration

#### Task 4.1: Collect User Feedback

**Process**:
1. Use the system daily for real coding tasks
2. Give 👍 to good generations
3. Give 👎 to bad generations
4. Collect 50-100 feedback examples

**Check**:
```bash
type brain\feedback\user_feedback.jsonl
# Should have 50-100 lines
```

#### Task 4.2: Fine-Tune on Feedback

```bash
python server\train_from_feedback.py
```

**This will**:
- Train on positive examples (SFT)
- Learn from negative examples (Wikipedia)
- Retrain transformer with corrections

#### Task 4.3: A/B Testing

**Test both models**:
1. Backup current model:
   ```bash
   copy models\transformer.bin models\transformer_v1_backup.bin
   ```

2. Train new model (saves to `transformer.bin`)

3. Compare quality:
   - Test 10 prompts with old model
   - Test same 10 prompts with new model
   - Keep the better one

---

## 📈 Expected Quality Improvements

### After Week 1 (More Data)
- **Perplexity**: 16.5 → 12.0
- **Output**: Still mostly gibberish, but occasional valid syntax
- **Improvement**: 25%

### After Week 2 (Bigger Model)
- **Perplexity**: 12.0 → 8.0
- **Output**: Valid Python syntax, wrong logic
- **Improvement**: 50% cumulative

### After Week 3 (Long Training)
- **Perplexity**: 8.0 → 5.5
- **Output**: Correct simple functions (fibonacci, factorial)
- **Improvement**: 75% cumulative

### After Week 4 (Fine-Tuning)
- **Perplexity**: 5.5 → 4.0
- **Output**: Production-quality simple code
- **Improvement**: 90% cumulative

---

## 🎯 Success Metrics

### Technical Metrics

| Metric | Current | Week 1 | Week 2 | Week 3 | Week 4 (Goal) |
|--------|---------|--------|--------|--------|---------------|
| Model Size | 3M | 3M | 50M | 50M | 50M |
| Training Data | 400 lines | 5,000 lines | 5,000 lines | 5,000 lines | 5,000 lines |
| Vocab Size | 282 | 282 | 8,192 | 8,192 | 8,192 |
| Perplexity | 16.5 | 12.0 | 8.0 | 5.5 | **4.0** |
| Training Time | 15 min | 30 min | 2 hours | 4 hours | 4 hours |

### Quality Metrics

| Task | Current | Week 1 | Week 2 | Week 3 | Week 4 (Goal) |
|------|---------|--------|--------|--------|---------------|
| Valid Syntax | 0% | 10% | 40% | 70% | **90%** |
| Correct Indentation | 0% | 20% | 60% | 85% | **95%** |
| Simple Functions (fibonacci) | 0% | 5% | 30% | 75% | **95%** |
| Bug Fixing (basic) | 0% | 0% | 10% | 40% | **70%** |
| Complex Functions | 0% | 0% | 0% | 10% | **30%** |

### User Experience Metrics

| Metric | Current | Goal |
|--------|---------|------|
| "Useful" Generations | 0/10 | 7/10 |
| Copy-Paste Ready | 0/10 | 6/10 |
| Needs Minor Edits | 1/10 | 8/10 |
| Completely Wrong | 9/10 | 2/10 |

---

## 💡 Quick Wins (Before Full Scale-Up)

While collecting more data, you can improve quality NOW:

### Quick Win 1: Few-Shot Prompting

**Modify**: `server/main.py` (line 1841)

```python
# Add examples to the prompt
few_shot_examples = """
Example 1:
User: Write a fibonacci function
Assistant: def fibonacci(n):
    if n <= 1:
        return n
    return fibonacci(n-1) + fibonacci(n-2)

Example 2:
User: Fix: def add(a,b) return a - b
Assistant: def add(a, b):
    return a + b

Now:
"""

enhanced_message = few_shot_examples + "User: " + req.message + "\nAssistant:"
cmd = [NEURAL_ENGINE_EXE, "transformer_generate", enhanced_message]
```

**Expected**: 2-3x better output immediately (no retraining!)

### Quick Win 2: Post-Processing

**Add**: Code cleanup after generation

```python
def cleanup_code(generated_code):
    """Fix common issues in generated code"""
    # Remove incomplete lines at the end
    lines = generated_code.split('\n')
    clean_lines = []
    for line in lines:
        if line.strip() and not line.endswith(('(', ',')):
            clean_lines.append(line)
        else:
            break  # Stop at incomplete line
    return '\n'.join(clean_lines)

# Use after generation
answer = cleanup_code(parsed.get("generated", ""))
```

**Expected**: Cleaner output, fewer broken functions

### Quick Win 3: Confidence Thresholding

**Add**: Only show generation if confident

```python
# After generation, check quality
if len(parsed.get("generated", "")) < 20:
    # Too short, likely failed
    fallback = "I need more training data to generate this. Try a simpler function?"
    response_text = json.dumps({
        "answer": fallback,
        "confidence": 30
    })
```

**Expected**: Better UX, fewer garbage outputs shown

---

## 🔧 Development Tools

### Tool 1: Training Monitor Script

**Create**: `monitor_training.py`

```python
import time
import subprocess

while True:
    # Check model quality every hour
    result = subprocess.run([
        'bin/neural_engine.exe',
        'transformer_generate',
        'Write a fibonacci function'
    ], capture_output=True, text=True)

    with open('training_log.txt', 'a') as f:
        f.write(f"{time.time()}: {result.stdout}\n")

    time.sleep(3600)  # 1 hour
```

### Tool 2: Quality Evaluator

**Create**: `evaluate_quality.py`

```python
test_prompts = [
    "Write a fibonacci function",
    "Write a factorial function",
    "Fix: def add(a,b) return a - b",
    "Reverse a string",
    "Binary search implementation"
]

for prompt in test_prompts:
    result = subprocess.run([...], capture_output=True)

    # Check if output is valid Python
    try:
        compile(result.stdout, '<string>', 'exec')
        print(f"✅ {prompt}: Valid syntax")
    except:
        print(f"❌ {prompt}: Syntax error")
```

### Tool 3: Auto-Retrain Script

**Create**: `auto_retrain.bat`

```batch
@echo off
:loop
echo [%date% %time%] Starting training cycle...
bin\neural_engine.exe train_transformer brain\training\large_code_corpus.txt 10 0.001 4

echo [%date% %time%] Testing quality...
bin\neural_engine.exe transformer_generate "Write a fibonacci function"

echo Sleeping 1 hour before next cycle...
timeout /t 3600 /nobreak

goto loop
```

---

## 📚 Learning Resources

### Data Collection
- **GitHub Code Search**: Search for "python examples" with <100 lines
- **Rosetta Code**: Algorithms in many languages
- **Python Docs**: Tutorial section has great examples
- **LeetCode Solutions**: Clean, well-documented code

### Training Techniques
- **Instruction Fine-Tuning**: Format matters (user/assistant tags)
- **Low-Rank Adaptation (LoRA)**: Efficient fine-tuning (future phase)
- **Curriculum Learning**: Train easy → hard
- **Data Augmentation**: Paraphrase prompts, vary styles

### Model Architecture
- **GPT-2 Paper**: Your architecture is based on this
- **Attention Is All You Need**: Transformer fundamentals
- **CodeBERT/CodeT5**: Code-specific models

---

## 🎯 Phase K Milestones

### Milestone 1: Data Collected (Week 1)
- [ ] 5,000+ lines of Python code collected
- [ ] Formatted as instruction pairs
- [ ] Corpus file created

### Milestone 2: Model Scaled (Week 2)
- [ ] 50M parameter config
- [ ] New tokenizer trained (8k+ vocab)
- [ ] Model compiles without errors

### Milestone 3: Quality Training (Week 3)
- [ ] 50-epoch training complete
- [ ] Perplexity <6.0
- [ ] Valid syntax 70%+ of the time

### Milestone 4: Production Ready (Week 4)
- [ ] Fine-tuned on user feedback
- [ ] Simple functions work 90%+
- [ ] UI shows useful code generations
- [ ] Users copy-paste code successfully

---

## 🚀 Start Phase K Now

### Immediate Next Steps

**Today**:
1. Start collecting code examples (aim for 100 lines/day)
2. Implement Quick Win 1 (few-shot prompting)
3. Test improved quality in UI

**This Week**:
1. Reach 1,000 lines of training data
2. Test incremental retraining
3. Measure quality improvements

**This Month**:
1. Complete 5,000-line corpus
2. Scale to 50M parameters
3. Achieve production quality

---

## 📊 Progress Tracking

**Current Status**:
```
Phase J: ████████████████████████ 100% COMPLETE ✅
Phase K: ░░░░░░░░░░░░░░░░░░░░░░░░   0% STARTING
```

**Phase K Breakdown**:
```
Week 1 (Data):    ░░░░░░ 0/6 tasks
Week 2 (Scale):   ░░░░░░ 0/6 tasks
Week 3 (Train):   ░░░░░░ 0/6 tasks
Week 4 (Polish):  ░░░░░░ 0/6 tasks
```

---

**Ready to start Phase K?** Begin with data collection - that's the #1 factor for quality! 🚀

Let me know when you want to tackle:
1. **Data Collection** (Week 1 tasks)
2. **Quick Wins** (improve quality now)
3. **Model Scaling** (Week 2 tasks)

Your choice! 💪
