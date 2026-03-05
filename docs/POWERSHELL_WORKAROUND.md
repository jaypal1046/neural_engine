# PowerShell Piping Workaround

## Issue #5: C++ Executable Encoding / Pipeline Issues in PowerShell

**Problem**: When piping or redirecting output from `bin\neural_engine.exe` in PowerShell, it crashes with:

```
NotSupportedException: FileStream was asked to open a device that was not a file
```

**Example Commands That Fail**:
```powershell
bin\neural_engine.exe ai_ask "What is AI?" 2>nul
bin\neural_engine.exe ai_ask "What is AI?" > output.txt
bin\neural_engine.exe ai_ask "What is AI?" | Select-String "answer"
```

## Root Cause

The C++ binary is doing something non-standard with stdout/stderr handles on Windows. Possible causes:
1. Attempting to seek on stdout (only valid for files, not pipes)
2. Using Windows Console API functions instead of standard file I/O
3. Mixing buffered/unbuffered I/O modes
4. Not checking if stdout is a console vs pipe before operations

## Workarounds

### Option 1: Use Bash Instead of PowerShell

Git Bash and WSL bash work correctly:
```bash
bin/neural_engine.exe ai_ask "What is AI?" 2>/dev/null
bin/neural_engine.exe ai_ask "What is AI?" | grep "answer"
```

### Option 2: Use CMD Instead of PowerShell

Windows CMD handles pipes correctly:
```cmd
bin\neural_engine.exe ai_ask "What is AI?" 2>nul
bin\neural_engine.exe ai_ask "What is AI?" | findstr "answer"
```

### Option 3: Use Python Subprocess (Recommended)

This is the most reliable method and what the backend server uses:

```python
import subprocess
import json

cmd = ["bin/neural_engine.exe", "ai_ask", "What is AI?"]
result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)

if result.returncode == 0:
    # Extract JSON from output (skipping debug messages)
    output = result.stdout.strip()
    json_start = output.find('{')
    json_end = output.rfind('}')
    if json_start >= 0:
        json_str = output[json_start:json_end+1]
        data = json.loads(json_str)
        print(data['answer'])
```

### Option 4: Batch File Wrapper

Create `ask.bat`:
```batch
@echo off
bin\neural_engine.exe ai_ask %*
```

Then use:
```powershell
.\ask.bat "What is AI?"
```

## Long-Term Fix

To fix this in the C++ code, ensure:

1. **Check if stdout is a console before console-specific operations**:
   ```cpp
   #include <io.h>
   bool isConsole = _isatty(_fileno(stdout));
   ```

2. **Use standard file I/O for output**, not Windows Console API

3. **Don't seek on stdout**:
   ```cpp
   // BAD - crashes in pipes
   fseek(stdout, 0, SEEK_SET);

   // GOOD - works everywhere
   fflush(stdout);
   ```

4. **Set binary mode explicitly** if needed:
   ```cpp
   #include <fcntl.h>
   _setmode(_fileno(stdout), _O_BINARY);
   ```

## Testing the Fix

Once C++ changes are made, test with:

```powershell
# Should NOT crash
bin\neural_engine.exe ai_ask "test" 2>$null

# Should capture JSON
$output = bin\neural_engine.exe ai_ask "test" 2>&1
$output | ConvertFrom-Json | Select -ExpandProperty answer

# Should work in pipeline
bin\neural_engine.exe ai_ask "test" | Out-File test.txt
```

## Current Status

- ❌ PowerShell piping: **BROKEN**
- ✅ Bash piping: **WORKS**
- ✅ CMD piping: **WORKS**
- ✅ Python subprocess: **WORKS** (used by backend server)
- ✅ Direct execution: **WORKS**

**Workaround**: Use Option 3 (Python subprocess) - this is what `START_ALL.bat` and the backend server already do, so normal users won't encounter this issue.

---

**Last Updated**: 2026-03-01
**Priority**: LOW (workarounds exist, doesn't affect normal usage)
