# Testing Guide - Neural Studio AI System

## Quick Start Testing

### Step 1: Build the System

```bash
cd c:\Jay\_Plugin\compress
build_unified.bat
```

**Expected**: `bin\neural_engine.exe` created successfully

### Step 2: Run Test Script

```bash
test_ai_system.bat
```

**Expected**: All 6 checks pass

### Step 3: Start Python Server

```bash
cd server
python main.py
```

**Expected Output**:
```
  +----------------------------------------------------+
  |  Neural Studio V10 Server (SMART BRAIN)          |
  |  Port: 8001 | Brain: ACTIVE                       |
  +----------------------------------------------------+

INFO:     Started server process
INFO:     Uvicorn running on http://127.0.0.1:8001
```

### Step 4: Start Desktop App (New Terminal)

```bash
cd desktop_app
npm run dev
```

**Expected Output**:
```
  VITE v4.x.x  ready in 1234 ms

  ➜  Local:   http://localhost:5173/
  ➜  Network: use --host to expose
```

### Step 5: Open in Chrome

Navigate to: **http://localhost:5173**

---

## AI Feature Testing Checklist

### ✅ Test 1: AI Chat Basic Functionality

**Steps**:
1. Click the **🧠 Brain icon** in the Activity Bar (or press Ctrl+Shift+A if mapped)
2. Wait for AI Chat panel to open
3. Type: "What is compression?"
4. Press Enter

**Expected Result**:
- AI responds within 5 seconds
- Response contains relevant information about compression
- Response is not an error message

**Screenshot Verification**:
- ✅ AI Copilot panel visible
- ✅ User message in chat
- ✅ AI response visible
- ✅ Connection status shows "Connected to Neural Engine" (green dot)

---

### ✅ Test 2: User Feedback Buttons (👍👎)

**Steps**:
1. In AI Chat, ensure you have at least one AI response
2. Look for 👍 and 👎 buttons below the AI response
3. Click **👍** (thumbs up)
4. Observe button highlight
5. Ask another question
6. Click **👎** (thumbs down) on the new response

**Expected Result**:
- 👍 button turns **green** when clicked
- 👎 button turns **red** when clicked
- Buttons are **gray** by default
- Only one feedback can be selected per message

**Verify Backend**:
```bash
# Check feedback file was created
type brain\feedback\user_feedback.jsonl
```

Expected: JSON lines with your feedback

---

### ✅ Test 3: AI Training Stats Dashboard

**Steps**:
1. Click the **TrendingUp icon** in the Activity Bar
2. Wait for dashboard to load
3. Verify all metrics display

**Expected Metrics**:
- **Overall AI Score**: Number between 0-100% (e.g., 74%)
- **Knowledge Items**: Number > 0
- **Weak Responses**: Number (may be 0)
- **Corrections Made**: Number (may be 0)
- **Training Runs**: Number (may be 0)
- **RLHF Iterations**: Number (may be 0)
- **Advanced Reasoning**: Number (may be 0)

**Test Auto-Refresh**:
1. Toggle "Auto-refresh" button OFF
2. Verify icon stops spinning
3. Toggle back ON
4. Verify stats update every 5 seconds

---

### ✅ Test 4: Self-Learning Settings

**Steps**:
1. Press **Ctrl+,** to open Settings
2. Click on **"Self-Learning AI"** in the left sidebar
3. Verify all 8 settings are visible:
   - ✅ Enable Auto-Learning (toggle)
   - ✅ Weak Response Threshold (number)
   - ✅ Training Threshold (number)
   - ✅ Monitor Interval (number)
   - ✅ Constitutional AI Critique (toggle)
   - ✅ Web Learning (toggle)
   - ✅ Reasoning Mode (dropdown)
   - ✅ Show Training Stats (toggle)

**Test Setting Changes**:
1. Toggle "Enable Auto-Learning" OFF, then ON
2. Change "Weak Response Threshold" to 70
3. Close Settings
4. Reopen Settings (Ctrl+,)
5. Verify changes persisted

---

### ✅ Test 5: Backend API Endpoints

**Test in Chrome (New Tab)**:

#### 5a. Health Check
- URL: **http://127.0.0.1:8001/api/health**
- Expected: `{"status": "online", "timestamp": "..."}`

#### 5b. API Documentation
- URL: **http://127.0.0.1:8001/docs**
- Expected: FastAPI Swagger UI with all endpoints listed

