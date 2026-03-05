# RLHF Training Guide - Reinforcement Learning from Human Feedback

**Neural Studio V10 - Phase F Complete**

---

## 🎯 Overview

**RLHF (Reinforcement Learning from Human Feedback)** is the training methodology used by Anthropic to create Claude. It consists of three phases:

1. **Phase F1: SFT (Supervised Fine-Tuning)** - Train on ideal {prompt, completion} pairs
2. **Phase F2: Reward Model** - Learn human preferences from comparisons
3. **Phase F3: PPO (Proximal Policy Optimization)** - Optimize policy to maximize reward

This guide explains how to use the C++ RLHF implementation in Neural Studio.

---

## 🚀 Quick Start

### **1. Create Sample Training Data**
```bash
neural_engine create_sample_rlhf_data
```

Output:
- `brain/training/sample_sft.json` - SFT training pairs
- `brain/training/sample_comparisons.json` - Preference comparisons
- `brain/training/sample_prompts.txt` - Prompts for PPO

### **2. Run SFT (Supervised Fine-Tuning)**
```bash
neural_engine sft brain/training/sample_sft.json 5 0.0005 4
```
- 5 epochs
- 0.0005 learning rate
- Batch size 4

### **3. Train Reward Model**
```bash
neural_engine train_reward_model brain/training/sample_comparisons.json models/reward_model.bin 10 0.001
```
- 10 epochs
- 0.001 learning rate
- Saves to `models/reward_model.bin`

### **4. Run PPO Training**
```bash
neural_engine ppo brain/training/sample_prompts.txt models/reward_model.bin 100 0.0001 0.2 0.01
```
- 100 iterations
- 0.0001 learning rate
- 0.2 clip epsilon
- 0.01 KL penalty

---

## 📋 Phase F1: Supervised Fine-Tuning (SFT)

### **What is SFT?**

SFT trains the model on **ideal responses** to specific prompts. This teaches the model:
- How to structure answers
- Domain-specific knowledge
- Conversation style
- Task-specific behaviors

### **Training Data Format**

**File**: JSON array of {prompt, completion} pairs

```json
[
  {
    "prompt": "What is the capital of France?",
    "completion": "The capital of France is Paris, a major European city known for its art, fashion, and culture. It's home to landmarks like the Eiffel Tower and the Louvre Museum."
  },
  {
    "prompt": "Explain quantum computing",
    "completion": "Quantum computing is a type of computation that harnesses quantum-mechanical phenomena like superposition and entanglement. Unlike classical computers that use bits (0 or 1), quantum computers use qubits that can exist in multiple states simultaneously, potentially enabling exponentially faster processing for certain problems."
  },
  {
    "prompt": "How do you make pasta?",
    "completion": "To make pasta: 1. Boil 4-6 quarts of water with 1 tablespoon of salt. 2. Add pasta and stir gently. 3. Cook for 8-12 minutes (check package for al dente timing). 4. Drain in a colander. 5. Serve with your favorite sauce."
  }
]
```

### **Command Syntax**

```bash
neural_engine sft <training_file> [epochs] [lr] [batch_size]
```

**Parameters**:
- `training_file` - Path to JSON file with SFT pairs
- `epochs` - Number of training epochs (default: 5)
- `lr` - Learning rate (default: 0.0005)
- `batch_size` - Batch size (default: 4)

**Example**:
```bash
# Basic training
neural_engine sft brain/training/sft_pairs.json

# Custom parameters
neural_engine sft brain/training/sft_pairs.json 10 0.001 8
```

### **Expected Output**

```
[RLHF] ========================================
[RLHF] Starting SFT (Supervised Fine-Tuning)
[RLHF] ========================================
[RLHF] Training file: brain/training/sft_pairs.json
[RLHF] Epochs: 5
[RLHF] Learning rate: 0.0005
[RLHF] Batch size: 4

[RLHF] Loaded 100 SFT training pairs
[RLHF] SFT Epoch 1/5 - Loss: 2.4532
[RLHF] SFT Epoch 2/5 - Loss: 1.8921
[RLHF] SFT Epoch 3/5 - Loss: 1.5432
[RLHF] SFT Epoch 4/5 - Loss: 1.3215
[RLHF] SFT Epoch 5/5 - Loss: 1.1843

[RLHF] SFT complete! Saving fine-tuned model...
{"success":true,"final_loss":1.1843}
```

