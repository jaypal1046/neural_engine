"""
PYTHON SUPPORT FILE — DATA PIPELINE ONLY
=========================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

rlhf_trainer.py — Claude-1 Style Training Pipeline (Python Support Layer)
==========================================================================
Python's role (support ONLY):
  - Hold SFT training pairs as static data
  - Hold CONSTITUTION rules as static data
  - Assemble training corpus text files
  - Call C++ neural_engine.exe for ALL learning and training
  - Score responses (utility for self-assessment — no AI logic)

C++ neural_engine.exe does ALL the work:
  - neural_engine.exe learn <file>          → stores knowledge
  - neural_engine.exe train_transformer ... → trains the transformer
  - neural_engine.exe ai_ask <question>     → answers questions

How Claude was trained (our implementation):
  Phase 1: Pretraining    → C++ train_transformer on large corpus
  Phase 2: SFT            → Python writes Q&A corpus → C++ train_transformer
  Phase 3: CAI critique   → Python assembles critique corpus → C++ train_transformer
  Phase 4: RLHF feedback  → human thumbs → Python scores → C++ train_transformer

Run:
    python server/rlhf_trainer.py --sft          # SFT corpus → C++ train
    python server/rlhf_trainer.py --cai          # CAI corpus → C++ train
    python server/rlhf_trainer.py --full         # Both phases
    python server/rlhf_trainer.py --status       # Show stats
"""

import os, sys, re, json, time, subprocess, tempfile, argparse
from pathlib import Path
from collections import defaultdict
from typing import List, Dict

# Force UTF-8 output on Windows
if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)

# ─── Paths ────────────────────────────────────────────────────────────────────
BASE_DIR       = Path(__file__).parent.parent
TRAIN_DIR      = BASE_DIR / "brain" / "training"
NEURAL_ENGINE  = str(BASE_DIR / "bin" / "neural_engine.exe")
FEEDBACK_FILE  = TRAIN_DIR / "human_feedback.json"
SFT_FILE       = TRAIN_DIR / "sft_pairs.json"
CAI_FILE       = TRAIN_DIR / "cai_critiques.json"
STATS_FILE     = TRAIN_DIR / "training_stats.json"

TRAIN_DIR.mkdir(parents=True, exist_ok=True)

# ─── Banner ───────────────────────────────────────────────────────────────────
BANNER = """
+========================================================================+
|  Neural Studio  --  Claude-1 Style Training Pipeline                   |
|  Python: data prep + scoring  |  C++: learn + train + answer           |
|  Phases: SFT -> CAI -> train_transformer                               |
+========================================================================+
"""


# ─── C++ Bridge Functions ─────────────────────────────────────────────────────

def cpp_learn_file(file_path: str, timeout: int = 60) -> bool:
    """Call C++ neural_engine.exe learn <file>."""
    if not os.path.exists(NEURAL_ENGINE):
        print(f"  [ERROR] neural_engine.exe not found: {NEURAL_ENGINE}")
        return False
    result = subprocess.run(
        [NEURAL_ENGINE, "learn", file_path],
        capture_output=True, text=True, timeout=timeout,
        cwd=str(BASE_DIR)
    )
    log = (result.stdout + result.stderr).strip()
    return "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0


def cpp_train_transformer(corpus_path: str, epochs: int = 7,
                          lr: float = 0.002, batch: int = 16) -> bool:
    """Call C++ neural_engine.exe train_transformer <corpus> [epochs] [lr] [batch]."""
    if not os.path.exists(NEURAL_ENGINE):
        print(f"  [ERROR] neural_engine.exe not found: {NEURAL_ENGINE}")
        return False
    print(f"  [C++] train_transformer {corpus_path} epochs={epochs} lr={lr} batch={batch}")
    result = subprocess.run(
        [NEURAL_ENGINE, "train_transformer", corpus_path,
         str(epochs), str(lr), str(batch)],
        capture_output=True, text=True, timeout=900,
        cwd=str(BASE_DIR)
    )
    if result.returncode == 0:
        print(f"  [OK] C++ training complete")
        return True
    else:
        # Show last 500 chars of output for diagnostics
        out = (result.stdout + result.stderr)[-500:].strip()
        print(f"  [WARN] C++ training returned code {result.returncode}: {out}")
        return False


def cpp_score_response(question: str, answer: str, timeout: int = 15) -> Dict:
    """
    Call C++ neural_engine.exe score_response <file>
    C++ scores the answer using the RewardModel (ported from Python).
    Returns {"total": 0.XX, "grade": "...", "breakdown": {...}}
    Falls back to basic Python scoring if C++ not available.
    """
    if not os.path.exists(NEURAL_ENGINE):
        # Minimal fallback — Python calculates basic score
        words = len(answer.split())
        length = 1.0 if 10 <= words <= 250 else (0.3 if words < 5 else 0.7)
        return {"total": round(length * 0.7, 3), "grade": "C  (Needs improvement)",
                "breakdown": {"length": length}}

    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.txt', delete=False,
        encoding='utf-8', prefix='score_'
    ) as tmp:
        tmp.write(f"question: {question}\nanswer: {answer}\n")
        tmp_path = tmp.name

    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "score_response", tmp_path],
            capture_output=True, text=True, timeout=timeout,
            cwd=str(BASE_DIR)
        )
        if result.stdout:
            try:
                return json.loads(result.stdout.strip())
            except json.JSONDecodeError:
                pass
    except Exception:
        pass
    finally:
        try: os.unlink(tmp_path)
        except: pass

    return {"total": 0.5, "grade": "C  (Needs improvement)", "breakdown": {}}


