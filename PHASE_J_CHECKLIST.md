# Phase J: Code Generation - Complete Checklist

**Goal**: Get your own transformer generating code in the UI

---

## ✅ Step-by-Step Guide

### Step 1: Restart the System

**Why**: Apply the backend routing fixes

**Commands**:
```bash
# 1. Stop everything
STOP_ALL.bat

# 2. Wait 5 seconds for processes to fully stop

# 3. Start everything fresh
START_ALL.bat

# 4. Wait ~15 seconds for both servers to start
# 5. Chrome should open automatically to http://localhost:5173
```

**Verify**:
- ✅ "Neural Engine Server" terminal window opens
- ✅ "Desktop App" terminal window opens
- ✅ Chrome opens to localhost:5173
- ✅ No error messages in terminals

---

### Step 2: Test Debug Endpoint (Verify Routing)

**Why**: Confirm keyword detection works before testing full flow

**Open new Command Prompt**:
```bash
cd c:\Jay\_Plugin\compress

curl -X POST http://127.0.0.1:8001/api/debug/code-detection ^
  -H "Content-Type: application/json" ^
  -d "{\"message\": \"Write a fibonacci function\", \"history\": []}"
```

**Expected Output**:
```json
{
  "message": "Write a fibonacci function",
  "is_code_request": true,
  "detected_keywords": ["write", "function"],
  "would_use_tool": "transformer_generate"
}
```

**If you see this** ✅ → Routing is working! Continue to Step 3.

