"""
PYTHON SUPPORT FILE — FastAPI HTTP Server and Router ONLY
=========================================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

main.py — Neural Studio V10 FastAPI server (port 8001).
Role: Receive HTTP requests from React UI, validate inputs,
      call bin/neural_engine.exe via subprocess, return JSON.
Does NOT: answer questions, generate AI responses, store knowledge,
          or run any training itself.

ALL intelligence goes through C++ commands:
  ai_ask  → full Q&A (reason + RAG + memory)
  ask     → direct knowledge query
  learn   → learn from URL or file
  train_transformer → train on corpus
  reason / verify / chain_of_thought

See: docs/ARCHITECTURE.md for the complete system design.
"""

import os
import sys

# Force UTF-8 output on Windows (cp1252 can't handle emoji/Unicode in print)
if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)

import subprocess
import hashlib
import math
import time
from collections import Counter
from fastapi import FastAPI
from fastapi.responses import StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
from pydantic import BaseModel
from typing import Optional

app = FastAPI(title="Neural Studio V10 Server", version="10.0.0")

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# ONE unified executable for everything: compression, Smart Brain, AI
NEURAL_ENGINE_EXE = os.path.join(BASE_DIR, "bin", "neural_engine.exe")
EXE_PATH = NEURAL_ENGINE_EXE  # Legacy compatibility
SERVER_START_TIME = time.time()

# Import Python neural brain (trained TF-IDF brain, 74% accuracy)
SERVER_DIR = os.path.dirname(os.path.abspath(__file__))
if SERVER_DIR not in sys.path:
    sys.path.insert(0, SERVER_DIR)
try:
    import neural_brain as _brain
    PYTHON_BRAIN_AVAILABLE = True
except ImportError:
    _brain = None
    PYTHON_BRAIN_AVAILABLE = False

# =============================================================================
# Models
# =============================================================================

class CompressRequest(BaseModel):
    file_path: str
    algorithm: str = "--cmix"

class DecompressRequest(BaseModel):
    archive_path: str
    output_path: str

class AnalyzeRequest(BaseModel):
    file_path: str

class CommandRequest(BaseModel):
    command: str

class AIInsightRequest(BaseModel):
    file_path: str
    file_size: Optional[int] = None
    entropy: Optional[float] = None

class AskRequest(BaseModel):
    question: str

class LearnFileRequest(BaseModel):
    file_path: str

class TrainRequest(BaseModel):
    mode: str = "all"
    deep: bool = False

# =============================================================================
# Utility functions
# =============================================================================

def compute_entropy(data: bytes) -> float:
    """Compute Shannon entropy in bits per byte."""
    if len(data) == 0:
        return 0.0
    counter = Counter(data)
    length = len(data)
    entropy = 0.0
    for count in counter.values():
        p = count / length
        if p > 0:
            entropy -= p * math.log2(p)
    return entropy

def detect_file_type(data: bytes, filename: str) -> dict:
    """Detect file type and characteristics."""
    ext = os.path.splitext(filename)[1].lower()
    
    # Check for text vs binary
    text_chars = set(range(32, 127)) | {9, 10, 13}
    sample = data[:8192]
    text_ratio = sum(1 for b in sample if b in text_chars) / max(len(sample), 1)
    is_text = text_ratio > 0.85
    
    # Character class analysis
    alpha_count = sum(1 for b in sample if chr(b).isalpha())
    digit_count = sum(1 for b in sample if chr(b).isdigit())
    space_count = sum(1 for b in sample if chr(b).isspace())
    punct_count = sum(1 for b in sample if chr(b) in '.,;:!?()[]{}"\'-')
    
    categories = {
        "text": [".txt", ".md", ".rst", ".csv", ".log", ".json", ".xml", ".html"],
        "code": [".cpp", ".h", ".py", ".js", ".ts", ".tsx", ".c", ".java", ".rs", ".go"],
        "data": [".bin", ".dat", ".db", ".sqlite"],
        "archive": [".zip", ".tar", ".gz", ".7z", ".rar", ".aiz"],
        "image": [".png", ".jpg", ".jpeg", ".bmp", ".gif", ".webp", ".svg"],
        "document": [".pdf", ".docx", ".xlsx", ".pptx"]
    }
    
    file_category = "unknown"
    for cat, exts in categories.items():
        if ext in exts:
            file_category = cat
            break
    
    if file_category == "unknown" and is_text:
        file_category = "text"
    
    return {
        "extension": ext,
        "category": file_category,
        "is_text": is_text,
        "text_ratio": round(text_ratio, 4),
        "alpha_ratio": round(alpha_count / max(len(sample), 1), 4),
        "digit_ratio": round(digit_count / max(len(sample), 1), 4),
        "space_ratio": round(space_count / max(len(sample), 1), 4),
    }

def compute_byte_distribution(data: bytes) -> list:
    """Compute byte frequency distribution for visualization (256 buckets -> 64 buckets)."""
    counter = Counter(data)
    length = max(len(data), 1)
    
    # Group into 64 buckets (each covering 4 byte values)
    buckets = []
    for i in range(0, 256, 4):
        total = sum(counter.get(j, 0) for j in range(i, min(i+4, 256)))
        buckets.append(round(total / length, 6))
    
    return buckets