#### 5c. Brain Stats
- URL: **http://127.0.0.1:8001/api/brain/stats**
- Expected: JSON with `ai_score`, `knowledge_items`, etc.

#### 5d. Chat Endpoint (Test with Postman/curl)
```bash
curl -X POST http://127.0.0.1:8001/api/chat \
  -H "Content-Type: application/json" \
  -d "{\"message\": \"What is AI?\", \"history\": [], \"web_search\": false}"
```

Expected: JSON response with `{"response": "...", "tool": "...", "status": "ok"}`

---

### ✅ Test 6: Feedback Collection Pipeline

**Steps**:
1. Give 3 positive (👍) feedback in AI Chat
2. Give 2 negative (👎) feedback in AI Chat
3. Check files created:

```bash
# Check raw feedback
type brain\feedback\user_feedback.jsonl

# Check positive feedback SFT pairs
type brain\training\sft_pairs_feedback.json

# Check negative feedback corrections
type brain\self_learning\manual_corrections.json
```

**Expected**:
- `user_feedback.jsonl`: 5 lines (3 positive + 2 negative)
- `sft_pairs_feedback.json`: 3 entries (positive only)
- `manual_corrections.json`: 2 entries (negative only)

---

### ✅ Test 7: Auto-Training from Feedback

**Prerequisites**: Complete Test 6 (have feedback data)

**Steps**:
```bash
cd c:\Jay\_Plugin\compress
python server\train_from_feedback.py
```

**Expected Output**:
```
==========================================================
AUTO-TRAINING FROM USER FEEDBACK (Phase I1)
==========================================================

Total feedback entries: 5
  Positive (👍): 3
  Negative (👎): 2

=== Training on 3 Positive Examples ===
Created SFT training file: brain\training\sft_from_feedback.json
Running SFT training...
✓ SFT training completed successfully!

=== Improving 2 Negative Examples ===
[Negative Feedback] Q: ...
  Learning from Wikipedia: ...
  ✓ Learned from: https://en.wikipedia.org/wiki/...
  Improved Answer: ...

✓ Saved 2 corrections to: brain\self_learning\feedback_corrections.json

=== Retraining Transformer ===
Training transformer (7 epochs, LR=0.002, batch=16)...
✓ Transformer training completed!

✓ Archived processed feedback to: brain\feedback\archive\feedback_20260301_143000.jsonl

==========================================================
✓ TRAINING FROM FEEDBACK COMPLETE!
==========================================================
```

---

### ✅ Test 8: Advanced Reasoning (If C++ Commands Exist)

**Test Tree-of-Thought**:
```bash
bin\neural_engine.exe tree_of_thought "How to reduce traffic congestion?" 3 2
```

Expected: JSON output with reasoning paths

**Test Multi-Agent Debate**:
```bash
bin\neural_engine.exe debate "Is nuclear energy safe?" 3 2
```

Expected: JSON output with agent responses and consensus

**Test Self-Reflection**:
```bash
bin\neural_engine.exe self_reflect "Explain photosynthesis" 80 3
```

Expected: JSON output with improved response and iterations

---

## Common Issues & Solutions

### Issue 1: "neural_engine.exe not found"
**Solution**:
```bash
cd c:\Jay\_Plugin\compress
build_unified.bat
```

### Issue 2: "Python server won't start"
**Solution**:
```bash
# Install dependencies
pip install fastapi uvicorn pydantic

# Check Python version (needs 3.8+)
python --version
```

### Issue 3: "Desktop app won't start"
**Solution**:
```bash
cd desktop_app
npm install
npm run dev
```

