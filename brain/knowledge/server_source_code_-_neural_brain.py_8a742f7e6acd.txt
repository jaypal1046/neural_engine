"""
Neural Brain v1.0 - A Learning AI that stores knowledge compressed.

The compression engine IS a neural network. This brain extends that concept:
- Learn: Scrape web pages, process text, extract knowledge
- Store: All knowledge compressed via myzip (1,046 advisors)
- Retrieve: TF-IDF + keyword matching to find relevant knowledge
- Respond: Generate answers from retrieved compressed knowledge
- Train: Every interaction makes it smarter

The key insight: compression = prediction = understanding.
Our CMIX engine already "understands" data at the byte level.
This brain adds semantic understanding on top.
"""

import os
import json
import math
import time
import hashlib
import subprocess
import re
from collections import Counter, defaultdict
from typing import Optional, List, Dict, Tuple

# =============================================================================
# Configuration
# =============================================================================

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
EXE_PATH = os.path.join(BASE_DIR, "bin", "myzip.exe")
BRAIN_DIR = os.path.join(BASE_DIR, "brain")
KNOWLEDGE_DIR = os.path.join(BRAIN_DIR, "knowledge")
MEMORY_DIR = os.path.join(BRAIN_DIR, "memory")
INDEX_FILE = os.path.join(BRAIN_DIR, "brain_index.json")
VOCAB_FILE = os.path.join(BRAIN_DIR, "vocabulary.json")

# =============================================================================
# Brain Index - The neural map of all knowledge
# =============================================================================

def ensure_brain():
    """Create brain directories if they don't exist."""
    os.makedirs(KNOWLEDGE_DIR, exist_ok=True)
    os.makedirs(MEMORY_DIR, exist_ok=True)
    if not os.path.exists(INDEX_FILE):
        save_index({
            "version": "1.0",
            "created": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "total_knowledge_items": 0,
            "total_raw_bytes": 0,
            "total_compressed_bytes": 0,
            "topics": {},
            "entries": {},
            "conversations": 0,
        })
    if not os.path.exists(VOCAB_FILE):
        save_vocab({"words": {}, "doc_count": 0, "idf_dirty": True})

def load_index() -> dict:
    ensure_brain()
    with open(INDEX_FILE, "r", encoding="utf-8") as f:
        return json.load(f)

def save_index(index: dict):
    with open(INDEX_FILE, "w", encoding="utf-8") as f:
        json.dump(index, f, indent=2, ensure_ascii=False)

def load_vocab() -> dict:
    ensure_brain()
    with open(VOCAB_FILE, "r", encoding="utf-8") as f:
        return json.load(f)

def save_vocab(vocab: dict):
    with open(VOCAB_FILE, "w", encoding="utf-8") as f:
        json.dump(vocab, f, ensure_ascii=False)

# =============================================================================
# Text Processing - The brain's language parser
# =============================================================================

STOP_WORDS = set([
    "the", "a", "an", "is", "are", "was", "were", "be", "been", "being",
    "have", "has", "had", "do", "does", "did", "will", "would", "could",
    "should", "may", "might", "shall", "can", "need", "dare", "ought",
    "used", "to", "of", "in", "for", "on", "with", "at", "by", "from",
    "as", "into", "through", "during", "before", "after", "above", "below",
    "between", "out", "off", "over", "under", "again", "further", "then",
    "once", "here", "there", "when", "where", "why", "how", "all", "both",
    "each", "few", "more", "most", "other", "some", "such", "no", "nor",
    "not", "only", "own", "same", "so", "than", "too", "very", "just",
    "because", "but", "and", "or", "if", "while", "that", "this", "it",
    "its", "i", "me", "my", "we", "our", "you", "your", "he", "him",
    "his", "she", "her", "they", "them", "their", "what", "which", "who",
    "whom", "these", "those", "am", "about", "up", "also", "well", "back",
    "even", "still", "new", "like", "get", "make", "go", "see", "know",
])