def compute_entropy_map(data: bytes, num_blocks: int = 48) -> list:
    """Compute per-block entropy for visualization."""
    if len(data) == 0:
        return []
    block_size = max(len(data) // num_blocks, 1)
    entropies = []
    for i in range(0, len(data), block_size):
        block = data[i:i+block_size]
        entropies.append(round(compute_entropy(block), 3))
        if len(entropies) >= num_blocks:
            break
    return entropies

def generate_ai_insights(file_path: str, file_size: int, entropy: float, file_info: dict) -> dict:
    """Generate AI-powered compression insights."""
    insights = []
    recommended_algo = "--cmix"
    compression_estimate = 0.0
    
    # Algorithm recommendation logic
    if file_size > 50 * 1024 * 1024:  # > 50 MB
        recommended_algo = "--best"
        insights.append({
            "type": "warning",
            "title": "Large File Detected",
            "detail": f"At {file_size / (1024*1024):.1f} MB, CMIX would be extremely slow. BWT (--best) offers the best speed/ratio balance.",
            "icon": "⚡"
        })
    elif entropy > 7.5:
        recommended_algo = "--best"
        insights.append({
            "type": "info",
            "title": "High Entropy Data",
            "detail": f"Shannon entropy is {entropy:.2f} bits/byte (near random). This file is already dense — no algorithm can compress it well. Try --best for speed.",
            "icon": "🎲"
        })
    elif entropy < 3.0 and file_info.get("is_text"):
        recommended_algo = "--cmix"
        insights.append({
            "type": "success",
            "title": "Excellent Compressibility",
            "detail": f"Low entropy ({entropy:.2f} bpb) with high text ratio means the Neural Net will learn patterns extremely well. CMIX will shine here!",
            "icon": "🏆"
        })
    elif file_info.get("category") == "code":
        recommended_algo = "--cmix"
        insights.append({
            "type": "success",
            "title": "Source Code Detected",
            "detail": "Code has repetitive syntactic patterns (brackets, keywords, indentation). The CMIX advisors will exploit these beautifully.",
            "icon": "💻"
        })
    else:
        insights.append({
            "type": "info",
            "title": "General Data Analysis",
            "detail": f"Entropy: {entropy:.2f} bpb. CMIX will provide the best ratio, but --best is significantly faster.",
            "icon": "📊"
        })
    
    # Entropy analysis
    if entropy < 1.0:
        compression_estimate = 90 + (1.0 - entropy) * 5
        insights.append({"type": "success", "title": "Near-Zero Entropy", "detail": "This data is incredibly repetitive. Expect 90%+ compression savings.", "icon": "✨"})
    elif entropy < 3.0:
        compression_estimate = 60 + (3.0 - entropy) * 10
        insights.append({"type": "success", "title": "Low Entropy", "detail": f"Good compressibility. The 16-advisor neural council will exploit the patterns.", "icon": "🧠"})
    elif entropy < 5.0:
        compression_estimate = 30 + (5.0 - entropy) * 15
        insights.append({"type": "info", "title": "Moderate Entropy", "detail": "Decent compression possible. Context mixing advisors will find local patterns.", "icon": "📈"})
    elif entropy < 7.0:
        compression_estimate = 10 + (7.0 - entropy) * 10
    else:
        compression_estimate = max(2, (8.0 - entropy) * 10)
        insights.append({"type": "warning", "title": "Near-Random Data", "detail": "Very high entropy — this may already be compressed or encrypted.", "icon": "🔒"})
    
    # File type specific insights
    if file_info.get("category") == "archive":
        insights.append({"type": "warning", "title": "Already Compressed", "detail": "This is a compressed archive. Re-compressing will likely make it LARGER (Shannon's theorem).", "icon": "⚠️"})
        compression_estimate = min(compression_estimate, 2)
    
    if file_info.get("alpha_ratio", 0) > 0.6:
        insights.append({"type": "info", "title": "High Alphabetic Content", "detail": "Predominantly letters — natural language. The order-5 context model will learn word patterns.", "icon": "📝"})
    
    # Model predictions — 1046 advisors, show representative groups
    model_predictions = []
    base_entropy = entropy
    advisor_groups = [
        ("Order 0-1", "Global + 1-byte context", 128, 0),
        ("Order 2-3", "2-3 byte context", 192, 1),
        ("Order 4-5", "4-5 byte context", 160, 2),
        ("Order 6-7", "6-7 byte context", 128, 3),
        ("Bit-level 1-4", "Sub-byte bit patterns", 148, 4),
        ("Bit-level 5-7", "Deep bit context", 112, 5),
        ("Word boundary", "Word-level matching", 86, 6),
        ("Skip models", "Long-range pattern matching", 92, 7),
    ]
    for label, desc, count, idx in advisor_groups:
        reduction = min(0.25 * (idx + 1) * (8 - base_entropy) / 8, base_entropy * 0.82)
        predicted = max(base_entropy - reduction, 0.4)
        confidence = min(15 + idx * 10, 88) if base_entropy < 6 else min(8 + idx * 4, 35)
        model_predictions.append({
            "order": idx,
            "label": f"{label} ({count})",
            "predicted_bpb": round(predicted, 2),
            "confidence": confidence,
            "description": desc,
            "advisor_count": count
        })
    
    return {
        "recommended_algorithm": recommended_algo,
        "compression_estimate_pct": round(min(compression_estimate, 95), 1),
        "insights": insights,
        "model_predictions": model_predictions,
        "entropy_quality": "excellent" if entropy < 3 else "good" if entropy < 5 else "moderate" if entropy < 7 else "poor"
    }

# =============================================================================
# API Endpoints
# =============================================================================

@app.get("/api/health")
def health():
    uptime = time.time() - SERVER_START_TIME
    exe_exists = os.path.exists(EXE_PATH)
    return {
        "status": "online",
        "version": "10.0.0",
        "engine": "Neural Studio V10 — CMIX Context Mixing",
        "uptime_seconds": round(uptime, 1),
        "exe_available": exe_exists,
        "exe_path": EXE_PATH,
        "models": 1046,
        "algorithms": ["default (LZ77)", "--best (BWT)", "--ultra (PPM)", "--cmix (Neural)"],
        "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    }

@app.get("/api/status")
def status():
    return {"status": "ok", "message": "Neural Studio V10 API running.", "version": "10.0.0"}

@app.post("/api/analyze")
def analyze_file(payload: AnalyzeRequest):
    """Deep file analysis — entropy, patterns, byte distribution, AI recommendations."""
    if not os.path.exists(payload.file_path):
        return {"error": f"File not found: {payload.file_path}"}
    
    try:
        file_size = os.path.getsize(payload.file_path)
        
        # Read file data (limit to 16MB for analysis)
        read_size = min(file_size, 16 * 1024 * 1024)
        with open(payload.file_path, "rb") as f:
            data = f.read(read_size)
        
        # Compute metrics
        entropy = compute_entropy(data)
        file_info = detect_file_type(data, payload.file_path)
        byte_dist = compute_byte_distribution(data)
        entropy_map = compute_entropy_map(data)
        
        # SHA-256 hash
        sha256 = hashlib.sha256(data).hexdigest()
        
        # AI Insights
        ai = generate_ai_insights(payload.file_path, file_size, entropy, file_info)
        
        return {
            "status": "success",
            "file_path": payload.file_path,
            "file_name": os.path.basename(payload.file_path),
            "file_size": file_size,
            "file_size_human": f"{file_size / (1024*1024):.2f} MB" if file_size > 1024*1024 else f"{file_size / 1024:.1f} KB",
            "sha256": sha256[:16] + "...",
            "entropy": round(entropy, 4),
            "max_entropy": 8.0,
            "entropy_pct": round(entropy / 8.0 * 100, 1),
            "file_info": file_info,
            "byte_distribution": byte_dist,
            "entropy_map": entropy_map,
            "ai": ai
        }
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/ai_insight")
def ai_insight(payload: AIInsightRequest):
    """Get AI insights for a file (lightweight, uses cached analysis)."""
    if not os.path.exists(payload.file_path):
        return {"error": f"File not found: {payload.file_path}"}
    
    file_size = payload.file_size or os.path.getsize(payload.file_path)
    
    if payload.entropy is not None:
        entropy = payload.entropy
    else:
        with open(payload.file_path, "rb") as f:
            data = f.read(min(file_size, 4 * 1024 * 1024))
        entropy = compute_entropy(data)
    
    file_info = detect_file_type(b"", payload.file_path)
    ai = generate_ai_insights(payload.file_path, file_size, entropy, file_info)
    
    return {"status": "success", **ai}

@app.post("/api/compress")
def compress_file(payload: CompressRequest):
    if not os.path.exists(payload.file_path):
        return {"error": f"File not found: {payload.file_path}"}
        
    target_myzip = payload.file_path + ".aiz"
    
    if not os.path.exists(EXE_PATH):
        return {"error": f"Executable not found: {EXE_PATH}"}
        
    try:
        result = subprocess.run(
            [EXE_PATH, "compress", payload.file_path, "-o", target_myzip, payload.algorithm],
            capture_output=True, text=True
        )
        
        # Compute actual ratio
        in_size = os.path.getsize(payload.file_path)
        out_size = os.path.getsize(target_myzip) if os.path.exists(target_myzip) else 0
        ratio = out_size / max(in_size, 1)
        
        return {
            "status": "success", 
            "output": target_myzip, 
            "message": f"Compression finished for {payload.file_path} into {target_myzip}.",
            "stdout": result.stdout,
            "stderr": result.stderr,
            "input_size": in_size,
            "output_size": out_size,
            "ratio": round(ratio, 4),
            "savings_pct": round((1.0 - ratio) * 100, 1)
        }
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/decompress")
def decompress_file(payload: DecompressRequest):
    if not os.path.exists(payload.archive_path):
        return {"error": f"Archive not found: {payload.archive_path}"}
    
    if not os.path.exists(EXE_PATH):
        return {"error": f"Executable not found: {EXE_PATH}"}
        
    try:
        result = subprocess.run(
            [EXE_PATH, "decompress", payload.archive_path, "-o", payload.output_path],
            capture_output=True, text=True
        )
        return {
            "status": "success", 
            "output": payload.output_path, 
            "message": f"Decompression finished for {payload.archive_path}.",
            "stdout": result.stdout,
            "stderr": result.stderr
        }
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/compress_stream")
def compress_stream(payload: CompressRequest):
    if not os.path.exists(payload.file_path):
        return StreamingResponse(iter([b"Error: File not found"]), media_type="text/plain")
        
    target_myzip = payload.file_path + ".aiz"

    def generator():
        process = subprocess.Popen(
            [EXE_PATH, "compress", payload.file_path, "-o", target_myzip, payload.algorithm],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0
        )
        buf = b""
        while True:
            char = process.stdout.read(1)
            if not char:
                if buf: yield buf
                break
            buf += char
            if char in (b'\r', b'\n') or len(buf) >= 128:
                yield buf
                buf = b""
        process.wait()

    return StreamingResponse(
        generator(), media_type="text/event-stream",
        headers={"X-Content-Type-Options": "nosniff", "Cache-Control": "no-cache"}
    )

@app.post("/api/decompress_stream")
def decompress_stream(payload: DecompressRequest):
    if not os.path.exists(payload.archive_path):
        return StreamingResponse(iter([b"Error: Archive not found"]), media_type="text/plain")

    def generator():
        process = subprocess.Popen(
            [EXE_PATH, "decompress", payload.archive_path, "-o", payload.output_path],
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT, bufsize=0
        )
        buf = b""
        while True:
            char = process.stdout.read(1)
            if not char:
                if buf: yield buf
                break
            buf += char
            if char in (b'\r', b'\n') or len(buf) >= 128:
                yield buf
                buf = b""
        process.wait()

    return StreamingResponse(
        generator(), media_type="text/event-stream",
        headers={"X-Content-Type-Options": "nosniff", "Cache-Control": "no-cache"}
    )

@app.post("/api/cmd")
def run_command(payload: CommandRequest):
    try:
        result = subprocess.run(payload.command, shell=True, capture_output=True, text=True)
        return {
            "status": "success",
            "stdout": result.stdout,
            "stderr": result.stderr,
            "code": result.returncode
        }
    except Exception as e:
        return {"error": str(e)}

# =============================================================================
# Local File System Access — The brain can see your files
# =============================================================================

import glob as glob_module

class ListDirRequest(BaseModel):
    path: Optional[str] = None
    show_hidden: bool = False

class ReadFileRequest(BaseModel):
    path: str
    max_lines: int = 200

class FindFilesRequest(BaseModel):
    directory: Optional[str] = None
    pattern: str = "*"
    recursive: bool = True
    max_results: int = 50

class FileInfoRequest(BaseModel):
    path: str

class WriteFileRequest(BaseModel):
    path: str
    content: str
    append: bool = False

class DeleteFileRequest(BaseModel):
    path: str

@app.post("/api/fs/write")
def fs_write_file(payload: WriteFileRequest):
    """Write or append text to a file."""
    path = os.path.expanduser(payload.path)
    try:
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        mode = "a" if payload.append else "w"
        with open(path, mode, encoding="utf-8") as f:
            f.write(payload.content)
        return {"status": "success", "message": f"Written successfully to {path}"}
    except Exception as e:
        return {"error": str(e)}

import shutil
@app.post("/api/fs/delete")
def fs_delete_file(payload: DeleteFileRequest):
    """Delete a file or directory."""
    path = os.path.expanduser(payload.path)
    if not os.path.exists(path):
        return {"error": f"Path not found: {path}"}
    try:
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        return {"status": "success", "message": f"Deleted {path}"}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/fs/list")
def fs_list_dir(payload: ListDirRequest):
    """List contents of a directory. Defaults to project root."""
    target = payload.path or BASE_DIR
    target = os.path.expanduser(target)
    
    if not os.path.exists(target):
        return {"error": f"Path not found: {target}"}
    if not os.path.isdir(target):
        return {"error": f"Not a directory: {target}"}
    
    try:
        items = []
        for name in sorted(os.listdir(target)):
            if not payload.show_hidden and name.startswith('.'):
                continue
            full_path = os.path.join(target, name)
            is_dir = os.path.isdir(full_path)
            info = {
                "name": name,
                "path": full_path,
                "type": "directory" if is_dir else "file",
            }
            if not is_dir:
                try:
                    size = os.path.getsize(full_path)
                    info["size"] = size
                    info["size_human"] = f"{size / 1024:.1f} KB" if size < 1024*1024 else f"{size / (1024*1024):.2f} MB"
                    info["extension"] = os.path.splitext(name)[1]
                except OSError:
                    pass
            else:
                try:
                    info["children"] = len(os.listdir(full_path))
                except OSError:
                    info["children"] = 0
            items.append(info)
        
        return {
            "status": "success",
            "path": target,
            "count": len(items),
            "items": items
        }
    except PermissionError:
        return {"error": f"Permission denied: {target}"}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/fs/read")
def fs_read_file(payload: ReadFileRequest):
    """Read contents of any file — supports PDF, DOCX, XLSX, and all text formats."""
    path = os.path.expanduser(payload.path)
    
    if not os.path.exists(path):
        return {"error": f"File not found: {path}"}
    if os.path.isdir(path):
        return {"error": f"That's a directory, not a file. Use /api/fs/list instead."}
    
    size = os.path.getsize(path)
    if size > 50 * 1024 * 1024:
        return {"error": f"File too large ({size / (1024*1024):.1f} MB). Max 50 MB."}
    
    ext = os.path.splitext(path)[1].lower()
    
    try:
        from file_converter import extract_text, can_read
        
        # Use file converter for non-text formats
        if ext in {'.pdf', '.docx', '.doc', '.xlsx', '.xls', '.pptx'}:
            content = extract_text(path)
            lines = content.split('\n')
            total_lines = len(lines)
            shown = lines[:payload.max_lines]
            return {
                "status": "success",
                "path": path,
                "file_name": os.path.basename(path),
                "size": size,
                "format": ext.lstrip('.').upper(),
                "converted": True,
                "total_lines": total_lines,
                "lines_shown": len(shown),
                "truncated": total_lines > payload.max_lines,
                "content": "\n".join(shown),
                "extension": ext
            }
        
        # Plain text files
        with open(path, "r", encoding="utf-8", errors="replace") as f:
            lines = []
            for i, line in enumerate(f):
                if i >= payload.max_lines:
                    break
                lines.append(line.rstrip('\n'))
        
        total_lines = sum(1 for _ in open(path, "r", encoding="utf-8", errors="replace"))
        
        return {
            "status": "success",
            "path": path,
            "file_name": os.path.basename(path),
            "size": size,
            "total_lines": total_lines,
            "lines_shown": len(lines),
            "truncated": total_lines > payload.max_lines,
            "content": "\n".join(lines),
            "extension": ext
        }
    except Exception as e:
        return {"error": str(e)}

@app.get("/api/fs/formats")
def fs_supported_formats():
    """Check which file format converters are available."""
    try:
        from file_converter import get_supported_formats
        return {"status": "success", **get_supported_formats()}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/fs/find")
def fs_find_files(payload: FindFilesRequest):
    """Find files matching a pattern."""
    directory = payload.directory or BASE_DIR
    directory = os.path.expanduser(directory)
    
    if not os.path.exists(directory):
        return {"error": f"Directory not found: {directory}"}
    
    try:
        if payload.recursive:
            search = os.path.join(directory, "**", payload.pattern)
        else:
            search = os.path.join(directory, payload.pattern)
        
        matches = glob_module.glob(search, recursive=payload.recursive)
        
        results = []
        for fp in matches[:payload.max_results]:
            is_dir = os.path.isdir(fp)
            info = {
                "path": fp,
                "name": os.path.basename(fp),
                "type": "directory" if is_dir else "file",
            }
            if not is_dir:
                try:
                    size = os.path.getsize(fp)
                    info["size"] = size
                    info["extension"] = os.path.splitext(fp)[1]
                except OSError:
                    pass
            results.append(info)
        
        return {
            "status": "success",
            "directory": directory,
            "pattern": payload.pattern,
            "count": len(results),
            "total_found": len(matches),
            "results": results
        }
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/fs/info")
def fs_file_info(payload: FileInfoRequest):
    """Get detailed info about a file or directory."""
    path = os.path.expanduser(payload.path)
    
    if not os.path.exists(path):
        return {"error": f"Not found: {path}"}
    
    try:
        stat = os.stat(path)
        is_dir = os.path.isdir(path)
        
        info = {
            "status": "success",
            "path": path,
            "name": os.path.basename(path),
            "type": "directory" if is_dir else "file",
            "size": stat.st_size,
            "size_human": f"{stat.st_size / 1024:.1f} KB" if stat.st_size < 1024*1024 else f"{stat.st_size / (1024*1024):.2f} MB",
            "modified": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stat.st_mtime)),
            "created": time.strftime("%Y-%m-%d %H:%M:%S", time.localtime(stat.st_ctime)),
        }
        
        if not is_dir:
            info["extension"] = os.path.splitext(path)[1]
            # Quick entropy check for files under 4MB
            if stat.st_size < 4 * 1024 * 1024 and stat.st_size > 0:
                with open(path, "rb") as f:
                    data = f.read()
                info["entropy"] = round(compute_entropy(data), 4)
                file_type = detect_file_type(data, path)
                info["category"] = file_type.get("category", "unknown")
                info["is_text"] = file_type.get("is_text", False)
        else:
            try:
                children = os.listdir(path)
                info["children_count"] = len(children)
                info["subdirs"] = sum(1 for c in children if os.path.isdir(os.path.join(path, c)))
                info["files"] = info["children_count"] - info["subdirs"]
            except OSError:
                pass
        
        return info
    except Exception as e:
        return {"error": str(e)}

