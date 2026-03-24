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
import shlex
import tempfile
import time
import threading
from collections import Counter
from typing import Optional, List, Any
from pathlib import Path
from fastapi import FastAPI, HTTPException
from fastapi.responses import StreamingResponse
from fastapi.middleware.cors import CORSMiddleware
import uvicorn
from pydantic import BaseModel
import json
import re
import requests
from urllib.parse import quote
try:
    from .llm_adapter import OllamaAdapter
except ImportError:
    try:
        from llm_adapter import OllamaAdapter
    except ImportError:
        OllamaAdapter = None

try:
    from .agent_orchestrator import NeroAgentOrchestrator
except ImportError:
    try:
        from agent_orchestrator import NeroAgentOrchestrator
    except ImportError:
        NeroAgentOrchestrator = None
try:
    from .context_provider import ContextProvider
except ImportError:
    try:
        from context_provider import ContextProvider
    except ImportError:
        ContextProvider = None
try:
    from .task_intelligence import LocalTaskIntelligence
except ImportError:
    try:
        from task_intelligence import LocalTaskIntelligence
    except ImportError:
        LocalTaskIntelligence = None
try:
    from .code_graph_engine import CodeGraphEngine
except ImportError:
    try:
        from code_graph_engine import CodeGraphEngine
    except ImportError:
        CodeGraphEngine = None
try:
    from .review_pipeline import (
        is_review_request,
        patch_review_system_prompt,
        normalize_patch_review_response,
        review_system_prompt,
        normalize_review_response,
        format_review_markdown,
    )
except ImportError:
    try:
        from review_pipeline import (
            is_review_request,
            patch_review_system_prompt,
            normalize_patch_review_response,
            review_system_prompt,
            normalize_review_response,
            format_review_markdown,
        )
    except ImportError:
        is_review_request = None
        patch_review_system_prompt = None
        normalize_patch_review_response = None
        review_system_prompt = None
        normalize_review_response = None
        format_review_markdown = None
try:
    from .skill_manager import skill_manager
except ImportError:
    try:
        from skill_manager import skill_manager
    except ImportError:
        skill_manager = None

try:
    from .modify_pipeline import (

        modify_system_prompt,
        build_impact_brief,
        extract_json_object as extract_modify_json,
        apply_selection_to_text,
        build_unified_diff,
        validate_candidate_change,
        format_modify_markdown,
    )
except ImportError:
    try:
        from modify_pipeline import (
            modify_system_prompt,
            build_impact_brief,
            extract_json_object as extract_modify_json,
            apply_selection_to_text,
            build_unified_diff,
            validate_candidate_change,
            format_modify_markdown,
        )
    except ImportError:
        modify_system_prompt = None
        build_impact_brief = None
        extract_modify_json = None
        apply_selection_to_text = None
        build_unified_diff = None
        validate_candidate_change = None
        format_modify_markdown = None

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
# Models & Configuration
# =============================================================================

DEFAULT_CHAT_MODEL = "llama3"
FAST_CHAT_MODEL = "tinyllama"

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

class OllamaStatusResponse(BaseModel):
    ollama_running: bool
    models: List[str]
    default_model: str
    error: Optional[str] = None

class PullModelRequest(BaseModel):
    model: str

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

# ============================================
# SECURITY: Workspace Path Validation
# ============================================
DEFAULT_WORKSPACE_ROOT = os.path.realpath(BASE_DIR)
WORKSPACE_ROOT = DEFAULT_WORKSPACE_ROOT
SAFE_COMMANDS = {
    "git",
    "python",
    "python3",
    "py",
    "pytest",
    "node",
    "npm",
    "npx",
    "cmake",
    "ctest",
    "g++",
    "clang++",
    "ollama",
}
SHELL_METACHARS = {"&&", "||", ";", "|", ">", "<", "$(", "`"}
MAX_CHAT_HISTORY = 8
_CONTEXT_PROVIDER_CACHE: dict[str, Any] = {}
_TASK_INTELLIGENCE_CACHE: dict[str, Any] = {}
_CODE_GRAPH_ENGINE_CACHE: dict[str, Any] = {}


def resolve_workspace_root(path_str: Optional[str] = None) -> str:
    """Resolve the active workspace root, falling back to the server repo."""
    if not path_str:
        return DEFAULT_WORKSPACE_ROOT

    try:
        resolved = os.path.realpath(os.path.abspath(os.path.expanduser(path_str)))
    except Exception:
        return DEFAULT_WORKSPACE_ROOT

    return resolved if os.path.isdir(resolved) else DEFAULT_WORKSPACE_ROOT


def get_context_provider(workspace_root: Optional[str] = None):
    root = resolve_workspace_root(workspace_root)
    if ContextProvider is None:
        return None
    if root not in _CONTEXT_PROVIDER_CACHE:
        _CONTEXT_PROVIDER_CACHE[root] = ContextProvider(root)
    return _CONTEXT_PROVIDER_CACHE[root]


def get_task_intelligence(workspace_root: str):
    if not workspace_root:
        return None
    # Use realpath for case-insensitive matching on Windows
    norm_root = os.path.realpath(workspace_root)
    if LocalTaskIntelligence is None:
        return None
    if norm_root not in _TASK_INTELLIGENCE_CACHE:
        _TASK_INTELLIGENCE_CACHE[norm_root] = LocalTaskIntelligence(norm_root)
    return _TASK_INTELLIGENCE_CACHE[norm_root]


def get_code_graph_engine(workspace_root: Optional[str] = None):
    root = resolve_workspace_root(workspace_root)
    if CodeGraphEngine is None:
        return None
    if root not in _CODE_GRAPH_ENGINE_CACHE:
        _CODE_GRAPH_ENGINE_CACHE[root] = CodeGraphEngine(root)
    return _CODE_GRAPH_ENGINE_CACHE[root]


def request_workspace_root(req: Optional["ChatRequest"] = None, task_prep: Optional[dict[str, Any]] = None) -> str:
    if task_prep and task_prep.get("workspace_root"):
        return resolve_workspace_root(str(task_prep.get("workspace_root")))
    if req is not None and getattr(req, "workspace_root", None):
        return resolve_workspace_root(str(req.workspace_root))
    return DEFAULT_WORKSPACE_ROOT


def is_within_workspace(path_str: str, workspace_root: Optional[str] = None) -> bool:
    """Return True when the resolved path stays inside the workspace root."""
    try:
        root = resolve_workspace_root(workspace_root)
        resolved = os.path.realpath(os.path.abspath(os.path.expanduser(path_str)))
        return os.path.commonpath([root, resolved]) == root
    except ValueError:
        return False


def validate_path(path_str: str, workspace_root: Optional[str] = None) -> str:
    """Ensure paths stay within the authorized workspace root."""
    if not path_str:
        raise HTTPException(status_code=400, detail="Path cannot be empty")

    root = resolve_workspace_root(workspace_root)
    resolved = os.path.realpath(os.path.abspath(os.path.expanduser(path_str)))
    if not is_within_workspace(resolved, root):
        print(f"[SECURITY] Path traversal attempt blocked: {resolved}", flush=True)
        raise HTTPException(status_code=403, detail=f"Access denied: {path_str} is outside workspace")
    return resolved


def format_chat_history(history: list["ChatMessage"]) -> str:
    """Format the most recent chat history for the C++ brain."""
    recent_messages = history[-4:]
    if not recent_messages:
        return ""

    lines = []
    for msg in recent_messages:
        role = (msg.role or "user").strip().lower()
        content = (msg.content or "").strip()
        if not content:
            continue
        compact = re.sub(r"\s+", " ", content)
        if len(compact) > 240:
            compact = compact[:240] + "..."
        lines.append(f"{role.upper()}: {compact}")
    return "\n".join(lines)


def build_agent_task(req: "ChatRequest", task_prep: Optional[dict[str, Any]] = None) -> str:
    """Build the connector payload sent to the C++ brain."""
    user_message = req.message.strip()
    if "User request:\n" in user_message:
        user_message = user_message.split("User request:\n", 1)[1].strip()

    history_block = format_chat_history(req.history)
    if not history_block and not req.web_search:
        return user_message

    sections = []
    
    # Inject browser capabilities
    sections.extend([
        "BROWSER CAPABILITY: You have a built-in web browser. DO NOT say you cannot browse the internet.",
        "Instead, use the following commands exactly as shown to surf the web:",
        "  [BROWSER: navigate <url>]       - go to a specific URL",
        "  [BROWSER: getContent]           - get page text and title",
        "  [BROWSER: getHTML]              - get raw HTML",
        "  [BROWSER: getElement <selector>]- copy an element by CSS selector",
        "  [BROWSER: consoleLogs]          - view browser console",
        "  [BROWSER: screenshot]           - take a screenshot",
        ""
    ])

    if history_block:
        sections.extend([
            "Recent conversation:",
            history_block,
            "",
        ])
    if req.web_search:
        sections.extend([
            "Web assistance is enabled if needed.",
            "",
        ])
    if task_prep and task_prep.get("analysis_summary"):
        sections.extend([
            "Local workspace preparation:",
            task_prep["analysis_summary"],
            "",
        ])
    sections.extend([
        "Current request:",
        user_message,
    ])
    return "\n".join(sections).strip()

@app.post("/api/cmd")
def run_command(payload: CommandRequest):
    """
    SECURITY: Run only explicitly allowed local development commands.
    """
    try:
        cmd_args = shlex.split(payload.command)
        if not cmd_args:
            raise HTTPException(status_code=400, detail="Command cannot be empty")

        lowered = payload.command.lower()
        if any(token in payload.command for token in SHELL_METACHARS):
            raise HTTPException(status_code=403, detail="Shell chaining and redirection are blocked")

        dangerous = ["rm ", "rmdir ", "del ", "format", "mkfs", "dd ", "shutdown", "taskkill /f"]
        if any(token in lowered for token in dangerous):
            raise HTTPException(status_code=403, detail="Dangerous command blocked")

        program_path = Path(cmd_args[0])
        program = program_path.stem.lower() if program_path.suffix.lower() == ".exe" else program_path.name.lower()
        if program not in SAFE_COMMANDS:
            raise HTTPException(status_code=403, detail=f"Command '{program}' is not allowed")

        result = subprocess.run(
            cmd_args,
            shell=False,
            capture_output=True,
            text=True,
            timeout=30,
            cwd=WORKSPACE_ROOT,
        )
        return {
            "status": "success",
            "stdout": result.stdout,
            "stderr": result.stderr,
            "code": result.returncode
        }
    except HTTPException as he:
        raise he
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
    """Write or append text to a file within workspace bounds."""
    try:
        path = validate_path(payload.path)
        os.makedirs(os.path.dirname(os.path.abspath(path)), exist_ok=True)
        mode = "a" if payload.append else "w"
        with open(path, mode, encoding="utf-8") as f:
            f.write(payload.content)
        return {"status": "success", "message": f"Written successfully to {payload.path}"}
    except HTTPException as he:
        raise he
    except Exception as e:
        return {"error": str(e)}

import shutil
@app.post("/api/fs/delete")
def fs_delete_file(payload: DeleteFileRequest):
    """Delete a file or directory within workspace bounds."""
    try:
        path = validate_path(payload.path)
        if not os.path.exists(path):
            return {"error": f"Path not found: {payload.path}"}
        
        if os.path.isdir(path):
            shutil.rmtree(path)
        else:
            os.remove(path)
        return {"status": "success", "message": f"Deleted {payload.path}"}
    except HTTPException as he:
        raise he
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/fs/list")
def fs_list_dir(payload: ListDirRequest):
    """List contents of a directory. Defaults to project root."""
    try:
        target = validate_path(payload.path or BASE_DIR)
        
        if not os.path.exists(target):
            return {"error": f"Path not found: {payload.path}"}
        if not os.path.isdir(target):
            return {"error": f"Not a directory: {payload.path}"}

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
 
def ensure_leaning_directory():
    """Ensure the .leaning directory and its manifest files exist."""
    leaning_dir = os.path.join(DEFAULT_WORKSPACE_ROOT, ".leaning")
    os.makedirs(leaning_dir, exist_ok=True)
    
    # Files to initialize if they don't exist
    initial_files = {
        "identity_manifest.md": "# Project Identity Manifest\n\n- Project Name: Neural Studio\n- Root Path: " + DEFAULT_WORKSPACE_ROOT,
        "project_blueprint.md": "# Project Blueprint\n\n- Tech Stack: React, Electron, FastAPI, C++",
        "logic_flows.md": "# Logic Flows\n\n- Defined data paths through the system.",
    }
    
    for filename, content in initial_files.items():
        filepath = os.path.join(leaning_dir, filename)
        if not os.path.exists(filepath):
            try:
                with open(filepath, "w", encoding="utf-8") as f:
                    f.write(content)
                print(f"✓ Initialized project knowledge: {filename}")
            except Exception as e:
                print(f"Warning: Could not initialize {filename}: {e}")
                
    # New: Auto-generate project summary if missing
    summary_path = os.path.join(leaning_dir, "project_summary.md")
    if not os.path.exists(summary_path):
        # We'll trigger this asynchronously or just do it once if not exists
        try:
            from threading import Thread
            Thread(target=update_leaning_summary, daemon=True).start()
        except Exception:
            pass

def update_leaning_summary():
    """Use AI to generate a concise project summary and store it in .leaning/."""
    try:
        leaning_dir = os.path.join(DEFAULT_WORKSPACE_ROOT, ".leaning")
        os.makedirs(leaning_dir, exist_ok=True)
        summary_path = os.path.join(leaning_dir, "project_summary.md")
        
        # Get context to summarize
        cp = get_context_provider(DEFAULT_WORKSPACE_ROOT)
        if not cp:
            return
            
        context = cp.build_context("Provide a high-level technical summary of this project for the .leaning directory.", max_files=10)
        
        prompt = (
            "You are a project architect. Based on the following workspace context, "
            "write a CONCISE (max 500 words) technical summary of the project. "
            "Focus on the core purpose, main tech stack, and high-level logic flow. "
            "Output in Markdown.\n\n"
            "Workspace Context:\n"
            f"{context['context_text']}"
        )
        
        adapter = OllamaAdapter(model=CONTEXT_CHAT_MODEL)
        response = adapter.chat([{"role": "user", "content": prompt}], timeout=300)
        
        if response and not response.startswith("Error"):
            with open(summary_path, "w", encoding="utf-8") as f:
                f.write(f"# AI-Generated Project Summary\n\n> Generated on: {os.path.basename(summary_path)}\n\n{response}")
            print("✓ AI-Generated Project Summary stored in .leaning/")
    except Exception as e:
        print(f"Failed to generate project summary: {e}")
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