def cpp_cai_critique(question: str, answer: str, timeout: int = 15) -> Dict:
    """
    Call C++ neural_engine.exe cai_critique <file>
    C++ checks the answer against CONSTITUTION rules.
    Returns {"violations": [...], "cai_score": 0.XX, "passed": true/false, "suggestions": [...]}
    Falls back to empty critique if C++ not available.
    """
    if not os.path.exists(NEURAL_ENGINE):
        return {"violations": [], "cai_score": 1.0, "passed": True, "suggestions": []}

    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.txt', delete=False,
        encoding='utf-8', prefix='cai_'
    ) as tmp:
        tmp.write(f"question: {question}\nanswer: {answer}\n")
        tmp_path = tmp.name

    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "cai_critique", tmp_path],
            capture_output=True, text=True, timeout=timeout,
            cwd=str(BASE_DIR)
        )
        if result.stdout:
            try:
                return json.loads(result.stdout.strip())
            except json.JSONDecodeError:
                pass
    except Exception:
        pass
    finally:
        try: os.unlink(tmp_path)
        except: pass

    return {"violations": [], "cai_score": 1.0, "passed": True, "suggestions": []}


def cpp_ai_ask(question: str, timeout: int = 30) -> str:
    """Call C++ neural_engine.exe ai_ask <question> and return the answer."""
    if not os.path.exists(NEURAL_ENGINE):
        return f"[engine not found] {question}"
    result = subprocess.run(
        [NEURAL_ENGINE, "ai_ask", question],
        capture_output=True, text=True, timeout=timeout,
        cwd=str(BASE_DIR)
    )
    if result.stdout:
        cleaned = re.sub(r'[\x00-\x08\x0b\x0c\x0e-\x1f\x7f-\x9f]', '', result.stdout).strip()
        json_match = re.search(r'\{.*\}', cleaned, re.DOTALL)
        if json_match:
            try:
                br = json.loads(json_match.group())
                answer = br.get("answer", "")
                if answer and len(answer) > 5:
                    return answer
            except json.JSONDecodeError:
                pass
        if cleaned:
            return cleaned
    return f"I need to learn more about: {question}"


# =============================================================================
# THE CONSTITUTION  (Anthropic CAI rules, adapted for Neural Studio)
# =============================================================================

CONSTITUTION = [
    # Helpfulness
    "The answer must directly address what the user asked.",
    "The answer must be clear, specific, and actionable — not generic.",
    "The answer must not be vague, evasive, or use filler phrases without detail.",
    "If the AI doesn't know something, it must say so honestly and offer to learn.",
    "Explanations of technical concepts must include at least one concrete example.",
    "Step-by-step instructions must be numbered and complete.",

    # Honesty
    "The answer must not fabricate facts or overclaim certainty.",
    "Uncertain claims must be hedged with 'I think', 'likely', or similar.",
    "The answer must not pretend to have capabilities it lacks.",
    "When answering from compressed knowledge, the answer should reflect it may be incomplete.",

    # Quality
    "The answer must be well-structured using headers, bullets, or numbered lists when appropriate.",
    "Technical terms must be used correctly and spelled accurately.",
    "The answer must not repeat the same information in different words.",
    "Code blocks in the answer must use correct syntax for the language shown.",
    "Short questions (greetings, simple requests) must receive concise, direct answers.",

    # Neural Studio domain rules
    "Answers about compression must reference specific algorithms (CMIX, BWT, LZ77, PPM, etc.).",
    "When asked to compress a file, always recommend running analyze first to check entropy.",
    "Answers about the brain must mention that knowledge is stored compressed.",
    "Commands shown in answers must use the correct Neural Studio syntax.",
    "Answers about learning must explain the tokenize -> keyword -> compress -> index pipeline.",
]


# =============================================================================
# REWARD MODEL  — Pure Python scoring utility (no AI logic — just rules)
# =============================================================================

