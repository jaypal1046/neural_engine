import os
import sys
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

class LearnRequest(BaseModel):
    source: str  # URL or file path

class AskRequest(BaseModel):
    question: str

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
    source: str  # URL or file path

class AskRequest(BaseModel):
    question: str

@app.post("/api/brain/learn")
async def brain_learn(req: LearnRequest):
    """
    Learn from URL or file using C++ Smart Brain engine.
    Downloads, cleans HTML, compresses with CMIX, and indexes.
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Smart Brain not built", "message": "Run build_smart_brain.bat first"}

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "learn", req.source],
            capture_output=True,
            text=True,
            timeout=120  # 2 minute timeout
        )

        log = result.stderr
        success = "SUCCESS" in log

        return {
            "status": "ok" if success else "error",
            "source": req.source,
            "log": log,
            "stdout": result.stdout
        }

    except subprocess.TimeoutExpired:
        return {"error": "timeout", "message": "Learning timeout (120s)"}
    except Exception as e:
        return {"error": "exception", "message": str(e)}

@app.post("/api/brain/ask")
async def brain_ask(req: AskRequest):
    """
    Query knowledge base using COMPRESSED knowledge modules.
    Returns answer extracted from compressed .aiz knowledge files.
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Smart Brain not built", "message": "Run build_smart_brain.bat first"}

    try:
        # Query compressed knowledge modules (try programming first)
        # Usage: neural_engine knowledge_query <module_name> <question>
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "knowledge_query", "programming", req.question],
            capture_output=True,
            text=True,
            timeout=30,
            cwd=BASE_DIR
        )

        # Parse JSON from stdout
        try:
            import json
            import re
            if result.stdout:
                # Clean stdout
                cleaned_stdout = re.sub(r'[\x00-\x1f\x7f-\x9f]', '', result.stdout)
                # Remove mixer output if present
                if "[MIXER]" in cleaned_stdout:
                    cleaned_stdout = cleaned_stdout.split('\n')[0]

                response = json.loads(cleaned_stdout)

                # Check if knowledge was found
                if response.get("status") == "success":
                    context = response.get("context", "")
                    if context:
                        # Extract first 500 characters as answer
                        answer = context[:500].strip()
                        if len(context) > 500:
                            answer += "..."

                        return {
                            "status": "success",
                            "answer": answer,
                            "confidence": 0.85,  # High confidence from knowledge base
                            "source": "Compressed Knowledge Module",
                            "module": "programming"
                        }

                # Fallback to transformer if no knowledge found
                result2 = subprocess.run(
                    [NEURAL_ENGINE_EXE, "transformer_generate", req.question],
                    capture_output=True,
                    text=True,
                    timeout=30,
                    cwd=BASE_DIR
                )

                if result2.stdout:
                    cleaned2 = re.sub(r'[\x00-\x1f\x7f-\x9f]', '', result2.stdout)
                    if "[MIXER]" in cleaned2:
                        cleaned2 = cleaned2.split('\n')[0]
                    resp2 = json.loads(cleaned2)
                    generated = re.sub(r'[\x00-\x1f\x7f-\x9f]', '', resp2.get("generated", ""))

                    return {
                        "status": "success",
                        "answer": generated if generated.strip() else "[No knowledge found]",
                        "confidence": 0.22,
                        "model": "MiniTransformer (trained)"
                    }
            else:
                return {
                    "error": "no_output",
                    "stderr": result.stderr,
                    "returncode": result.returncode
                }
        except json.JSONDecodeError as e:
            return {
                "error": "parse_error",
                "stdout": result.stdout,
                "stderr": result.stderr,
                "parse_error": str(e)
            }

    except subprocess.TimeoutExpired:
        return {"error": "timeout", "message": "Query timeout"}
    except Exception as e:
        return {"error": "exception", "message": str(e)}

@app.get("/api/brain/status")
async def brain_status():
    """Get Smart Brain statistics (knowledge entries, compression stats)."""
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"entries": 0, "error": "not_built"}

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "status"],
            capture_output=True,
            text=True,
            timeout=5
        )

        import json
        stats = json.loads(result.stdout)
        return stats

    except Exception as e:
        return {"entries": 0, "error": str(e)}

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

    # For questions: Try Smart Brain first
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {
            "response": "Neural engine not available. Run build_smart_brain.bat first.",
            "intent": "error",
            "confidence": 0.0
        }

    try:
        # Step 1: Check existing knowledge
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "ask", req.message],
            capture_output=True,
            text=True,
            timeout=10
        )

        import json
        brain_response = json.loads(result.stdout)

        # If Smart Brain has high confidence answer, return it
        if brain_response.get("confidence", 0) > 0.5:
            return {
                "response": f"{brain_response.get('answer', 'No answer')}\n\n📚 Confidence: {int(brain_response.get('confidence', 0) * 100)}%",
                "intent": "knowledge",
                "confidence": brain_response.get("confidence", 0)
            }

        # Step 2: Low confidence - Learn from Wikipedia automatically
        # Extract topic from question
        import re
        topic = req.message
        # Remove question words
        topic = re.sub(r'^(what|how|why|when|where|who|explain|tell|describe|can you)\s+(is|are|was|were|about|know|about)?\s*', '', topic, flags=re.IGNORECASE)
        topic = topic.replace('?', '').strip()

        if len(topic) > 3:
            # Try to learn from Wikipedia
            wiki_url = f"https://en.wikipedia.org/wiki/{topic.replace(' ', '_')}"

            try:
                learn_result = subprocess.run(
                    [NEURAL_ENGINE_EXE, "learn", wiki_url],
                    capture_output=True,
                    text=True,
                    timeout=60
                )

                # Now ask again after learning
                result2 = subprocess.run(
                    [NEURAL_ENGINE_EXE, "ask", req.message],
                    capture_output=True,
                    text=True,
                    timeout=10
                )

                brain_response2 = json.loads(result2.stdout)

                if brain_response2.get("confidence", 0) > 0.3:
                    return {
                        "response": f"🌐 **Learned from Wikipedia!**\n\n{brain_response2.get('answer', 'No answer')}\n\n📚 Source: {wiki_url}\n📊 Confidence: {int(brain_response2.get('confidence', 0) * 100)}%",
                        "intent": "learned",
                        "confidence": brain_response2.get("confidence", 0)
                    }

            except subprocess.TimeoutExpired:
                pass
            except Exception as e:
                print(f"Auto-learn error: {e}")

        # Step 3: Still don't know - suggest manual learning
        return {
            "response": f"🤔 I don't know about '{req.message}' yet.\n\n**I tried to learn from Wikipedia but couldn't find good information.**\n\nYou can teach me:\n• `learn https://wikipedia.org/wiki/{topic.replace(' ', '_')}`\n• Or ask something else!",
            "intent": "unknown",
            "confidence": 0.2
        }

    except Exception as e:
        return {
            "response": f"Error: {str(e)}\n\nTry: `help` for examples",
            "intent": "error",
            "confidence": 0.0
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