@app.get("/api/setup/status", response_model=OllamaStatusResponse)
def get_ollama_status():
    """Check if Ollama is installed and running using both API and CLI."""
    ollama_installed = False
    models = []
    error = None
    
    # 1. Check if 'ollama' command exists
    try:
        ver_res = subprocess.run(["ollama", "--version"], capture_output=True, text=True, timeout=3, shell=True)
        if ver_res.returncode == 0:
            ollama_installed = True
    except Exception as e:
        error = f"Ollama CLI not found: {str(e)}"

    # 2. Try API check
    hosts = [
        os.environ.get("OLLAMA_HOST", "http://127.0.0.1:11434"),
        "http://127.0.0.1:11434",
        "http://localhost:11434"
    ]
    
    ollama_running = False
    for host in hosts:
        base_url = host.rstrip("/")
        if "://" not in base_url:
            base_url = f"http://{base_url}"
        try:
            res = requests.get(f"{base_url}/api/tags", timeout=1.5)
            if res.ok:
                ollama_running = True
                data = res.json()
                models = [m["name"] for m in data.get("models", [])]
                break
        except:
            continue
            
    # 3. Fallback to CLI if API fails but command exists
    if ollama_installed and not ollama_running:
        try:
            list_res = subprocess.run(["ollama", "list"], capture_output=True, text=True, timeout=4, shell=True)
            if list_res.returncode == 0:
                ollama_running = True
                # Parse simple table output
                lines = list_res.stdout.strip().split("\n")
                if len(lines) > 1:
                    for line in lines[1:]: # Skip "NAME" header
                        if line:
                            parts = line.split()
                            if parts: models.append(parts[0])
        except Exception as e:
            if not error: error = f"CLI failed: {str(e)}"

    return {
        "ollama_running": ollama_running,
        "models": list(set(models)),
        "default_model": DEFAULT_CHAT_MODEL,
        "error": error if not ollama_running else None
    }

@app.post("/api/setup/pull-model")
def pull_ollama_model(payload: PullModelRequest):
    """Trigger a streaming pull for an Ollama model."""
    def generate():
        try:
            res = requests.post(
                "http://127.0.0.1:11434/api/pull",
                json={"name": payload.model},
                stream=True,
                timeout=None
            )
            for line in res.iter_lines():
                if line:
                    yield f"data: {line.decode('utf-8')}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'error': str(e)})}\n\n"
    
    return StreamingResponse(generate(), media_type="text/event-stream")

@app.post("/api/setup/install-ollama")
async def install_ollama():
    """Trigger Ollama installation via winget and stream output."""
    def generate():
        try:
            # winget install -e --id Ollama.Ollama --accept-source-agreements --accept-package-agreements
            cmd = ["winget", "install", "-e", "--id", "Ollama.Ollama", "--accept-source-agreements", "--accept-package-agreements"]
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, shell=True)
            
            yield f"data: {json.dumps({'status': 'starting', 'message': 'Starting Ollama installation via winget...'})}\n\n"
            
            if process.stdout:
                for line in process.stdout:
                    if line.strip():
                        yield f"data: {json.dumps({'status': 'installing', 'message': line.strip()})}\n\n"
            
            process.wait()
            if process.returncode == 0:
                yield f"data: {json.dumps({'status': 'success', 'message': 'Ollama installed successfully! Please restart the app if detection still fails.'})}\n\n"
            else:
                yield f"data: {json.dumps({'status': 'error', 'message': f'Installation failed with exit code {process.returncode}'})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'status': 'error', 'message': str(e)})}\n\n"
            
    return StreamingResponse(generate(), media_type="text/event-stream")

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
    """Learn from a URL using the C++ Neural Engine native URL learner."""
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "learn_url", req.url],
            capture_output=True, text=True, timeout=120, cwd=BASE_DIR
        )
        log = (result.stderr + result.stdout).strip()
        success = "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0
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

class InternetLearnRequest(BaseModel):
    max_topics: int = 5
    max_articles: int = 3
    training_epochs: int = 3
    min_corpus_lines: int = 100

@app.post("/api/brain/internet_learn")
async def brain_internet_learn(req: InternetLearnRequest):
    """Trigger the AI to autonomously learn from the internet using Wikipedia."""
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        args = [
            NEURAL_ENGINE_EXE, "internet_learn",
            f"--topics={req.max_topics}",
            f"--articles={req.max_articles}",
            f"--epochs={req.training_epochs}",
            f"--min-corpus={req.min_corpus_lines}"
        ]
        
        # This can take a while depending on topics, set a high timeout
        result = subprocess.run(
            args, capture_output=True, text=True, timeout=300, cwd=BASE_DIR
        )
        
        log = (result.stderr + result.stdout).strip()
        success = result.returncode == 0
        
        # Try to parse stats from the end of the log
        stats = {}
        for line in log.split("\\n"):
            line = line.strip()
            if line.startswith("Topics discovered:"):
                stats["topics_discovered"] = int(line.split(":")[-1].strip())
            elif line.startswith("Articles fetched:"):
                stats["articles_fetched"] = int(line.split(":")[-1].strip())
            elif line.startswith("Words learned:"):
                stats["words_learned"] = int(line.split(":")[-1].strip())
            elif line.startswith("Corpus lines added:"):
                stats["corpus_lines"] = int(line.split(":")[-1].strip())
            elif line.startswith("Training triggered:"):
                stats["training_triggered"] = line.split(":")[-1].strip() == "YES"
                
        return {
            "status": "ok" if success else "error",
            "stats": stats,
            "log": log[-2000:],
        }
    except subprocess.TimeoutExpired:
        return {"error": "timeout", "message": "Internet learning timeout (300s)"}
    except Exception as e:
        return {"error": str(e)}