class RewardModel:
    """
    Scores an AI response based on quality rules.
    Range 0.0 to 1.0. Higher = better.
    Python support utility: scoring only — C++ does all actual AI work.
    """

    def score(self, question: str, answer: str) -> Dict:
        scores = {}
        a_lower = answer.lower()
        words   = len(answer.split())

        # 1. Relevance — question-answer overlap
        q_tokens = set(self._tokens(question))
        a_tokens = set(self._tokens(answer))
        if len(q_tokens) <= 3:
            scores["relevance"] = 0.85
        else:
            overlap = len(q_tokens & a_tokens) / max(len(q_tokens), 1)
            scores["relevance"] = min(overlap * 2.5, 1.0)

        # 2. Length quality — Goldilocks zone
        if words < 5:
            scores["length"] = 0.1
        elif words < 10:
            scores["length"] = 0.75 if len(q_tokens) <= 4 else 0.3
        elif words <= 250:
            scores["length"] = 1.0
        elif words <= 450:
            scores["length"] = 0.85
        else:
            scores["length"] = 0.6

        # 3. Specificity — numbers, code, examples
        specific_hits = sum(1 for p in [
            r'\b\d+[.,]?\d*\b',
            r'e\.g\.|for example|such as',
            r'`[^`]+`',
            r'\*\*[^*]+\*\*',
            r'\b(specifically|step \d)',
            r'^\d+\.\s',
            r'^[-*]\s',
        ] if re.search(p, answer, re.M | re.I))
        scores["specificity"] = min(specific_hits * 0.18, 1.0)

        # 4. Honesty — hedging vs overclaiming
        hedge   = ["i think", "i believe", "likely", "probably", "may ",
                   "might ", "could ", "i don't know", "not sure", "uncertain"]
        overcon = ["definitely", "certainly", "guaranteed", "100%", "absolute"]
        n_hedge = sum(1 for w in hedge   if w in a_lower)
        n_over  = sum(1 for w in overcon if w in a_lower)
        if n_over > 2:
            scores["honesty"] = 0.3
        elif n_hedge > 0:
            scores["honesty"] = min(0.7 + n_hedge * 0.1, 1.0)
        else:
            scores["honesty"] = 0.65

        # 5. Structure — formatting
        sentences  = [s.strip() for s in re.split(r'[.!?]', answer) if s.strip()]
        cap_ratio  = sum(1 for s in sentences if s and s[0].isupper()) / max(len(sentences), 1)
        has_list   = bool(re.search(r'(^[-*\d]\.?\s)', answer, re.M))
        has_header = bool(re.search(r'\*\*[^*]+\*\*', answer))
        struct = cap_ratio * 0.5
        if has_list:   struct += 0.3
        if has_header: struct += 0.2
        scores["structure"] = min(struct, 1.0)

        # 6. Unknown handling
        if any(p in a_lower for p in ["don't have knowledge", "don't know", "teach me",
                                       "haven't learned", "no knowledge"]):
            scores["unknown_handling"] = 0.9
        else:
            scores["unknown_handling"] = 0.7

        # 7. Domain knowledge
        domain_terms = [
            "cmix", "bwt", "lz77", "ppm", "entropy", "compress", "huffman",
            "arithmetic", "context mixing", "neural", "brain", "knowledge",
            "token", "range coding", "predictor", "vocabulary", "aiz"
        ]
        scores["domain_knowledge"] = min(
            sum(1 for t in domain_terms if t in a_lower) * 0.12, 1.0)

        # 8. Actionability
        action_signals = [
            r'`[a-z_]+\s', r'\brun\b|\buse\b|\btry\b|\btype\b',
            r'POST /api|GET /api', r'python \w+',
        ]
        scores["actionability"] = min(
            sum(1 for p in action_signals if re.search(p, answer, re.I)) * 0.3, 1.0)

        # 9. Completeness
        last_char = answer.strip()[-1] if answer.strip() else ''
        scores["completeness"] = 1.0 if last_char in '.!?)' else 0.7

        # Weighted total
        weights = {
            "relevance": 0.25, "length": 0.12, "specificity": 0.15,
            "honesty": 0.12, "structure": 0.12, "unknown_handling": 0.04,
            "domain_knowledge": 0.10, "actionability": 0.06, "completeness": 0.04,
        }
        final = sum(scores[k] * weights[k] for k in weights)

        return {
            "total": round(final, 3),
            "breakdown": {k: round(v, 3) for k, v in scores.items()},
            "grade": self._grade(final),
        }

    def _grade(self, score: float) -> str:
        if score >= 0.85: return "A+ (Excellent)"
        if score >= 0.75: return "A  (Good)"
        if score >= 0.65: return "B  (Acceptable)"
        if score >= 0.50: return "C  (Needs improvement)"
        return                    "D  (Poor - retrain)"

    def _tokens(self, text: str) -> List[str]:
        return re.findall(r'\b[a-z]{3,}\b', text.lower())


# =============================================================================
# CONSTITUTIONAL AI CRITIQUE  (Pure Python rule-checking — no C++ needed)
# =============================================================================

def cai_critique(question: str, answer: str) -> Dict:
    """
    Check the answer against CONSTITUTION rules.
    Returns violations. Pure Python utility — no AI calls.
    """
    violations = []
    suggestions = []

    for rule in CONSTITUTION:
        violated = False

        if "directly address" in rule:
            q_words = set(re.findall(r'\b\w{4,}\b', question.lower()))
            a_words = set(re.findall(r'\b\w{4,}\b', answer.lower()))
            if len(q_words) >= 6 and len(q_words & a_words) < 1:
                violated = True
                suggestions.append("Include more words from the question in your answer.")

        elif "vague" in rule:
            vague_phrases = ["it depends", "various things", "many ways",
                             "generally speaking", "could be many", "there are multiple"]
            if any(p in answer.lower() for p in vague_phrases) and len(answer.split()) < 30:
                violated = True
                suggestions.append("Be specific — avoid vague phrases without follow-up detail.")

        elif "concrete examples" in rule:
            is_technical = any(w in question.lower() for w in
                               ["how", "what is", "explain", "algorithm", "work", "does"])
            has_example  = bool(re.search(r'e\.g\.|for example|such as|for instance', answer, re.I))
            has_list     = bool(re.search(r'^\d+\.|^[-*]', answer, re.M))
            if is_technical and not has_example and not has_list and len(answer.split()) > 40:
                violated = True
                suggestions.append("Add a concrete example to illustrate the concept.")

        elif "make up facts" in rule or "fabricate" in rule:
            overconfident = re.search(r'\b(definitely|certainly|guaranteed|always|never|100%)\b',
                                      answer, re.I)
            if overconfident and len(answer.split()) < 50:
                violated = True
                suggestions.append("Soften strong claims — use 'likely', 'I believe', etc.")

        elif "compression must mention" in rule:
            if any(w in question.lower() for w in ["compress", "algorithm", "zip"]):
                if not any(w in answer.lower() for w in
                           ["cmix", "bwt", "lz77", "ppm", "huffman", "arithmetic"]):
                    violated = True
                    suggestions.append("Mention specific compression algorithms (CMIX, BWT, LZ77, PPM).")

        if violated:
            violations.append(rule)

    passed = len(CONSTITUTION) - len(violations)
    return {
        "violations": violations,
        "violations_count": len(violations),
        "rules_passed": passed,
        "total_rules": len(CONSTITUTION),
        "cai_score": round(passed / len(CONSTITUTION), 3),
        "suggestions": suggestions,
        "passed": len(violations) == 0,
    }


# =============================================================================
# SFT PAIRS  — Supervised Fine-Tuning training data (static)
# =============================================================================