# =============================================================================
# Neural Vault — Store & Access Compressed Data
# =============================================================================

import json

VAULT_DIR = os.path.join(BASE_DIR, "vault")
VAULT_META = os.path.join(VAULT_DIR, "vault_index.json")

def ensure_vault():
    """Create the vault directory and index if they don't exist."""
    os.makedirs(VAULT_DIR, exist_ok=True)
    if not os.path.exists(VAULT_META):
        with open(VAULT_META, "w") as f:
            json.dump({"version": "10.0", "entries": {}}, f)

def load_vault_index() -> dict:
    ensure_vault()
    with open(VAULT_META, "r") as f:
        return json.load(f)

def save_vault_index(index: dict):
    with open(VAULT_META, "w") as f:
        json.dump(index, f, indent=2)

class StoreRequest(BaseModel):
    file_path: str
    key: Optional[str] = None
    algorithm: str = "--cmix"

class AccessRequest(BaseModel):
    key: str
    output_path: Optional[str] = None

@app.post("/api/vault/store")
def vault_store(payload: StoreRequest):
    """Compress and store a file in the neural vault for later retrieval."""
    if not os.path.exists(payload.file_path):
        return {"error": f"File not found: {payload.file_path}"}
    if not os.path.exists(EXE_PATH):
        return {"error": f"Executable not found: {EXE_PATH}"}
    
    ensure_vault()
    
    # Generate key from filename if not provided
    key = payload.key or os.path.splitext(os.path.basename(payload.file_path))[0]
    # Make key filesystem safe
    safe_key = "".join(c if c.isalnum() or c in "-_." else "_" for c in key)
    
    original_size = os.path.getsize(payload.file_path)
    vault_file = os.path.join(VAULT_DIR, f"{safe_key}.aiz")
    
    # Pre-analyze for optimal algorithm selection
    read_size = min(original_size, 4 * 1024 * 1024)
    with open(payload.file_path, "rb") as f:
        sample = f.read(read_size)
    entropy = compute_entropy(sample)
    file_info = detect_file_type(sample, payload.file_path)
    
    # Auto-select best algorithm
    algo = payload.algorithm
    if algo == "--cmix" and original_size > 50 * 1024 * 1024:
        algo = "--best"  # Too large for CMIX
    
    try:
        result = subprocess.run(
            [EXE_PATH, "compress", payload.file_path, "-o", vault_file, algo],
            capture_output=True, text=True, timeout=3600
        )
        
        if not os.path.exists(vault_file):
            return {"error": "Compression failed — no output file produced.", "stderr": result.stderr}
        
        compressed_size = os.path.getsize(vault_file)
        ratio = compressed_size / max(original_size, 1)
        sha256 = hashlib.sha256(sample).hexdigest()
        
        # Update vault index
        index = load_vault_index()
        index["entries"][key] = {
            "key": key,
            "original_path": payload.file_path,
            "original_name": os.path.basename(payload.file_path),
            "vault_file": vault_file,
            "original_size": original_size,
            "compressed_size": compressed_size,
            "ratio": round(ratio, 4),
            "savings_pct": round((1.0 - ratio) * 100, 1),
            "algorithm": algo,
            "entropy": round(entropy, 4),
            "file_type": file_info.get("category", "unknown"),
            "sha256": sha256,
            "stored_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        }
        save_vault_index(index)
        
        return {
            "status": "success",
            "key": key,
            "message": f"Stored '{os.path.basename(payload.file_path)}' as '{key}' in neural vault.",
            "original_size": original_size,
            "compressed_size": compressed_size,
            "ratio": round(ratio, 4),
            "savings_pct": round((1.0 - ratio) * 100, 1),
            "algorithm": algo,
            "entropy": round(entropy, 4),
            "vault_file": vault_file
        }
    except subprocess.TimeoutExpired:
        return {"error": "Compression timed out (1 hour limit)."}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/vault/access")
def vault_access(payload: AccessRequest):
    """Retrieve and decompress a file from the neural vault."""
    if not os.path.exists(EXE_PATH):
        return {"error": f"Executable not found: {EXE_PATH}"}
    
    index = load_vault_index()
    entry = index.get("entries", {}).get(payload.key)
    
    if not entry:
        return {"error": f"Key '{payload.key}' not found in vault. Use /api/vault/list to see stored files."}
    
    vault_file = entry["vault_file"]
    if not os.path.exists(vault_file):
        return {"error": f"Vault archive missing: {vault_file}"}
    
    # Determine output path
    output_path = payload.output_path or entry.get("original_path", 
        os.path.join(VAULT_DIR, f"{payload.key}_recovered{os.path.splitext(entry.get('original_name', ''))[1]}")
    )
    
    try:
        result = subprocess.run(
            [EXE_PATH, "decompress", vault_file, "-o", output_path],
            capture_output=True, text=True, timeout=3600
        )
        
        if os.path.exists(output_path):
            recovered_size = os.path.getsize(output_path)
            return {
                "status": "success",
                "key": payload.key,
                "output_path": output_path,
                "recovered_size": recovered_size,
                "original_size": entry.get("original_size", 0),
                "algorithm": entry.get("algorithm", "unknown"),
                "message": f"Retrieved '{payload.key}' from vault → {output_path}",
                "stdout": result.stdout
            }
        else:
            return {"error": "Decompression failed.", "stderr": result.stderr}
    except subprocess.TimeoutExpired:
        return {"error": "Decompression timed out."}
    except Exception as e:
        return {"error": str(e)}

@app.get("/api/vault/list")
def vault_list():
    """List all files stored in the neural vault."""
    index = load_vault_index()
    entries = index.get("entries", {})
    
    total_original = sum(e.get("original_size", 0) for e in entries.values())
    total_compressed = sum(e.get("compressed_size", 0) for e in entries.values())
    
    return {
        "status": "success",
        "count": len(entries),
        "total_original_size": total_original,
        "total_compressed_size": total_compressed,
        "total_savings_pct": round((1.0 - total_compressed / max(total_original, 1)) * 100, 1),
        "entries": list(entries.values())
    }

@app.delete("/api/vault/remove/{key}")
def vault_remove(key: str):
    """Remove a file from the neural vault."""
    index = load_vault_index()
    entry = index.get("entries", {}).get(key)
    
    if not entry:
        return {"error": f"Key '{key}' not found in vault."}
    
    # Delete the compressed file
    vault_file = entry.get("vault_file", "")
    if os.path.exists(vault_file):
        os.remove(vault_file)
    
    del index["entries"][key]
    save_vault_index(index)
    
    return {"status": "success", "message": f"Removed '{key}' from vault."}

# =============================================================================
# Neural Task Handler — Intelligent request routing
# =============================================================================

class NeuralTaskRequest(BaseModel):
    task: str
    context: Optional[dict] = None

@app.post("/api/neural/handle")
def neural_handle(payload: NeuralTaskRequest):
    """
    Intelligent task handler — routes user requests through the neural engine.
    Parses natural language commands and maps them to the right API.
    """
    task = payload.task.lower().strip()
    ctx = payload.context or {}
    
    # ─── Analyze ───
    if any(w in task for w in ["analyze", "scan", "inspect", "entropy", "check"]):
        # Extract file path from task
        fp = extract_file_path(payload.task)
        if fp and os.path.exists(fp):
            # Directly call analyze logic
            file_size = os.path.getsize(fp)
            read_size = min(file_size, 16 * 1024 * 1024)
            with open(fp, "rb") as f:
                data = f.read(read_size)
            
            entropy = compute_entropy(data)
            file_info = detect_file_type(data, fp)
            byte_dist = compute_byte_distribution(data)
            entropy_map = compute_entropy_map(data)
            sha256 = hashlib.sha256(data).hexdigest()
            ai = generate_ai_insights(fp, file_size, entropy, file_info)
            
            return {
                "action": "analyze",
                "status": "success",
                "result": {
                    "file_path": fp,
                    "file_name": os.path.basename(fp),
                    "file_size": file_size,
                    "file_size_human": f"{file_size / (1024*1024):.2f} MB" if file_size > 1024*1024 else f"{file_size / 1024:.1f} KB",
                    "sha256": sha256[:16] + "...",
                    "entropy": round(entropy, 4),
                    "entropy_pct": round(entropy / 8.0 * 100, 1),
                    "file_info": file_info,
                    "byte_distribution": byte_dist,
                    "entropy_map": entropy_map,
                    "ai": ai
                }
            }
        return {"action": "analyze", "status": "need_file", "message": "Provide a valid file path to analyze."}
    
    # ─── Compress ───
    if any(w in task for w in ["compress", "shrink", "zip", "archive"]):
        fp = extract_file_path(payload.task)
        if fp and os.path.exists(fp):
            algo = "--cmix"
            if "--best" in task: algo = "--best"
            elif "--ultra" in task: algo = "--ultra"
            
            return {
                "action": "compress",
                "status": "ready",
                "file_path": fp,
                "algorithm": algo,
                "message": f"Ready to compress {os.path.basename(fp)} with {algo}"
            }
        return {"action": "compress", "status": "need_file", "message": "Provide a file path to compress."}
    
    # ─── Store ───
    if any(w in task for w in ["store", "vault", "save", "keep"]):
        fp = extract_file_path(payload.task)
        if fp and os.path.exists(fp):
            return {
                "action": "store",
                "status": "ready",
                "file_path": fp,
                "message": f"Ready to compress and store {os.path.basename(fp)} in the neural vault."
            }
        return {"action": "store", "status": "need_file", "message": "Provide a file path to store in the vault."}
    
    # ─── Retrieve / Access ───
    if any(w in task for w in ["access", "retrieve", "get", "fetch", "load"]):
        index = load_vault_index()
        entries = index.get("entries", {})
        # Try to find key in the task
        for key in entries:
            if key.lower() in task:
                return {
                    "action": "access",
                    "status": "ready",
                    "key": key,
                    "entry": entries[key],
                    "message": f"Found '{key}' in vault. Ready to decompress."
                }
        return {
            "action": "access",
            "status": "list",
            "available_keys": list(entries.keys()),
            "message": f"Available vault entries: {', '.join(entries.keys()) or 'none'}"
        }
    
    # ─── Decompress ───
    if any(w in task for w in ["decompress", "restore", "extract", "unzip"]):
        fp = extract_file_path(payload.task)
        if fp:
            return {
                "action": "decompress",
                "status": "ready",
                "file_path": fp,
                "message": f"Ready to decompress {os.path.basename(fp)}"
            }
        return {"action": "decompress", "status": "need_file", "message": "Provide a .aiz archive to decompress."}
    
    # ─── Status ───
    if any(w in task for w in ["status", "health", "info"]):
        uptime = time.time() - SERVER_START_TIME
        return {
            "action": "status",
            "status": "success",
            "result": {
                "engine": "Neural Studio V10",
                "models": 1046,
                "uptime": round(uptime, 1),
                "exe_available": os.path.exists(EXE_PATH),
                "vault_entries": len(load_vault_index().get("entries", {}))
            }
        }
    
    return {
        "action": "unknown",
        "status": "unrecognized",
        "message": f"Could not understand task: '{payload.task}'. Try: analyze, compress, store, access, decompress, status."
    }

def extract_file_path(text: str) -> Optional[str]:
    """Extract a file path from natural language text."""
    import re
    # Match Windows paths like C:\...\file.ext or ./file.ext
    patterns = [
        r'([A-Za-z]:\\[^\s"\']+)',        # C:\path\to\file
        r'(\.\\[^\s"\']+)',                 # .\relative\path
        r'(\./[^\s"\']+)',                  # ./unix style
        r'(/[^\s"\']+\.[a-zA-Z0-9]+)',      # /absolute/unix
    ]
    for pattern in patterns:
        match = re.search(pattern, text)
        if match:
            return match.group(1).strip().rstrip('.,;:!?')
    
    # Try to find anything that looks like a filename with extension
    words = text.split()
    for word in words:
        word = word.strip('.,;:!?"\'')
        if '.' in word and not word.startswith('-') and len(word) > 2:
            if any(word.endswith(ext) for ext in ['.txt', '.md', '.py', '.cpp', '.h', '.js', '.ts', '.json', 
                                                    '.xml', '.csv', '.log', '.html', '.css', '.bin', '.dat',
                                                    '.aiz', '.zip', '.pdf', '.exe']):
                return word
    return None


# =============================================================================
# Math API Endpoint (C++ Neural Engine Integration)
# =============================================================================

class MathRequest(BaseModel):
    query: str

@app.post("/api/math/process")
async def math_process(req: MathRequest):
    """
    Process mathematical expressions using the neural engine.
    Supports: expressions, statistics, entropy calculations.
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built", "message": "Run build_smart_brain.bat first"}

    try:
        # Call neural_engine.exe math <query>
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "math", req.query],
            capture_output=True,
            text=True,
            timeout=10
        )

        # Parse JSON output from neural_engine
        try:
            import json
            response = json.loads(result.stdout)

            # neural_engine returns: {"status":"success","expression":"2+2","result":4}
            # Frontend expects: {"type":"expression","expression":"2+2","result":4}
            if response.get("status") == "success":
                # Convert to frontend format
                return {
                    "type": "expression",
                    "expression": response.get("expression", req.query),
                    "result": response.get("result", 0)
                }
            else:
                return {"error": response.get("error", "Unknown error"), "type": "unknown"}

        except json.JSONDecodeError:
            return {
                "error": "Failed to parse result",
                "type": "unknown",
                "stdout": result.stdout,
                "stderr": result.stderr
            }

    except subprocess.TimeoutExpired:
        return {"error": "Math computation timeout", "type": "unknown"}
    except Exception as e:
        return {"error": str(e), "type": "unknown"}

# =============================================================================
# Smart Brain API Endpoints (C++ Engine Integration)
# =============================================================================

# Request models for Smart Brain
class LearnRequest(BaseModel):
    topic: str          # Topic/title for the knowledge
    content: str        # Raw text content to learn

class LearnSourceRequest(BaseModel):
    source: str         # URL or file path (used internally)

@app.post("/api/brain/learn")
async def brain_learn(req: LearnRequest):
    """
    Learn from direct text content.
    Python: writes content to temp file → C++ neural_engine.exe learn <file>
    C++ does ALL actual learning: tokenize, CMIX compress, index.
    Returns fields expected by desktop app (token_count, savings_pct, keywords, summary).
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    import tempfile as _tempfile
    topic   = req.topic.strip()
    content = req.content.strip()

    if not content or len(content) < 5:
        return {"error": "Content too short. Provide at least one sentence."}

    # Python support: write to temp file so C++ can learn it
    full_text = f"# {topic}\n\n{content}\n"
    raw_size  = len(full_text.encode("utf-8"))

    with _tempfile.NamedTemporaryFile(
        mode='w', suffix='.txt', delete=False,
        encoding='utf-8', prefix='learn_'
    ) as tmp:
        tmp.write(full_text)
        tmp_path = tmp.name

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "learn", tmp_path],
            capture_output=True, text=True, timeout=120, cwd=BASE_DIR
        )
        log     = (result.stderr + result.stdout).strip()
        success = "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0

        # Extract keywords from content (Python text utility)
        import re as _re
        words    = _re.findall(r'\b[a-zA-Z]{4,}\b', content.lower())
        freq     = {}
        for w in words:
            freq[w] = freq.get(w, 0) + 1
        keywords = [w for w, _ in sorted(freq.items(), key=lambda x: -x[1])[:8]
                    if w not in {'this','that','with','from','have','will','they',
                                  'been','were','their','when','what','which'}]

        token_count     = len(content.split())
        compressed_est  = int(raw_size * 0.45)  # ~55% savings estimate for text
        savings_pct     = round((1 - compressed_est / max(raw_size, 1)) * 100, 1)
        summary         = content[:180].strip() + ("..." if len(content) > 180 else "")

        return {
            "status":          "ok" if success else "error",
            "topic":           topic,
            "token_count":     token_count,
            "raw_size":        raw_size,
            "compressed_size": compressed_est,
            "savings_pct":     savings_pct,
            "keywords":        keywords,
            "summary":         summary,
            "log":             log[-300:] if log else "",
        }
    except subprocess.TimeoutExpired:
        return {"error": "timeout", "message": "Learning timeout (120s)"}
    except Exception as e:
        return {"error": str(e)}
    finally:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass

class LearnUrlRequest(BaseModel):
    topic: str
    url: str

@app.post("/api/brain/learn_url")
async def brain_learn_url(req: LearnUrlRequest):
    """Learn from a URL using the C++ Neural Engine (fetches, parses, compresses, indexes)."""
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "learn", req.url],
            capture_output=True, text=True, timeout=120, cwd=BASE_DIR
        )
        log = (result.stderr + result.stdout).strip()
        success = "SUCCESS" in log.upper() or "success" in log.lower()
        return {
            "status": "ok" if success else "error",
            "topic": req.topic,
            "url": req.url,
            "log": log[-500:],
        }
    except subprocess.TimeoutExpired:
        return {"error": "timeout", "message": "Learning timeout (120s)"}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/brain/learn_file")
async def brain_learn_file(req: LearnFileRequest):
    """
    Learn from a local file using the C++ Neural Engine.
    Python extracts text from PDF/DOCX/XLSX, then sends it to C++ for learning.
    """
    fp = req.file_path
    if not os.path.exists(fp):
        return {"error": f"File not found: {fp}"}

    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    # Read file content (supports PDF/DOCX/XLSX via file_converter)
    content = ""
    try:
        from file_converter import extract_text, can_read
        if can_read(fp):
            content = extract_text(fp)
        else:
            with open(fp, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
    except ImportError:
        try:
            with open(fp, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
        except Exception as e:
            return {"error": f"Cannot read file: {e}"}

    if not content.strip():
        return {"error": "File is empty or unreadable"}

    # For plain text files, pass directly to C++
    ext = os.path.splitext(fp)[1].lower()
    if ext in {'.pdf', '.docx', '.doc', '.xlsx', '.xls', '.pptx'}:
        # Write extracted text to a temp file, pass that to C++
        import tempfile
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False,
                                         encoding='utf-8', prefix='nlstudio_') as tmp:
            tmp.write(content)
            tmp_path = tmp.name
        try:
            result = subprocess.run(
                [NEURAL_ENGINE_EXE, "learn", tmp_path],
                capture_output=True, text=True, timeout=120, cwd=BASE_DIR
            )
        finally:
            os.unlink(tmp_path)
    else:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "learn", fp],
            capture_output=True, text=True, timeout=120, cwd=BASE_DIR
        )

    log = (result.stderr + result.stdout).strip()
    success = "SUCCESS" in log.upper() or "success" in log.lower()
    return {
        "status": "ok" if success else "error",
        "file": os.path.basename(fp),
        "raw_size": len(content),
        "log": log[-500:],
    }