@app.post("/api/brain/auto_learn_stats")
async def brain_auto_learn_stats():
    """Get statistics about the AI's autonomous learning progress."""
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "self_learn_stats"],
            capture_output=True, text=True, timeout=10, cwd=BASE_DIR
        )
        log = (result.stderr + result.stdout).strip()
        
        import json
        import re
        
        # Find JSON response at the end
        cleaned = re.sub(r'[\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f\\x7f-\\x9f]', '', result.stdout).strip()
        json_match = re.search(r'\\{.*\\}', cleaned, re.DOTALL)
        
        if json_match:
            try:
                response = json.loads(json_match.group(0))
                return {
                    "status": "success",
                    "stats": response,
                    "log": log
                }
            except json.JSONDecodeError:
                pass
                
        return {
            "status": "success", 
            "log": log
        }
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
    Query the C++ Agent Brain (agent_task = Reason + Act + Local Tools).
    This gives the desktop app direct access to your local project!
    """
    if not os.path.exists(NEURAL_ENGINE_EXE):
        return {"error": "Neural engine not built. Run build command first."}

    try:
        import json, re
        # Use agent_task for full autonomous capabilities
        result = subprocess.run(
            [NEURAL_ENGINE_EXE, "agent_task", req.question],
            capture_output=True, text=True, timeout=60, cwd=BASE_DIR
        )
        
        if result.stdout:
            # Look for FINAL_ANSWER in the agentic output
            output = result.stdout
            final_match = re.search(r'FINAL_ANSWER:\s*(.*)', output, re.DOTALL)
            
            if final_match:
                answer = final_match.group(1).strip()
                return {
                    "status": "success",
                    "answer": answer,
                    "confidence": 0.95,
                    "source": "Local Agentic Brain (Ollama + Tools)",
                    "reasoning_steps": [line.strip() for line in output.split('\n') if "Thought:" in line or "Executing" in line]
                }
            else:
                # Fallback: if no FINAL_ANSWER, just return the whole output or clean it
                answer = output.strip()
                return {
                    "status": "success",
                    "answer": answer,
                    "confidence": 0.7,
                    "source": "Local Brain (Direct Output)"
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

if False and __name__ == "__main__":
    ensure_vault()
    # ensure_brain() - OLD Python brain removed, using C++ neural_engine.exe now

    warm_ollama_model_background()

    # Initialize AI self-awareness (auto-load ALL knowledge + project files)
    threading.Thread(target=initialize_ai_capabilities, daemon=True).start()

    # Index all project files for complete AI awareness
    print("\n>> Indexing all project files for AI awareness...")
    try:
        import dynamic_indexer
        import threading
        index_thread = threading.Thread(target=dynamic_indexer.start_dynamic_indexing, daemon=True)
        index_thread.start()
        threading.Thread(target=project_indexer.load_project_files_into_ai, daemon=True).start()
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
    uvicorn.run("main:app", host="127.0.0.1", port=8001, reload=False)


# =============================================================================
# Chat & Feedback API - Phase H-I Desktop Integration
# =============================================================================

class ChatMessage(BaseModel):
    role: str
    content: str

class ChatRequest(BaseModel):
    message: str
    model: Optional[str] = None
    history: list[ChatMessage] = []
    web_search: bool = False
    workspace_root: Optional[str] = None
    project_index_path: Optional[str] = None
    editor_context: Optional[dict[str, Any]] = None

class CommandPreferenceRequest(BaseModel):
    intent: str
    command_name: str
    workspace_root: Optional[str] = None

class GraphSymbolRequest(BaseModel):
    symbol: str

class GraphFileRequest(BaseModel):
    path: str

class GraphImpactRequest(BaseModel):
    symbol: Optional[str] = None
    path: Optional[str] = None

class FeedbackRequest(BaseModel):
    question: str
    answer: str
    feedback: str  # 'positive' or 'negative'
    timestamp: str
    messageId: str

# ============================================
# QUICK WINS: Code Quality Improvements
# ============================================

SMALL_TALK_MESSAGES = {
    "hi",
    "hello",
    "hey",
    "yo",
    "hola",
    "good morning",
    "good afternoon",
    "good evening",
}
FAST_HELP_MESSAGES = {
    "help",
    "what can you do",
    "what do you do",
}
LOCAL_LOOKUP_SUMMARIES = {
    "java": "Java is a high-level, object-oriented programming language used for backend services, Android apps, desktop tools, and large enterprise systems.",
    "python": "Python is a high-level programming language known for readable syntax and wide use in automation, web backends, data work, and AI tooling.",
    "javascript": "JavaScript is the main programming language of the web and is used for browser apps, Node.js servers, and many full-stack tools.",
    "typescript": "TypeScript is JavaScript with static typing, which helps catch errors earlier and makes larger codebases easier to maintain.",
    "react": "React is a UI library for building component-based web interfaces with reusable stateful views.",
    "electron": "Electron packages web technologies like React and Node.js into desktop applications for Windows, macOS, and Linux.",
    "fastapi": "FastAPI is a Python web framework for building APIs quickly with type hints, validation, and high performance.",
    "ollama": "Ollama runs language models locally on your machine and exposes them through a local API for chat and generation.",
    "c++": "C++ is a compiled systems language used when you need speed, memory control, and native performance.",
    "cpp": "C++ is a compiled systems language used when you need speed, memory control, and native performance.",
    "node": "Node.js is a JavaScript runtime used to build servers, tooling, and desktop integrations outside the browser.",
    "node.js": "Node.js is a JavaScript runtime used to build servers, tooling, and desktop integrations outside the browser.",
}

FAST_CHAT_MODEL = os.environ.get("OLLAMA_FAST_MODEL") or "qwen2.5-coder:7b"
CONTEXT_CHAT_MODEL = os.environ.get("OLLAMA_CONTEXT_MODEL") or "qwen2.5-coder:7b"
HTTP_HEADERS = {
    "accept": "application/json",
    "user-agent": "NeuralStudio/1.0 (local desktop assistant)",
}
PROJECT_TASK_KEYWORDS = {
    "code", "project", "repo", "repository", "file", "folder", "directory", "path",
    "class", "function", "method", "module", "component", "server", "desktop",
    "react", "python", "c++", "cpp", "ollama", "build", "compile", "error",
    "exception", "bug", "fix", "implement", "generate code", "review", "analyze",
    "architecture", "refactor", "test", "tests", "diff", "patch", "workspace",
    "src", "include", "bin", "server/main.py", "desktop_app", "neural_engine",
}
DEEP_AGENT_KEYWORDS = {
    "write", "generate", "implement", "fix", "modify", "edit", "patch", "refactor",
    "create file", "delete", "rename", "apply", "run command", "terminal", "execute",
}


def small_talk_reply(message: str) -> Optional[str]:
    print(f">>> DEBUG: small_talk_reply(message='{message}')")
    cleaned = re.sub(r"\s+", " ", message.strip().lower())
    normalized = re.sub(r"[!?.,]+$", "", cleaned).strip()
    if normalized in SMALL_TALK_MESSAGES:
        return "Hello! I'm online and ready to help with your project, code review, or architecture questions."
    if normalized in {"hi there", "hello there", "hey there"}:
        return "Hello! I'm online and ready to help with your project, code review, or architecture questions."
    if normalized in {"how are you", "how are you doing", "how's it going", "whats up", "what's up"}:
        return "I'm online, warmed up, and ready to help with local coding, review, and project understanding."
    if normalized in {"thanks", "thank you", "thx"}:
        return "You're welcome. I'm here whenever you want to review code or work through the project."
    if normalized in {"bye", "goodbye", "see you"}:
        return "See you soon. Your local workspace context will still be here when you come back."
    if normalized in FAST_HELP_MESSAGES:
        return "I can explain files and functions, review code, trace project flow, propose patches, run local checks, and keep the context local."
    return None


def should_use_fast_chat(message: str, web_search: bool) -> bool:
    cleaned = message.strip().lower()
    if not cleaned:
        return True

    if web_search:
        return False

    if "\n" in message or len(message) > 220:
        return False

    if any(token in cleaned for token in ("`", "\\", ".py", ".cpp", ".ts", ".tsx", ".js", ".json")):
        # Allow slashes if they are part of a URL
        if "/" in cleaned and not any(url_indicator in cleaned for url_indicator in ("http://", "https://", "localhost:")):
            return False
        return False

    return not any(keyword in cleaned for keyword in PROJECT_TASK_KEYWORDS)


def should_use_context_chat(message: str) -> bool:
    cleaned = message.strip().lower()
    if any(keyword in cleaned for keyword in DEEP_AGENT_KEYWORDS):
        return False
    return any(keyword in cleaned for keyword in PROJECT_TASK_KEYWORDS)


def fast_chat_messages(req: "ChatRequest", user_message: str) -> list[dict[str, str]]:
    messages: list[dict[str, str]] = []
    for msg in req.history[-6:]:
        if msg.role not in {"user", "assistant"}:
            continue
        content = (msg.content or "").strip()
        if not content:
            continue
        messages.append({"role": msg.role, "content": content[:600]})
    messages.append({"role": "user", "content": user_message})
    return messages


def fast_chat_system_prompt(web_search: bool, task_prep: Optional[dict[str, Any]] = None, local_context: Optional[str] = None) -> str:
    base = (
        "[DEBUG: V2] You are Nero, a local AI assistant inside Neural Studio. "
        "Reply quickly and clearly. "
        "If the question is about the current project files, coding changes, code review, debugging, or architecture, "
        "incorporate the provided context or state briefly that you are switching to deep project mode.\n\n"
        "BROWSER CAPABILITY: You have a real embedded browser inside Neural Studio. "
        "When the user asks you to visit a URL, browse a website, check a page, read console logs, or inspect an element, "
        "you MUST use [BROWSER: cmd] directives — do NOT say you cannot browse the internet. "
        "Available browser directives (include them literally in your response):\n"
        "  [BROWSER: navigate <url>]       — open and navigate to any URL\n"
        "  [BROWSER: getContent]           — get the page title, URL and text\n"
        "  [BROWSER: getHTML]              — get the page outer HTML\n"
        "  [BROWSER: getElement <selector>]— copy a specific element (CSS selector)\n"
        "  [BROWSER: consoleLogs]          — show browser console messages\n"
        "  [BROWSER: screenshot]           — take a screenshot of the page\n"
        "Example: if asked to 'open google.com', respond with: [BROWSER: navigate https://google.com]"
    )
    if web_search:
        base += "\nWeb mode is enabled. You can also use [BROWSER: navigate <url>] to visit any page."
    if local_context:
        base += "\n\nRelevant Local Context:\n" + local_context
    if task_prep and task_prep.get("analysis_summary"):
        base += "\n\nLocal task routing summary:\n" + task_prep["analysis_summary"]
    return base



def context_chat_system_prompt(summary: str, task_prep: Optional[dict[str, Any]] = None) -> str:
    prompt = (
        "You are Nero context chat mode inside Neural Studio. "
        "Answer using the provided workspace context first. "
        "Be concrete, do not invent files, and say when context is incomplete. "
        "Keep the answer clear and practical. "
        f"{summary}\n\n"
        "BROWSER CAPABILITY: You have a real embedded browser inside Neural Studio. "
        "When the user asks you to visit a URL, browse a website, or inspect an element, "
        "you MUST use [BROWSER: cmd] directives — do NOT say you cannot browse the internet. "
        "Available browser directives:\n"
        "  [BROWSER: navigate <url>]       — open and navigate to any URL\n"
        "  [BROWSER: getContent]           — get the page title, URL and text\n"
        "  [BROWSER: getHTML]              — get the page outer HTML\n"
        "  [BROWSER: getElement <selector>]— copy a specific element (CSS selector)\n"
        "  [BROWSER: consoleLogs]          — show browser console messages\n"
        "  [BROWSER: screenshot]           — take a screenshot of the page\n"
    )
    if task_prep and task_prep.get("analysis_summary"):
        prompt += "\n\nLocal task preparation:\n" + task_prep["analysis_summary"]
    return prompt


def extract_lookup_query(message: str) -> Optional[str]:
    cleaned = re.sub(r"\s+", " ", message.strip())
    lowered = cleaned.lower()
    prefixes = ["what is ", "who is ", "what are ", "explain ", "tell me about "]
    for prefix in prefixes:
        if lowered.startswith(prefix):
            return cleaned[len(prefix):].strip(" ?.")
    return cleaned if len(cleaned.split()) <= 5 else None


def quick_local_lookup_summary(message: str) -> Optional[str]:
    query = extract_lookup_query(message)
    if not query:
        return None

    normalized = query.strip().lower()
    aliases = {
        "nodejs": "node.js",
        "node js": "node.js",
        "js": "javascript",
        "ts": "typescript",
    }
    normalized = aliases.get(normalized, normalized)
    return LOCAL_LOOKUP_SUMMARIES.get(normalized)


def quick_web_summary(message: str) -> Optional[str]:
    query = extract_lookup_query(message)
    if not query:
        return None

    try:
        lowered = message.lower()
        if query.lower() == "java" and any(term in lowered for term in ("code", "program", "language", "java")):
            summary_resp = requests.get(
                "https://en.wikipedia.org/api/rest_v1/page/summary/Java_(programming_language)",
                timeout=6,
                headers=HTTP_HEADERS,
            )
            summary_resp.raise_for_status()
            summary = summary_resp.json()
            extract = (summary.get("extract") or "").strip()
            if extract:
                return extract[:420] if len(extract) <= 420 else extract[:420].rsplit(" ", 1)[0] + "..."

        duck = requests.get(
            "https://api.duckduckgo.com/",
            params={"q": query, "format": "json", "no_html": 1, "skip_disambig": 1},
            timeout=5,
            headers=HTTP_HEADERS,
        )
        duck.raise_for_status()
        duck_data = duck.json()
        extract = (duck_data.get("AbstractText") or "").strip()
        if extract:
            if len(extract) > 420:
                extract = extract[:420].rsplit(" ", 1)[0] + "..."
            return extract

        search_resp = requests.get(
            "https://en.wikipedia.org/w/api.php",
            params={
                "action": "opensearch",
                "search": query,
                "limit": 1,
                "namespace": 0,
                "format": "json",
            },
            timeout=5,
        )
        search_resp.raise_for_status()
        data = search_resp.json()
        titles = data[1] if isinstance(data, list) and len(data) > 1 else []
        if not titles:
            return None

        title = titles[0]
        summary_resp = requests.get(
            f"https://en.wikipedia.org/api/rest_v1/page/summary/{quote(title)}",
            timeout=5,
            headers=HTTP_HEADERS,
        )
        summary_resp.raise_for_status()
        summary = summary_resp.json()
        extract = (summary.get("extract") or "").strip()
        if not extract:
            return None

        if len(extract) > 420:
            extract = extract[:420].rsplit(" ", 1)[0] + "..."
        return extract
    except Exception:
        return None


def build_context_fallback_answer(
    user_message: str,
    context: dict[str, Any],
    return_payload: bool = False,
    workspace_root: Optional[str] = None,
    code_graph_engine: Any = None,
) -> Any:
    sources = context.get("sources", [])
    project_overview = str(context.get("project_overview", "") or "").strip()
    lowered = user_message.lower()
    workspace_root_path = Path(resolve_workspace_root(workspace_root))
    code_graph = code_graph_engine

    def _finalize(
        response: str,
        *,
        override_sources: Optional[list[dict[str, Any]]] = None,
        override_flow_sections: Optional[list[dict[str, Any]]] = None,
    ) -> Any:
        payload = {
            "response": response.strip(),
            "sources": override_sources if override_sources is not None else sources,
            "flow_sections": override_flow_sections if override_flow_sections is not None else context.get("flow_sections", []),
        }
        return payload if return_payload else payload["response"]

    is_project_overview_request = any(
        phrase in lowered
        for phrase in (
            "about project",
            "about this project",
            "tell me about project",
            "tell me about the project",
            "explain this project",
            "project structure",
            "project architecture",
        )
    )

    def _extract_explicit_paths() -> list[str]:
        explicit_paths: list[str] = []
        for match in re.findall(
            r"[\w./\\-]+\.(?:cpp|h|hpp|hh|py|ts|tsx|js|jsx|json|md|txt)\b",
            user_message,
            re.IGNORECASE,
        ):
            normalized = match.replace("\\", "/").lstrip("./")
            if "/" in normalized and normalized not in explicit_paths:
                explicit_paths.append(normalized)
        return explicit_paths[:3]

    def _extract_symbol_candidates() -> list[str]:
        candidates: list[str] = []
        for pattern in (
            r"`([A-Za-z_][A-Za-z0-9_]*)`",
            r"\bfunction\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"\bmethod\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"\bclass\s+([A-Za-z_][A-Za-z0-9_]*)",
            r"\b([A-Za-z_][A-Za-z0-9_]*)\s*\(",
        ):
            for match in re.findall(pattern, user_message):
                if match not in candidates:
                    candidates.append(match)

        for token in re.findall(r"\b[A-Za-z_][A-Za-z0-9_]{2,}\b", user_message):
            lowered_token = token.lower()
            if lowered_token in {
                "tell", "about", "project", "file", "used", "where", "does", "what",
                "this", "that", "current", "called", "function", "method", "class",
                "explain", "summarize", "describe", "show", "flow", "architecture",
            }:
                continue
            if ("_" in token or any(ch.isupper() for ch in token[1:])) and token not in candidates:
                candidates.append(token)

        for source in sources:
            reason = str(source.get("reason") or "")
            for prefix in ("symbol:", "symbol-flow-definition:", "symbol-flow-reference:"):
                if prefix in reason:
                    candidate = reason.split(prefix, 1)[1].split(",", 1)[0].strip()
                    if candidate and candidate not in candidates:
                        candidates.append(candidate)

        for section in context.get("flow_sections", []) or []:
            if str(section.get("type") or "").lower() == "symbol":
                title = str(section.get("title") or "").strip()
                if title and title not in candidates:
                    candidates.append(title)

        return candidates[:6]

    def _is_architecture_flow_query() -> bool:
        flow_words = ("flow", "path", "route", "pipeline", "journey")
        if not any(word in lowered for word in flow_words):
            return False
        component_groups = (
            ("ui", "react", "electron", "desktop"),
            ("server", "python", "api", "backend"),
            ("c++", "cpp", "brain", "engine", "neural_engine"),
            ("ollama", "model", "llm"),
        )
        return sum(1 for group in component_groups if any(token in lowered for token in group)) >= 2

    def _build_architecture_flow_payload() -> Optional[dict[str, Any]]:
        if not _is_architecture_flow_query():
            return None

        def find_marker_line(rel_path: str, marker: str, occurrence: str = "last") -> int:
            try:
                text = (workspace_root_path / rel_path).read_text(encoding="utf-8", errors="replace")
            except OSError:
                return 1
            first_match = 1
            last_match = 1
            for line_number, line in enumerate(text.splitlines(), start=1):
                if marker.lower() in line.lower():
                    if first_match == 1:
                        first_match = line_number
                    last_match = line_number
            return first_match if occurrence == "first" else last_match

        entries: list[dict[str, Any]] = []
        flow_sources: list[dict[str, Any]] = []

        def add_entry(label: str, path: str, marker: str, snippet: str, kind: str = "flow", occurrence: str = "last") -> None:
            line_number = find_marker_line(path, marker, occurrence=occurrence)
            entries.append({
                "label": label,
                "path": path,
                "line_start": line_number,
                "line_end": line_number,
                "kind": kind,
                "snippet": snippet,
            })
            flow_sources.append({
                "path": path,
                "line_start": line_number,
                "line_end": line_number,
                "reason": f"architecture-flow:{label.lower().replace(' ', '-')}",
            })

        add_entry(
            "UI sends chat request",
            "desktop_app/src/components/AIChatPanel.tsx",
            "/api/chat",
            "The React chat panel sends the user message to the local FastAPI connector with fetch(`${API}/api/chat`, ...).",
        )
        add_entry(
            "Python API receives chat",
            "server/main.py",
            "async def chat(req: ChatRequest):",
            "FastAPI receives the message at /api/chat and becomes the local coordinator for routing and context prep.",
        )
        add_entry(
            "Router chooses local chat path",
            "server/main.py",
            'elif route == "context_chat":',
            "The Python router decides whether the request should use fast chat, context chat, review, modify, or generation mode.",
        )
        add_entry(
            "Python talks to Ollama",
            "server/llm_adapter.py",
            "/api/chat",
            "For the normal local LLM path, OllamaAdapter posts the prepared chat payload to the local Ollama HTTP API.",
        )
        add_entry(
            "Optional deep agent path",
            "server/main.py",
            "Use agent_task for full autonomous capabilities",
            "For heavier agentic work, Python can invoke bin/neural_engine.exe so the C++ brain handles reasoning and tools first.",
            occurrence="first",
        )
        add_entry(
            "C++ brain can call Ollama natively",
            "src/agent_brain.cpp",
            "const auto direct_response = query_ollama_local(prompt);",
            "Inside the native brain path, the C++ agent has its own local Ollama bridge and can call the Ollama API directly.",
        )

        lines = [
            "Here is the local flow from UI to Ollama.",
            "",
            "Normal chat path:",
            "1. The React/Electron UI sends the message from `desktop_app/src/components/AIChatPanel.tsx` to the local `/api/chat` endpoint.",
            "2. `server/main.py` receives that request and routes it into the right local mode such as fast chat, context chat, review, modify, or generate.",
            "3. For the usual chat path, `server/llm_adapter.py` sends the prepared request to the local Ollama API, so the model answer stays on your machine.",
            "",
            "Deeper agent path:",
            "4. For more agentic or tool-heavy work, the Python connector can invoke `bin/neural_engine.exe` from `server/main.py`.",
            "5. In that path, `src/agent_brain.cpp` can query Ollama through its native local bridge, so the C++ brain can still use local models directly.",
            "",
            "In short: UI -> Python API/router -> Ollama for normal chat, with an optional UI -> Python -> C++ brain -> Ollama path for deeper local tasks.",
        ]

        return {
            "response": "\n".join(lines).strip(),
            "sources": flow_sources,
            "flow_sections": [{
                "type": "file",
                "title": "UI -> Ollama",
                "summary": "End-to-end local message flow from the desktop UI through the Python router and into local Ollama, with the optional C++ agent branch.",
                "entries": entries,
            }],
        }

    def _is_modify_patch_flow_query() -> bool:
        flow_words = ("flow", "path", "route", "pipeline", "journey")
        selection_words = ("selection", "selected", "editor", "cursor")
        modify_words = ("modify", "patch", "edit", "apply", "reviewed patch")
        return (
            any(word in lowered for word in flow_words)
            and any(word in lowered for word in selection_words)
            and any(word in lowered for word in modify_words)
        )

    def _build_modify_patch_flow_payload() -> Optional[dict[str, Any]]:
        if not _is_modify_patch_flow_query():
            return None

        def find_marker_line(rel_path: str, marker: str, occurrence: str = "last") -> int:
            try:
                text = (workspace_root_path / rel_path).read_text(encoding="utf-8", errors="replace")
            except OSError:
                return 1
            first_match = 1
            last_match = 1
            for line_number, line in enumerate(text.splitlines(), start=1):
                if marker.lower() in line.lower():
                    if first_match == 1:
                        first_match = line_number
                    last_match = line_number
            return first_match if occurrence == "first" else last_match

        entries: list[dict[str, Any]] = []
        flow_sources: list[dict[str, Any]] = []

        def add_entry(label: str, path: str, marker: str, snippet: str, kind: str = "flow", occurrence: str = "last") -> None:
            line_number = find_marker_line(path, marker, occurrence=occurrence)
            entries.append({
                "label": label,
                "path": path,
                "line_start": line_number,
                "line_end": line_number,
                "kind": kind,
                "snippet": snippet,
            })
            flow_sources.append({
                "path": path,
                "line_start": line_number,
                "line_end": line_number,
                "reason": f"modify-flow:{label.lower().replace(' ', '-')}",
            })

        add_entry(
            "Editor tracks selection",
            "desktop_app/src/components/MonacoEditor.tsx",
            "setSelectionInfo({",
            "When the user selects code, MonacoEditor captures the selected text and the start/end line+column range.",
        )
        add_entry(
            "Editor syncs selection to local context",
            "desktop_app/src/components/MonacoEditor.tsx",
            "window.appApi.updateEditorContext({",
            "The editor sends file path, cursor position, selection range, selected text, and current symbol into the local workspace context store.",
        )
        add_entry(
            "Electron stores editor context",
            "desktop_app/electron/main.ts",
            "ipcMain.handle('workspace:updateEditorContext'",
            "The Electron main process receives workspace:updateEditorContext and writes that selection context outside the repo.",
        )
        add_entry(
            "Python enters modify mode",
            "server/main.py",
            'def run_modify_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None) -> dict:',
            "When the request is a real code-change task, the Python connector enters run_modify_chat and loads the active file plus the saved editor selection.",
        )
        add_entry(
            "Selected range becomes a candidate patch",
            "server/main.py",
            "candidate_full_text = apply_selection_to_text(original_full_text, selection, updated_code)",
            "The generated replacement code is applied against the selected range to build a candidate full-file result.",
        )
        add_entry(
            "Patch helper applies exact selection range",
            "server/modify_pipeline.py",
            "def apply_selection_to_text(full_text: str, selection: dict[str, Any], replacement: str) -> str:",
            "The modify pipeline converts the saved start/end line and column into exact text offsets and replaces only that region.",
        )
        add_entry(
            "Chat returns reviewed patch result",
            "server/main.py",
            '"patch_diff": diff_text,',
            "Modify mode returns the diff, proposed code, validation result, target file, and applied full content back to the desktop chat.",
        )
        add_entry(
            "Editor can apply reviewed patch",
            "desktop_app/src/components/MonacoEditor.tsx",
            "window.addEventListener('ai-apply-reviewed-patch'",
            "If the user confirms, the editor writes the reviewed patch into the open file and updates the saved editor state.",
        )

        lines = [
            "Here is the local flow from editor selection to modify patch.",
            "",
            "1. In `desktop_app/src/components/MonacoEditor.tsx`, the editor captures the current selection and saves the selected text plus start/end positions.",
            "2. The editor syncs that selection through `window.appApi.updateEditorContext(...)`, and Electron stores it in the separate local workspace memory outside the repo.",
            "3. When you send a real modify request, `server/main.py` enters `run_modify_chat(...)` and loads the active file plus the saved selection context.",
            "4. The modify pipeline generates replacement code for the selected region, then `apply_selection_to_text(...)` builds a candidate full-file result from the exact selection range.",
            "5. The server validates and reviews that candidate patch, creates a diff, and returns the reviewed patch payload to the chat UI.",
            "6. If you click apply, `MonacoEditor.tsx` writes the reviewed patch into the open file and updates the editor state.",
            "",
            "In short: editor selection -> Electron context sync -> Python modify mode -> exact selection replacement -> validation/review -> reviewed patch apply.",
        ]

        return {
            "response": "\n".join(lines).strip(),
            "sources": flow_sources,
            "flow_sections": [{
                "type": "file",
                "title": "Editor selection -> modify patch",
                "summary": "How selected code in the editor turns into a reviewed local patch through the Electron bridge, Python modify mode, and patch application.",
                "entries": entries,
            }],
        }

    architecture_flow_payload = _build_architecture_flow_payload()
    if architecture_flow_payload:
        return _finalize(
            architecture_flow_payload["response"],
            override_sources=architecture_flow_payload["sources"],
            override_flow_sections=architecture_flow_payload["flow_sections"],
        )

    modify_patch_flow_payload = _build_modify_patch_flow_payload()
    if modify_patch_flow_payload:
        return _finalize(
            modify_patch_flow_payload["response"],
            override_sources=modify_patch_flow_payload["sources"],
            override_flow_sections=modify_patch_flow_payload["flow_sections"],
        )

    if project_overview and is_project_overview_request:
        if code_graph is not None:
            try:
                overview = code_graph.architecture_overview()
            except Exception:
                overview = None
            if overview:
                narrative = [str(line).strip() for line in (overview.get("narrative") or []) if str(line).strip()]
                folders = overview.get("folders") or []
                lines = [
                    "Here is a fast local explanation of this project from the code graph.",
                    "",
                ]
                if narrative:
                    lines.extend(narrative[:5])
                if any(folder.get("folder") == "desktop_app/src" for folder in folders) and any(
                    folder.get("folder") == "server" for folder in folders
                ) and any(folder.get("folder") == "src" for folder in folders):
                    lines.append(
                        "The main runtime flow is: React/Electron desktop UI -> Python server/connector -> C++ brain/engine -> local Ollama models."
                    )
                top_folders = [folder for folder in folders[:5] if folder.get("folder")]
                if top_folders:
                    lines.extend(["", "Main folders:"])
                    for folder in top_folders:
                        sample_files = [sample for sample in (folder.get("sample_files") or []) if sample][:2]
                        detail = f"- `{folder['folder']}` has about {folder.get('file_count', 0)} indexed files"
                        if sample_files:
                            detail += f" such as {', '.join(sample_files)}"
                        detail += "."
                        lines.append(detail)
                return _finalize("\n".join(lines).strip())

        return _finalize(
            "Here is a fast local explanation from the indexed project context.\n\n"
            f"{project_overview.replace('PROJECT OVERVIEW:\\n', '')}"
        )

    if not sources and project_overview:
        return _finalize(
            "Here is a fast local explanation from the indexed project context.\n\n"
            + project_overview.replace("PROJECT OVERVIEW:\n", "")
        )

    if not sources:
        return _finalize("I could not find enough relevant local project context for that question.")
    workspace_summary = str(context.get("workspace_summary", "") or "")
    primary_path = str(sources[0].get("path") or "")
    explicit_paths = _extract_explicit_paths()
    if explicit_paths:
        primary_path = explicit_paths[0]

    concise_summary_lines = []
    for line in workspace_summary.splitlines():
        stripped = line.strip()
        if stripped.startswith("Workspace has "):
            concise_summary_lines.append(stripped)
        elif stripped.startswith("Active editor target:"):
            active_target = stripped.split("Active editor target:", 1)[1].strip()
            if primary_path and primary_path in active_target:
                concise_summary_lines.append(stripped)

    def _read_local_file(rel_path: str) -> str:
        try:
            return (workspace_root_path / rel_path).read_text(encoding="utf-8", errors="replace")
        except OSError:
            return ""

    def _extract_comment_summary(rel_path: str) -> str:
        text = _read_local_file(rel_path)
        if not text:
            return ""

        lines = text.splitlines()
        collected: list[str] = []
        in_block = False
        started = False
        for raw_line in lines[:120]:
            stripped = raw_line.strip()
            if not stripped and not started:
                continue
            if stripped.startswith("#include") or stripped.startswith("import ") or stripped.startswith("from "):
                if not started:
                    continue
            if stripped.startswith("/*"):
                in_block = True
                started = True
                stripped = stripped[2:].strip()
            if in_block:
                if "*/" in stripped:
                    before, _, _ = stripped.partition("*/")
                    stripped = before.strip()
                    in_block = False
                stripped = stripped.lstrip("*").strip()
                if stripped:
                    collected.append(stripped)
                if not in_block and collected:
                    break
                continue
            if stripped.startswith("//"):
                started = True
                stripped = stripped[2:].strip()
                if stripped:
                    collected.append(stripped)
                continue
            if started:
                break

        if not collected:
            return ""

        filtered = []
        for line in collected:
            if re.fullmatch(r"[-=]{4,}", line):
                continue
            if line not in filtered:
                filtered.append(line)
            if len(filtered) >= 4:
                break

        summary = " ".join(filtered).strip()
        if len(summary) > 260:
            summary = summary[:260].rsplit(" ", 1)[0] + "..."
        return summary

    def _format_file_overview(rel_path: str) -> list[str]:
        text = _read_local_file(rel_path)
        if not text:
            return []

        lines = text.splitlines()
        non_empty = [line.strip() for line in lines if line.strip()]
        preview = ""
        for candidate in non_empty:
            if candidate in {'"""', "'''", '/*', '*/'}:
                continue
            if candidate.startswith(("//", "#", "*")):
                continue
            if candidate.startswith(("#include", "import ", "from ")):
                continue
            preview = candidate
            break

        import_count = 0
        function_names: list[str] = []
        class_names: list[str] = []

        suffix = Path(rel_path).suffix.lower()
        for line in lines:
            stripped = line.strip()
            if suffix == ".py":
                if stripped.startswith(("import ", "from ")):
                    import_count += 1
                func_match = re.match(r"def\s+([A-Za-z_][A-Za-z0-9_]*)", stripped)
                class_match = re.match(r"class\s+([A-Za-z_][A-Za-z0-9_]*)", stripped)
                if func_match and func_match.group(1) not in function_names:
                    function_names.append(func_match.group(1))
                if class_match and class_match.group(1) not in class_names:
                    class_names.append(class_match.group(1))
            elif suffix in {".ts", ".tsx", ".js", ".jsx"}:
                if stripped.startswith(("import ", "export ")):
                    import_count += 1
                func_match = re.match(r"(?:export\s+)?function\s+([A-Za-z_][A-Za-z0-9_]*)", stripped)
                class_match = re.match(r"(?:export\s+)?class\s+([A-Za-z_][A-Za-z0-9_]*)", stripped)
                const_match = re.match(r"(?:export\s+)?const\s+([A-Za-z_][A-Za-z0-9_]*)\s*=\s*\(", stripped)
                if func_match and func_match.group(1) not in function_names:
                    function_names.append(func_match.group(1))
                if class_match and class_match.group(1) not in class_names:
                    class_names.append(class_match.group(1))
                if const_match and const_match.group(1) not in function_names:
                    function_names.append(const_match.group(1))
            elif suffix in {".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"}:
                if stripped.startswith(("#include", "using ")):
                    import_count += 1
                func_match = re.match(
                    r"(?:inline\s+)?(?:static\s+)?(?:[\w:<>,~*&]+\s+)+([A-Za-z_][A-Za-z0-9_:]*)\s*\([^;{}]*\)\s*(?:const\s*)?\{\s*$",
                    stripped,
                )
                class_match = re.match(r"(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)", stripped)
                if func_match and "::" not in func_match.group(1) and func_match.group(1) not in function_names:
                    function_names.append(func_match.group(1))
                if class_match and class_match.group(1) not in class_names:
                    class_names.append(class_match.group(1))

        comment_summary = _extract_comment_summary(rel_path)
        overview: list[str] = []
        if comment_summary:
            overview.append(f"- Purpose: {comment_summary}")
        if preview:
            if len(preview) > 140:
                preview = preview[:140].rsplit(" ", 1)[0] + "..."
            if not comment_summary or preview.lower() not in comment_summary.lower():
                overview.append(f"- Starts with: {preview}")
        if import_count:
            overview.append(f"- It has about {import_count} import/include statements.")
        if class_names:
            overview.append(f"- Main classes: {', '.join(class_names[:4])}.")
        if function_names:
            overview.append(f"- Main functions: {', '.join(function_names[:6])}.")
        if len(lines):
            overview.append(f"- File size: about {len(lines)} lines.")
        return overview

    def _format_flow_summary() -> list[str]:
        flow_sections = context.get("flow_sections", []) or []
        lines_out: list[str] = []
        for section in flow_sections[:2]:
            entries = section.get("entries") or []
            if not entries:
                continue
            section_type = str(section.get("type") or "").lower()
            if section_type in {"symbol_flow", "symbol"}:
                lines_out.append(f"- Symbol flow found for {section.get('title', 'current symbol')}.")
                for entry in entries[:3]:
                    label = entry.get("label") or entry.get("kind") or "reference"
                    path = entry.get("path") or "unknown"
                    line_start = entry.get("line_start")
                    snippet = str(entry.get("snippet") or "").strip().replace("\n", " ")
                    if len(snippet) > 100:
                        snippet = snippet[:100].rsplit(" ", 1)[0] + "..."
                    detail = f"- {label}: {path}"
                    if line_start:
                        detail += f":{line_start}"
                    if snippet:
                        detail += f" -> {snippet}"
                    lines_out.append(detail)
            elif section_type in {"file_flow", "file"}:
                lines_out.append(f"- File flow found for {section.get('title', 'current file')}.")
            elif section_type == "impact":
                lines_out.append(f"- Impact analysis found {len(entries)} likely affected local location(s).")
        return lines_out

    def _layer_label(path_text: str) -> str:
        normalized = path_text.replace("\\", "/")
        if normalized.startswith("desktop_app/src/"):
            return "React/Electron UI layer"
        if normalized.startswith("desktop_app/electron/"):
            return "Electron desktop shell"
        if normalized.startswith("server/"):
            return "Python connector/orchestration layer"
        if normalized.startswith("src/"):
            return "C++ brain/engine layer"
        if normalized.startswith("include/"):
            return "shared C++ interface layer"
        if normalized.startswith("tests/"):
            return "test/regression layer"
        if normalized.startswith(("knowledge/", "knowledge_sample/")):
            return "local knowledge layer"
        return "workspace layer"

    def _build_graph_file_explanation(rel_path: str) -> str:
        if code_graph is None or not rel_path:
            return ""
        try:
            summary = code_graph.describe_file(rel_path)
        except Exception:
            return ""
        if not summary or not summary.get("found"):
            return ""

        overview_lines = _format_file_overview(rel_path)
        purpose = ""
        other_overview: list[str] = []
        for line in overview_lines:
            if line.startswith("- Purpose:"):
                purpose = line.split(":", 1)[1].strip()
            else:
                other_overview.append(line)

        lines = [f"Here is a fast local explanation of `{rel_path}` from the code graph.", ""]
        if purpose:
            lines.append(f"`{rel_path}` is responsible for {purpose}.")
        else:
            lines.append(f"`{rel_path}` is a relevant local workspace file.")

        if other_overview:
            lines.extend(["", "What this file contains:"])
            lines.extend(other_overview[:4])

        dependencies = [item for item in (summary.get("dependencies") or []) if item.get("path")]
        dependents = [item for item in (summary.get("dependents") or []) if item.get("path")]
        dependency_layers = []
        for dependency in dependencies:
            layer = _layer_label(str(dependency["path"]))
            if layer not in dependency_layers:
                dependency_layers.append(layer)
        dependent_layers = []
        for dependent in dependents:
            layer = _layer_label(str(dependent["path"]))
            if layer not in dependent_layers:
                dependent_layers.append(layer)

        if dependencies:
            lines.extend(["", "This file depends on:"])
            for dependency in dependencies[:4]:
                detail = f"- `{dependency['path']}`"
                if dependency.get("line_start"):
                    detail += f" around line {dependency['line_start']}"
                snippet = str(dependency.get("snippet") or "").strip().replace("\n", " ")
                if snippet:
                    detail += f" via `{snippet[:90]}`"
                lines.append(detail)
        if dependents:
            lines.extend(["", "This file is used by:"])
            for dependent in dependents[:4]:
                detail = f"- `{dependent['path']}`"
                if dependent.get("line_start"):
                    detail += f" around line {dependent['line_start']}"
                snippet = str(dependent.get("snippet") or "").strip().replace("\n", " ")
                if snippet:
                    detail += f" via `{snippet[:90]}`"
                lines.append(detail)

        fit_sentence = f"This file sits in the {_layer_label(rel_path)}."
        relationship_bits: list[str] = []
        if dependency_layers:
            relationship_bits.append(f"it reads or imports pieces from the {', '.join(dependency_layers[:3])}")
        if dependent_layers:
            relationship_bits.append(f"it feeds behavior into the {', '.join(dependent_layers[:3])}")
        if relationship_bits:
            fit_sentence += " In the project flow, " + " and ".join(relationship_bits) + "."
        lines.extend(["", "How it fits into the project:", f"- {fit_sentence}"])

        lines.append("")
        if dependents:
            lines.append("If this file changes, these areas are most likely affected:")
            for dependent in dependents[:4]:
                detail = f"- `{dependent['path']}`"
                if dependent.get("line_start"):
                    detail += f" around line {dependent['line_start']}"
                kind = str(dependent.get("kind") or "").strip()
                if kind:
                    detail += f" ({kind})"
                lines.append(detail)
        else:
            lines.append("If this file changes, no strong local dependent files were detected in the current graph.")
        return "\n".join(lines).strip()

    def _build_graph_symbol_explanation() -> str:
        if code_graph is None:
            return ""
        for symbol_name in _extract_symbol_candidates():
            try:
                summary = code_graph.describe_symbol(symbol_name)
            except Exception:
                continue
            if not summary or not summary.get("found"):
                continue

            definitions = [item for item in (summary.get("definitions") or []) if item.get("path")]
            callers = [item for item in (summary.get("callers") or []) if item.get("path")]
            references = [item for item in (summary.get("references") or []) if item.get("path")]
            if not definitions:
                continue

            primary_definition = definitions[0]
            definition_path = str(primary_definition.get("path") or "")
            owner_layer = _layer_label(definition_path)
            caller_layers: list[str] = []
            for caller in callers:
                layer = _layer_label(str(caller.get("path") or ""))
                if layer not in caller_layers:
                    caller_layers.append(layer)
            reference_layers: list[str] = []
            for reference in references:
                layer = _layer_label(str(reference.get("path") or ""))
                if layer not in reference_layers:
                    reference_layers.append(layer)

            lines = [f"Here is a fast local explanation of `{summary['symbol']}` from the code graph.", ""]
            lines.append(
                f"`{summary['symbol']}` is defined in `{definition_path}`"
                f" around lines {primary_definition.get('line_start', 1)}-{primary_definition.get('line_end', primary_definition.get('line_start', 1))}."
            )
            definition_line = str(primary_definition.get("definition_line") or "").strip()
            if definition_line:
                lines.append(f"It is declared as `{definition_line}`.")

            if callers:
                lines.extend(["", "It is called or referenced by:"])
                for caller in callers[:5]:
                    detail = f"- `{caller['path']}`"
                    if caller.get("line_start"):
                        detail += f" around line {caller['line_start']}"
                    snippet = str(caller.get("snippet") or "").strip().replace("\n", " ")
                    if snippet:
                        detail += f" via `{snippet[:90]}`"
                    lines.append(detail)
            elif references:
                lines.extend(["", "It is referenced by:"])
                for reference in references[:5]:
                    detail = f"- `{reference['path']}`"
                    if reference.get("line_start"):
                        detail += f" around line {reference['line_start']}"
                    snippet = str(reference.get("snippet") or "").strip().replace("\n", " ")
                    if snippet:
                        detail += f" via `{snippet[:90]}`"
                    lines.append(detail)

            fit_sentence = f"This symbol sits in the {owner_layer} because that is where it is defined."
            fit_bits: list[str] = []
            if callers:
                fit_bits.append(f"it is reached from the {', '.join(caller_layers[:3])}")
            elif references:
                fit_bits.append(f"it is referenced from the {', '.join(reference_layers[:3])}")
            if definition_path == "server/main.py" and summary["symbol"].startswith("run_") and summary["symbol"].endswith("_chat"):
                fit_bits.append("it appears to be part of the Python chat handling pipeline")
            elif definition_path.startswith("desktop_app/src/"):
                fit_bits.append("it contributes directly to the desktop UI behavior")
            elif definition_path.startswith("src/"):
                fit_bits.append("it contributes directly to the native C++ brain behavior")
            if fit_bits:
                fit_sentence += " In the project flow, " + " and ".join(fit_bits) + "."
            lines.extend(["", "How it fits into the project:", f"- {fit_sentence}"])

            try:
                impact = code_graph.impact_analysis(symbol=summary["symbol"])
            except Exception:
                impact = None
            impacts = [item for item in ((impact or {}).get("impacts") or []) if item.get("path")]
            lines.append("")
            if impacts:
                lines.append("If this symbol changes, these areas are most likely affected:")
                for item in impacts[:5]:
                    detail = f"- `{item['path']}`"
                    if item.get("line_start"):
                        detail += f" around line {item['line_start']}"
                    label = str(item.get("label") or "").strip()
                    if label:
                        detail += f" ({label})"
                    lines.append(detail)
            else:
                lines.append("If this symbol changes, no strong local callers or references were detected in the current graph.")

            return "\n".join(lines).strip()
        return ""

    if primary_path and (
        lowered.startswith("explain ")
        or lowered.startswith("tell me about ")
        or lowered.startswith("summarize ")
        or lowered.startswith("describe ")
        or "what does" in lowered
        or "how does" in lowered
    ):
        graph_file_answer = _build_graph_file_explanation(primary_path)
        if graph_file_answer:
            return _finalize(graph_file_answer)

    if any(phrase in lowered for phrase in ("how is", "used", "called", "where is", "where does", "what does")):
        graph_symbol_answer = _build_graph_symbol_explanation()
        if graph_symbol_answer:
            return _finalize(graph_symbol_answer)

    lines = [
        "Project Context Summary:",
    ]
    if concise_summary_lines:
        lines.extend(["", *concise_summary_lines])
    lines.extend(["", "Relevant files:"])
    for source in sources[:4]:
        start = source.get("line_start")
        end = source.get("line_end")
        line_text = f"- {source['path']}"
        if start:
            line_text += f":{start}"
            if end:
                line_text += f"-{end}"
        if source.get("reason"):
            line_text += f" ({source['reason']})"
        lines.append(line_text)

    if primary_path and (
        lowered.startswith("explain ")
        or lowered.startswith("tell me about ")
        or lowered.startswith("summarize ")
        or "what does" in lowered
        or "how does" in lowered
    ):
        overview = _format_file_overview(primary_path)
        flow_summary = _format_flow_summary()
        if overview or flow_summary:
            lines.extend(["", f"About `{primary_path}`:"])
            lines.extend(overview[:5])
            lines.extend(flow_summary[:3])

    if "project structure" in lowered:
        top_roots = []
        for source in sources[:4]:
            path = source["path"]
            root = path.split("/", 1)[0]
            if root not in top_roots:
                top_roots.append(root)
        if top_roots:
            lines.extend([
                "",
                "Initial structure summary:",
                f"- Most relevant roots are: {', '.join(top_roots)}.",
                "- `server/` is your Python connector layer.",
                "- `src/` and `include/` are the main C++ brain code paths.",
                "- `desktop_app/` is the Electron/React desktop client.",
            ])
    return _finalize("\n".join(line for line in lines if line is not None).strip())