### **How to Create SFT Data**

**Method 1: Manual Curation**
```json
[
  {
    "prompt": "Your question here",
    "completion": "Ideal answer that demonstrates good style, accuracy, and helpfulness"
  }
]
```

**Method 2: From Auto-Learning Corrections**
```bash
# Auto-learning creates corrections automatically
neural_engine auto_learn --daemon

# After running for 24 hours, extract corrections
python tools/extract_sft_from_corrections.py brain/self_learning/corrections.json brain/training/sft_pairs.json
```

**Method 3: From User Interactions**
```python
# Log good user interactions
good_interactions = [
    {"prompt": user_question, "completion": ai_response}
    for (user_question, ai_response, rating) in interactions
    if rating >= 4  # User rated 4+ stars
]

with open("brain/training/sft_pairs.json", "w") as f:
    json.dump(good_interactions, f, indent=2)
```

---

## 📊 Phase F2: Reward Model Training

### **What is a Reward Model?**

A reward model learns to **score responses** based on human preferences. It's trained on comparison data:
- Given a prompt and two responses (A and B)
- Humans pick which response is better
- Model learns to assign higher scores to preferred responses

### **Training Data Format**

**File**: JSON array of comparisons

```json
[
  {
    "prompt": "What is AI?",
    "response_a": "AI is artificial intelligence.",
    "response_b": "AI is when computers can think and learn like humans, using algorithms and data to perform tasks that typically require human intelligence.",
    "preferred": "b"
  },
  {
    "prompt": "How does a car work?",
    "response_a": "A car has an engine that makes it go.",
    "response_b": "A car works through internal combustion: fuel and air ignite in cylinders, creating force that moves pistons connected to a crankshaft, ultimately turning the wheels through a transmission system.",
    "preferred": "b"
  }
]
```

**Note**: `preferred` must be `"a"` or `"b"`.

### **Command Syntax**

```bash
neural_engine train_reward_model <comparisons_file> [output_path] [epochs] [lr]
```

**Parameters**:
- `comparisons_file` - Path to JSON comparisons file
- `output_path` - Where to save reward model (default: `models/reward_model.bin`)
- `epochs` - Training epochs (default: 10)
- `lr` - Learning rate (default: 0.001)

**Example**:
```bash
# Basic training
neural_engine train_reward_model brain/training/comparisons.json

# Custom parameters
neural_engine train_reward_model brain/training/comparisons.json models/my_reward_model.bin 20 0.002
```

### **Expected Output**

```
[RLHF] ========================================
[RLHF] Training Reward Model
[RLHF] ========================================
[RLHF] Comparisons file: brain/training/comparisons.json
[RLHF] Output model: models/reward_model.bin
[RLHF] Epochs: 10
[RLHF] Learning rate: 0.001

[RLHF] Loaded 50 reward comparisons
[RLHF] Reward model initialized (64 -> 128 -> 1)
[RLHF] Reward Model Epoch 1/10 - Loss: 0.6931
[RLHF] Reward Model Epoch 2/10 - Loss: 0.5432
[RLHF] Reward Model Epoch 4/10 - Loss: 0.3821
[RLHF] Reward Model Epoch 6/10 - Loss: 0.2654
[RLHF] Reward Model Epoch 8/10 - Loss: 0.1923
[RLHF] Reward Model Epoch 10/10 - Loss: 0.1432
[RLHF] Reward model training complete!
[RLHF] Reward model saved to models/reward_model.bin
{"success":true,"final_loss":0.1432,"model_path":"models/reward_model.bin"}
```

### **How to Create Comparison Data**

**Method 1: A/B Testing UI**

Create a UI where users compare two AI responses:

```typescript
// Desktop app component
function ResponseComparison({ prompt, responseA, responseB }) {
  const [preferred, setPreferred] = useState(null);

  const saveComparison = () => {
    fetch('/api/rlhf/comparison', {
      method: 'POST',
      body: JSON.stringify({
        prompt,
        response_a: responseA,
        response_b: responseB,
        preferred
      })
    });
  };

  return (
    <div>
      <h3>{prompt}</h3>
      <div className="responses">
        <div onClick={() => setPreferred('a')}>
          <h4>Response A</h4>
          <p>{responseA}</p>
        </div>
        <div onClick={() => setPreferred('b')}>
          <h4>Response B</h4>
          <p>{responseB}</p>
        </div>
      </div>
      <button onClick={saveComparison}>Submit</button>
    </div>
  );
}
```