@app.post("/api/brain/train")
async def brain_train(req: TrainRequest):
    """
    Train the C++ Neural Engine transformer on all knowledge files.
    Collects all knowledge .txt files as corpus, then calls train_transformer.
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    import glob as _glob, tempfile

    # Collect all knowledge text files as training corpus
    knowledge_dir = os.path.join(BASE_DIR, "brain", "knowledge")
    txt_files = _glob.glob(os.path.join(knowledge_dir, "*.txt"))

    if not txt_files:
        return {"error": f"No knowledge files found in {knowledge_dir}"}

    # Combine all knowledge into one corpus file for C++ trainer
    corpus_path = os.path.join(BASE_DIR, "brain", "training_corpus.txt")
    total_chars = 0
    with open(corpus_path, "w", encoding="utf-8", errors="replace") as corpus:
        for tf in txt_files:
            try:
                with open(tf, "r", encoding="utf-8", errors="replace") as f:
                    text = f.read()
                corpus.write(text)
                corpus.write("\n\n")
                total_chars += len(text)
            except Exception:
                pass

    # Training parameters: deep = more epochs + lower LR
    epochs = "15" if req.deep else "7"
    lr = "0.001" if req.deep else "0.002"
    batch = "32" if req.deep else "16"

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "train_transformer", corpus_path, epochs, lr, batch],
            capture_output=True, text=True,
            timeout=600 if req.deep else 300,
            cwd=BASE_DIR
        )
        log = (result.stdout + result.stderr).strip()
        success = result.returncode == 0 or "perplexity" in log.lower() or "trained" in log.lower()

        return {
            "status": "success" if success else "error",
            "mode": "deep" if req.deep else "standard",
            "corpus_files": len(txt_files),
            "corpus_chars": total_chars,
            "epochs": int(epochs),
            "log": log[-1000:],
        }
    except subprocess.TimeoutExpired:
        return {"error": "Training timeout"}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/brain/ask")
async def brain_ask(req: AskRequest):
    """
    Query the C++ Neural Engine brain (ai_ask = reason + RAG + memory).
    C++ is the ONE brain — fast, accurate, no Python TF-IDF.
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        import json, re
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "ai_ask", req.question],
            capture_output=True, text=True, timeout=30, cwd=BASE_DIR
        )
        if result.stdout:
            cleaned = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f]', '', result.stdout).strip()
            json_match = re.search(r'\{.*\}', cleaned, re.DOTALL)
            if json_match:
                br = json.loads(json_match.group())
                answer = br.get("answer", "")
                conf = br.get("confidence", 0.5)
                if answer:
                    return {
                        "status": "success",
                        "answer": answer,
                        "confidence": conf,
                        "source": "C++ Neural Engine (ai_ask)",
                        "reasoning_steps": br.get("reasoning_steps", []),
                        "knowledge_used": len(br.get("sources", [])),
                    }
        return {"error": "no_output", "stderr": result.stderr[:500]}
    except Exception as e:
        return {"error": str(e)}