def should_prefer_context_fallback(user_message: str, context: dict[str, Any]) -> bool:
    lowered = user_message.strip().lower()
    is_modify_patch_flow_request = (
        any(word in lowered for word in ("flow", "path", "route", "pipeline"))
        and any(word in lowered for word in ("selection", "selected", "editor", "cursor"))
        and any(word in lowered for word in ("modify", "patch", "edit", "apply"))
    )
    is_architecture_flow_request = (
        any(word in lowered for word in ("flow", "path", "route", "pipeline"))
        and any(word in lowered for word in ("ui", "react", "electron", "desktop", "server", "python", "c++", "cpp", "brain", "ollama"))
    )
    if is_modify_patch_flow_request or is_architecture_flow_request:
        return True

    action_pattern = r"(?<![A-Za-z0-9_])(review|modify|patch|fix|implement|refactor)(?![A-Za-z0-9_])"
    if re.search(action_pattern, lowered):
        return False

    has_explicit_path = bool(re.search(r"[\w./\\-]+\.(?:cpp|h|hpp|py|ts|tsx|js|jsx|json|md)\b", user_message, re.IGNORECASE))
    flow_sections = context.get("flow_sections") or []
    is_project_overview_request = any(
        phrase in lowered
        for phrase in (
            "about project",
            "about this project",
            "tell me about project",
            "tell me about the project",
            "explain this project",
            "project structure",
            "project architecture",
        )
    )
    is_explain_style = (
        lowered.startswith(("explain ", "summarize ", "describe ", "tell me about "))
        or "what does" in lowered
        or "how does" in lowered
        or "how is" in lowered
    )
    asks_usage_or_location = any(phrase in lowered for phrase in ("used", "called", "where is", "where does"))
    has_symbol_or_flow = bool(flow_sections or len(context.get("sources", [])) <= 2)
    if is_project_overview_request:
        return True
    if asks_usage_or_location and flow_sections:
        return True
    return bool(is_explain_style and (has_explicit_path or has_symbol_or_flow))