def tokenize(text: str) -> List[str]:
    """Split text into clean lowercase tokens."""
    text = text.lower()
    text = re.sub(r'[^a-z0-9\s]', ' ', text)
    tokens = text.split()
    return [t for t in tokens if len(t) > 1 and t not in STOP_WORDS]

def extract_keywords(text: str, top_n: int = 20) -> List[str]:
    """Extract the most important keywords from text using frequency."""
    tokens = tokenize(text)
    freq = Counter(tokens)
    return [word for word, _ in freq.most_common(top_n)]

def compute_tfidf(query_tokens: List[str], doc_tokens: List[str], vocab: dict) -> float:
    """Compute TF-IDF similarity between query and document."""
    if not query_tokens or not doc_tokens:
        return 0.0
    
    doc_count = max(vocab.get("doc_count", 1), 1)
    word_data = vocab.get("words", {})
    
    doc_freq = Counter(doc_tokens)
    doc_len = len(doc_tokens)
    
    score = 0.0
    for token in query_tokens:
        tf = doc_freq.get(token, 0) / max(doc_len, 1)
        df = word_data.get(token, {}).get("df", 1)
        idf = math.log(doc_count / max(df, 1)) + 1
        score += tf * idf
    
    return score

def summarize_text(text: str, max_sentences: int = 5) -> str:
    """Extract the most important sentences from text."""
    sentences = re.split(r'[.!?]+', text)
    sentences = [s.strip() for s in sentences if len(s.strip()) > 20]
    
    if len(sentences) <= max_sentences:
        return '. '.join(sentences) + '.'
    
    # Score sentences by keyword density
    all_tokens = tokenize(text)
    keyword_freq = Counter(all_tokens)
    top_keywords = set(w for w, _ in keyword_freq.most_common(30))
    
    scored = []
    for i, sent in enumerate(sentences):
        tokens = tokenize(sent)
        keyword_hits = sum(1 for t in tokens if t in top_keywords)
        # Boost early sentences (usually more important)
        position_boost = 1.0 / (1.0 + i * 0.1)
        score = keyword_hits * position_boost
        scored.append((score, i, sent))
    
    scored.sort(reverse=True)
    selected = sorted(scored[:max_sentences], key=lambda x: x[1])
    return '. '.join(s[2] for s in selected) + '.'

# =============================================================================
# Knowledge Storage - Compress and store learnings
# =============================================================================

