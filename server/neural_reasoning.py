"""
Neural Reasoning Engine — The brain's ability to THINK.

Makes the brain independent by adding:
1. Intent Detection — Understand what the user wants without explicit commands
2. Context Manager — Remember conversation flow, handle follow-ups
3. Chain of Thought — Break complex problems into steps
4. Response Composer — Build coherent multi-sentence answers
5. Auto-Learning — Learn from every interaction automatically
6. Self-Assessment — Know what it knows vs doesn't know
7. Task Planner — Plan multi-step actions
"""

import re
import time
import json
import os
from typing import List, Dict, Optional, Tuple
from collections import defaultdict

# =============================================================================
# Intent Detection — What does the user actually want?
# =============================================================================

# Intent patterns: (intent_name, keywords/patterns, confidence_weight)
INTENT_PATTERNS = [
    # Compression intents
    ("compress", ["compress", "shrink", "zip", "pack", "reduce size", "make smaller", "myzip"], 0.95),
    ("decompress", ["decompress", "unzip", "unpack", "extract", "restore", "recover"], 0.95),
    ("analyze", ["analyze", "analyse", "scan", "inspect", "check", "examine", "look at", "entropy"], 0.9),
    
    # Brain intents
    ("learn", ["learn", "teach", "remember", "memorize", "study", "know about"], 0.9),
    ("learn_url", ["learn from url", "learn_url", "study url", "scrape", "learn from web"], 0.95),
    ("learn_file", ["learn from file", "learn_file", "read and learn", "ingest"], 0.95),
    ("ask", ["what is", "what are", "how does", "how do", "why is", "why do", "explain", "tell me about",
             "define", "describe", "who is", "who was", "when did", "where is"], 0.85),
    ("recall", ["do you remember", "what did i", "recall", "you said", "earlier you", "last time"], 0.9),
    
    # Math intents
    ("math", ["calculate", "compute", "solve", "math", "calc", "equation", "formula",
              "stats", "statistics", "mean", "median", "average", "entropy of"], 0.9),
    
    # File system intents  
    ("list_files", ["list", "ls", "dir", "show files", "what files", "what's in", "contents of", "folder"], 0.85),
    ("read_file", ["read", "open", "cat", "show me", "view file", "display file", "contents"], 0.85),
    ("find_files", ["find", "search files", "locate", "where is", "find file"], 0.85),
    
    # Vault intents
    ("store", ["store", "vault store", "save to vault", "keep", "archive"], 0.9),
    ("access", ["access", "vault access", "retrieve from vault", "get from vault"], 0.9),
    ("vault_list", ["vault list", "list vault", "what's stored", "stored files"], 0.9),
    
    # System intents
    ("status", ["status", "health", "server status", "is server running", "online"], 0.85),
    ("help", ["help", "commands", "what can you do", "capabilities", "how to use"], 0.85),
    ("train", ["train", "training", "start training", "learn everything"], 0.9),
    
    # Terminal
    ("cmd", ["cmd", "run", "execute", "terminal", "command line", "shell", "powershell"], 0.9),
    
    # Conversation
    ("greeting", ["hello", "hi", "hey", "good morning", "good evening", "howdy", "sup", "yo"], 0.95),
    ("thanks", ["thank", "thanks", "thank you", "thx", "appreciated", "great job", "awesome"], 0.95),
    ("goodbye", ["bye", "goodbye", "see you", "later", "exit", "quit", "close"], 0.95),
    ("opinion", ["what do you think", "your opinion", "do you like", "best way", "recommend", "suggest"], 0.85),
]

def detect_intent(text: str) -> List[Dict]:
    """
    Detect user intent from natural language.
    Returns ranked list of possible intents with confidence.
    """
    lower = text.lower().strip()
    results = []
    
    for intent_name, patterns, weight in INTENT_PATTERNS:
        score = 0.0
        matched_patterns = []
        
        for pattern in patterns:
            if pattern in lower:
                # Exact words get higher score than substrings
                pattern_words = pattern.split()
                text_words = lower.split()
                
                if all(w in text_words for w in pattern_words):
                    score += weight * 1.0  # Word-level match
                else:
                    score += weight * 0.6  # Substring match
                matched_patterns.append(pattern)
        
        if score > 0:
            results.append({
                "intent": intent_name,
                "confidence": min(score, 1.0),
                "matched": matched_patterns,
            })
    
    # Sort by confidence
    results.sort(key=lambda x: x["confidence"], reverse=True)
    return results