**Method 2: Thumbs Up/Down**

```python
# When user rates responses
if user_rating == "thumbs_up":
    # Generate a weaker alternative
    weak_response = generate_weak_response(prompt)

    comparisons.append({
        "prompt": prompt,
        "response_a": weak_response,
        "response_b": ai_response,  # The good one user liked
        "preferred": "b"
    })

elif user_rating == "thumbs_down":
    # Generate a better alternative
    better_response = generate_better_response(prompt)

    comparisons.append({
        "prompt": prompt,
        "response_a": ai_response,  # The bad one user disliked
        "response_b": better_response,
        "preferred": "b"
    })
```

**Method 3: Use RewardModel from Python**

```python
# Generate multiple responses, score them, create comparisons
responses = [generate_response(prompt) for _ in range(5)]
scored = [(r, score_response(prompt, r)) for r in responses]
scored.sort(key=lambda x: x[1], reverse=True)

# Create comparisons: best vs worst, best vs 2nd worst, etc.
best = scored[0][0]
for i in range(len(scored) // 2, len(scored)):
    comparisons.append({
        "prompt": prompt,
        "response_a": scored[i][0],  # Worse response
        "response_b": best,           # Best response
        "preferred": "b"
    })
```

---

## 🎮 Phase F3: PPO (Proximal Policy Optimization)

### **What is PPO?**

PPO is the final alignment phase. It:
1. Generates responses with current policy (the model)
2. Scores them with the reward model
3. Updates policy to generate higher-reward responses
4. Prevents too-large updates (clip epsilon)
5. Adds KL penalty to stay close to original model

**Result**: Model learns to maximize reward while staying aligned with the original pre-trained model.

### **Training Data Format**

**File**: Plain text file with one prompt per line

```
Explain photosynthesis
What is the theory of relativity?
How do neural networks work?
What causes climate change?
Describe the French Revolution
```

### **Command Syntax**

```bash
neural_engine ppo <prompts_file> <reward_model_path> [iterations] [lr] [clip_eps] [kl_penalty]
```

**Parameters**:
- `prompts_file` - Text file with prompts (one per line)
- `reward_model_path` - Path to trained reward model
- `iterations` - Number of PPO iterations (default: 100)
- `lr` - Learning rate (default: 0.0001)
- `clip_eps` - PPO clip epsilon (default: 0.2)
- `kl_penalty` - KL divergence penalty (default: 0.01)

**Example**:
```bash
# Basic training
neural_engine ppo brain/training/prompts.txt models/reward_model.bin

# Custom parameters
neural_engine ppo brain/training/prompts.txt models/reward_model.bin 200 0.0002 0.1 0.02
```

### **Expected Output**

```
[RLHF] ========================================
[RLHF] Starting PPO Training
[RLHF] ========================================
[RLHF] Prompts file: brain/training/prompts.txt
[RLHF] Reward model: models/reward_model.bin
[RLHF] Iterations: 100
[RLHF] Learning rate: 0.0001
[RLHF] Clip epsilon: 0.2
[RLHF] KL penalty: 0.01

[RLHF] Loaded 50 prompts
[RLHF] Reward model loaded from models/reward_model.bin
[RLHF] PPO Iteration 1/100 - Avg Reward: 12.5432
[RLHF] PPO Iteration 10/100 - Avg Reward: 15.8921
[RLHF] PPO Iteration 20/100 - Avg Reward: 18.3215
[RLHF] PPO Iteration 30/100 - Avg Reward: 21.1843
...
[RLHF] PPO Iteration 100/100 - Avg Reward: 35.6754

[RLHF] PPO training complete!
{"success":true,"final_reward":35.6754}
```

---

## 🔄 Full RLHF Pipeline

### **Step-by-Step Workflow**