def store_knowledge(topic: str, content: str, source: str = "user",
                    source_url: str = "") -> dict:
    """
    Process text, extract knowledge, and store it compressed.
    Returns metadata about what was stored.
    """
    ensure_brain()
    
    # Generate unique ID
    content_hash = hashlib.sha256(content.encode('utf-8')).hexdigest()[:12]
    entry_id = f"{topic.replace(' ', '_')}_{content_hash}"
    
    # Process text
    tokens = tokenize(content)
    keywords = extract_keywords(content, top_n=25)
    summary = summarize_text(content, max_sentences=4)
    
    # Save raw text to file
    raw_file = os.path.join(KNOWLEDGE_DIR, f"{entry_id}.txt")
    with open(raw_file, "w", encoding="utf-8") as f:
        f.write(content)
    
    raw_size = os.path.getsize(raw_file)
    compressed_file = os.path.join(KNOWLEDGE_DIR, f"{entry_id}.txt.myzip")
    compressed_size = raw_size  # Default if compression fails
    
    # Compress with our neural engine
    if os.path.exists(EXE_PATH):
        try:
            result = subprocess.run(
                [EXE_PATH, "compress", raw_file, compressed_file, "--cmix"],
                capture_output=True, text=True, timeout=300
            )
            if os.path.exists(compressed_file):
                compressed_size = os.path.getsize(compressed_file)
                # Keep the raw file too for fast retrieval
        except (subprocess.TimeoutExpired, Exception):
            compressed_size = raw_size
    
    # Update vocabulary (IDF)
    vocab = load_vocab()
    seen_in_doc = set(tokens)
    for word in seen_in_doc:
        if word not in vocab["words"]:
            vocab["words"][word] = {"df": 0, "total_freq": 0}
        vocab["words"][word]["df"] += 1
        vocab["words"][word]["total_freq"] += tokens.count(word)
    vocab["doc_count"] = vocab.get("doc_count", 0) + 1
    save_vocab(vocab)
    
    # Update brain index
    index = load_index()
    entry = {
        "id": entry_id,
        "topic": topic,
        "source": source,
        "source_url": source_url,
        "raw_file": raw_file,
        "compressed_file": compressed_file,
        "raw_size": raw_size,
        "compressed_size": compressed_size,
        "ratio": round(compressed_size / max(raw_size, 1), 4),
        "savings_pct": round((1.0 - compressed_size / max(raw_size, 1)) * 100, 1),
        "keywords": keywords,
        "summary": summary,
        "token_count": len(tokens),
        "unique_tokens": len(set(tokens)),
        "stored_at": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
    }
    
    index["entries"][entry_id] = entry
    index["total_knowledge_items"] = len(index["entries"])
    index["total_raw_bytes"] = sum(e.get("raw_size", 0) for e in index["entries"].values())
    index["total_compressed_bytes"] = sum(e.get("compressed_size", 0) for e in index["entries"].values())
    
    # Track topics
    if topic not in index["topics"]:
        index["topics"][topic] = {"count": 0, "first_seen": entry["stored_at"]}
    index["topics"][topic]["count"] += 1
    index["topics"][topic]["last_updated"] = entry["stored_at"]
    
    save_index(index)
    
    return {
        "status": "learned",
        "entry_id": entry_id,
        "topic": topic,
        "raw_size": raw_size,
        "compressed_size": compressed_size,
        "savings_pct": entry["savings_pct"],
        "keywords": keywords[:10],
        "summary": summary,
        "token_count": len(tokens),
    }

# =============================================================================
# Knowledge Retrieval - Find relevant compressed knowledge
# =============================================================================

def retrieve_knowledge(query: str, top_k: int = 5) -> List[dict]:
    """
    Search the brain for knowledge relevant to the query.
    Uses TF-IDF matching against stored documents.
    """
    ensure_brain()
    index = load_index()
    vocab = load_vocab()
    
    query_tokens = tokenize(query)
    if not query_tokens:
        return []
    
    results = []
    
    for entry_id, entry in index.get("entries", {}).items():
        # Score by keyword overlap first (fast)
        entry_keywords = set(entry.get("keywords", []))
        keyword_overlap = len(set(query_tokens) & entry_keywords)
        
        # Load document tokens for TF-IDF (from raw file if available)
        doc_tokens = []
        raw_file = entry.get("raw_file", "")
        if os.path.exists(raw_file):
            with open(raw_file, "r", encoding="utf-8") as f:
                doc_text = f.read()
            doc_tokens = tokenize(doc_text)
        else:
            # Use keywords as proxy
            doc_tokens = entry.get("keywords", []) * 3
        
        tfidf_score = compute_tfidf(query_tokens, doc_tokens, vocab)
        
        # Combined score
        combined_score = tfidf_score + keyword_overlap * 2.0
        
        # Topic bonus
        topic_tokens = tokenize(entry.get("topic", ""))
        topic_overlap = len(set(query_tokens) & set(topic_tokens))
        combined_score += topic_overlap * 5.0
        
        if combined_score > 0:
            results.append({
                "entry_id": entry_id,
                "topic": entry.get("topic", ""),
                "score": round(combined_score, 3),
                "summary": entry.get("summary", ""),
                "keywords": entry.get("keywords", [])[:8],
                "source": entry.get("source", ""),
                "source_url": entry.get("source_url", ""),
                "compressed_size": entry.get("compressed_size", 0),
                "savings_pct": entry.get("savings_pct", 0),
                "stored_at": entry.get("stored_at", ""),
            })
    
    results.sort(key=lambda x: x["score"], reverse=True)
    return results[:top_k]