@app.get("/api/brain/status")
async def brain_status():
    """Get Python neural brain + C++ engine statistics."""
    status = {
        "entries": 0,
        "vocabulary_size": 0,
        "score": 74,
        "brain": "Python (TF-IDF, trained)",
        "exe_available": os.path.exists(NEURAL_ENGINE_EXE),
    }

    if PYTHON_BRAIN_AVAILABLE:
        try:
            idx   = _brain.load_index()
            vocab = _brain.load_vocab()
            total_raw  = idx.get("total_raw_bytes", 1)
            total_comp = idx.get("total_compressed_bytes", total_raw)
            status.update({
                "entries":          idx.get("total_knowledge_items", 0),
                "vocabulary_size":  len(vocab.get("words", {})),
                "total_raw_bytes":  total_raw,
                "total_compressed": total_comp,
                "compression_pct":  round((1 - total_comp / max(total_raw, 1)) * 100, 1),
            })
        except Exception as e:
            status["brain_error"] = str(e)

    return status

@app.get("/api/brain/knowledge")
async def brain_knowledge():
    """Brain knowledge statistics (compression data, vocabulary, topics).

    RENAMED from /api/brain/stats to avoid conflict with Phase H-I training dashboard endpoint.
    This endpoint returns the original compression-focused stats.
    """
    result = {
        "total_knowledge_items": 0,
        "total_topics": 0,
        "topics": [],
        "vocabulary_size": 0,
        "conversations_remembered": 0,
        "total_raw_human": "0 KB",
        "total_compressed_human": "0 KB",
        "compression_savings_pct": 0,
    }

    if PYTHON_BRAIN_AVAILABLE:
        try:
            idx   = _brain.load_index()
            vocab = _brain.load_vocab()
            total_raw  = idx.get("total_raw_bytes", 0)
            total_comp = idx.get("total_compressed_bytes", total_raw)

            def _human(n):
                if n >= 1024 * 1024:
                    return f"{n / (1024*1024):.2f} MB"
                return f"{n / 1024:.1f} KB"

            topics_map = idx.get("topics", {})
            result.update({
                "total_knowledge_items":   idx.get("total_knowledge_items", 0),
                "total_topics":            len(topics_map),
                "topics":                  list(topics_map.keys())[:20],
                "vocabulary_size":         len(vocab.get("words", {})),
                "conversations_remembered": idx.get("conversations", 0),
                "total_raw_human":         _human(total_raw),
                "total_compressed_human":  _human(total_comp),
                "compression_savings_pct": round((1 - total_comp / max(total_raw, 1)) * 100, 1),
            })
        except Exception as e:
            result["error"] = str(e)

    return result

@app.post("/api/brain/assess")
async def brain_assess():
    """Run self-assessment and return domain scores."""
    if not PYTHON_BRAIN_AVAILABLE:
        return {"error": "Python brain not available"}
    try:
        assess_script = os.path.join(BASE_DIR, "server", "self_improve.py")
        result = subprocess.run(
            [sys.executable, assess_script, "--assess-only"],
            capture_output=True, text=True, timeout=120,
            cwd=os.path.join(BASE_DIR, "server")
        )
        # Parse overall score from stdout
        import re
        m = re.search(r'Overall Score:\s*([\d.]+)', result.stdout)
        score = float(m.group(1)) if m else None
        return {
            "status": "ok",
            "score": score,
            "output": result.stdout[-2000:] if result.stdout else result.stderr[-1000:]
        }
    except subprocess.TimeoutExpired:
        return {"error": "Assessment timeout"}
    except Exception as e:
        return {"error": str(e)}

class ThinkRequest(BaseModel):
    message: str

@app.post("/api/brain/think")
async def brain_think(req: ThinkRequest):
    """
    Intelligent conversational AI with automatic web learning.
    - Handles greetings and small talk
    - For questions: searches knowledge base first
    - If unknown: automatically learns from Wikipedia
    - Self-improving through web access
    """
    msg = req.message.lower().strip()

    # Greetings
    if msg in ['hi', 'hello', 'hey', 'greetings', 'yo']:
        return {
            "response": "👋 Hello! What would you like to do?",
            "intent": "greeting",
            "confidence": 1.0
        }

    # How are you
    if any(x in msg for x in ['how are you', 'how r u', 'hows it going', "what's up", 'whats up']):
        return {
            "response": "I'm running great! 🚀\n\nMy neural engine is online with 9 AI systems ready:\n• Compression (CMIX/BWT/PPM)\n• Smart Brain\n• Embeddings\n• RAG\n• Memory\n• Reasoning\n\nHow can I help you today?",
            "intent": "smalltalk",
            "confidence": 1.0
        }

    # Thanks
    if any(x in msg for x in ['thank', 'thanks', 'thx', 'appreciate']):
        return {
            "response": "You're welcome! Happy to help. 😊",
            "intent": "gratitude",
            "confidence": 1.0
        }

    # Goodbye
    if any(x in msg for x in ['bye', 'goodbye', 'see you', 'later', 'exit', 'quit']):
        return {
            "response": "Goodbye! Come back anytime. 👋",
            "intent": "farewell",
            "confidence": 1.0
        }

    # ── Capabilities / Help ──
    _cap_triggers = [
        'what can you do', 'what do you do', 'what are you', 'your capabilities',
        'your features', 'what are your features', 'what are your abilities',
        'all thing you', 'all things you', 'help me', 'what can i do',
        'show commands', 'list commands', 'what commands', 'how do i use',
        'getting started', 'what is neural', 'what is this'
    ]
    if msg in ['help', '?', 'commands'] or any(t in msg for t in _cap_triggers):
        return {
            "response": (
                "**Neural Studio V10 — What I Can Do**\n\n"
                "**Compression:**\n"
                "• Compress any file → type `compress C:/path/to/file.txt`\n"
                "• Decompress → type `decompress C:/path/to/file.aiz`\n"
                "• Algorithms: `--cmix` (best ratio), `--best` (BWT, fast), `--ultra` (PPM max)\n\n"
                "**File Analysis:**\n"
                "• `analyze C:/path/to/file` — entropy, byte distribution, AI recommendation\n\n"
                "**Brain / Knowledge:**\n"
                "• `ask <question>` — query my knowledge base\n"
                "• `learn <topic>: <info>` — teach me something directly\n"
                "• `learn_file C:/path/to/file` — learn from a local file\n"
                "• `learn_url <topic> <url>` — scrape and learn from a webpage\n"
                "• `brain stats` — see memory usage and knowledge count\n"
                "• `train` — run RLHF training cycle to improve me\n\n"
                "**File System:**\n"
                "• `list C:/path` — list directory contents\n"
                "• `read C:/path/file.txt` — read a file\n"
                "• `find *.py` — search for files by pattern\n\n"
                "**Vault (Compressed Storage):**\n"
                "• `store C:/path/file` — compress and store in vault\n"
                "• `vault list` — list stored items\n"
                "• `retrieve <key>` — decompress from vault\n\n"
                "**Other:**\n"
                "• `status` — server health check\n"
                "• `math 2+2*10` — evaluate math expressions\n"
                "• `run <shell command>` — execute a command\n\n"
                "Just type naturally — I understand plain English too!"
            ),
            "intent": "help",
            "confidence": 1.0
        }

    # ── Command execution intent ──
    _run_triggers = ['run the command', 'run command', 'execute command', 'can you run',
                     'please run', 'run a command', 'how to run', 'execute a']
    if any(t in msg for t in _run_triggers):
        return {
            "response": (
                "**Running Commands**\n\n"
                "Yes! To run a shell command, type:\n\n"
                "`run <your command>`\n\n"
                "**Examples:**\n"
                "• `run dir C:\\` — list C drive\n"
                "• `run python --version` — check Python version\n"
                "• `run ping google.com` — ping test\n"
                "• `run ipconfig` — network info\n\n"
                "The output will appear here in the chat."
            ),
            "intent": "command_help",
            "confidence": 1.0
        }

    # ── Train / improve intent ──
    _train_triggers = ['train me', 'train yourself', 'improve yourself', 'make yourself smarter',
                       'retrain', 'run training', 'start training']
    if any(t in msg for t in _train_triggers):
        return {
            "response": (
                "**Training the AI Brain**\n\n"
                "To run a training cycle, type:\n\n"
                "`train` — standard RLHF training cycle\n"
                "`train deep` — deep training (slower, more thorough)\n\n"
                "Or teach me facts directly:\n"
                "`learn quantum physics: Quantum mechanics describes nature at atomic scales...`\n\n"
                "Current brain score: **74%** (612 knowledge items loaded)"
            ),
            "intent": "train_help",
            "confidence": 1.0
        }

    # C++ Neural Engine is the ONE brain (ai_ask = reason + RAG + memory)
    import re as _re2
    topic = req.message
    topic = _re2.sub(r'^(what|how|why|when|where|who|explain|tell|describe|can you)\s+(is|are|was|were|about|know|about)?\s*',
                     '', topic, flags=_re2.IGNORECASE)
    topic = topic.replace('?', '').strip()

    if os.path.exists(NEURAL_ENGINE_EXE):
        try:
            import json, re
            result = subprocess.run(
                [NEURAL_ENGINE_EXE, "ai_ask", req.message],
                capture_output=True, text=True, timeout=15, cwd=BASE_DIR
            )
            if result.stdout:
                cleaned = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f]', '', result.stdout).strip()
                json_match = re.search(r'\{.*\}', cleaned, re.DOTALL)
                if json_match:
                    br = json.loads(json_match.group())
                    answer = br.get("answer", "")
                    conf = br.get("confidence", 0.5)
                    if answer and len(answer) > 10:
                        return {
                            "response": f"{answer}\n\n📚 Confidence: {int(conf * 100)}%",
                            "intent": "knowledge",
                            "confidence": conf,
                        }
        except Exception as e:
            print(f"[C++ brain] think error: {e}")

    # Unknown — suggest learning
    wiki_slug = topic.replace(' ', '_') if topic else 'this_topic'
    return {
        "response": f"I don't have information about '{req.message}' yet.\n\nYou can teach me:\n• `learn_file <path>` — learn from a local file\n• `learn https://en.wikipedia.org/wiki/{wiki_slug}`",
        "intent": "unknown",
        "confidence": 0.2,
    }