def warm_ollama_model(timeout: int = 20) -> bool:
    if OllamaAdapter is None:
        return False

    try:
        adapter = OllamaAdapter(model=FAST_CHAT_MODEL)
        response = adapter.chat(
            [{"role": "user", "content": "Reply with warm."}],
            system_prompt="Warm the local model cache. Reply with one word only.",
            temperature=0.0,
            num_ctx=8192,
            timeout=300,
            num_predict=1024,
        )
        return bool(response and not response.startswith("Error contacting Ollama:"))
    except Exception:
        return False


def warm_ollama_model_background() -> None:
    if OllamaAdapter is None:
        return

    def _warm() -> None:
        warm_ollama_model(timeout=300)

    threading.Thread(target=_warm, daemon=True).start()


def warm_ollama_model_sync(timeout: int = 300) -> bool:
    return warm_ollama_model(timeout=timeout)


def start_background_services() -> None:
    def _boot() -> None:
        try:
            initialize_ai_capabilities()
        except Exception as e:
            print(f">> Warning: AI capability init skipped: {e}")

        print("\n>> Indexing all project files for AI awareness...")
        try:
            import dynamic_indexer
            index_thread = threading.Thread(target=dynamic_indexer.start_dynamic_indexing, daemon=True)
            index_thread.start()
            project_indexer.load_project_files_into_ai()
        except Exception as e:
            print(f">> Warning: Project indexing skipped: {e}")

        try:
            import chat_port
            t = threading.Thread(target=chat_port.start_server, daemon=True)
            t.start()
            print(">> Background AI Chat Port started securely on TCP :9000 <<")
        except Exception as e:
            print(f"Failed to start TCP Chat Port: {e}")

    threading.Thread(target=_boot, daemon=True).start()


def run_review_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None):
    print(">>> INVOKING: review_chat")
    workspace_root = request_workspace_root(req, task_prep)
    context_provider = get_context_provider(workspace_root)
    
    # Build context for review (explicitly include the target file if found)
    context = ""
    if context_provider:
        context = context_provider.build_context(user_message, max_files=5, max_chars=12000)
    
    system_prompt = review_system_prompt
    if "User request:\n" in user_message:
        # If it's a patch-based review or similar
        system_prompt = patch_review_system_prompt
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    messages = fast_chat_messages(req, user_message)
    
    if context:
        messages.insert(0, {"role": "system", "content": f"Use the following local project context to inform your review:\n\n{context}"})
    
    if stream_callback:
        # Real-time streaming
        full_response = ""
        for chunk in adapter.chat_stream(messages, system_prompt=system_prompt, temperature=0.1):
            if chunk:
                full_response += chunk
                stream_callback(chunk)
        return {
            "response": full_response,
            "tool": "review_chat",
            "status": "ok",
            "analysis": build_analysis_payload(task_prep),
        }
    else:
        # Synchronous
        response = adapter.chat(messages, system_prompt=system_prompt, temperature=0.1)
        return {
            "response": response,
            "tool": "review_chat",
            "status": "ok",
            "analysis": build_analysis_payload(task_prep),
        }