def get_knowledge_text(entry_id: str) -> Optional[str]:
    """Retrieve the full text of a knowledge entry (decompress if needed)."""
    index = load_index()
    entry = index.get("entries", {}).get(entry_id)
    if not entry:
        return None
    
    raw_file = entry.get("raw_file", "")
    if os.path.exists(raw_file):
        with open(raw_file, "r", encoding="utf-8") as f:
            return f.read()
    
    # Try decompressing
    compressed_file = entry.get("compressed_file", "")
    if os.path.exists(compressed_file) and os.path.exists(EXE_PATH):
        try:
            subprocess.run(
                [EXE_PATH, "decompress", compressed_file, raw_file],
                capture_output=True, text=True, timeout=120
            )
            if os.path.exists(raw_file):
                with open(raw_file, "r", encoding="utf-8") as f:
                    return f.read()
        except Exception:
            pass
    
    return None

# =============================================================================
# Response Generation - Answer questions from compressed knowledge
# =============================================================================

def generate_response(query: str) -> dict:
    """
    Generate a response to a query using retrieved knowledge.
    This is our own "LLM" — retrieval-augmented from compressed data.
    """
    ensure_brain()
    
    # Retrieve relevant knowledge
    results = retrieve_knowledge(query, top_k=5)
    
    if not results:
        return {
            "response": "I don't have knowledge about this yet. Teach me by saying:\n"
                       "`learn [topic]: [information]` or `learn_url [topic] [url]`\n\n"
                       "I'll learn, compress, and remember it forever!",
            "sources": [],
            "confidence": 0.0,
        }
    
    # Get full text of top results
    context_parts = []
    sources = []
    
    for r in results[:3]:
        text = get_knowledge_text(r["entry_id"])
        if text:
            context_parts.append(text)
            sources.append({
                "topic": r["topic"],
                "score": r["score"],
                "source_url": r.get("source_url", ""),
                "savings_pct": r.get("savings_pct", 0),
            })
    
    if not context_parts:
        return {
            "response": "I found related topics but couldn't load the knowledge. It may need decompression.",
            "sources": [{"topic": r["topic"], "score": r["score"]} for r in results[:3]],
            "confidence": 0.1,
        }
    
    # Build response from context
    query_tokens = set(tokenize(query))
    
    # Find the most relevant sentences across all context
    all_sentences = []
    for text in context_parts:
        sentences = re.split(r'[.!?\n]+', text)
        for sent in sentences:
            sent = sent.strip()
            if len(sent) < 15:
                continue
            sent_tokens = set(tokenize(sent))
            relevance = len(query_tokens & sent_tokens)
            if relevance > 0:
                all_sentences.append((relevance, sent))
    
    all_sentences.sort(reverse=True)
    
    # Build coherent response
    if all_sentences:
        top_sentences = all_sentences[:6]
        response_text = '. '.join(s[1].strip().rstrip('.') for s in top_sentences) + '.'
    else:
        # Fall back to summaries
        response_text = '\n\n'.join(r.get("summary", "") for r in results[:3])
    
    # Confidence based on score
    max_score = max(r["score"] for r in results)
    confidence = min(max_score / 10.0, 1.0)
    
    return {
        "response": response_text,
        "sources": sources,
        "confidence": round(confidence, 2),
        "knowledge_items_used": len(sources),
        "total_knowledge": load_index().get("total_knowledge_items", 0),
    }

# =============================================================================
# Web Learning - Scrape and learn from the internet
# =============================================================================