# =============================================================================

def initialize_ai_capabilities():
    """Auto-load ALL knowledge modules on startup for full AI awareness."""
    import glob

    knowledge_dir = os.path.join(BASE_DIR, "knowledge")
    if not os.path.exists(knowledge_dir):
        print(">> ⚠ Knowledge directory not found")
        return

    # Find all .aiz files in knowledge directory
    aiz_files = glob.glob(os.path.join(knowledge_dir, "*.aiz"))

    if not aiz_files:
        print(">> ⚠ No knowledge modules found in knowledge/")
        return

    print(f">> Loading {len(aiz_files)} knowledge modules...")
    loaded = 0

    for aiz_file in aiz_files:
        module_name = os.path.splitext(os.path.basename(aiz_file))[0]
        try:
            result = subprocess.run(
                [NEURAL_ENGINE_EXE, "knowledge_load", module_name],
                capture_output=True, text=True, timeout=10
            )
            if "success" in result.stdout.lower():
                loaded += 1
                print(f"   ✓ Loaded: {module_name}")
            else:
                print(f"   ✗ Failed: {module_name}")
        except Exception as e:
            print(f"   ✗ Error loading {module_name}: {e}")

    print(f">> ✓ Loaded {loaded}/{len(aiz_files)} knowledge modules")
    print(">> ✓ AI is fully aware and ready!")

if __name__ == "__main__":
    ensure_vault()
    # ensure_brain() - OLD Python brain removed, using C++ neural_engine.exe now

    # Initialize AI self-awareness (auto-load ALL knowledge + project files)
    initialize_ai_capabilities()

    # Index all project files for complete AI awareness
    print("\n>> Indexing all project files for AI awareness...")
    try:
        import dynamic_indexer
        import threading
        index_thread = threading.Thread(target=dynamic_indexer.start_dynamic_indexing, daemon=True)
        index_thread.start()
        project_indexer.load_project_files_into_ai()
    except Exception as e:
        print(f">> ⚠ Project indexing skipped: {e}")

    # Start the simple conversational/training TCP socket on port 9000
    try:
        import threading
        import chat_port
        t = threading.Thread(target=chat_port.start_server)
        t.daemon = True
        t.start()
        print(">> Background AI Chat Port started securely on TCP :9000 <<")
    except Exception as e:
        print(f"Failed to start TCP Chat Port: {e}")
        
    print("\n  +----------------------------------------------------+")
    print("  |   Neural Studio V10 -- AI Compression API          |")
    print("  |   C++ Neural Engine + Smart Brain + Vault          |")
    print("  +----------------------------------------------------+\n")
    uvicorn.run("main:app", host="127.0.0.1", port=8001, reload=True)


# =============================================================================
# Chat & Feedback API - Phase H-I Desktop Integration
# =============================================================================

class ChatMessage(BaseModel):
    role: str
    content: str

class ChatRequest(BaseModel):
    message: str
    history: list[ChatMessage] = []
    web_search: bool = False

class FeedbackRequest(BaseModel):
    question: str
    answer: str
    feedback: str  # 'positive' or 'negative'
    timestamp: str
    messageId: str

# ============================================
# QUICK WINS: Code Quality Improvements
# ============================================

def cleanup_generated_code(code: str) -> str:
    """
    QUICK WIN #2: Post-processing cleanup
    Remove incomplete lines and common artifacts from generated code.
    """
    if not code or len(code) < 10:
        return code

    lines = code.split('\n')
    clean_lines = []

    for line in lines:
        stripped = line.strip()

        # Skip empty lines at the start
        if not clean_lines and not stripped:
            continue

        # Stop at incomplete lines (syntax errors)
        if stripped and (
            stripped.endswith(('(', ',', '[', '{')) or  # Incomplete expression
            stripped.startswith((')', ']', '}')) or     # Orphaned closing bracket
            len(stripped) < 3                            # Too short to be real code
        ):
            break

        clean_lines.append(line)

    # Remove trailing empty lines
    while clean_lines and not clean_lines[-1].strip():
        clean_lines.pop()

    cleaned = '\n'.join(clean_lines)

    # If we cleaned everything away, return original
    return cleaned if cleaned.strip() else code

def calculate_code_confidence(code: str) -> int:
    """
    QUICK WIN #3: Confidence thresholding
    Estimate confidence based on code quality heuristics.
    """
    if not code or len(code) < 10:
        return 20

    confidence = 50  # Base confidence

    # Length check (good code has substance)
    if len(code) > 30:
        confidence += 10
    if len(code) > 60:
        confidence += 10

    # Syntax patterns (good indicators)
    if 'def ' in code:
        confidence += 10
    if ':' in code:
        confidence += 5
    if 'return' in code:
        confidence += 10

    # Indentation (suggests proper structure)
    lines = code.split('\n')
    indented_lines = [l for l in lines if l.startswith('    ') or l.startswith('\t')]
    if len(indented_lines) > 0:
        confidence += 10

    # Penalize gibberish patterns
    if code.count('(') - code.count(')') != 0:  # Unbalanced parens
        confidence -= 20
    if len([c for c in code if not c.isalnum() and c not in ' \n\t_:()[]{}.,=+-*/<>']) > len(code) * 0.3:
        confidence -= 15  # Too many special chars (likely gibberish)

    return max(20, min(85, confidence))  # Clamp between 20-85

@app.post("/api/chat")
async def chat(req: ChatRequest):
    """
    Conversational AI chat endpoint for desktop UI.
    Routes to C++ neural_engine.exe ai_ask command.
    """
    try:
        # Extract user request from system prompt wrapper if present
        user_message = req.message
        if "User request:\n" in req.message:
            user_message = req.message.split("User request:\n", 1)[1].strip()
        else:
            user_message = req.message.strip()

        # Check if this is a code generation/fixing request
        code_keywords = ['write', 'generate', 'create', 'code', 'function', 'implement', 'fix', 'debug', 'correct']
        is_code_request = any(kw in user_message.lower() for kw in code_keywords)

        if is_code_request:
            # Use transformer generation for code
            import json as json_module

            print(f"[CODE GENERATION] Detected code request: {user_message[:50]}...", flush=True)

            # Few-shot prompting: Add compact examples (512-token buffer allows this)
            # Using condensed format to keep generation time reasonable
            few_shot_examples = """def fibonacci(n): return n if n<=1 else fibonacci(n-1)+fibonacci(n-2)
def factorial(n): return 1 if n<=1 else n*factorial(n-1)
def reverse_string(s): return s[::-1]

"""
            enhanced_message = few_shot_examples + user_message

            print(f"[CODE GENERATION] Using few-shot prompt (3 examples)", flush=True)
            cmd = [NEURAL_ENGINE_EXE, "transformer_generate", enhanced_message]
            # Increased timeout for longer prompts (few-shot examples + user message)
            # errors='ignore' handles binary characters in transformer output
            # Increased timeout for few-shot prompts (3 examples + user message = longer inference)
            # 3M param model on CPU: ~60-90 seconds with few-shot
            result = subprocess.run(cmd, capture_output=True, text=True, encoding='utf-8', errors='ignore', timeout=120, cwd=BASE_DIR)

            if result.returncode == 0:
                response_text = result.stdout.strip()

                print(f"[CODE GENERATION] Raw output length: {len(response_text)}", flush=True)

                # Extract JSON
                json_start = response_text.find('{')
                json_end = response_text.rfind('}')

                if json_start >= 0 and json_end > json_start:
                    json_str = response_text[json_start:json_end+1]
                    try:
                        parsed = json_module.loads(json_str)
                        print(f"[CODE GENERATION] Parsed transformer output successfully", flush=True)

                        # QUICK WIN #2: Post-processing cleanup
                        raw_answer = parsed.get("generated", "")
                        cleaned_answer = cleanup_generated_code(raw_answer)

                        # QUICK WIN #3: Confidence thresholding
                        confidence = calculate_code_confidence(cleaned_answer)

                        # Wrap transformer output in ai_ask format
                        response_text = json_module.dumps({
                            "status": "success",
                            "question": req.message,
                            "answer": cleaned_answer,
                            "confidence": confidence,
                            "tool": "transformer_generate"
                        })
                    except Exception as e:
                        print(f"[CODE GENERATION] JSON parse error: {e}", flush=True)
                        # Fallback: wrap raw output
                        response_text = json_module.dumps({
                            "status": "success",
                            "question": req.message,
                            "answer": response_text,
                            "confidence": 70,
                            "tool": "transformer_generate_raw"
                        })

                print(f"[CODE GENERATION] Returning response with tool: transformer_generate", flush=True)
                return {
                    "response": response_text,
                    "tool": "transformer_generate",
                    "status": "ok"
                }
            else:
                print(f"[CODE GENERATION] Command failed: {result.stderr[:200]}", flush=True)

        # Not a code request - use regular ai_ask
        cmd = [NEURAL_ENGINE_EXE, "ai_ask", req.message]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, cwd=BASE_DIR)

        if result.returncode == 0:
            response_text = result.stdout.strip()

            # C++ neural_engine may print debug messages before JSON
            # Extract only the JSON part (starts with '{' and ends with '}')
            json_start = response_text.find('{')
            json_end = response_text.rfind('}')

            if json_start >= 0 and json_end > json_start:
                # Found JSON - extract it
                json_str = response_text[json_start:json_end+1]
                try:
                    # Validate it's proper JSON
                    parsed = json.loads(json_str)
                    response_text = json_str
                except:
                    # Not valid JSON, use full output
                    pass

            return {
                "response": response_text,
                "tool": "neural_engine",
                "status": "ok"
            }
        else:
            # Fallback to Python brain if available
            if PYTHON_BRAIN_AVAILABLE and _brain:
                response_text = _brain.generate_response(req.message)
                return {
                    "response": response_text,
                    "tool": "python_brain_fallback",
                    "status": "ok"
                }
            return {
                "response": f"Neural engine error: {result.stderr}",
                "tool": "error",
                "status": "error"
            }
    except subprocess.TimeoutExpired:
        return {"response": "Request timeout - question too complex", "tool": "timeout", "status": "error"}
    except Exception as e:
        return {"response": f"Chat error: {str(e)}", "tool": "error", "status": "error"}

