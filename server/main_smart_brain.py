"""
Smart Brain Python Server - Ultra-thin REST wrapper around C++ engine

This replaces the heavy Python logic with simple subprocess calls to smart_brain.exe.
All intelligence lives in C++ for maximum speed and memory efficiency.
"""

import subprocess
import json
import os
from pathlib import Path
from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import StreamingResponse
from pydantic import BaseModel
import time

app = FastAPI(title="Smart Brain API", version="2.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# Paths
BASE_DIR = Path(__file__).parent.parent
SMART_BRAIN_EXE = BASE_DIR / "bin" / "smart_brain.exe"
MYZIP_EXE = BASE_DIR / "bin" / "myzip.exe"

# Verify executables exist
if not SMART_BRAIN_EXE.exists():
    print(f"⚠️  WARNING: {SMART_BRAIN_EXE} not found!")
    print("   Run build_smart_brain.bat first")

# =============================================================================
# Models
# =============================================================================

class LearnRequest(BaseModel):
    source: str  # URL or file path

class AskRequest(BaseModel):
    question: str

class CompressRequest(BaseModel):
    file_path: str

# =============================================================================
# API Endpoints
# =============================================================================

@app.get("/")
async def root():
    return {
        "name": "Smart Brain API",
        "version": "2.0.0",
        "engine": "C++ Neural Compression",
        "endpoints": {
            "learn": "POST /api/learn",
            "ask": "POST /api/ask",
            "compress": "POST /api/compress",
            "status": "GET /api/status"
        }
    }

@app.post("/api/learn")
async def learn(req: LearnRequest):
    """
    Learn from URL or file and store in brain.

    Example:
        POST /api/learn
        { "source": "https://en.wikipedia.org/wiki/Data_compression" }
    """
    try:
        result = subprocess.run(
            [str(SMART_BRAIN_EXE), "learn", req.source],
            capture_output=True,
            text=True,
            timeout=120  # 2 minute timeout for large downloads
        )

        # Parse stderr for progress/logs
        log = result.stderr

        # Check if it succeeded (look for "SUCCESS" in log)
        success = "SUCCESS" in log

        return {
            "status": "ok" if success else "error",
            "source": req.source,
            "log": log,
            "stdout": result.stdout
        }

    except subprocess.TimeoutExpired:
        raise HTTPException(status_code=408, detail="Learning timeout (120s)")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/ask")
async def ask(req: AskRequest):
    """
    Query knowledge base.

    Returns JSON with:
    - confidence: 0.0-1.0 score
    - answer: text excerpt (if confidence >= 0.5)
    - action: "learn_from_web" (if confidence < 0.5)

    Example:
        POST /api/ask
        { "question": "What is data compression?" }
    """
    try:
        result = subprocess.run(
            [str(SMART_BRAIN_EXE), "ask", req.question],
            capture_output=True,
            text=True,
            timeout=30
        )

        # Parse JSON from stdout
        try:
            response = json.loads(result.stdout)
            return response
        except json.JSONDecodeError:
            # If JSON parse fails, return raw output
            return {
                "error": "parse_error",
                "stdout": result.stdout,
                "stderr": result.stderr
            }

    except subprocess.TimeoutExpired:
        raise HTTPException(status_code=408, detail="Query timeout")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/compress")
async def compress(req: CompressRequest):
    """
    Smart compression using persistent neural weights.

    Example:
        POST /api/compress
        { "file_path": "myfile.txt" }
    """
    if not os.path.exists(req.file_path):
        raise HTTPException(status_code=404, detail="File not found")

    try:
        result = subprocess.run(
            [str(SMART_BRAIN_EXE), "compress", req.file_path],
            capture_output=True,
            text=True,
            timeout=300  # 5 minute timeout
        )

        log = result.stderr
        success = "SUCCESS" in log

        return {
            "status": "ok" if success else "error",
            "file_path": req.file_path,
            "output_path": req.file_path + ".myzip",
            "log": log
        }

    except subprocess.TimeoutExpired:
        raise HTTPException(status_code=408, detail="Compression timeout")
    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.get("/api/status")
async def status():
    """
    Get brain statistics.

    Returns JSON with:
    - entries: number of knowledge items
    - total_original_mb: total size before compression
    - total_compressed_mb: total size after compression
    - average_ratio: compression ratio
    - savings_percent: percentage saved
    """
    try:
        result = subprocess.run(
            [str(SMART_BRAIN_EXE), "status"],
            capture_output=True,
            text=True,
            timeout=5
        )

        # Parse JSON from stdout
        try:
            stats = json.loads(result.stdout)
            return stats
        except json.JSONDecodeError:
            return {
                "entries": 0,
                "error": "parse_error",
                "raw": result.stdout
            }

    except Exception as e:
        raise HTTPException(status_code=500, detail=str(e))

@app.post("/api/compress_stream")
async def compress_stream(req: CompressRequest):
    """
    Stream compression output (for real-time progress in UI).

    Example:
        POST /api/compress_stream
        { "file_path": "myfile.txt", "algorithm": "--cmix" }
    """
    if not os.path.exists(req.file_path):
        raise HTTPException(status_code=404, detail="File not found")

    async def generate():
        proc = subprocess.Popen(
            [str(MYZIP_EXE), "compress", req.file_path, "--cmix"],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1
        )

        for line in iter(proc.stdout.readline, ''):
            if line:
                yield line

        proc.wait()

    return StreamingResponse(generate(), media_type="text/plain")

# =============================================================================
# Startup
# =============================================================================

@app.on_event("startup")
async def startup_event():
    print("\n" + "="*60)
    print("🧠 Smart Brain API Server")
    print("="*60)
    print(f"C++ Engine: {SMART_BRAIN_EXE}")
    print(f"Exists: {SMART_BRAIN_EXE.exists()}")

    if SMART_BRAIN_EXE.exists():
        # Get initial status
        try:
            result = subprocess.run(
                [str(SMART_BRAIN_EXE), "status"],
                capture_output=True,
                text=True,
                timeout=5
            )
            stats = json.loads(result.stdout)
            print(f"Knowledge entries: {stats.get('entries', 0)}")
            print(f"Storage saved: {stats.get('savings_percent', 0):.1f}%")
        except:
            print("Brain is empty (first run)")
    else:
        print("⚠️  Smart Brain not built yet!")
        print("   Run: build_smart_brain.bat")

    print("="*60)
    print("Ready at: http://127.0.0.1:8001")
    print("Docs at:  http://127.0.0.1:8001/docs")
    print("="*60 + "\n")

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(
        app,
        host="127.0.0.1",
        port=8001,
        log_level="info"
    )