def learn_from_url(topic: str, url: str) -> dict:
    """Fetch a URL, extract text, and store as compressed knowledge."""
    try:
        import urllib.request
        
        req = urllib.request.Request(url, headers={
            'User-Agent': 'NeuralStudio/10.0 KnowledgeBot'
        })
        
        with urllib.request.urlopen(req, timeout=15) as response:
            html = response.read().decode('utf-8', errors='replace')
        
        # Extract text from HTML (simple extraction)
        text = extract_text_from_html(html)
        
        if len(text) < 50:
            return {"error": "Could not extract meaningful text from the URL."}
        
        # Store as knowledge
        result = store_knowledge(topic, text, source="web", source_url=url)
        result["url"] = url
        return result
        
    except Exception as e:
        return {"error": f"Failed to fetch URL: {str(e)}"}

def extract_text_from_html(html: str) -> str:
    """Simple HTML to text extraction without external dependencies."""
    # Remove script and style tags
    html = re.sub(r'<script[^>]*>[\s\S]*?</script>', '', html, flags=re.I)
    html = re.sub(r'<style[^>]*>[\s\S]*?</style>', '', html, flags=re.I)
    html = re.sub(r'<nav[^>]*>[\s\S]*?</nav>', '', html, flags=re.I)
    html = re.sub(r'<footer[^>]*>[\s\S]*?</footer>', '', html, flags=re.I)
    
    # Convert common tags to text
    html = re.sub(r'<br\s*/?>', '\n', html, flags=re.I)
    html = re.sub(r'<p[^>]*>', '\n\n', html, flags=re.I)
    html = re.sub(r'<h[1-6][^>]*>', '\n\n', html, flags=re.I)
    html = re.sub(r'<li[^>]*>', '\n- ', html, flags=re.I)
    
    # Remove all remaining HTML tags
    text = re.sub(r'<[^>]+>', ' ', html)
    
    # Decode HTML entities
    text = text.replace('&amp;', '&').replace('&lt;', '<').replace('&gt;', '>')
    text = text.replace('&quot;', '"').replace('&#39;', "'").replace('&nbsp;', ' ')
    
    # Clean up whitespace
    text = re.sub(r'\n\s*\n+', '\n\n', text)
    text = re.sub(r'[ \t]+', ' ', text)
    text = text.strip()
    
    # Limit to reasonable size (first 50KB)
    if len(text) > 50000:
        text = text[:50000]
    
    return text

# =============================================================================
# Memory - Remember conversations
# =============================================================================

def remember_conversation(user_msg: str, ai_response: str):
    """Store a conversation exchange in memory."""
    ensure_brain()
    
    index = load_index()
    conv_num = index.get("conversations", 0) + 1
    index["conversations"] = conv_num
    save_index(index)
    
    memory_file = os.path.join(MEMORY_DIR, f"conv_{conv_num:06d}.json")
    with open(memory_file, "w", encoding="utf-8") as f:
        json.dump({
            "id": conv_num,
            "timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
            "user": user_msg,
            "ai": ai_response,
            "keywords": extract_keywords(user_msg + " " + ai_response, top_n=10),
        }, f, ensure_ascii=False)

# =============================================================================
# Brain Stats
# =============================================================================

def brain_stats() -> dict:
    """Get statistics about the neural brain."""
    index = load_index()
    vocab = load_vocab()
    
    total_raw = index.get("total_raw_bytes", 0)
    total_compressed = index.get("total_compressed_bytes", 0)
    
    return {
        "total_knowledge_items": index.get("total_knowledge_items", 0),
        "total_topics": len(index.get("topics", {})),
        "topics": list(index.get("topics", {}).keys()),
        "vocabulary_size": len(vocab.get("words", {})),
        "conversations_remembered": index.get("conversations", 0),
        "total_raw_bytes": total_raw,
        "total_compressed_bytes": total_compressed,
        "compression_savings_pct": round((1.0 - total_compressed / max(total_raw, 1)) * 100, 1),
        "total_raw_human": f"{total_raw / 1024:.1f} KB" if total_raw < 1024*1024 else f"{total_raw / (1024*1024):.2f} MB",
        "total_compressed_human": f"{total_compressed / 1024:.1f} KB" if total_compressed < 1024*1024 else f"{total_compressed / (1024*1024):.2f} MB",
    }