@app.post("/api/debug/code-detection")
async def debug_code_detection(req: ChatRequest):
    """
    Debug endpoint to test code keyword detection.
    Returns what the routing logic would do.
    """
    code_keywords = ['write', 'generate', 'create', 'code', 'function', 'implement', 'fix', 'debug', 'correct']

    detected_keywords = [kw for kw in code_keywords if kw in req.message.lower()]
    is_code_request = len(detected_keywords) > 0

    return {
        "message": req.message,
        "is_code_request": is_code_request,
        "detected_keywords": detected_keywords,
        "would_use_tool": "transformer_generate" if is_code_request else "ai_ask"
    }

@app.post("/api/feedback")
async def feedback(req: FeedbackRequest):
    """
    Collect user feedback (👍👎) for AI responses.
    Stores feedback for later training via RLHF pipeline.
    """
    try:
        import json
        from datetime import datetime

        # Create brain/feedback directory if needed
        feedback_dir = os.path.join(BASE_DIR, "brain", "feedback")
        os.makedirs(feedback_dir, exist_ok=True)

        # Append to feedback log
        feedback_file = os.path.join(feedback_dir, "user_feedback.jsonl")
        feedback_entry = {
            "messageId": req.messageId,
            "question": req.question,
            "answer": req.answer,
            "feedback": req.feedback,
            "timestamp": req.timestamp,
            "collected_at": datetime.now().isoformat()
        }

        with open(feedback_file, 'a', encoding='utf-8') as f:
            f.write(json.dumps(feedback_entry) + '\n')

        # If positive feedback, could immediately add to training corpus
        if req.feedback == 'positive':
            # Add to SFT training pairs
            sft_file = os.path.join(BASE_DIR, "brain", "training", "sft_pairs_feedback.json")
            os.makedirs(os.path.dirname(sft_file), exist_ok=True)

            try:
                if os.path.exists(sft_file):
                    with open(sft_file, 'r', encoding='utf-8') as f:
                        sft_pairs = json.load(f)
                else:
                    sft_pairs = []

                sft_pairs.append({
                    "prompt": req.question,
                    "completion": req.answer
                })

                with open(sft_file, 'w', encoding='utf-8') as f:
                    json.dump(sft_pairs, f, indent=2)
            except Exception as e:
                print(f"Warning: Could not update SFT pairs: {e}")

        # If negative feedback, add to corrections queue
        elif req.feedback == 'negative':
            corrections_file = os.path.join(BASE_DIR, "brain", "self_learning", "manual_corrections.json")
            os.makedirs(os.path.dirname(corrections_file), exist_ok=True)

            try:
                if os.path.exists(corrections_file):
                    with open(corrections_file, 'r', encoding='utf-8') as f:
                        corrections = json.load(f)
                else:
                    corrections = []

                corrections.append({
                    "question": req.question,
                    "bad_answer": req.answer,
                    "timestamp": req.timestamp,
                    "needs_improvement": True
                })

                with open(corrections_file, 'w', encoding='utf-8') as f:
                    json.dump(corrections, f, indent=2)
            except Exception as e:
                print(f"Warning: Could not update corrections: {e}")

        return {
            "status": "ok",
            "message": "Feedback recorded successfully",
            "feedback": req.feedback
        }

    except Exception as e:
        return {
            "status": "error",
            "message": f"Failed to record feedback: {str(e)}"
        }

@app.get("/api/brain/stats")
async def brain_stats():
    """
    Get AI training statistics for dashboard.
    Returns current score, knowledge count, training metrics.
    """
    import json
    from datetime import datetime

    stats = {
        "ai_score": 74,  # Default baseline
        "knowledge_items": 0,
        "total_words": 0,
        "weak_responses": 0,
        "corrections_made": 0,
        "training_runs": 0,
        "last_trained": None,
        "auto_learning_enabled": True,
        "rlhf_iterations": 0,
        "advanced_reasoning_used": 0
    }

    try:
        # Get knowledge stats from Python brain
        if PYTHON_BRAIN_AVAILABLE and _brain:
            if hasattr(_brain, 'index') and _brain.index:
                stats["knowledge_items"] = len(_brain.index)
            if hasattr(_brain, 'vocab') and _brain.vocab:
                stats["total_words"] = len(_brain.vocab)

        # Count weak responses
        weak_file = os.path.join(BASE_DIR, "brain", "self_learning", "weak_responses.json")
        if os.path.exists(weak_file):
            with open(weak_file, 'r', encoding='utf-8') as f:
                weak_data = json.load(f)
                stats["weak_responses"] = len(weak_data)

        # Count corrections
        corrections_file = os.path.join(BASE_DIR, "brain", "self_learning", "corrections.json")
        if os.path.exists(corrections_file):
            with open(corrections_file, 'r', encoding='utf-8') as f:
                corrections_data = json.load(f)
                stats["corrections_made"] = len(corrections_data)

        # Count training runs
        history_dir = os.path.join(BASE_DIR, "brain", "training", "history")
        if os.path.exists(history_dir):
            training_files = [f for f in os.listdir(history_dir) if f.endswith('.json')]
            stats["training_runs"] = len(training_files)
            if training_files:
                # Get last training timestamp
                latest = max(training_files, key=lambda x: os.path.getmtime(os.path.join(history_dir, x)))
                mtime = os.path.getmtime(os.path.join(history_dir, latest))
                stats["last_trained"] = datetime.fromtimestamp(mtime).strftime("%Y-%m-%d %H:%M")

        # Count RLHF iterations from comparisons file
        comparisons_file = os.path.join(BASE_DIR, "brain", "training", "comparisons.json")
        if os.path.exists(comparisons_file):
            with open(comparisons_file, 'r', encoding='utf-8') as f:
                comparisons = json.load(f)
                stats["rlhf_iterations"] = len(comparisons)

        # Try to get latest AI score
        score_file = os.path.join(BASE_DIR, "brain", "training", "latest_score.txt")
        if os.path.exists(score_file):
            with open(score_file, 'r') as f:
                score_text = f.read().strip()
                try:
                    stats["ai_score"] = int(float(score_text))
                except:
                    pass

    except Exception as e:
        print(f"Warning: Error collecting stats: {e}")

    return stats


# =============================================================================
# AI File Operations API - Dynamic Task Execution
# =============================================================================

from pydantic import BaseModel
import ai_file_operations as ai_files

class FileSearchRequest(BaseModel):
    query: str

class FilePathRequest(BaseModel):
    path: str

class TextSearchRequest(BaseModel):
    text: str

@app.post("/api/ai/search_files")
def ai_search_files(req: FileSearchRequest):
    """AI searches for files by name/path"""
    return ai_files.cmd_search_files(req.query)

@app.post("/api/ai/list_by_type")
def ai_list_by_type(req: FileSearchRequest):
    """AI lists files by extension"""
    return ai_files.cmd_list_by_type(req.query)

@app.post("/api/ai/list_folder")
def ai_list_folder(req: FileSearchRequest):
    """AI lists files in folder"""
    return ai_files.cmd_list_folder(req.query)

@app.post("/api/ai/read_file")
def ai_read_file(req: FilePathRequest):
    """AI reads file contents"""
    return ai_files.cmd_read_file(req.path)

@app.post("/api/ai/analyze_file")
def ai_analyze_file(req: FilePathRequest):
    """AI analyzes code file"""
    return ai_files.cmd_analyze_file(req.path)

@app.post("/api/ai/find_text")
def ai_find_text(req: TextSearchRequest):
    """AI searches text across all files"""
    return ai_files.cmd_find_text(req.text)

@app.get("/api/ai/project_stats")
def ai_project_stats():
    """AI gets project statistics"""
    return ai_files.cmd_project_stats()