def run_design_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: frontend-design")
    system_prompt = "You are an expert Frontend Designer."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("frontend-design", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.7,
        num_ctx=4096,
    )
    return {
        "response": response,
        "tool": "frontend_design_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_visualize_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: chart-visualization")
    system_prompt = "You are a Data Visualization expert."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("chart-visualization", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.2,
        num_ctx=4096,
    )
    return {
        "response": response,
        "tool": "chart_viz_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_research_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: deep-research")
    system_prompt = "You are a Deep Research specialist."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("deep-research", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.4,
        num_ctx=8192,
    )
    return {
        "response": response,
        "tool": "deep_research_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_data_analysis_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: data-analysis")
    system_prompt = "You are a Data Analysis expert using DuckDB."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("data-analysis", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.1,
        num_ctx=8192,
    )
    return {
        "response": response,
        "tool": "data_analysis_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_consulting_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: consulting-analysis")
    system_prompt = "You are a Strategic Technical Consultant."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("consulting-analysis", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.5,
        num_ctx=4096,
    )
    return {
        "response": response,
        "tool": "consulting_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_web_guidelines_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: web-design-guidelines")
    system_prompt = "You are a Web Design Systems expert."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("web-design-guidelines", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.4,
        num_ctx=4096,
    )
    return {
        "response": response,
        "tool": "web_design_guidelines_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_skill_creator_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: skill-creator")
    system_prompt = "You are a meta-skill authoring expert."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("skill-creator", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.7,
        num_ctx=4096,
    )
    return {
        "response": response,
        "tool": "skill_creator_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }

def run_github_research_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    print(">>> INVOKING SKILL: github-deep-research")
    system_prompt = "You are a GitHub Repository Research specialist."
    if skill_manager:
        system_prompt = skill_manager.inject_skill("github-deep-research", system_prompt)
    
    adapter = OllamaAdapter(model=req.model or DEFAULT_CHAT_MODEL)
    response = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=system_prompt,
        temperature=0.3,
        num_ctx=8192,
    )
    return {
        "response": response,
        "tool": "github_research_skill",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }



def run_fast_local_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None):
    print(">>> DEBUG: run_fast_local_chat entered")
    full = []
    local_lookup = quick_local_lookup_summary(user_message) or ""
    if local_lookup:
        lookup_msg = "Project Context Summary:\n" + local_lookup + "\n\n"
        full.append(lookup_msg)
        yield lookup_msg
        if stream_callback: stream_callback(lookup_msg)

    if OllamaAdapter is None:
        msg = "Fast local chat is unavailable because the Ollama adapter could not be loaded."
        if stream_callback: stream_callback(msg)
        yield msg
        return

    adapter = OllamaAdapter(model=req.model or FAST_CHAT_MODEL)
    response_stream = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=fast_chat_system_prompt(req.web_search, task_prep, local_context=local_lookup),
        temperature=0.3,
        num_ctx=1024,
        num_predict=512,
        stream=True,
        timeout=300,
    )
    
    for chunk in response_stream:
        full.append(chunk)
        if stream_callback: stream_callback(chunk)
        yield chunk
    
    response_text = "".join(full).strip()
    yield {
        "response": response_text,
        "tool": "fast_local_chat",
        "status": "ok",
        "confidence": 92,
        "analysis": build_analysis_payload(task_prep),
    }


CONTEXT_PROVIDER = get_context_provider(DEFAULT_WORKSPACE_ROOT)
TASK_INTELLIGENCE = get_task_intelligence(DEFAULT_WORKSPACE_ROOT)
CODE_GRAPH_ENGINE = get_code_graph_engine(DEFAULT_WORKSPACE_ROOT)


def build_analysis_payload(task_prep: Optional[dict[str, Any]]) -> Optional[dict[str, Any]]:
    if not isinstance(task_prep, dict):
        return None

    analysis = task_prep.get("analysis") or {}
    steps = analysis.get("steps") or []
    if not steps:
        return None

    intent = str(task_prep.get("intent") or "")
    preferences = (task_prep.get("command_preferences") or {}).get("intents") or {}
    preferred_commands = list(((preferences.get(intent) or {}).get("preferred_commands") or []))

    normalized_steps = []
    for step in steps:
        if not isinstance(step, dict):
            continue
        normalized_steps.append({
            "name": step.get("name"),
            "category": step.get("category"),
            "status": step.get("status"),
            "reason": step.get("reason"),
            "command": step.get("command"),
            "duration_ms": step.get("durationMs"),
            "summary": step.get("summary"),
            "preferred": step.get("name") in preferred_commands,
        })

    if not normalized_steps:
        return None

    target_hints = [str(hint).lower() for hint in (task_prep.get("target_hints") or [])]
    discovery = (task_prep.get("command_discovery") or {}).get("commands") or []
    preferred_stack = ""
    if any(hint.startswith("desktop_app/") or hint.endswith((".tsx", ".ts", ".jsx", ".js")) for hint in target_hints):
        preferred_stack = "desktop"
    elif any(hint.startswith("src/") or hint.startswith("include/") or hint.endswith((".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh")) for hint in target_hints):
        preferred_stack = "cpp"
    elif any(hint.startswith("server/") or hint.startswith("symbol:") or hint.startswith("function:") or hint.endswith(".py") for hint in target_hints):
        preferred_stack = "python"
    elif intent in {"explain_or_review", "review", "modify"}:
        preferred_stack = "python"

    preferred_categories = ["validate", "lint", "build", "test"]
    if intent == "modify":
        preferred_categories = ["build", "validate", "test", "lint"]
    elif intent == "review":
        preferred_categories = ["lint", "build", "test", "validate"]

    seen_names = {str(step.get("name") or "") for step in normalized_steps}
    alternatives = []
    for category in preferred_categories:
        for command in discovery:
            if not isinstance(command, dict):
                continue
            name = str(command.get("name") or "")
            if not name or name in seen_names:
                continue
            if preferred_stack and str(command.get("stack") or "") != preferred_stack:
                continue
            if str(command.get("category") or "") != category:
                continue
            alternatives.append({
                "name": name,
                "category": command.get("category"),
                "reason": command.get("reason"),
                "command": " ".join(command.get("command") or []),
                "preferred": name in preferred_commands,
            })
            seen_names.add(name)
            if len(alternatives) >= 3:
                break
        if len(alternatives) >= 3:
            break

    return {
        "status": analysis.get("overallStatus"),
        "last_run_at": analysis.get("lastRunAt"),
        "preference_intent": intent,
        "preferred_commands": preferred_commands,
        "steps": normalized_steps,
        "alternatives": alternatives,
    }


def run_context_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None) -> dict:
    context_provider = get_context_provider(request_workspace_root(req, task_prep))
    code_graph_engine = get_code_graph_engine(request_workspace_root(req, task_prep))
    if OllamaAdapter is None or context_provider is None:
        return {
            "response": "Context chat is unavailable because required local modules could not be loaded.",
            "tool": "error",
            "status": "error"
        }

    context = context_provider.build_context(user_message)
    # Disable fast fallback when streaming to ensure LLM handles it
    if not stream_callback and should_prefer_context_fallback(user_message, context):
        fallback = build_context_fallback_answer(
            user_message,
            context,
            return_payload=True,
            workspace_root=request_workspace_root(req, task_prep),
            code_graph_engine=code_graph_engine,
        )
        return {
            "response": fallback["response"],
            "tool": "context_chat_fast_fallback",
            "status": "ok",
            "confidence": 86,
            "sources": fallback.get("sources", context["sources"]),
            "flow_sections": fallback.get("flow_sections", context.get("flow_sections", [])),
            "analysis": build_analysis_payload(task_prep),
        }

    prompt = user_message
    if context["context_text"]:
        prompt = (
            "Task preparation:\n"
            f"{(task_prep or {}).get('analysis_summary', 'No local task preparation was available.')}\n\n"
            "Workspace context:\n"
            f"{context['context_text']}\n\n"
            "User request:\n"
            f"{user_message}"
        )

def run_context_chat(req: "ChatRequest", user_message: str, context: dict[str, Any], task_prep: Optional[dict[str, Any]] = None, stream_callback=None):
    if OllamaAdapter is None:
        msg = "Context chat is unavailable (Ollama adapter missing)."
        if stream_callback: stream_callback(msg)
        yield msg
        return

    adapter = OllamaAdapter(model=req.model or CONTEXT_CHAT_MODEL)
    response_stream = adapter.chat(
        fast_chat_messages(req, user_message),
        system_prompt=context_chat_system_prompt(context["workspace_summary"], task_prep),
        timeout=300,
        stream=True,
    )
    
    full = []
    for chunk in response_stream:
        full.append(chunk)
        if stream_callback: stream_callback(chunk)
        yield chunk
    
    response_text = "".join(full).strip()
    if not response_text:
        msg = "I'm analyzing your workspace but the local model didn't provide a response. Try a more specific question."
        if stream_callback: stream_callback(msg)
        yield msg
    
    # Return a final metadata dict if needed, but the worker will handle it
    yield {
        "response": response_text,
        "tool": "context_chat",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
        "sources": context.get("sources", []),
    }


def run_review_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None) -> dict:
    context_provider = get_context_provider(request_workspace_root(req, task_prep))
    if (
        OllamaAdapter is None
        or context_provider is None
        or review_system_prompt is None
        or normalize_review_response is None
        or format_review_markdown is None
    ):
        return {
            "response": "Review mode is unavailable because required local modules could not be loaded.",
            "tool": "error",
            "status": "error",
        }

    context = context_provider.build_context(user_message, max_files=5, max_chars=9500)
    prompt = (
        "Review request:\n"
        f"{user_message}\n\n"
        "Workspace context:\n"
        f"{context['context_text']}\n\n"
        "Return strict JSON only."
    )

    adapter = OllamaAdapter(model=req.model or CONTEXT_CHAT_MODEL)
    response_stream = adapter.chat(
        fast_chat_messages(req, prompt),
        system_prompt=review_system_prompt(context["workspace_summary"], (task_prep or {}).get("analysis_summary", "")),
        timeout=300,
        stream=bool(stream_callback),
        format="json",
    )
    if stream_callback and not isinstance(response_stream, str):
        full = []
        for chunk in response_stream:
            full.append(chunk)
            stream_callback(chunk)
        raw_response = "".join(full).strip()
    elif isinstance(response_stream, str):
        raw_response = response_stream.strip()
    else:
        raw_response = ""

    review = normalize_review_response(raw_response, context)
    response_text = format_review_markdown(review)

    return {
        "response": response_text,
        "tool": "review_chat",
        "status": "ok",
        "confidence": review.get("confidence", 82),
        "sources": context["sources"],
        "flow_sections": context.get("flow_sections", []),
        "findings": review.get("findings", []),
        "test_gaps": review.get("test_gaps", []),
        "analysis": build_analysis_payload(task_prep),
    }


def detect_generation_language(message: str) -> str:
    lowered = message.lower()
    if re.search(r"(?<![A-Za-z0-9_])(python|py)(?![A-Za-z0-9_])", lowered):
        return "python"
    if re.search(r"(?<![A-Za-z0-9_])(typescript|ts)(?![A-Za-z0-9_])", lowered):
        return "typescript"
    if re.search(r"(?<![A-Za-z0-9_])(javascript|js)(?![A-Za-z0-9_])", lowered):
        return "javascript"
    if re.search(r"(?<![A-Za-z0-9_])(cpp|c\+\+)(?![A-Za-z0-9_])", lowered):
        return "cpp"
    return "python"


def build_generation_fallback(message: str, language: str) -> str:
    lowered = message.lower()

    if language == "python" and any(token in lowered for token in ("addition", "add", "sum")):
        return (
            "Here is a simple Python example:\n\n"
            "```python\n"
            "def add(a: float, b: float) -> float:\n"
            "    return a + b\n\n"
            "result = add(2, 3)\n"
            "print(result)\n"
            "```\n\n"
            "If you want, I can also turn this into a file for this project or adapt it to your current module structure."
        )

    if language == "typescript" and any(token in lowered for token in ("addition", "add", "sum")):
        return (
            "Here is a simple TypeScript example:\n\n"
            "```ts\n"
            "function add(a: number, b: number): number {\n"
            "  return a + b;\n"
            "}\n\n"
            "console.log(add(2, 3));\n"
            "```\n\n"
            "If you want, I can adapt this to the current project file you have open."
        )

    if language == "javascript" and any(token in lowered for token in ("addition", "add", "sum")):
        return (
            "Here is a simple JavaScript example:\n\n"
            "```js\n"
            "function add(a, b) {\n"
            "  return a + b;\n"
            "}\n\n"
            "console.log(add(2, 3));\n"
            "```\n\n"
            "If you want, I can adapt this to the current project file you have open."
        )

    return (
        f"I did not get a fast local model answer, but I understood this as a {language} code-generation request.\n\n"
        f"If you open a target file, I can generate code directly into your project context. Otherwise I can still draft a standalone {language} snippet for you."
    )