### Issue 4: "AI responses are errors"
**Check**:
1. Is Python server running? (http://127.0.0.1:8001/api/health)
2. Is neural_engine.exe working? (`bin\neural_engine.exe --help`)
3. Check server console for errors

### Issue 5: "Feedback buttons don't appear"
**Solution**:
1. Open Settings (Ctrl+,)
2. Go to "AI Copilot"
3. Enable "Collect User Feedback"
4. Refresh AI Chat panel

### Issue 6: "Stats dashboard shows all zeros"
**Solution**:
- This is normal for first run
- Give some feedback (👍👎)
- Run `python server\train_from_feedback.py`
- Stats will update after training

### Issue 7: "CORS errors in browser console"
**Solution**:
- Restart Python server
- Check server is on port 8001
- Check desktop app is on port 5173

---

## Performance Benchmarks

### Expected Response Times

| Action | Expected Time | Status |
|--------|---------------|--------|
| AI Chat Response | < 5 seconds | ✅ |
| Feedback Button Click | < 200ms | ✅ |
| Stats Dashboard Load | < 1 second | ✅ |
| Stats Auto-Refresh | 5 seconds interval | ✅ |
| Settings Save | Instant | ✅ |
| Auto-Training Script | 2-10 minutes | ✅ |

### Expected AI Quality

| Metric | Baseline | Target |
|--------|----------|--------|
| AI Score | 74% | 95%+ |
| Knowledge Items | 612 | 1000+ |
| Response Relevance | Good | Excellent |
| Feedback Accuracy | N/A | User-driven |

---

## Full System Integration Test

**Complete Test Scenario**:

1. **Start servers** (Python + Desktop)
2. **Open Chrome** → http://localhost:5173
3. **Open AI Chat** (🧠 icon)
4. **Ask**: "What is quantum computing?"
5. **Verify**: AI responds with relevant information
6. **Click** 👎 (negative feedback)
7. **Ask**: "Explain more about quantum computing"
8. **Verify**: AI tries to provide better answer
9. **Click** 👍 (positive feedback)
10. **Open AI Stats** (TrendingUp icon)
11. **Verify**: Metrics updated (may need refresh)
12. **Run training**: `python server\train_from_feedback.py`
13. **Verify**: Training completes successfully
14. **Ask again**: "What is quantum computing?"
15. **Verify**: AI gives improved answer (after learning from Wikipedia)
16. **Click** 👍 on improved answer

**Success Criteria**:
- ✅ All 16 steps complete without errors
- ✅ AI response improved from step 6 to step 15
- ✅ Feedback collected and processed
- ✅ Stats dashboard shows updated metrics
- ✅ Training script runs successfully

---

## Screenshot Checklist

### Required Screenshots for Verification

1. **AI Chat Panel**:
   - ✅ User message
   - ✅ AI response
   - ✅ Thumbs up/down buttons
   - ✅ Timestamp
   - ✅ Connection status (green dot)

2. **AI Training Stats**:
   - ✅ Overall AI Score with grade (A+/A/B/C/D)
   - ✅ All 6 metric cards
   - ✅ Auto-refresh toggle
   - ✅ Action buttons

3. **Settings Panel**:
   - ✅ Self-Learning AI section
   - ✅ All 8 settings visible
   - ✅ AI Copilot section with feedback toggle

4. **Backend API**:
   - ✅ FastAPI docs page (http://127.0.0.1:8001/docs)
   - ✅ Health check response
   - ✅ Brain stats JSON

---

## Final Checklist

Before declaring "AI is working perfectly":

- [ ] ✅ Build successful (`build_unified.bat`)
- [ ] ✅ Python server starts without errors
- [ ] ✅ Desktop app loads in Chrome
- [ ] ✅ AI Chat responds to questions
- [ ] ✅ Feedback buttons (👍👎) appear and work
- [ ] ✅ Feedback stored in `brain/feedback/user_feedback.jsonl`
- [ ] ✅ AI Stats dashboard loads with metrics
- [ ] ✅ Auto-refresh works (stats update every 5s)
- [ ] ✅ Settings panel shows all self-learning options
- [ ] ✅ Backend API endpoints respond (health, stats, chat)
- [ ] ✅ Auto-training script runs successfully
- [ ] ✅ AI improves after training (test before/after)

**ALL CHECKBOXES MUST BE CHECKED FOR "PERFECT WORKING" STATUS**

---

## Quick Validation Commands

```bash
# Quick test all components
cd c:\Jay\_Plugin\compress

# 1. Test neural_engine
bin\neural_engine.exe ai_ask "Hello"

# 2. Test Python server (in separate terminal)
cd server && python main.py

# 3. Test desktop app (in separate terminal)
cd desktop_app && npm run dev

# 4. Test feedback collection (after giving feedback in UI)
type brain\feedback\user_feedback.jsonl

# 5. Test training
python server\train_from_feedback.py --positive-only

# 6. Test stats API
curl http://127.0.0.1:8001/api/brain/stats
```

---

**Ready to Test!** Follow this guide step-by-step to verify everything is working perfectly. 🚀