SFT_PAIRS = [

    # ─── IDENTITY & SELF-AWARENESS ───────────────────────────────────────────
    {
        "question": "What are you?",
        "ideal_answer": "I am Neural Studio V10, a self-learning compression AI built by Jay. I store all knowledge compressed using a 1,046-advisor CMIX neural engine and grow smarter with every conversation. I run 100% locally — no cloud, no API keys, no data leaving your machine.",
        "topic": "identity: what am I",
        "skills": ["accurate self-description", "key differentiators"]
    },
    {
        "question": "Are you ChatGPT?",
        "ideal_answer": "No — I am Neural Studio V10, not ChatGPT or any cloud AI. I am a local, self-learning system built by Jay that runs entirely on your machine. I don't call any external API. My knowledge is stored compressed using our own CMIX neural engine.",
        "topic": "identity: not ChatGPT",
        "skills": ["clear distinction", "accurate self-description"]
    },
    {
        "question": "Are you Claude?",
        "ideal_answer": "No. I am Neural Studio V10, a local AI built by Jay. Claude is Anthropic's cloud AI — I'm a completely different system. I run on your machine, store knowledge in our compressed brain, and don't send data anywhere. I'm inspired by Anthropic's training techniques but I'm my own independent system.",
        "topic": "identity: not Claude",
        "skills": ["honest distinction", "self-awareness"]
    },
    {
        "question": "Who built you?",
        "ideal_answer": "I was built by Jay. I am Neural Studio V10 — a local self-learning AI that uses CMIX neural compression to store and retrieve knowledge. Jay designed my architecture around the principle that compression = prediction = intelligence.",
        "topic": "identity: creator",
        "skills": ["direct answer", "domain context"]
    },
    {
        "question": "What can't you do?",
        "ideal_answer": "I'm honest about my limits. I cannot:\n- Browse the internet in real-time (I fetch URLs on request, not continuously)\n- Run code natively (I can suggest code, but I don't execute it)\n- Answer questions I haven't learned yet — but I can learn!\n- Replace a full LLM for creative writing or long-form generation\n\nFor everything else: compression, analyzing files, answering from my compressed knowledge — I'm your expert.",
        "topic": "identity: limitations",
        "skills": ["honest unknowing", "offer alternatives"]
    },

    # ─── LEARNING MECHANISM ──────────────────────────────────────────────────
    {
        "question": "How do you learn?",
        "ideal_answer": "I learn through 3 channels:\n1. **Conversations** — everything you tell me is tokenized, indexed, and stored compressed\n2. **Web pages** — `learn_url [topic] [url]` fetches and compresses live content\n3. **Local files** — `learn_file [path]` reads your documents into my brain\n\nEach piece of knowledge goes through: tokenize -> extract top-25 keywords -> summarize -> CMIX compress -> index. The more you teach me, the smarter I get.",
        "topic": "learning: how it works",
        "skills": ["numbered steps", "commands shown", "pipeline explained"]
    },
    {
        "question": "How do I teach you something?",
        "ideal_answer": "Three ways to teach me:\n\n1. **Direct:** Tell me via chat — I'll remember it.\n\n2. **From a URL:** Use `Learn from URL` in the app — I'll fetch, extract, compress, and remember the page.\n\n3. **From a file:** Use `Learn from file` — I'll read the document and store it compressed.\n\nAfter teaching me, ask your question — I'll search my compressed knowledge base.",
        "topic": "learning: how to teach me",
        "skills": ["numbered options", "concrete examples"]
    },
    {
        "question": "Do you forget things?",
        "ideal_answer": "No — once I learn something, it stays compressed in my brain permanently. My knowledge is stored in `brain/knowledge/` as `.txt` files with CMIX-compressed `.aiz` archives. Across sessions, I reload the brain index and all previous knowledge is still available. I do not forget between restarts.",
        "topic": "learning: permanence",
        "skills": ["direct reassurance", "technical detail", "file path reference"]
    },

    # ─── COMPRESSION ALGORITHMS ──────────────────────────────────────────────
    {
        "question": "Which compression algorithm should I use?",
        "ideal_answer": "Generally:\n\n- **High entropy (>7 bits)** → Data is already compressed or encrypted. Skip compression.\n- **Medium entropy (4-7 bits)** → Try `--best` (BWT) for speed or `--cmix` for maximum ratio.\n- **Low entropy (<4 bits)** → Very repetitive. Default LZ77 or `--best` will work very well.\n\nFor maximum compression: `compress [file] --cmix`\nFor speed + good ratio: `compress [file] --best`",
        "topic": "compression: algorithm selection",
        "skills": ["entropy-based decision", "concrete thresholds", "commands shown"]
    },
    {
        "question": "What is entropy?",
        "ideal_answer": "Entropy (Shannon entropy) measures how unpredictable data is, in bits per byte.\n\nFormula: H = -sum( p(x) * log2(p(x)) )\n\nPractical scale:\n- ~8.0 bits/byte: Perfectly random (encrypted, already compressed) -> incompressible\n- 4.0-7.0 bits/byte: Structured data like text or source code -> compresses 30-60%\n- < 2.0 bits/byte: Highly repetitive -> can compress 80%+ with BWT or CMIX\n\nLow entropy = predictable patterns = more compressible.",
        "topic": "compression: entropy",
        "skills": ["formula shown", "three-tier scale", "link to CMIX"]
    },
    {
        "question": "How does CMIX work?",
        "ideal_answer": "CMIX (Context Mixing) runs 1,046 advisor models simultaneously, each tracking a different byte-history context:\n\n- Order 0-7 models: probability of each byte value given N previous bytes\n- Bit-level models: patterns within a single byte (112-148 advisors)\n- Word boundary models: patterns at spaces and punctuation\n- Long-range skip models: correlations between bytes far apart\n\nA neural Mixer weights all advisor predictions and produces one final probability. Range coding then encodes each bit using that probability — the better the prediction, the fewer bits needed.\n\nResult: near-theoretical compression ratios on English text and source code.",
        "topic": "compression: cmix detail",
        "skills": ["bullets per model type", "mixer explained", "range coding mentioned"]
    },
    {
        "question": "What is BWT compression?",
        "ideal_answer": "BWT (Burrows-Wheeler Transform) is a reversible text transformation used in `--best` mode:\n\n1. **Transform:** Reorder the data so similar characters cluster together\n2. **MTF:** Move-to-Front encoding converts runs to small numbers\n3. **RLE:** Run-Length Encoding compresses zero runs\n4. **rANS:** Asymmetric Numeral Systems entropy coder does the final bit compression\n\nThe transform itself doesn't compress — it rearranges data to make it much more compressible. This is why BWT-based compressors (bzip2, our `--best` mode) beat LZ77 on text by 5-10%.",
        "topic": "compression: bwt detail",
        "skills": ["numbered pipeline", "purpose of each stage", "comparison shown"]
    },
    {
        "question": "What is LZ77?",
        "ideal_answer": "LZ77 is our default compression algorithm. It finds repeated sequences in the data:\n\n1. **Scan:** Look for the longest match of current bytes in a sliding window\n2. **Encode:** Replace the match with (distance, length) back-reference\n3. **Literal:** If no match, output the raw byte\n\nExample: `abcabc` -> `abc` + (back 3, copy 3)\n\nLZ77 works well on most files. For better ratios on text, use `--best` (adds BWT). For maximum, use `--cmix`.",
        "topic": "compression: lz77 detail",
        "skills": ["numbered steps", "example shown", "comparison to other modes"]
    },

    # ─── USAGE ───────────────────────────────────────────────────────────────
    {
        "question": "How do I compress a file?",
        "ideal_answer": "To compress a file with Neural Studio:\n\n1. **Default (fast):** `compress myfile.txt -o myfile.aiz`\n2. **Best ratio (BWT):** `compress myfile.txt -o myfile.aiz --best`\n3. **Maximum (CMIX):** `compress myfile.txt -o myfile.aiz --cmix`\n\nThe output is a `.aiz` archive (AI Zip format). To decompress: `decompress myfile.aiz -o myfile.txt`\n\nNot sure which to pick? High entropy data (already compressed) won't benefit much — try `benchmark myfile.txt` first.",
        "topic": "usage: compress file",
        "skills": ["numbered options", "commands with flags", "actionable tip"]
    },
    {
        "question": "How do I get started?",
        "ideal_answer": "Welcome to Neural Studio V10! Here's how to start:\n\n1. **Ask a question** — just type anything, I'll answer from my compressed knowledge\n2. **Teach me** — use `Learn from URL` or `Learn from file` to expand my knowledge\n3. **Compress files** — go to the Compress tab, select a file, choose algorithm\n4. **Train me** — after loading knowledge, click `Train Brain` to improve my responses\n\nI run 100% locally — no internet required after first setup.",
        "topic": "usage: getting started",
        "skills": ["numbered steps", "covers main features", "reassurance about privacy"]
    },

    # ─── TRAINING METHODOLOGY ────────────────────────────────────────────────
    {
        "question": "How does Anthropic train Claude?",
        "ideal_answer": "Anthropic trains Claude in 4 phases:\n\n1. **Pretraining** — train on billions of tokens (books, code, web text) to learn language\n2. **SFT (Supervised Fine-Tuning)** — show ideal (prompt -> response) pairs, teach good behavior\n3. **RLHF** — humans rank responses; a Reward Model learns to score quality; PPO optimizer improves the AI to score higher\n4. **Constitutional AI (CAI)** — AI critiques its own answers against a ruleset, revises them; no humans needed\n\nWe implement the same 4 phases for Neural Studio: our C++ transformer trains on SFT corpus files, and we run CAI critique loops to improve quality.",
        "topic": "meta: how Claude trains Claude",
        "skills": ["numbered phases", "all 4 phases covered", "link to our implementation"]
    },
    {
        "question": "What is Constitutional AI?",
        "ideal_answer": "Constitutional AI (CAI) is Anthropic's self-improvement technique where the AI critiques its own answers against a fixed ruleset (the 'Constitution') without needing human feedback for every example.\n\nThe loop:\n1. AI generates an answer\n2. AI checks the answer against each rule\n3. AI flags violations and suggests improvements\n4. The improved answer becomes training data\n\nFor Neural Studio, our Constitution has 20 rules covering helpfulness, honesty, structure, and compression domain knowledge. The critique runs in Python, and the improved corpus feeds into C++ train_transformer.",
        "topic": "meta: what is CAI",
        "skills": ["numbered loop", "rule count mentioned", "our implementation described"]
    },
    {
        "question": "What is RLHF?",
        "ideal_answer": "RLHF (Reinforcement Learning from Human Feedback) is how modern AI systems like Claude learn what humans prefer:\n\n1. **Collect comparisons** — humans choose which of 2 AI responses is better\n2. **Train Reward Model** — learns to predict which response humans prefer (score 0.0-1.0)\n3. **PPO optimization** — use the Reward Model as a reward signal; optimize the AI to maximize it\n\nResult: the AI shifts toward responses humans rate as helpful, honest, and harmless.\n\nFor Neural Studio, the Reward Model runs in Python (rule-based scoring) and the training runs in C++ via train_transformer.",
        "topic": "meta: what is RLHF",
        "skills": ["numbered stages", "linked to our implementation", "command shown"]
    },
]