def run_generate_code_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None) -> dict:
    language = detect_generation_language(user_message)
    context_provider = get_context_provider(request_workspace_root(req, task_prep))
    context = context_provider.build_context(user_message, max_files=3, max_chars=4500) if context_provider is not None else {
        "workspace_summary": "",
        "context_text": "",
        "sources": [],
        "flow_sections": [],
    }
    editor_context = (task_prep or {}).get("editor_context") or {}
    active_file = editor_context.get("relativePath") or editor_context.get("activeFilePath") or "none"

    if OllamaAdapter is None:
        return {
            "response": build_generation_fallback(user_message, language),
            "tool": "generate_chat_fallback",
            "status": "ok",
            "confidence": 78,
            "sources": context.get("sources", []),
            "flow_sections": context.get("flow_sections", []),
            "analysis": build_analysis_payload(task_prep),
        }

    prompt = (
        f"User request:\n{user_message}\n\n"
        f"Preferred language: {language}\n"
        f"Active project file: {active_file}\n\n"
        "Workspace summary:\n"
        f"{context.get('workspace_summary', '')}\n\n"
        "Relevant project context:\n"
        f"{context.get('context_text', '')}\n\n"
        "Return a practical code answer in markdown. "
        "If no target file is specified, provide a standalone snippet first, then one short note about how it could fit this project."
    )

    adapter = OllamaAdapter(model=req.model or FAST_CHAT_MODEL)
    response_stream = adapter.chat(
        fast_chat_messages(req, prompt),
        system_prompt=(
            "You are Nero local code generation mode inside Neural Studio. "
            "The user is inside a multi-stack project with a C++ brain, Python connector, and React/Electron desktop app. "
            "When no target file is explicitly given, generate a small standalone snippet in the requested language and keep it concise. "
            "When project context is relevant, mention how the snippet would fit the current workspace."
        ),
        timeout=300,
        stream=bool(stream_callback),
    )
    if stream_callback and not isinstance(response_stream, str):
        full = []
        for chunk in response_stream:
            full.append(chunk)
            stream_callback(chunk)
        response_text = "".join(full).strip()
    elif isinstance(response_stream, str):
        response_text = response_stream.strip()
    else:
        response_text = ""

    if not response_text or response_text.startswith("Error contacting Ollama:"):
        response_text = build_generation_fallback(user_message, language)
        tool_name = "generate_chat_fallback"
        confidence = 80
    else:
        response_text = cleanup_generated_code(response_text)
        tool_name = "generate_chat"
        confidence = calculate_code_confidence(response_text)

    return {
        "response": response_text,
        "tool": tool_name,
        "status": "ok",
        "confidence": confidence,
        "sources": context.get("sources", []),
        "flow_sections": context.get("flow_sections", []),
        "analysis": build_analysis_payload(task_prep),
    }


def run_modify_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None, stream_callback=None) -> dict:
    workspace_root = request_workspace_root(req, task_prep)
    context_provider = get_context_provider(workspace_root)
    if (
        OllamaAdapter is None
        or context_provider is None
        or modify_system_prompt is None
        or build_impact_brief is None
        or extract_modify_json is None
        or apply_selection_to_text is None
        or build_unified_diff is None
        or validate_candidate_change is None
        or patch_review_system_prompt is None
        or normalize_patch_review_response is None
        or format_modify_markdown is None
    ):
        return {
            "response": "Modify mode is unavailable because required local modules could not be loaded.",
            "tool": "error",
            "status": "error",
        }

    editor_context = (task_prep or {}).get("editor_context") or {}
    print(f">>> DEBUG: run_modify_chat editor_context keys: {list(editor_context.keys())}")
    active_file = editor_context.get("activeFilePath")
    print(f">>> DEBUG: run_modify_chat active_file: {active_file}")
    if not active_file:
        if (task_prep or {}).get("intent") == "generate":
            return run_generate_code_chat(req, user_message, task_prep)
        return {
            "response": f"Modify mode needs an active file in the editor (Found keys: {list(editor_context.keys())}). Open the target file and select code if possible, then try again.",
            "tool": "modify_chat",
            "status": "error",
        }

    try:
        active_file = validate_path(str(active_file), workspace_root)
    except HTTPException as exc:
        return {
            "response": exc.detail,
            "tool": "modify_chat",
            "status": "error",
        }

    try:
        original_full_text = Path(active_file).read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        return {
            "response": f"Could not read the active file for patch generation: {exc}",
            "tool": "modify_chat",
            "status": "error",
        }

    selection = editor_context.get("selection") or {}
    selected_text = str(editor_context.get("selectedText") or "").strip()
    target_context = selected_text or editor_context.get("nearbySnippet") or original_full_text[:2000]
    relative_file = os.path.relpath(active_file, workspace_root).replace("\\", "/")

    context = context_provider.build_context(user_message, max_files=5, max_chars=9000)
    impact_brief = build_impact_brief(context.get("flow_sections", []))
    prompt = "".join([
        "Modify request:\n",
        f"{user_message}\n\n",
        f"Target file: {relative_file}\n",
        f"Current symbol: {editor_context.get('currentSymbolName') or 'unknown'}\n",
        "Selected or nearby code:\n",
        f"{target_context}\n\n",
        f"{impact_brief}\n\n" if impact_brief else "",
        "Workspace context:\n",
        f"{context['context_text']}\n\n",
        "Return strict JSON only.",
    ])

    adapter = OllamaAdapter(model=req.model or CONTEXT_CHAT_MODEL)
    raw_response = adapter.chat(
        fast_chat_messages(req, prompt),
        system_prompt=modify_system_prompt(context["workspace_summary"], (task_prep or {}).get("analysis_summary", "")),
        timeout=300,
    ).strip()

    parsed = extract_modify_json(raw_response) or {}
    updated_code = str(parsed.get("updated_code") or "").strip()
    summary = str(parsed.get("summary") or "Patch proposal generated from the local context.").strip()
    confidence = parsed.get("confidence", 82)
    try:
        confidence = int(confidence)
    except (TypeError, ValueError):
        confidence = 82

    if not updated_code:
        return {
            "response": "The local model did not return a patch proposal. Try selecting a smaller code block and asking again.",
            "tool": "modify_chat",
            "status": "error",
            "sources": context["sources"],
            "flow_sections": context.get("flow_sections", []),
            "analysis": build_analysis_payload(task_prep),
        }

    if selection and selection.get("startLine") and selection.get("endLine"):
        candidate_full_text = apply_selection_to_text(original_full_text, selection, updated_code)
    else:
        candidate_full_text = updated_code

    line_start = int(selection.get("startLine") or editor_context.get("cursorLine") or 1)
    line_end = int(selection.get("endLine") or selection.get("startLine") or editor_context.get("cursorLine") or line_start)
    diff_text = build_unified_diff(relative_file, original_full_text, candidate_full_text)
    validation = validate_candidate_change(relative_file, candidate_full_text, workspace_root)

    if validation.get("status") == "error":
        repair_prompt = "".join([
            "The previous patch failed local validation.\n\n",
            f"Original request:\n{user_message}\n\n",
            f"Target file: {relative_file}\n",
            "Original selected or nearby code:\n",
            f"{target_context}\n\n",
            f"{impact_brief}\n\n" if impact_brief else "",
            "Previous proposed replacement:\n",
            f"{updated_code}\n\n",
            "Validation error:\n",
            f"{validation.get('summary', '')}\n\n",
            "Return strict JSON only with a corrected updated_code.",
        ])
        repair_raw = adapter.chat(
            fast_chat_messages(req, repair_prompt),
            system_prompt=modify_system_prompt(
                context["workspace_summary"],
                ((task_prep or {}).get("analysis_summary", "") + "\nFix the patch so it passes local validation."),
            ),
            timeout=300,
        ).strip()
        repaired = extract_modify_json(repair_raw) or {}
        repaired_code = str(repaired.get("updated_code") or "").strip()
        if repaired_code and repaired_code != updated_code:
            repaired_full_text = apply_selection_to_text(original_full_text, selection, repaired_code) if selection and selection.get("startLine") and selection.get("endLine") else repaired_code
            repaired_validation = validate_candidate_change(relative_file, repaired_full_text, workspace_root)
            if repaired_validation.get("status") == "ok":
                updated_code = repaired_code
                candidate_full_text = repaired_full_text
                validation = repaired_validation
                diff_text = build_unified_diff(relative_file, original_full_text, candidate_full_text)
                summary = (summary + " The patch was automatically repaired after a validation failure.").strip()

    patch_review_prompt = "".join([
        "Patch review request:\n",
        f"{user_message}\n\n",
        f"Target file: {relative_file}\n",
        f"Current symbol: {editor_context.get('currentSymbolName') or 'unknown'}\n",
        f"{impact_brief}\n\n" if impact_brief else "",
        f"Validation summary:\n{validation.get('summary', 'No validation summary was available.')}\n\n",
        "Review the staged diff below and return strict JSON only.\n\n",
        f"{diff_text or 'No diff was produced.'}",
    ])
    patch_review_raw = adapter.chat(
        fast_chat_messages(req, patch_review_prompt),
        system_prompt=patch_review_system_prompt(
            context["workspace_summary"],
            ((task_prep or {}).get("analysis_summary", "") + "\nThe patch was staged in a temporary workspace before review."),
        ),
        timeout=300,
    ).strip()
    patch_review = normalize_patch_review_response(
        patch_review_raw,
        file_path=relative_file,
        diff_text=diff_text,
        validation=validation,
        line_start=line_start,
    )

    review_findings = list(patch_review.get("findings", []))
    if validation.get("status") == "error" and not any(finding.get("title") == "Staged patch failed validation" for finding in review_findings):
        review_findings.append({
            "title": "Validation failed",
            "severity": "high",
            "file": relative_file,
            "line_start": line_start,
            "line_end": line_end,
            "body": validation.get("summary", "The proposed patch did not pass local validation."),
            "confidence": 0.92,
        })

    response_text = format_modify_markdown(summary, relative_file, diff_text, validation, confidence, patch_review, impact_brief)

    return {
        "response": response_text,
        "tool": "modify_chat",
        "status": "ok",
        "confidence": confidence,
        "sources": context["sources"],
        "flow_sections": context.get("flow_sections", []),
        "impact_summary": impact_brief,
        "findings": review_findings,
        "test_gaps": patch_review.get("test_gaps", []),
        "proposed_code": updated_code,
        "patch_diff": diff_text,
        "validation": validation,
        "target_file": relative_file,
        "target_path": active_file,
        "applied_content": candidate_full_text,
        "analysis": build_analysis_payload(task_prep),
    }


@app.post("/api/analysis/preferences")
def set_analysis_preference(req: CommandPreferenceRequest):
    task_intelligence = get_task_intelligence(req.workspace_root)
    if task_intelligence is None:
        return {
            "status": "error",
            "message": "Task intelligence is unavailable.",
        }

    try:
        updated = task_intelligence.set_command_preference(req.intent, req.command_name)
        return {
            "status": "ok",
            "intent": req.intent,
            "command_name": req.command_name,
            "preferences": updated.get("intents", {}),
        }
    except ValueError as exc:
        return {
            "status": "error",
            "message": str(exc),
        }


@app.get("/api/graph/overview")
def graph_overview():
    if CODE_GRAPH_ENGINE is None:
        return {"status": "error", "message": "Code graph engine is unavailable."}
    return {
        "status": "ok",
        "overview": CODE_GRAPH_ENGINE.architecture_overview(),
    }


@app.post("/api/graph/symbol")
def graph_symbol(req: GraphSymbolRequest):
    if CODE_GRAPH_ENGINE is None:
        return {"status": "error", "message": "Code graph engine is unavailable."}
    return {
        "status": "ok",
        "result": CODE_GRAPH_ENGINE.describe_symbol(req.symbol),
    }


@app.post("/api/graph/file")
def graph_file(req: GraphFileRequest):
    if CODE_GRAPH_ENGINE is None:
        return {"status": "error", "message": "Code graph engine is unavailable."}
    return {
        "status": "ok",
        "result": CODE_GRAPH_ENGINE.describe_file(req.path),
    }


@app.post("/api/graph/impact")
def graph_impact(req: GraphImpactRequest):
    if CODE_GRAPH_ENGINE is None:
        return {"status": "error", "message": "Code graph engine is unavailable."}
    return {
        "status": "ok",
        "result": CODE_GRAPH_ENGINE.impact_analysis(symbol=req.symbol, rel_path=req.path),
    }

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

def run_webapp_testing_chat(req: "ChatRequest", user_message: str, task_prep: Optional[dict[str, Any]] = None):
    workspace_root = request_workspace_root(req, task_prep)
    orchestrator = NeroAgentOrchestrator(workspace_root)
    
    history = []
    if req.history:
        for msg in req.history:
             history.append({"role": msg.role, "content": msg.content})

    responses = orchestrator.process_query(user_message, history)
    
    last_response = responses[-1] if responses else "No response generated."
    if "[FINAL_ANSWER]" in last_response:
        final_answer = last_response.split("[FINAL_ANSWER]")[-1].strip()
    else:
        final_answer = last_response
        
    return {
        "response": final_answer,
        "tool": "webapp_testing",
        "status": "ok",
        "analysis": build_analysis_payload(task_prep),
    }


# =============================================================================
# Project Context & Index  (used by AI Chat for auto project awareness)
# =============================================================================

IGNORED_CONTEXT_DIRS = {
    ".git", "node_modules", "__pycache__", "dist", "dist-electron", "build",
    "out", ".next", ".vscode", ".idea", "coverage", ".angular", ".svn",
    "venv", ".venv", "env", ".env", "target", "vendor",
}
IGNORED_CONTEXT_EXTS = {
    ".aiz", ".exe", ".dll", ".so", ".bin", ".jpg", ".jpeg", ".png", ".gif",
    ".ico", ".webp", ".mp3", ".mp4", ".avi", ".mov", ".pdf", ".zip", ".rar",
    ".7z", ".tar", ".gz", ".woff", ".woff2", ".ttf", ".eot", ".otf", ".db",
    ".sqlite", ".pyc", ".pyo", ".class", ".o", ".a", ".lib",
}
KEY_FILE_NAMES = {
    "package.json", "pyproject.toml", "setup.py", "Cargo.toml", "CMakeLists.txt",
    "go.mod", "pom.xml", "build.gradle", "README.md", "readme.md", "main.py",
    "main.ts", "index.ts", "index.js", "App.tsx", "app.py", "main.cpp",
    ".env.example", "Makefile", "Dockerfile", "docker-compose.yml",
}

# In-memory cache: workspace_root -> (mtime_hash, index_path, context_dict)
_PROJECT_CONTEXT_CACHE: dict = {}


def _detect_stack(root: str) -> list[str]:
    stacks = []
    checks = {
        "Node.js": ["package.json"],
        "Python": ["pyproject.toml", "setup.py", "requirements.txt"],
        "C++": ["CMakeLists.txt", "Makefile"],
        "Rust": ["Cargo.toml"],
        "Go": ["go.mod"],
        "Java": ["pom.xml", "build.gradle"],
        "Docker": ["Dockerfile", "docker-compose.yml"],
    }
    for stack, files in checks.items():
        if any(os.path.exists(os.path.join(root, f)) for f in files):
            stacks.append(stack)
    return stacks or ["Unknown"]