def extract_entity(text: str, intent: str) -> Dict:
    """Extract relevant entities (file paths, topics, questions) from text based on intent."""
    entities = {}
    
    # File path extraction
    path_patterns = [
        r'([A-Za-z]:\\[^\s"\'<>|]+)',  # Windows: C:\path\file
        r'((?:\./|\.\./)[\w/\\.\-]+)',   # Relative: ./path or ../path
        r'(/[\w/.\-]+)',                  # Unix: /path/file
    ]
    for pattern in path_patterns:
        match = re.search(pattern, text)
        if match:
            entities["file_path"] = match.group(1)
            break
    
    # URL extraction
    url_match = re.search(r'(https?://\S+)', text)
    if url_match:
        entities["url"] = url_match.group(1)
    
    # Topic extraction (for learn intent)
    if intent == "learn":
        colon_idx = text.find(':')
        if colon_idx > 0:
            topic_part = re.sub(r'^(learn|teach|remember)\s*', '', text, flags=re.I).strip()
            colon_in_topic = topic_part.find(':')
            if colon_in_topic > 0:
                entities["topic"] = topic_part[:colon_in_topic].strip()
                entities["content"] = topic_part[colon_in_topic+1:].strip()
    
    # Question extraction (for ask intent)
    if intent == "ask":
        entities["question"] = text.strip()
    
    # Math expression extraction
    if intent == "math":
        expr = re.sub(r'^(calculate|compute|solve|math|calc)\s*', '', text, flags=re.I).strip()
        entities["expression"] = expr
    
    # Command extraction
    if intent == "cmd":
        cmd = re.sub(r'^(cmd|run|execute)\s*', '', text, flags=re.I).strip()
        entities["command"] = cmd
    
    return entities

# =============================================================================
# Context Manager — Conversation memory and follow-up handling
# =============================================================================

class ConversationContext:
    """Tracks conversation state for coherent multi-turn interaction."""
    
    def __init__(self, max_history: int = 20):
        self.history: List[Dict] = []
        self.max_history = max_history
        self.current_topic: Optional[str] = None
        self.current_file: Optional[str] = None
        self.last_intent: Optional[str] = None
        self.topics_discussed: List[str] = []
        self.facts_mentioned: List[str] = []
        self.session_start = time.time()
    
    def add_turn(self, user_msg: str, ai_response: str, intent: str, entities: Dict):
        """Record a conversation turn."""
        turn = {
            "user": user_msg,
            "ai": ai_response[:300],  # Truncate for memory
            "intent": intent,
            "entities": entities,
            "timestamp": time.time(),
        }
        self.history.append(turn)
        
        # Update state
        self.last_intent = intent
        if "file_path" in entities:
            self.current_file = entities["file_path"]
        if "topic" in entities:
            self.current_topic = entities["topic"]
            if entities["topic"] not in self.topics_discussed:
                self.topics_discussed.append(entities["topic"])
        
        # Trim history
        if len(self.history) > self.max_history:
            self.history = self.history[-self.max_history:]
    
    def get_context_summary(self) -> str:
        """Get a summary of the conversation context."""
        parts = []
        if self.current_topic:
            parts.append(f"Current topic: {self.current_topic}")
        if self.current_file:
            parts.append(f"Current file: {self.current_file}")
        if self.topics_discussed:
            parts.append(f"Topics discussed: {', '.join(self.topics_discussed[-5:])}")
        return "; ".join(parts) if parts else "No context yet"
    
    def is_followup(self, text: str) -> bool:
        """Check if user's message is a follow-up to previous conversation."""
        lower = text.lower()
        followup_indicators = [
            "it", "that", "this", "those", "these", "the same",
            "also", "and", "more", "another", "again", "what about",
            "how about", "similarly", "likewise", "as well", "too",
            "tell me more", "go on", "continue", "elaborate",
        ]
        # Short messages that refer to previous context
        if len(lower.split()) <= 4:
            for indicator in followup_indicators:
                if indicator in lower:
                    return True
        return bool(self.last_intent and len(lower.split()) <= 3)
    
    def resolve_references(self, text: str) -> str:
        """Replace pronouns and references with actual entities from context."""
        lower = text.lower()
        resolved = text
        
        # "it" / "that file" → current file
        if self.current_file and any(ref in lower for ref in ["it", "that file", "the file", "same file", "this file"]):
            for ref in ["that file", "the file", "same file", "this file", "it"]:
                if ref in lower:
                    resolved = resolved.replace(ref, self.current_file)
                    break
        
        # "that topic" → current topic
        if self.current_topic and "that" in lower and "topic" in lower:
            resolved = resolved.replace("that topic", self.current_topic)
        
        return resolved