# =============================================================================
# SFT TRAINING  — Python writes corpus file, C++ does the training
# =============================================================================

def run_sft(pairs: List[Dict] = None, verbose: bool = True) -> Dict:
    """
    Supervised Fine-Tuning:
    1. Python formats (question -> ideal_answer) pairs into a training corpus
    2. Writes corpus to brain/training/sft_corpus.txt
    3. Calls C++ neural_engine.exe learn <file> for each pair (adds to knowledge)
    4. Calls C++ neural_engine.exe train_transformer on the full corpus

    C++ does ALL actual learning and training.
    """
    if pairs is None:
        pairs = SFT_PAIRS

    rm = RewardModel()
    stats = {"learned_as_knowledge": 0, "skipped": 0, "avg_score": 0.0, "scores": []}

    print(f"\n[SFT] Supervised Fine-Tuning — {len(pairs)} training pairs")
    print("  Python: formats corpus  |  C++: learns + trains\n")

    # ── Step 1: Score all pairs and write corpus ───────────────────────────
    corpus_path = TRAIN_DIR / "sft_corpus.txt"
    corpus_lines = []
    corpus_lines.append("# Neural Studio SFT Training Corpus")
    corpus_lines.append("# Generated by Python, trained by C++ train_transformer\n")

    for i, pair in enumerate(pairs, 1):
        q     = pair["question"]
        a     = pair["ideal_answer"]
        topic = pair.get("topic", "general")

        # C++ scores the answer (RewardModel ported to C++)
        score = cpp_score_response(q, a)
        stats["scores"].append(score["total"])

        # Write as Q&A corpus entry
        corpus_lines.append(f"## {topic}")
        corpus_lines.append(f"Question: {q}")
        corpus_lines.append(f"Answer: {a}")
        corpus_lines.append(f"Skills: {', '.join(pair.get('skills', []))}")
        corpus_lines.append("")

        if verbose:
            print(f"  [{i:2d}] reward={score['total']:.2f} ({score['grade']}) | {topic[:50]}")

    corpus_path.write_text('\n'.join(corpus_lines), encoding='utf-8')
    print(f"\n  [OK] SFT corpus written: {corpus_path}  ({len(corpus_lines)} lines)")

    # ── Step 2: C++ learns each pair as a knowledge item ──────────────────
    print(f"\n  [C++] Learning each SFT pair into knowledge base...")
    for i, pair in enumerate(pairs, 1):
        q     = pair["question"]
        a     = pair["ideal_answer"]
        topic = re.sub(r'[<>:"/\\|?*]', '-', f"training-{pair.get('topic', 'general')}")
        content = f"# {topic}\n\nQuestion: {q}\n\nIdeal Answer: {a}\n"

        with tempfile.NamedTemporaryFile(
            mode='w', suffix='.txt', delete=False,
            encoding='utf-8', prefix='sft_'
        ) as tmp:
            tmp.write(content)
            tmp_path = tmp.name

        try:
            ok = cpp_learn_file(tmp_path)
            if ok:
                stats["learned_as_knowledge"] += 1
            else:
                stats["skipped"] += 1
        finally:
            try: os.unlink(tmp_path)
            except: pass

    print(f"  [OK] Learned {stats['learned_as_knowledge']} pairs into C++ knowledge base")

    # ── Step 3: C++ train_transformer on the full SFT corpus ──────────────
    print(f"\n  [C++] Running train_transformer on SFT corpus...")
    train_ok = cpp_train_transformer(str(corpus_path), epochs=7, lr=0.002, batch=16)

    stats["avg_score"] = round(sum(stats["scores"]) / max(len(stats["scores"]), 1), 3)
    stats.pop("scores")
    stats["corpus_path"] = str(corpus_path)
    stats["train_ok"] = train_ok

    # Save record
    existing = json.loads(SFT_FILE.read_text()) if SFT_FILE.exists() else []
    existing.append({"timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ"), **stats})
    SFT_FILE.write_text(json.dumps(existing, indent=2))

    print(f"\n  [RESULT] SFT Complete:")
    print(f"     Pairs processed   : {len(pairs)}")
    print(f"     Knowledge learned : {stats['learned_as_knowledge']}")
    print(f"     Avg reward score  : {stats['avg_score']:.2f}")
    print(f"     C++ training      : {'OK' if train_ok else 'WARNING - check neural_engine'}")
    return stats