**If you don't see this** ❌:
- Check server is running (http://127.0.0.1:8001/api/health)
- Restart again (STOP_ALL.bat → START_ALL.bat)
- Check "Neural Engine Server" terminal for errors

---

### Step 3: Test Backend API Directly

**Why**: Test routing with actual transformer call

**Command**:
```bash
curl -X POST http://127.0.0.1:8001/api/chat ^
  -H "Content-Type: application/json" ^
  -d "{\"message\": \"Write a fibonacci function\", \"history\": [], \"web_search\": false}"
```

**Expected Output** (look for these key fields):
```json
{
  "response": "{...\"tool\":\"transformer_generate\"...}",
  "tool": "transformer_generate",  ← This should say transformer_generate!
  "status": "ok"
}
```

**Check Server Terminal**:
You should see these logs:
```
[CODE GENERATION] Detected code request: Write a fibonacci function...
[CODE GENERATION] Raw output length: 450
[CODE GENERATION] Parsed transformer output successfully
[CODE GENERATION] Returning response with tool: transformer_generate
```

**If you see these** ✅ → Backend is routing correctly! Continue to Step 4.

**If tool says "neural_engine" instead** ❌:
- Server didn't reload properly
- Close server terminal manually (Ctrl+C)
- Run STOP_ALL.bat again
- Run START_ALL.bat again

---

### Step 4: Test in UI (The Real Test!)

**Steps**:
1. Go to Chrome (http://localhost:5173)
2. Click **🧠 Brain icon** in left sidebar
3. Type: **"Write a fibonacci function"**
4. Press **Enter**
5. Wait 5-10 seconds

**Expected Result** ✅:
- AI responds (even if output is gibberish)
- Response appears in chat
- 👍👎 buttons appear below response
- No error message like "I don't have knowledge..."

**What to Check**:
1. **In Browser DevTools** (Press F12):
   - Go to Network tab
   - Find the `chat` request
   - Look at Response → should see `"tool":"transformer_generate"`

2. **In Server Terminal**:
   - Should see `[CODE GENERATION]` log messages
   - Should show it's calling transformer

**If it works** ✅ → Phase J is COMPLETE! 🎉

**If you see "I don't have knowledge..."** ❌:
- Backend routing still broken
- Go back to Step 1 (restart again)
- Check server logs for errors

---

### Step 5: Test Different Code Keywords

**Try these prompts in UI**:

1. `"Generate a factorial function"`
2. `"Create a function to reverse a string"`
3. `"Fix this code: def add(a,b) return a - b"`
4. `"Implement binary search"`
5. `"Debug this function"`

**All should trigger transformer** ✅

---

### Step 6: Test Non-Code (Should NOT Use Transformer)

**Try these prompts**:

1. `"What is compression?"`
2. `"Explain BWT algorithm"`
3. `"How does CMIX work?"`

**These should use ai_ask** (not transformer) ✅

**Check**: Network tab should show `"tool":"neural_engine"`

---

### Step 7: Test Feedback Loop

**Steps**:
1. Ask a code question
2. Get response (gibberish is OK)
3. Click **👍** or **👎**
4. Verify button highlights

**Check file**:
```bash
type brain\feedback\user_feedback.jsonl
```

Should show your feedback ✅

---

### Step 8: Final Verification

**Run full test script**:
```bash
test_code_routing.bat
```

**Or check each test manually**:
- ✅ Test 1: Model files exist
- ✅ Test 2: CLI works
- ✅ Test 3: Backend routes (should PASS now)
- ✅ Test 4: UI generates (should PASS now)
- ✅ Test 5: Non-code uses ai_ask
- ✅ Test 6: Feedback works
- ✅ Test 7: Keywords detected (should PASS now)
- ✅ Test 8: Stats dashboard
- ✅ Test 9: Settings persist
- ✅ Test 10: No crashes

**All 10 should PASS** ✅

---

## 🎯 Success Criteria

**Phase J is complete when**:
1. ✅ Backend routes code requests to transformer_generate
2. ✅ UI triggers code generation on keywords
3. ✅ Server logs show `[CODE GENERATION]` messages
4. ✅ All 10 tests PASS
5. ✅ Feedback loop works end-to-end

**Output quality doesn't matter** - gibberish is expected for 3M params!

---

## 🚨 Troubleshooting

### Issue: "Connection refused" or "Can't reach server"

**Solution**:
```bash
# Check if server is running
curl http://127.0.0.1:8001/api/health

# If no response, restart
STOP_ALL.bat
START_ALL.bat
```

### Issue: Backend still returns "neural_engine" tool

**Solution**:
1. Close "Neural Engine Server" terminal window manually
2. Open new terminal
3. Run:
```bash
cd c:\Jay\_Plugin\compress\server
python main.py
```
4. Try curl test again

### Issue: Model files not found

**Solution**:
```bash
# Check if models exist
dir models\transformer.bin
dir models\tokenizer.bin

# If missing, retrain
bin\neural_engine.exe train_transformer brain\training\code_corpus.txt 15 0.002 8
```

### Issue: UI shows old cached data

**Solution**:
1. Press **Ctrl+Shift+R** (hard refresh)
2. Or clear browser cache
3. Refresh page

---

## 📊 Expected Timeline

- **Step 1 (Restart)**: 30 seconds
- **Step 2 (Debug endpoint)**: 10 seconds
- **Step 3 (Backend test)**: 30 seconds
- **Step 4 (UI test)**: 1 minute
- **Steps 5-8 (Full testing)**: 5 minutes

**Total: ~7 minutes** to complete Phase J ✅

---

## 🎉 What Happens After Phase J

Once all tests pass:
- ✅ You have a working code-gen LLM
- ✅ Infrastructure is 100% functional
- ✅ Ready for Phase K (Scale Up)

**Next steps**:
- Collect more training data (5,000+ lines)
- Scale model to 50M parameters
- Retrain for better quality

---

## 📝 Quick Commands Reference

```bash
# Restart system
STOP_ALL.bat
START_ALL.bat

# Test debug endpoint
curl -X POST http://127.0.0.1:8001/api/debug/code-detection -H "Content-Type: application/json" -d "{\"message\": \"Write code\", \"history\": []}"

# Test chat endpoint
curl -X POST http://127.0.0.1:8001/api/chat -H "Content-Type: application/json" -d "{\"message\": \"Write a fibonacci function\", \"history\": [], \"web_search\": false}"

# Check feedback
type brain\feedback\user_feedback.jsonl

# Check server health
curl http://127.0.0.1:8001/api/health

# Full test
test_code_routing.bat
```

---

**Ready? Start with Step 1!** 🚀

Let me know what you see at each step and I'll help debug if anything fails.