def _walk_project(root: str, max_files: int = 300) -> list[dict]:
    """Walk workspace and return lightweight file metadata list."""
    files = []
    root_abs = os.path.realpath(root)
    for dirpath, dirnames, filenames in os.walk(root_abs):
        # Prune ignored directories in-place
        dirnames[:] = [d for d in dirnames if d not in IGNORED_CONTEXT_DIRS and not d.startswith(".")]
        rel_dir = os.path.relpath(dirpath, root_abs)
        for fname in sorted(filenames):
            if len(files) >= max_files:
                break
            ext = os.path.splitext(fname)[1].lower()
            if ext in IGNORED_CONTEXT_EXTS:
                continue
            full = os.path.join(dirpath, fname)
            rel = os.path.join(rel_dir, fname).replace("\\", "/").lstrip("./")
            try:
                size = os.path.getsize(full)
            except OSError:
                size = 0
            files.append({"path": rel, "size": size, "key": fname in KEY_FILE_NAMES})
        if len(files) >= max_files:
            break
    return files


def _read_key_file_preview(full_path: str, max_lines: int = 60) -> str:
    """Read first N lines of a text file safely."""
    try:
        with open(full_path, "r", encoding="utf-8", errors="replace") as f:
            lines = []
            for i, line in enumerate(f):
                if i >= max_lines:
                    lines.append(f"... ({i}+ more lines)")
                    break
                lines.append(line.rstrip())
            return "\n".join(lines)
    except Exception:
        return "(could not read file)"


def _build_project_index_doc(root: str, files: list[dict], stacks: list[str]) -> str:
    """Build a markdown document summarising the project for the AI."""
    name = os.path.basename(root)
    lines = [
        f"# Project: {name}",
        f"**Root:** `{root}`",
        f"**Stack:** {', '.join(stacks)}",
        f"**Files:** {len(files)} indexed",
        "",
        "## File Tree",
        "```",
    ]
    for f in files[:150]:
        prefix = "* " if f["key"] else "  "
        size_str = f"{f['size']:,}B" if f["size"] < 1024 else f"{f['size']//1024}KB"
        lines.append(f"{prefix}{f['path']}  ({size_str})")
    lines += ["```", "", "## Key File Previews"]

    root_abs = os.path.realpath(root)
    for f in files:
        if not f["key"]:
            continue
        full = os.path.join(root_abs, f["path"])
        if not os.path.exists(full):
            continue
        ext = os.path.splitext(f["path"])[1].lstrip(".")
        lines += [
            f"### {f['path']}",
            f"```{ext}",
            _read_key_file_preview(full),
            "```",
            "",
        ]
    return "\n".join(lines)


class ProjectContextRequest(BaseModel):
    workspace_root: str


@app.get("/api/project/context")
def get_project_context(workspace_root: str = ""):
    """Return a structured JSON snapshot of the workspace for the AI chat."""
    root = resolve_workspace_root(workspace_root)
    if not os.path.isdir(root):
        return {"error": f"Directory not found: {root}"}

    files = _walk_project(root)
    stacks = _detect_stack(root)
    name = os.path.basename(root)

    key_files = [f["path"] for f in files if f["key"]]
    total_size = sum(f["size"] for f in files)

    return {
        "name": name,
        "root": root,
        "stack": stacks,
        "total_files": len(files),
        "total_size": total_size,
        "key_files": key_files,
        "files": files,
    }


@app.get("/api/project/index")
def get_project_index(workspace_root: str = ""):
    """
    Generate (or return cached) a markdown Project Index document written to
    a temp file. Returns the path and content so the AI can use it.
    """
    root = resolve_workspace_root(workspace_root)
    if not os.path.isdir(root):
        return {"error": f"Directory not found: {root}"}

    # Check cache freshness using mtime of root dir
    try:
        mtime = str(os.path.getmtime(root))
    except OSError:
        mtime = "0"

    cached = _PROJECT_CONTEXT_CACHE.get(root)
    if cached and cached.get("mtime") == mtime and os.path.exists(cached.get("path", "")):
        return {"path": cached["path"], "content": cached["content"], "cached": True}

    files = _walk_project(root)
    stacks = _detect_stack(root)
    doc = _build_project_index_doc(root, files, stacks)

    # Write to temp file
    try:
        fd, tmp_path = tempfile.mkstemp(prefix="neural_project_", suffix=".md")
        with os.fdopen(fd, "w", encoding="utf-8") as fh:
            fh.write(doc)
    except Exception as e:
        return {"error": f"Could not write temp file: {e}"}

    _PROJECT_CONTEXT_CACHE[root] = {"mtime": mtime, "path": tmp_path, "content": doc}
    return {"path": tmp_path, "content": doc, "cached": False}


@app.post("/api/project/file")
def read_project_file(payload: ProjectContextRequest):
    """Read a single file from the workspace safely and return its content."""
    # payload.workspace_root is used as the file path here (misname kept for model compat)
    file_path = payload.workspace_root
    if not os.path.isfile(file_path):
        return {"error": f"File not found: {file_path}"}
    ext = os.path.splitext(file_path)[1].lower()
    if ext in IGNORED_CONTEXT_EXTS:
        return {"error": "Binary file type not readable"}
    content = _read_key_file_preview(file_path, max_lines=300)
    return {"path": file_path, "content": content, "lines": content.count("\n") + 1}


@app.post("/api/chat")

async def chat(req: ChatRequest):
    """
    Conversational AI chat endpoint for desktop UI.
    Local-first routing keeps simple chat fast and only uses deeper project
    context when the request actually needs it.
    """
    try:
        user_message = (req.message or "").strip()
        if not user_message:
            return {
                "response": "Please type a message first.",
                "tool": "validation",
                "status": "error",
            }

        quick_reply = small_talk_reply(user_message)
        if quick_reply:
            return {
                "response": quick_reply,
                "tool": "small_talk_fast_path",
                "status": "ok",
                "confidence": 99,
            }

        workspace_root = request_workspace_root(req)
        task_prep = None
        task_intelligence = get_task_intelligence(workspace_root)
        if task_intelligence is not None:
            task_prep = task_intelligence.prepare_task(user_message, bool(req.web_search), req.editor_context)

        route = str((task_prep or {}).get("route") or "")

        if route == "web_lookup":
            web_summary = quick_web_summary(user_message)
            if web_summary:
                return {
                    "response": web_summary,
                    "tool": "quick_web_summary",
                    "status": "ok",
                    "confidence": 88,
                    "analysis": build_analysis_payload(task_prep),
                }
            route = "fast_local_chat"

        if route == "webapp_testing":
            response = run_webapp_testing_chat(req, user_message, task_prep)
        elif route == "modify_chat":
            response = run_modify_chat(req, user_message, task_prep)
        elif route == "generate_chat":
            response = run_generate_code_chat(req, user_message, task_prep)
        elif route == "review_chat":
            response = run_review_chat(req, user_message, task_prep)
        elif route == "design_chat":
            response = run_design_chat(req, user_message, task_prep)
        elif route == "visualize_chat":
            response = run_visualize_chat(req, user_message, task_prep)
        elif route == "research_chat":
            response = run_research_chat(req, user_message, task_prep)
        elif route == "data_analysis_chat":
            response = run_data_analysis_chat(req, user_message, task_prep)
        elif route == "consulting_chat":
            response = run_consulting_chat(req, user_message, task_prep)
        elif route == "web_guidelines_chat":
            response = run_web_guidelines_chat(req, user_message, task_prep)
        elif route == "skill_creator_chat":
            response = run_skill_creator_chat(req, user_message, task_prep)
        elif route == "github_research_chat":
            response = run_github_research_chat(req, user_message, task_prep)
        elif route == "context_chat":

            response = run_context_chat(req, user_message, task_prep)
        elif route == "fast_local_chat" or should_use_fast_chat(user_message, bool(req.web_search)):
            response = run_fast_local_chat(req, user_message, task_prep)
        elif should_use_context_chat(user_message):
            response = run_context_chat(req, user_message, task_prep)
        else:
            response = run_fast_local_chat(req, user_message, task_prep)

        if isinstance(response, dict) and task_prep and "analysis" not in response:
            response["analysis"] = build_analysis_payload(task_prep)
        return response
            
    except Exception as e:
        print(f"[API ERROR] {str(e)}")
        return {"response": f"Chat error: {str(e)}", "tool": "error", "status": "error"}

@app.post("/api/chat_stream")
async def chat_stream(req: ChatRequest):
    print(f">>> API CALL: chat_stream | message={req.message[:30]}...")
    import queue as _queue
    import threading as _threading
    q = _queue.Queue()

    def stream_callback(chunk: str):
        if chunk:
            q.put({"type": "chunk", "content": chunk})

    def worker():
        try:
            user_message = (req.message or "").strip()
            
            # --- Inject Project Index Context if provided ---
            if req.project_index_path and os.path.exists(req.project_index_path):
                try:
                    with open(req.project_index_path, "r", encoding="utf-8") as f:
                        index_content = f.read()
                    user_message = f"--- PROJECT CONTEXT ---\n{index_content}\n--- END PROJECT CONTEXT ---\n\n{user_message}"
                except Exception as e:
                    print(f">>> ERROR: Could not read project index {req.project_index_path}: {e}")

            if not user_message:
                q.put({"type": "final", "data": {
                    "response": "Please type a message first.",
                    "tool": "validation",
                    "status": "error",
                }})
                return

            quick_reply = small_talk_reply(user_message)
            if quick_reply:
                q.put({"type": "chunk", "content": quick_reply})
                q.put({"type": "final", "data": {
                    "response": quick_reply,
                    "tool": "small_talk_fast_path",
                    "status": "ok",
                    "confidence": 99,
                }})
                return

            workspace_root = request_workspace_root(req)
            task_prep = None
            ti = get_task_intelligence(workspace_root)
            if ti is not None:
                task_prep = ti.prepare_task(user_message, bool(req.web_search), req.editor_context)

            route = str((task_prep or {}).get("route") or "")
            print(f">>> DEBUG: worker routing: route={route}, user_message={user_message[:50]}")

            if route == "web_lookup":
                web_summary = quick_web_summary(user_message)
                if web_summary:
                    q.put({"type": "chunk", "content": web_summary})
                    q.put({"type": "final", "data": {
                        "response": web_summary,
                        "tool": "quick_web_summary",
                        "status": "ok",
                        "confidence": 88,
                        "analysis": build_analysis_payload(task_prep),
                    }})
                    return
                route = "fast_local_chat"

            if route == "webapp_testing":
                # For stream, we currently just run it normally as orchestrator loop isn't streamed yet
                response = run_webapp_testing_chat(req, user_message, task_prep)
            elif route == "modify_chat":
                response = run_modify_chat(req, user_message, task_prep, stream_callback=stream_callback)
            elif route == "generate_chat":
                response = run_generate_code_chat(req, user_message, task_prep, stream_callback=stream_callback)
            elif route == "review_chat":
                response = run_review_chat(req, user_message, task_prep, stream_callback=stream_callback)
            elif route == "design_chat":
                response = run_design_chat(req, user_message, task_prep)
            elif route == "visualize_chat":
                response = run_visualize_chat(req, user_message, task_prep)
            elif route == "research_chat":
                response = run_research_chat(req, user_message, task_prep)
            elif route == "data_analysis_chat":
                response = run_data_analysis_chat(req, user_message, task_prep)
            elif route == "consulting_chat":
                response = run_consulting_chat(req, user_message, task_prep)
            elif route == "web_guidelines_chat":
                response = run_web_guidelines_chat(req, user_message, task_prep)
            elif route == "skill_creator_chat":
                response = run_skill_creator_chat(req, user_message, task_prep)
            elif route == "github_research_chat":
                response = run_github_research_chat(req, user_message, task_prep)
            elif route == "context_chat":
                # Handle direct context chat (not as a route but as a primary action)
                context_provider = get_context_provider(workspace_root)
                context = context_provider.build_context(user_message, max_files=5, max_chars=9500)
                gen = run_context_chat(req, user_message, context, task_prep, stream_callback=stream_callback)
                response = None
                for chunk in gen:
                    if isinstance(chunk, dict): response = chunk
                if not response: response = {"response": "Completed", "status": "ok"}
            elif route == "fast_local_chat" or should_use_fast_chat(user_message, bool(req.web_search)):
                gen = run_fast_local_chat(req, user_message, task_prep, stream_callback=stream_callback)
                response = None
                for chunk in gen:
                    if isinstance(chunk, dict): response = chunk
                if not response: response = {"response": "Completed", "status": "ok"}
            elif should_use_context_chat(user_message):
                context_provider = get_context_provider(workspace_root)
                context = context_provider.build_context(user_message, max_files=5, max_chars=9500)
                gen = run_context_chat(req, user_message, context, task_prep, stream_callback=stream_callback)
                response = None
                for chunk in gen:
                    if isinstance(chunk, dict): response = chunk
                if not response: response = {"response": "Completed", "status": "ok"}
            else:
                gen = run_fast_local_chat(req, user_message, task_prep, stream_callback=stream_callback)
                response = None
                for chunk in gen:
                    if isinstance(chunk, dict): response = chunk
                if not response: response = {"response": "Completed", "status": "ok"}

            if isinstance(response, dict) and task_prep and "analysis" not in response:
                response["analysis"] = build_analysis_payload(task_prep)

            q.put({"type": "final", "data": response})
        except Exception as _exc:
            _err = str(_exc)
            print(f"[API STREAM ERROR] {_err}")
            q.put({"type": "error", "error": _err})
        finally:
            q.put(None)

    threading.Thread(target=worker, daemon=True).start()

    def generate():
        while True:
            item = q.get()
            if item is None:
                break
            yield f"data: {json.dumps(item)}\n\n"

    return StreamingResponse(generate(), media_type="text/event-stream")

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


if __name__ == "__main__":
    print("\n>> Starting Neural Studio V10 Services...")
    
    print(">> Initializing Vault and Knowledge directories...")
    ensure_vault()
    ensure_leaning_directory()
    
    # Warm up model in background to avoid blocking server startup (port bind)
    print(">> Starting Ollama model warm-up in background...")
    warm_ollama_model_background()
    
    print(">> Launching background AI services...")
    start_background_services()

    print("\n  +----------------------------------------------------+")
    print("  |   Neural Studio V10 -- AI Compression API          |")
    print("  |   C++ Neural Engine + Smart Brain + Vault          |")
    print("  +----------------------------------------------------+\n")
    
    print(f">> Server binding to 127.0.0.1:8001...")
    uvicorn.run("main:app", host="127.0.0.1", port=8001, reload=False)