# Global conversation context
_context = ConversationContext()

def get_context() -> ConversationContext:
    return _context

# =============================================================================
# Response Composer — Build intelligent coherent responses
# =============================================================================

def compose_greeting() -> str:
    """Generate a contextual greeting."""
    hour = int(time.strftime("%H"))
    time_greeting = "Good morning" if hour < 12 else "Good afternoon" if hour < 17 else "Good evening"
    ctx = get_context()
    
    if ctx.history:
        return f"{time_greeting}! Welcome back. We were discussing {ctx.current_topic or 'compression'}. How can I help?"
    return (f"{time_greeting}! I'm Neural Studio, your compression-powered AI. "
            f"I learn, remember, and get smarter over time.\n\n"
            f"Try: `help` to see what I can do, or just ask me anything!")

def compose_thanks() -> str:
    return "You're welcome! I'm always here to help. Anything else you'd like to know or explore? 🧠"

def compose_goodbye() -> str:
    ctx = get_context()
    items = len(ctx.history)
    return (f"Goodbye! We had a great session ({items} exchanges). "
            f"Everything I learned is saved compressed. See you next time! 👋")

def compose_opinion(query: str, knowledge_response: Optional[str] = None) -> str:
    """Generate an opinion/recommendation based on knowledge."""
    lower = query.lower()
    
    if any(w in lower for w in ["best algorithm", "which algorithm", "recommend"]):
        return ("Based on my compression knowledge:\n\n"
                "• **Small text files (<1MB)**: Use `--cmix` for best ratio. The 1,046 advisors excel at learning text patterns.\n"
                "• **Large files (>10MB)**: Use `--best` (BWT) for good balance of speed and ratio.\n"
                "• **Quick compression**: Default LZ77 is fast and decent.\n"
                "• **Maximum compression**: Use `--ultra` (PPM) or `--cmix`. Slower but worth it.\n\n"
                "Always `analyze` first — I'll tell you the entropy and recommend the optimal algorithm!")
    
    if knowledge_response:
        return f"Based on what I know:\n\n{knowledge_response}\n\n*This is from my compressed knowledge base. Teach me more to improve my answers!*"
    
    return ("I'd need more knowledge to give a strong opinion. "
            "Teach me about this topic:\n\n`learn [topic]: [your expertise]`")

def compose_unknown(query: str, ctx: ConversationContext) -> str:
    """Generate a helpful response when we don't understand."""
    suggestions = [
        f"• `learn {query}: [explanation]` — teach me about this",
        f"• `learn_url {query} https://...` — I'll learn from a web page",
        f"• `ask {query}` — explicitly search my knowledge",
    ]
    
    if ctx.current_topic:
        suggestions.insert(0, f"• `ask {ctx.current_topic}` — ask about our current topic")
    
    return (f"I'm not sure about \"{query}\" yet, but I can learn!\n\n"
            + "\n".join(suggestions) +
            "\n\nOr try `help` for all available commands.")

# =============================================================================
# Chain of Thought — Multi-step reasoning
# =============================================================================

def plan_steps(intent: str, entities: Dict, context: ConversationContext) -> List[Dict]:
    """
    Plan execution steps for a given intent.
    Returns list of steps to execute.
    """
    steps = []
    
    if intent == "compress" and "file_path" in entities:
        fp = entities["file_path"]
        steps = [
            {"action": "analyze", "description": f"Analyze {fp} for entropy and patterns", "params": {"file_path": fp}},
            {"action": "recommend", "description": "Determine best algorithm based on analysis"},
            {"action": "compress", "description": f"Compress with recommended algorithm", "params": {"file_path": fp}},
            {"action": "report", "description": "Report compression results and savings"},
        ]
    
    elif intent == "learn_url" and "url" in entities:
        topic = entities.get("topic", "web_content")
        steps = [
            {"action": "fetch", "description": f"Fetch content from {entities['url']}"},
            {"action": "extract", "description": "Extract readable text from HTML"},
            {"action": "process", "description": "Tokenize, extract keywords, build index"},
            {"action": "compress", "description": "Compress knowledge with CMIX engine"},
            {"action": "store", "description": f"Store under topic: {topic}"},
        ]
    
    elif intent == "ask":
        question = entities.get("question", "")
        steps = [
            {"action": "search", "description": "Search knowledge base with TF-IDF"},
            {"action": "retrieve", "description": "Load relevant compressed documents"},
            {"action": "score", "description": "Score and rank relevant sentences"},
            {"action": "compose", "description": "Build coherent response"},
        ]
    
    return steps