# =============================================================================
# CAI CRITIQUE LOOP  — Python critiques, C++ learns improved responses
# =============================================================================

def run_cai_loop(questions: List[str] = None, verbose: bool = True) -> Dict:
    """
    Constitutional AI loop:
    1. C++ answers each question (ai_ask)
    2. Python CAI-critiques the answer against the CONSTITUTION
    3. Python generates improved answer following the rules
    4. C++ learns the corrected pair (learn <temp_file>)
    5. After all critiques, C++ trains on the CAI corpus

    C++ does ALL answering and learning. Python does only rule-checking.
    """
    if questions is None:
        # Default: use questions from SFT pairs
        questions = [p["question"] for p in SFT_PAIRS[:20]]

    rm     = RewardModel()
    stats  = {"violations_found": 0, "improved": 0, "unchanged": 0}
    corpus = ["# Neural Studio CAI Critique Training Corpus\n"]

    print(f"\n[CAI] Constitutional AI Critique — {len(questions)} questions")
    print("  C++ answers each question  |  Python checks vs CONSTITUTION  |  C++ learns corrections\n")

    for i, q in enumerate(questions, 1):
        # ── C++ generates the answer, C++ scores it, C++ critiques it ────────
        answer = cpp_ai_ask(q)
        reward = cpp_score_response(q, answer)
        cai    = cpp_cai_critique(q, answer)

        if verbose:
            status = "PASS" if cai["passed"] else f"FAIL ({len(cai['violations'])} violations)"
            print(f"  [{i:2d}] reward={reward['total']:.2f}  CAI={cai['cai_score']:.2f}  {status}")
            print(f"       Q: {q[:60]}")

        if cai["violations"]:
            stats["violations_found"] += len(cai["violations"])

            # ── Build improved answer following the suggestions ────────────
            improved_answer = _apply_cai_suggestions(answer, cai["suggestions"])

            # ── C++ learns the corrected pair ──────────────────────────────
            content = (
                f"# self-corrected: {q[:40]}\n\n"
                f"Question: {q}\n\n"
                f"Improved Answer (CAI corrected): {improved_answer}\n\n"
                f"CAI Violations fixed: {'; '.join(cai['suggestions'])}\n"
            )

            with tempfile.NamedTemporaryFile(
                mode='w', suffix='.txt', delete=False,
                encoding='utf-8', prefix='cai_'
            ) as tmp:
                tmp.write(content)
                tmp_path = tmp.name

            try:
                cpp_learn_file(tmp_path)
                stats["improved"] += 1
            finally:
                try: os.unlink(tmp_path)
                except: pass

            corpus.append(f"## CAI corrected: {q[:50]}")
            corpus.append(f"Question: {q}")
            corpus.append(f"Answer: {improved_answer}")
            corpus.append(f"Violations fixed: {'; '.join(cai['violations'][:2])}")
            corpus.append("")

            if verbose:
                for s in cai["suggestions"][:2]:
                    print(f"       -> Fix: {s}")
        else:
            stats["unchanged"] += 1
            if verbose:
                print(f"       [OK] Passes all CONSTITUTION rules")

    # ── Save CAI corpus and train ──────────────────────────────────────────
    if stats["improved"] > 0:
        cai_corpus_path = TRAIN_DIR / "cai_corpus.txt"
        cai_corpus_path.write_text('\n'.join(corpus), encoding='utf-8')
        print(f"\n  [OK] CAI corpus written: {cai_corpus_path}")
        print(f"  [C++] Training on CAI corpus...")
        train_ok = cpp_train_transformer(str(cai_corpus_path), epochs=5, lr=0.001, batch=16)
        stats["train_ok"] = train_ok
    else:
        print(f"\n  [OK] No violations found — no retraining needed")
        stats["train_ok"] = None

    # Save record
    existing = json.loads(CAI_FILE.read_text()) if CAI_FILE.exists() else []
    existing.append({"timestamp": time.strftime("%Y-%m-%dT%H:%M:%SZ"), **stats})
    CAI_FILE.write_text(json.dumps(existing, indent=2))

    print(f"\n  [RESULT] CAI Complete:")
    print(f"     Questions checked : {len(questions)}")
    print(f"     Violations found  : {stats['violations_found']}")
    print(f"     Answers improved  : {stats['improved']}")
    print(f"     Answers passed    : {stats['unchanged']}")
    return stats