```bash
# 1. Create sample data (for testing)
neural_engine create_sample_rlhf_data

# 2. Run SFT (teach model ideal responses)
neural_engine sft brain/training/sample_sft.json 5 0.0005 4

# 3. Train reward model (learn preferences)
neural_engine train_reward_model brain/training/sample_comparisons.json models/reward_model.bin 10 0.001

# 4. Run PPO (align model to maximize reward)
neural_engine ppo brain/training/sample_prompts.txt models/reward_model.bin 100 0.0001 0.2 0.01

# 5. Test the aligned model
neural_engine ai_ask "Explain quantum computing"
```

### **Real-World Pipeline (Production)**

```bash
# 1. Collect training data from users (over days/weeks)
# - Log good interactions → sft_pairs.json
# - Collect A/B comparisons → comparisons.json
# - Extract common user questions → prompts.txt

# 2. Run SFT with real data
neural_engine sft brain/training/sft_pairs.json 10 0.0005 8

# 3. Train reward model on user preferences
neural_engine train_reward_model brain/training/comparisons.json models/reward_model.bin 20 0.001

# 4. Run PPO for 500-1000 iterations
neural_engine ppo brain/training/prompts.txt models/reward_model.bin 1000 0.0001 0.2 0.01

# 5. Deploy aligned model
# (Model is now saved and will be loaded by ai_ask command)
```

---

## 📈 Expected Results

### **Before RLHF**
```
Q: "What is quantum computing?"
A: "It's a type of computer."
Score: 35/100
```

### **After SFT (Phase F1)**
```
Q: "What is quantum computing?"
A: "Quantum computing uses quantum-mechanical phenomena like superposition and entanglement to perform computations."
Score: 68/100
```

### **After SFT + Reward Model + PPO (Full RLHF)**
```
Q: "What is quantum computing?"
A: "**Quantum Computing**: Unlike classical computers that use bits (0 or 1), quantum computers use qubits that can exist in multiple states simultaneously (superposition). This enables:

1. **Exponential speedup** for certain problems (factorization, optimization)
2. **Quantum entanglement** allows qubits to be correlated across distances
3. **Applications**: Cryptography, drug discovery, AI, financial modeling

Current systems (IBM Q, Google Sycamore) have 50-100 qubits but face decoherence challenges. Use `learn quantum_computing: [text]` to expand. Note: technology is still in early stages."

Score: 94/100
```

**Improvements**:
- ✅ Clear structure (numbered points)
- ✅ Specific examples
- ✅ Actionable suggestions
- ✅ Honest about limitations
- ✅ Appropriate length
- ✅ Domain knowledge demonstrated

---

## 🔧 Troubleshooting

### **Problem**: SFT loss not decreasing

**Cause**: Learning rate too low or batch size too small

**Solution**:
```bash
# Increase learning rate and batch size
neural_engine sft brain/training/sft_pairs.json 10 0.001 16
```

---

### **Problem**: Reward model loss stagnates

**Cause**: Not enough comparison data or preferences are too similar

**Solution**:
- Collect more diverse comparisons (100+ minimum)
- Ensure clear quality differences between response A and B
- Balance preferences (not all "a" or all "b")

---

### **Problem**: PPO reward not increasing

**Cause**: Clip epsilon too small or KL penalty too high

**Solution**:
```bash
# Loosen constraints
neural_engine ppo prompts.txt reward_model.bin 100 0.0001 0.3 0.005
```

---

### **Problem**: Model quality degrades after PPO

**Cause**: Over-optimization (model gaming the reward model)

**Solution**:
- Increase KL penalty (keeps model close to original)
- Reduce iterations
- Use better reward model (train on more comparisons)

---

## 📚 References

- **Anthropic Claude Training**: This RLHF implementation is based on Anthropic's published methodology
- **PPO Paper**: "Proximal Policy Optimization Algorithms" (Schulman et al., 2017)
- **Bradley-Terry Model**: Used for reward model training from preferences
- **Constitutional AI**: See [docs/ARCHITECTURE.md](ARCHITECTURE.md) for CAI integration

---

## 🚀 Next Steps

1. **Integrate with Auto-Learning**: Auto-learning can generate SFT pairs and comparisons automatically
2. **Add Desktop UI**: Create comparison UI in desktop app for easy human feedback
3. **Continuous RLHF**: Run RLHF pipeline weekly with accumulated user data
4. **Multi-Agent Debate**: Use multiple models to generate diverse responses for comparisons

---

**Last Updated**: 2026-03-01
**Version**: 1.0 (Phase F Complete)
**Author**: Neural Studio V10 Team