# =============================================================================
# Self-Assessment — Know what it knows
# =============================================================================

def assess_knowledge(topic: str, brain_stats_func, retrieve_func) -> Dict:
    """
    Assess how well the brain knows about a topic.
    Returns confidence level and gaps.
    """
    from neural_brain import tokenize
    
    results = retrieve_func(topic, top_k=10)
    
    if not results:
        return {
            "knowledge_level": "none",
            "confidence": 0.0,
            "suggestion": f"I have no knowledge about '{topic}'. Teach me with:\n"
                         f"`learn {topic}: [info]` or `learn_url {topic} [url]`"
        }
    
    top_score = results[0]["score"] if results else 0
    total_items = len(results)
    
    if top_score > 8 and total_items >= 3:
        level = "expert"
        confidence = 0.9
    elif top_score > 5 and total_items >= 2:
        level = "good"
        confidence = 0.7
    elif top_score > 2:
        level = "basic"
        confidence = 0.4
    else:
        level = "minimal"
        confidence = 0.2
    
    return {
        "knowledge_level": level,
        "confidence": confidence,
        "relevant_items": total_items,
        "top_score": top_score,
        "topics": [r["topic"] for r in results[:5]],
        "suggestion": (
            f"My knowledge of '{topic}' is **{level}** (confidence: {confidence:.0%}). "
            + (f"I have {total_items} related items." if total_items > 0 else "")
            + ("\nTeach me more to improve!" if confidence < 0.7 else "")
        )
    }

# =============================================================================
# Auto-Learning — Extract facts from every interaction
# =============================================================================

def extract_learnable_facts(user_msg: str, ai_response: str) -> List[Dict]:
    """
    Extract facts from conversation that could be stored as knowledge.
    Returns list of potential facts to learn.
    """
    facts = []
    
    # User stating facts (declarative sentences)
    sentences = re.split(r'[.!]', user_msg)
    for sent in sentences:
        sent = sent.strip()
        if len(sent) < 20:
            continue
        
        # Declarative patterns (user teaching something)
        if any(sent.lower().startswith(p) for p in [
            "the ", "a ", "an ", "in ", "on ", "for ", "when ",
            "there ", "it ", "this ", "that ", "my ", "our ",
        ]):
            # Check if it contains factual content
            words = sent.split()
            if len(words) >= 5 and any(w.lower() in ["is", "are", "was", "were", "means", "refers", "involves", "works"] for w in words):
                facts.append({
                    "text": sent,
                    "type": "user_statement",
                    "confidence": 0.6,
                })
    
    return facts

# =============================================================================
# Main Processing Pipeline — The brain's thought process
# =============================================================================

def process_input(user_msg: str) -> Dict:
    """
    Main reasoning pipeline. Takes user input and produces a structured response plan.
    Returns: intent, entities, steps, context info, and whether to auto-learn.
    """
    ctx = get_context()
    
    # Step 1: Resolve references from context
    resolved_msg = ctx.resolve_references(user_msg)
    
    # Step 2: Detect intent
    intents = detect_intent(resolved_msg)
    primary_intent = intents[0] if intents else {"intent": "unknown", "confidence": 0}
    
    # Step 3: Handle follow-ups
    is_followup = ctx.is_followup(resolved_msg)
    if is_followup and not intents:
        primary_intent = {"intent": ctx.last_intent or "ask", "confidence": 0.6}
    
    # Step 4: Extract entities
    entities = extract_entity(resolved_msg, primary_intent["intent"])
    
    # Inherit file from context if not specified
    if "file_path" not in entities and ctx.current_file:
        if primary_intent["intent"] in ("compress", "decompress", "analyze", "learn_file"):
            entities["file_path"] = ctx.current_file
    
    # Step 5: Plan steps
    steps = plan_steps(primary_intent["intent"], entities, ctx)
    
    # Step 6: Extract learnable facts
    learnable = extract_learnable_facts(user_msg, "")
    
    return {
        "original": user_msg,
        "resolved": resolved_msg,
        "intent": primary_intent,
        "all_intents": intents[:3],
        "entities": entities,
        "steps": steps,
        "is_followup": is_followup,
        "context_summary": ctx.get_context_summary(),
        "learnable_facts": learnable,
    }

def update_context(user_msg: str, ai_response: str, intent: str, entities: Dict):
    """Update conversation context after a turn."""
    ctx = get_context()
    ctx.add_turn(user_msg, ai_response, intent, entities)