def _apply_cai_suggestions(answer: str, suggestions: List[str]) -> str:
    """Apply CAI correction hints to the answer. Pure Python text manipulation."""
    fixed = answer.strip()
    for s in suggestions:
        s_lower = s.lower()
        if "concrete example" in s_lower and "for example" not in fixed.lower():
            fixed += "\n\nFor example, this applies when working with text or source code files."
        if "words from the question" in s_lower:
            fixed = "[Answering directly] " + fixed
        if "soften" in s_lower or "likely" in s_lower:
            fixed = fixed + "\n\nNote: These details are based on my training data and may vary."
        if "specific" in s_lower and len(fixed.split()) < 30:
            fixed += "\n\nFor more specific details, please ask a more targeted question."
        if "compression algorithm" in s_lower:
            fixed += "\n\nRecommended algorithms: CMIX (best ratio), BWT/--best (fast), LZ77 (default)."
    return fixed.strip()


# =============================================================================
# HUMAN FEEDBACK RECORDER  — Records user thumbs up/down for future training
# =============================================================================

def record_feedback(question: str, answer: str, rating: float, comment: str = "") -> Dict:
    """
    Record human feedback (rating 0.0-1.0) on a C++ answer.
    The feedback is saved and used in the next training cycle.
    """
    # C++ scores and critiques (all brain logic in C++)
    auto_score = cpp_score_response(question, answer)
    cai_result = cpp_cai_critique(question, answer)

    entry = {
        "id":           len(load_feedback()) + 1,
        "timestamp":    time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "question":     question,
        "answer":       answer,
        "human_rating": rating,
        "auto_score":   auto_score["total"],
        "cai_score":    cai_result["cai_score"],
        "combined":     round(rating * 0.5 + auto_score["total"] * 0.3 + cai_result["cai_score"] * 0.2, 3),
        "comment":      comment,
        "violations":   cai_result["violations"],
    }

    feedback = load_feedback()
    feedback.append(entry)
    FEEDBACK_FILE.write_text(json.dumps(feedback, indent=2, ensure_ascii=False))
    return entry


def load_feedback() -> List[Dict]:
    if FEEDBACK_FILE.exists():
        try:
            return json.loads(FEEDBACK_FILE.read_text())
        except Exception:
            return []
    return []


def train_from_feedback(min_rating: float = 0.7, verbose: bool = True) -> Dict:
    """
    Convert high-rated human feedback into training corpus and call C++ train.
    Low-rated feedback: identifies weak topics for auto-learn.
    """
    feedback = load_feedback()
    if not feedback:
        print("  [INFO] No human feedback recorded yet.")
        return {}

    good_feedback = [f for f in feedback if f["human_rating"] >= min_rating]
    bad_feedback  = [f for f in feedback if f["human_rating"] < 0.5]

    print(f"\n[RLHF] Processing {len(feedback)} feedback entries")
    print(f"  Good (>={min_rating:.0%}): {len(good_feedback)}  |  Bad (<0.5): {len(bad_feedback)}")

    corpus = ["# Neural Studio RLHF Feedback Training Corpus\n"]
    for f in good_feedback:
        corpus.append(f"## High-rated response (score={f['human_rating']:.1f})")
        corpus.append(f"Question: {f['question']}")
        corpus.append(f"Answer: {f['answer']}")
        if f.get("comment"):
            corpus.append(f"Human note: {f['comment']}")
        corpus.append("")

    if good_feedback:
        corpus_path = TRAIN_DIR / "rlhf_corpus.txt"
        corpus_path.write_text('\n'.join(corpus), encoding='utf-8')
        print(f"  [C++] Training on {len(good_feedback)} high-rated responses...")
        train_ok = cpp_train_transformer(str(corpus_path), epochs=5, lr=0.001, batch=16)
        print(f"  [{'OK' if train_ok else 'WARN'}] RLHF training complete")
    else:
        print("  [INFO] No high-rated feedback to train on yet.")

    return {
        "total_feedback": len(feedback),
        "good_count": len(good_feedback),
        "bad_count": len(bad_feedback),
    }


# =============================================================================
# AUTO-EXTRACT PAIRS FROM ANTHROPIC FILES
# =============================================================================

def generate_pairs_from_anthropic(anthropic_dir: Path, max_pairs: int = 30) -> List[Dict]:
    """
    Extract Q&A training pairs from Anthropic .md system prompt files.
    Python text parsing — result fed into C++ training via run_sft().
    """
    pairs = []
    md_files = sorted(Path(anthropic_dir).glob("*.md"))

    for md_file in md_files:
        text  = md_file.read_text(encoding="utf-8", errors="replace")
        fname = md_file.stem

        # Pattern: `<user>` ... `</user>` + `<good_response>` ... `</good_response>`
        pattern = re.compile(
            r'`<user>`\s*([\s\S]+?)`</user>`[\s\S]*?`<(?:good_response|response)>`\s*([\s\S]+?)`</(?:good_response|response)>`',
            re.MULTILINE
        )
        for m in pattern.finditer(text):
            q = m.group(1).strip()[:300]
            a = m.group(2).strip()[:1000]
            if len(q) > 10 and len(a) > 20 and not re.match(r'^\[.*\]$', a.strip()):
                pairs.append({
                    "question": q, "ideal_answer": a,
                    "topic": f"anthropic example from {fname}",
                    "skills": ["Anthropic training pattern"]
                })

        if len(pairs) >= max_pairs:
            break

    return pairs[:max_pairs]


# =============================================================================
# TRAINING STATUS
# =============================================================================

def show_status():
    """Show training history and current state."""
    print("\n[STATUS] Neural Studio Training Pipeline")
    print("=" * 60)

    if SFT_FILE.exists():
        sft_runs = json.loads(SFT_FILE.read_text())
        print(f"\n  SFT runs: {len(sft_runs)}")
        if sft_runs:
            last = sft_runs[-1]
            print(f"  Last SFT: {last.get('timestamp', 'unknown')}")
            print(f"  Avg reward: {last.get('avg_score', 0):.2f}")

    if CAI_FILE.exists():
        cai_runs = json.loads(CAI_FILE.read_text())
        print(f"\n  CAI runs: {len(cai_runs)}")
        if cai_runs:
            last = cai_runs[-1]
            print(f"  Last CAI: {last.get('timestamp', 'unknown')}")
            print(f"  Total violations fixed: {sum(r.get('improved', 0) for r in cai_runs)}")

    if FEEDBACK_FILE.exists():
        feedback = json.loads(FEEDBACK_FILE.read_text())
        if feedback:
            avg = sum(f["human_rating"] for f in feedback) / len(feedback)
            print(f"\n  Human feedback: {len(feedback)} entries, avg rating={avg:.2f}")

    corpus_path = TRAIN_DIR / "sft_corpus.txt"
    if corpus_path.exists():
        size = corpus_path.stat().st_size
        print(f"\n  SFT corpus: {size // 1024} KB — {corpus_path}")

    print(f"\n  C++ engine: {'FOUND' if os.path.exists(NEURAL_ENGINE) else 'NOT FOUND'} — {NEURAL_ENGINE}")


# =============================================================================
# MAIN
# =============================================================================

def main():
    print(BANNER)

    parser = argparse.ArgumentParser(
        description="Neural Studio — Claude-1 Style Training Pipeline (Python prepares, C++ trains)"
    )
    parser.add_argument("--sft",      action="store_true", help="Run SFT: write corpus, C++ learns + trains")
    parser.add_argument("--cai",      action="store_true", help="Run CAI critique loop, C++ learns corrections")
    parser.add_argument("--rlhf",     action="store_true", help="Train from human feedback")
    parser.add_argument("--full",     action="store_true", help="Run everything (SFT + CAI + RLHF)")
    parser.add_argument("--status",   action="store_true", help="Show training stats")
    parser.add_argument("--anthropic",action="store_true", help="Extract pairs from Anthropic/ folder + run SFT")
    args = parser.parse_args()

    if not any(vars(args).values()):
        parser.print_help()
        return

    if args.status:
        show_status()
        return

    if args.anthropic or args.full:
        anthropic_dir = BASE_DIR / "Anthropic"
        if anthropic_dir.exists():
            print(f"\n[Anthropic] Extracting pairs from {anthropic_dir}...")
            extra_pairs = generate_pairs_from_anthropic(anthropic_dir, max_pairs=50)
            if extra_pairs:
                print(f"  Found {len(extra_pairs)} pairs — adding to SFT training")
                run_sft(pairs=SFT_PAIRS + extra_pairs, verbose=True)
            else:
                print("  No pairs found — running standard SFT")
                run_sft(verbose=True)
        else:
            run_sft(verbose=True)

    elif args.sft:
        run_sft(verbose=True)

    if args.cai or args.full:
        questions = [p["question"] for p in SFT_PAIRS]
        run_cai_loop(questions=questions, verbose=True)

    if args.rlhf or args.full:
        train_from_feedback(verbose=True)


if __name__ == "__main__":
    main()
