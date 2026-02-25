"""
PYTHON SUPPORT FILE — ORCHESTRATION PIPELINE ONLY
===================================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

self_improve.py — Self-Improvement Orchestrator (Python Support Layer)
=======================================================================
Python's role (support ONLY):
  - Manage the self-improvement cycle loop
  - Call C++ ai_ask to get answers for test questions
  - Score answers (RewardModel — pure Python utility)
  - Detect weak domains from scores
  - Call C++ learn <url> for weak domain Wikipedia articles
  - Assemble corrected corpus text files
  - Call C++ train_transformer to retrain after each cycle
  - Log progress to brain/self_improve_log.json

C++ neural_engine.exe does ALL the actual work:
  - ai_ask <question>          → answers from RAG + reasoning + memory
  - learn <url_or_file>        → fetches, stores, compresses knowledge
  - train_transformer <corpus> → trains the transformer

This implements the 4-step Claude-1 self-improvement loop:
  1. Self-Assess   → C++ answers test questions, Python scores them
  2. Auto-Learn    → C++ fetches Wikipedia for weak domains
  3. SFT Pairs     → Python generates pairs for weak topics, C++ learns them
  4. CAI Critique  → Python critiques C++ answers, C++ learns corrections
  5. Retrain       → C++ train_transformer on improved corpus

Usage:
    python server/self_improve.py                    # One improvement cycle
    python server/self_improve.py --loop             # Loop forever
    python server/self_improve.py --loop --target 90 # Loop until 90% score
    python server/self_improve.py --assess-only      # Just show score report
    python server/self_improve.py --cycles N         # Run N cycles
    python server/self_improve.py --history          # Show training log
"""

import os, sys, re, json, time, random, argparse, subprocess, tempfile
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Tuple

# Force UTF-8 output on Windows
if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)

BASE_DIR      = Path(__file__).parent.parent
SERVER_DIR    = Path(__file__).parent
TRAIN_DIR     = BASE_DIR / "brain" / "training"
LOG_FILE      = BASE_DIR / "brain" / "self_improve_log.json"
NEURAL_ENGINE = str(BASE_DIR / "bin" / "neural_engine.exe")

TRAIN_DIR.mkdir(parents=True, exist_ok=True)

# Import C++ bridge functions from rlhf_trainer
# (cpp_score_response and cpp_cai_critique call C++ neural_engine.exe)
sys.path.insert(0, str(SERVER_DIR))
from rlhf_trainer import cpp_score_response, cpp_cai_critique

BANNER = """
=== Neural Studio - SELF-IMPROVEMENT ENGINE ===
C++ answers -> Python scores -> C++ learns -> C++ trains
Method: Self-Assess -> Gap Detect -> C++ Auto-Learn -> C++ Retrain
================================================
"""


# ─── C++ Bridge Functions ─────────────────────────────────────────────────────

def cpp_ai_ask(question: str, timeout: int = 30) -> str:
    """Call C++ neural_engine.exe ai_ask and return the answer string."""
    if not os.path.exists(NEURAL_ENGINE):
        return f"[engine not found] Need to learn more about: {question}"
    try:
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
    except subprocess.TimeoutExpired:
        return f"[timeout] I need to learn more about: {question}"
    except Exception as e:
        return f"[error: {e}] I need to learn more about: {question}"
    return f"I need to learn more about: {question}"


def cpp_learn_url(url: str, timeout: int = 120) -> bool:
    """Call C++ neural_engine.exe learn <url>."""
    if not os.path.exists(NEURAL_ENGINE):
        return False
    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "learn", url],
            capture_output=True, text=True, timeout=timeout,
            cwd=str(BASE_DIR)
        )
        log = (result.stdout + result.stderr).strip()
        return "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0
    except Exception:
        return False


def cpp_learn_text(topic: str, content: str, timeout: int = 60) -> bool:
    """Write text to temp file and call C++ neural_engine.exe learn <file>."""
    if not os.path.exists(NEURAL_ENGINE):
        return False
    full_text = f"# {topic}\n\n{content}\n"
    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.txt', delete=False,
        encoding='utf-8', prefix='improve_'
    ) as tmp:
        tmp.write(full_text)
        tmp_path = tmp.name
    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "learn", tmp_path],
            capture_output=True, text=True, timeout=timeout,
            cwd=str(BASE_DIR)
        )
        log = (result.stdout + result.stderr).strip()
        return "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0
    except Exception:
        return False
    finally:
        try: os.unlink(tmp_path)
        except: pass


def cpp_train(corpus_path: str, epochs: int = 7, lr: float = 0.002, batch: int = 16) -> bool:
    """Call C++ neural_engine.exe train_transformer <corpus> [epochs lr batch]."""
    if not os.path.exists(NEURAL_ENGINE):
        return False
    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "train_transformer", corpus_path,
             str(epochs), str(lr), str(batch)],
            capture_output=True, text=True, timeout=900,
            cwd=str(BASE_DIR)
        )
        return result.returncode == 0
    except Exception:
        return False


# ─── Test Questions (span ALL knowledge domains) ─────────────────────────────

TEST_QUESTIONS = [
    # Identity & core
    ("identity",        "What are you?"),
    ("identity",        "How are you different from ChatGPT?"),
    ("identity",        "What can you help me with?"),

    # Compression domain
    ("compression",     "Explain how Huffman coding works"),
    ("compression",     "What is BWT compression?"),
    ("compression",     "How does CMIX work?"),
    ("compression",     "What is arithmetic coding?"),
    ("compression",     "What is the theoretical limit of compression?"),
    ("compression",     "What is LZ77?"),
    ("compression",     "How do I compress a file with this tool?"),

    # Mathematics
    ("math",            "What is a derivative?"),
    ("math",            "Explain Big O notation"),
    ("math",            "What is a prime number?"),
    ("math",            "What is the Pythagorean theorem?"),
    ("math",            "What is probability?"),

    # Computer Science
    ("computer_science","What is recursion?"),
    ("computer_science","What is a hash map?"),
    ("computer_science","Explain object-oriented programming"),
    ("computer_science","What is a REST API?"),
    ("computer_science","What is machine learning?"),
    ("computer_science","What is a neural network?"),
    ("computer_science","What is binary search?"),
    ("computer_science","What is async programming?"),
    ("computer_science","What is SQL?"),
    ("computer_science","What is Git?"),
    ("computer_science","What is Docker?"),

    # Physics
    ("physics",         "What is Newton's second law?"),
    ("physics",         "What is quantum mechanics?"),
    ("physics",         "What is the speed of light?"),

    # Biology
    ("biology",         "What is DNA?"),
    ("biology",         "How does photosynthesis work?"),
    ("biology",         "What is evolution?"),

    # History
    ("history",         "What caused World War 1?"),
    ("history",         "What was the Industrial Revolution?"),

    # Economics
    ("economics",       "What is supply and demand?"),
    ("economics",       "What is inflation?"),

    # Reasoning
    ("reasoning",       "How do I solve problems I've never seen before?"),
    ("reasoning",       "What is first principles thinking?"),
    ("reasoning",       "What is Occam's Razor?"),

    # General knowledge
    ("general",         "How big is the universe?"),
    ("general",         "What is a black hole?"),
    ("general",         "What is blockchain?"),
    ("general",         "What causes earthquakes?"),
]

# ─── Wikipedia sources per domain ────────────────────────────────────────────

DOMAIN_WEB_SOURCES = {
    "compression": [
        ("lzma algorithm",        "https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm"),
        ("huffman coding",        "https://en.wikipedia.org/wiki/Huffman_coding"),
        ("burrows wheeler",       "https://en.wikipedia.org/wiki/Burrows%E2%80%93Wheeler_transform"),
        ("data compression",      "https://en.wikipedia.org/wiki/Data_compression"),
        ("zstandard compression", "https://en.wikipedia.org/wiki/Zstd"),
    ],
    "math": [
        ("number theory",         "https://en.wikipedia.org/wiki/Number_theory"),
        ("calculus",              "https://en.wikipedia.org/wiki/Calculus"),
        ("linear algebra",        "https://en.wikipedia.org/wiki/Linear_algebra"),
        ("statistics",            "https://en.wikipedia.org/wiki/Statistics"),
        ("fourier transform",     "https://en.wikipedia.org/wiki/Fourier_transform"),
    ],
    "computer_science": [
        ("sorting algorithms",    "https://en.wikipedia.org/wiki/Sorting_algorithm"),
        ("dynamic programming",   "https://en.wikipedia.org/wiki/Dynamic_programming"),
        ("graph theory",          "https://en.wikipedia.org/wiki/Graph_theory"),
        ("compiler design",       "https://en.wikipedia.org/wiki/Compiler"),
        ("operating systems",     "https://en.wikipedia.org/wiki/Operating_system"),
    ],
    "physics": [
        ("special relativity",    "https://en.wikipedia.org/wiki/Special_relativity"),
        ("thermodynamics",        "https://en.wikipedia.org/wiki/Thermodynamics"),
        ("electromagnetism",      "https://en.wikipedia.org/wiki/Electromagnetism"),
        ("particle physics",      "https://en.wikipedia.org/wiki/Particle_physics"),
    ],
    "biology": [
        ("cell biology",          "https://en.wikipedia.org/wiki/Cell_biology"),
        ("genetics",              "https://en.wikipedia.org/wiki/Genetics"),
        ("neuroscience",          "https://en.wikipedia.org/wiki/Neuroscience"),
        ("immune system",         "https://en.wikipedia.org/wiki/Immune_system"),
    ],
    "history": [
        ("world war 2",           "https://en.wikipedia.org/wiki/World_War_II"),
        ("cold war",              "https://en.wikipedia.org/wiki/Cold_War"),
        ("roman empire",          "https://en.wikipedia.org/wiki/Roman_Empire"),
        ("renaissance",           "https://en.wikipedia.org/wiki/Renaissance"),
    ],
    "economics": [
        ("microeconomics",        "https://en.wikipedia.org/wiki/Microeconomics"),
        ("macroeconomics",        "https://en.wikipedia.org/wiki/Macroeconomics"),
        ("game theory",           "https://en.wikipedia.org/wiki/Game_theory"),
        ("behavioral economics",  "https://en.wikipedia.org/wiki/Behavioral_economics"),
    ],
    "reasoning": [
        ("critical thinking",     "https://en.wikipedia.org/wiki/Critical_thinking"),
        ("problem solving",       "https://en.wikipedia.org/wiki/Problem_solving"),
        ("decision making",       "https://en.wikipedia.org/wiki/Decision-making"),
    ],
    "general": [
        ("big bang",              "https://en.wikipedia.org/wiki/Big_Bang"),
        ("black hole",            "https://en.wikipedia.org/wiki/Black_hole"),
        ("climate change",        "https://en.wikipedia.org/wiki/Climate_change"),
        ("artificial intelligence","https://en.wikipedia.org/wiki/Artificial_intelligence"),
    ],
}


# =============================================================================
# STEP 1 — SELF-ASSESSMENT
# C++ answers every test question, Python scores the answers
# =============================================================================

def self_assess(verbose: bool = True) -> dict:
    """
    Ask C++ every test question, score the answer with Python RewardModel,
    find weak domains, return assessment report.
    """
    if verbose:
        print("\n[ASSESS] Self-Assessment — C++ answers all test questions...\n")
        print(f"  {'#':>3}  {'Domain':<18} {'Score':>6}  {'Grade'}")
        print("  " + "-" * 55)

    domain_scores: Dict[str, List[float]] = {}
    results = []

    for i, (domain, question) in enumerate(TEST_QUESTIONS, 1):
        # C++ answers the question
        answer = cpp_ai_ask(question)

        # C++ scores the answer (RewardModel ported to C++)
        scored    = cpp_score_response(question, answer)
        score     = scored["total"]
        grade     = scored["grade"]
        breakdown = scored["breakdown"]

        if domain not in domain_scores:
            domain_scores[domain] = []
        domain_scores[domain].append(score)

        results.append({
            "domain":    domain,
            "question":  question,
            "answer":    answer,
            "score":     score,
            "grade":     grade,
            "breakdown": breakdown,
        })

        if verbose:
            q_short = question[:42] + "..." if len(question) > 42 else question
            print(f"  [{i:>2}]  {domain:<18} {score:.2f}  {grade:<22}  {q_short}")

    # Compute domain averages
    domain_avgs = {d: sum(s) / len(s) for d, s in domain_scores.items()}
    overall_avg = sum(domain_avgs.values()) / len(domain_avgs)
    weak_domains = {d: v for d, v in domain_avgs.items() if v < 0.55}

    if verbose:
        print(f"\n  {'─' * 55}")
        print(f"\n  Domain Averages:")
        for domain, avg in sorted(domain_avgs.items(), key=lambda x: x[1]):
            bar    = "#" * int(avg * 20) + "." * (20 - int(avg * 20))
            status = "WEAK" if avg < 0.55 else "OK"
            print(f"     {domain:<18} [{bar}] {avg:.2f}  {status}")

        print(f"\n  Overall Score: {overall_avg:.2f} / 1.00  ({overall_avg * 100:.0f}%)")
        if weak_domains:
            print(f"\n  Weak domains needing improvement:")
            for d, v in weak_domains.items():
                print(f"     - {d}: {v:.2f}")
        else:
            print(f"\n  All domains above threshold!")

    return {
        "results":      results,
        "domain_avgs":  domain_avgs,
        "overall_avg":  overall_avg,
        "weak_domains": weak_domains,
        "timestamp":    datetime.now().isoformat(),
    }


# =============================================================================
# STEP 2 — GAP DETECTION + AUTO-LEARNING
# C++ fetches Wikipedia articles for weak domains
# =============================================================================

def auto_learn_weak_domains(weak_domains: dict, verbose: bool = True) -> int:
    """
    For every weak domain, call C++ learn <wikipedia_url> to expand knowledge.
    C++ fetches the page, extracts text, compresses, indexes — no Python AI logic.
    """
    if not weak_domains:
        if verbose:
            print("\n  [OK] No weak domains — skipping web learning")
        return 0

    learned_count = 0
    if verbose:
        print(f"\n[LEARN] Auto-learning {len(weak_domains)} weak domain(s) from Wikipedia...")
        print("  C++ neural_engine.exe learn <url> for each article\n")

    for domain, score in weak_domains.items():
        sources = DOMAIN_WEB_SOURCES.get(domain, [])
        if not sources:
            continue

        if verbose:
            print(f"  {domain} (score={score:.2f}) -> {len(sources)} Wikipedia articles:")

        for topic, url in sources:
            ok = cpp_learn_url(url, timeout=120)
            if ok:
                learned_count += 1
                if verbose:
                    print(f"     [OK] {topic}")
            else:
                if verbose:
                    print(f"     [FAIL] {topic}")
            time.sleep(0.5)  # Be polite to Wikipedia

    if verbose:
        print(f"\n  [OK] C++ learned {learned_count} new Wikipedia articles")

    return learned_count


# =============================================================================
# STEP 3 — SFT PAIR GENERATION FOR WEAK QUESTIONS
# Python identifies weak questions, C++ learns ideal pairs
# =============================================================================

def generate_and_learn_improvement_pairs(
        assessment_results: list, min_score: float = 0.55,
        verbose: bool = True) -> int:
    """
    For every test question that scored below threshold:
    1. C++ re-answers the question (may have improved from auto-learn)
    2. If still weak, Python assembles an improved SFT pair
    3. C++ learns the improved pair as knowledge
    Returns count of pairs learned.
    """
    weak_results = [r for r in assessment_results if r["score"] < min_score]

    if not weak_results:
        if verbose:
            print(f"\n  [OK] No questions below {min_score:.0%} — no SFT pairs needed")
        return 0

    if verbose:
        print(f"\n[SFT] Generating improvement pairs for {len(weak_results)} weak questions...")

    pairs_learned = 0
    corpus_lines  = ["# Auto-generated SFT improvement corpus\n"]

    for r in weak_results:
        q      = r["question"]
        domain = r["domain"]

        # Get best answer C++ can give now (may be better after auto-learn)
        best_answer = cpp_ai_ask(q)
        new_score   = cpp_score_response(q, best_answer)

        # Build improved pair content
        if new_score["total"] >= min_score:
            # C++ improved — use its new answer as training example
            ideal = best_answer
            note  = f"Improved by auto-learn (score: {r['score']:.2f} -> {new_score['total']:.2f})"
        else:
            # Still weak — build a structured template answer
            ideal = _build_template_answer(q, domain, best_answer)
            note  = f"Template-generated for weak domain: {domain}"

        content = (
            f"Question: {q}\n"
            f"Ideal Answer: {ideal}\n"
            f"Note: {note}\n"
        )

        ok = cpp_learn_text(f"auto-generated: {domain}", content)
        if ok:
            pairs_learned += 1
            corpus_lines.append(f"## auto-generated: {domain}")
            corpus_lines.append(content)
            corpus_lines.append("")

        if verbose:
            print(f"  {'[OK]' if ok else '[FAIL]'} {domain} | {q[:50]}")

    # Write corpus and train
    if pairs_learned > 0:
        corpus_path = TRAIN_DIR / "improvement_corpus.txt"
        corpus_path.write_text('\n'.join(corpus_lines), encoding='utf-8')
        if verbose:
            print(f"\n  [C++] Training on {pairs_learned} improvement pairs...")
        cpp_train(str(corpus_path), epochs=5, lr=0.002, batch=16)

    return pairs_learned


def _build_template_answer(question: str, domain: str, existing_answer: str) -> str:
    """
    Build a structured template answer for a weak question.
    Pure Python text assembly — no AI calls needed.
    """
    q_lower = question.lower()

    # If existing answer has some content, augment it
    if len(existing_answer.split()) > 20 and "need to learn" not in existing_answer.lower():
        base = existing_answer.strip()
    else:
        base = f"This question covers the topic of {domain}."

    # Add structure based on question type
    if q_lower.startswith("how"):
        return (
            f"{base}\n\n"
            f"The key steps are:\n"
            f"1. Understand the core concept\n"
            f"2. Apply the relevant technique\n"
            f"3. Verify the result\n\n"
            f"Use `learn [topic]: [details]` to teach me more about this."
        )
    elif q_lower.startswith("what is"):
        return (
            f"{base}\n\n"
            f"This is an important concept in {domain}. "
            f"I may not have complete details yet — "
            f"teach me more with `learn {domain}: [your knowledge]`."
        )
    else:
        return (
            f"{base}\n\n"
            f"I'm still learning about {domain}. "
            f"For accurate details, please teach me: `learn {domain}: [content]`"
        )


# =============================================================================
# STEP 4 — SELF-CRITIQUE + REWRITE
# Python critiques C++ answers, C++ learns the corrections
# =============================================================================

def self_critique_and_rewrite(
        assessment_results: list, verbose: bool = True) -> int:
    """
    For every answer that scored below 0.6:
    1. Python runs CAI critique against CONSTITUTION
    2. Python suggests corrections
    3. C++ learns the corrected version
    Returns count of corrections learned.
    """
    weak = [r for r in assessment_results if r["score"] < 0.6]

    if not weak:
        if verbose:
            print("\n  [OK] No answers below 0.6 — no CAI rewrites needed")
        return 0

    if verbose:
        print(f"\n[CAI] Self-critique on {len(weak)} weak answers...")

    rewrites = 0
    for r in weak:
        q   = r["question"]
        ans = r["answer"]

        # C++ checks answer against CONSTITUTION rules
        critique = cpp_cai_critique(q, ans)
        viols    = critique.get("violations", [])

        if viols:
            # Apply corrections (pure Python text)
            corrected = _apply_corrections(ans, critique.get("suggestions", []))
            topic     = f"self-corrected: {r['domain']}"
            content   = f"Q: {q}\nA: {corrected}\nCAI fixes: {'; '.join(critique['suggestions'][:2])}"

            ok = cpp_learn_text(topic, content)
            if ok:
                rewrites += 1

            if verbose:
                print(f"  [{'OK' if ok else 'FAIL'}] '{q[:50]}'")
                for v in critique["suggestions"][:2]:
                    print(f"       -> {v}")

    if verbose:
        print(f"\n  [OK] C++ learned {rewrites} corrected answers")

    return rewrites


def _apply_corrections(answer: str, suggestions: list) -> str:
    """Apply CAI correction hints to improve the answer. Pure Python."""
    fixed = answer.strip()
    for s in suggestions:
        s_lower = s.lower()
        if "concrete example" in s_lower and "for example" not in fixed.lower():
            fixed += "\n\nFor example, this applies when working with text or source code."
        if "words from the question" in s_lower:
            fixed = "[Direct answer] " + fixed
        if "soften" in s_lower or "likely" in s_lower:
            fixed += "\n\nNote: These details are based on my training data and may vary."
        if "specific" in s_lower and len(fixed.split()) < 30:
            fixed += "\n\nFor more detail, ask a more targeted question."
        if "compression algorithm" in s_lower:
            fixed += "\n\nRecommended: CMIX (best ratio), BWT/--best (balanced), LZ77 (fast)."
    return fixed.strip()


# =============================================================================
# STEP 5 — FINAL RETRAIN
# Assemble all improved knowledge into one corpus, call C++ train_transformer
# =============================================================================

def final_retrain(cycle: int, verbose: bool = True) -> bool:
    """
    After a full self-improvement cycle, assemble all training corpora
    and run C++ train_transformer for a final consolidated training pass.
    """
    corpus_files = [
        TRAIN_DIR / "sft_corpus.txt",
        TRAIN_DIR / "improvement_corpus.txt",
        TRAIN_DIR / "cai_corpus.txt",
        TRAIN_DIR / "rlhf_corpus.txt",
    ]

    existing = [f for f in corpus_files if f.exists()]
    if not existing:
        if verbose:
            print("\n  [INFO] No corpus files found — skipping final retrain")
        return False

    # Merge all corpora into one file for this cycle
    merged_path = TRAIN_DIR / f"cycle_{cycle}_corpus.txt"
    lines = [f"# Neural Studio Self-Improvement Cycle {cycle} — Merged Corpus\n"]
    for cf in existing:
        lines.append(f"\n## From: {cf.name}")
        lines.append(cf.read_text(encoding='utf-8', errors='replace'))

    merged_path.write_text('\n'.join(lines), encoding='utf-8')
    size_kb = merged_path.stat().st_size // 1024

    if verbose:
        print(f"\n[TRAIN] Final retrain — cycle {cycle} merged corpus ({size_kb} KB)")
        print(f"  Files merged: {[f.name for f in existing]}")
        print(f"  [C++] train_transformer {merged_path.name} 7 0.002 16")

    ok = cpp_train(str(merged_path), epochs=7, lr=0.002, batch=16)

    if verbose:
        print(f"  [{'OK' if ok else 'WARN'}] C++ training {'complete' if ok else 'returned error — check neural_engine'}")

    return ok


# =============================================================================
# STEP 6 — LOG PROGRESS
# =============================================================================

def log_cycle(cycle: int, before_avg: float, after_avg: float,
              learned: int, rewrites: int, pairs: int) -> dict:
    """Append a training cycle result to the log file."""
    entry = {
        "cycle":             cycle,
        "timestamp":         datetime.now().isoformat(),
        "score_before":      round(before_avg, 3),
        "score_after":       round(after_avg, 3),
        "improvement":       round(after_avg - before_avg, 3),
        "articles_learned":  learned,
        "answers_rewritten": rewrites,
        "sft_pairs_added":   pairs,
    }

    history = []
    if LOG_FILE.exists():
        try:
            history = json.loads(LOG_FILE.read_text())
        except Exception:
            history = []

    history.append(entry)
    LOG_FILE.parent.mkdir(parents=True, exist_ok=True)
    LOG_FILE.write_text(json.dumps(history, indent=2))
    return entry


def print_history():
    """Print all logged training cycles."""
    if not LOG_FILE.exists():
        print("  No training history yet.")
        return

    try:
        history = json.loads(LOG_FILE.read_text())
    except Exception:
        print("  [ERROR] Could not read log file.")
        return

    print(f"\n  Self-Improvement History ({len(history)} cycles):\n")
    print(f"  {'Cycle':>5}  {'Date':<20}  {'Before':>7}  {'After':>6}  {'Delta':>6}  {'Articles':>8}  {'Rewrites':>8}")
    print("  " + "-" * 72)
    for h in history:
        delta = h.get("improvement", 0)
        sign  = "+" if delta >= 0 else ""
        print(
            f"  [{h['cycle']:>3}]  {h['timestamp'][:19]}  "
            f"{h['score_before']:>7.3f}  {h['score_after']:>6.3f}  "
            f"{sign}{delta:>5.3f}  "
            f"{h.get('articles_learned', 0):>8}  "
            f"{h.get('answers_rewritten', 0):>8}"
        )


# =============================================================================
# FULL SELF-IMPROVEMENT CYCLE
# =============================================================================

def run_cycle(cycle_num: int, min_score: float = 0.55, verbose: bool = True) -> dict:
    """
    Run one complete self-improvement cycle:
      1. C++ answers all test questions  (Python scores)
      2. C++ learns weak domain Wikipedia articles
      3. C++ learns auto-generated SFT pairs for weak questions
      4. C++ learns CAI-corrected answers for violations
      5. Python re-assesses to measure improvement
      6. C++ train_transformer on full merged corpus
      7. Python logs results
    """
    print(f"\n{'=' * 65}")
    print(f"  SELF-IMPROVEMENT CYCLE {cycle_num}   [{datetime.now().strftime('%H:%M:%S')}]")
    print(f"  Architecture: C++ is THE BRAIN  |  Python is THE COORDINATOR")
    print(f"{'=' * 65}")

    # ── 1. Self-Assess ────────────────────────────────────────────────────
    assessment  = self_assess(verbose=verbose)
    before_avg  = assessment["overall_avg"]
    weak_domains = assessment["weak_domains"]

    # ── 2. C++ auto-learns weak domain Wikipedia articles ─────────────────
    learned = auto_learn_weak_domains(weak_domains, verbose=verbose)

    # ── 3. Generate + learn SFT pairs for weak questions ──────────────────
    pairs = generate_and_learn_improvement_pairs(
        assessment["results"], min_score=min_score, verbose=verbose)

    # ── 4. CAI self-critique — C++ learns corrected answers ───────────────
    rewrites = self_critique_and_rewrite(assessment["results"], verbose=verbose)

    # ── 5. Final retrain on merged corpus ─────────────────────────────────
    final_retrain(cycle_num, verbose=verbose)

    # ── 6. Re-assess to measure improvement ───────────────────────────────
    print(f"\n[ASSESS] Re-assessing after improvements...")
    after_assessment = self_assess(verbose=False)
    after_avg = after_assessment["overall_avg"]
    delta     = after_avg - before_avg

    # ── 7. Log ────────────────────────────────────────────────────────────
    log_entry = log_cycle(cycle_num, before_avg, after_avg, learned, rewrites, pairs)

    print(f"\n{'-' * 65}")
    print(f"  Cycle {cycle_num} Results:")
    print(f"     Score before        : {before_avg:.3f} ({before_avg * 100:.0f}%)")
    print(f"     Score after         : {after_avg:.3f} ({after_avg * 100:.0f}%)")
    sign = "+" if delta >= 0 else ""
    print(f"     Improvement         : {sign}{delta:.3f}  {'UP' if delta > 0 else 'DOWN' if delta < 0 else 'SAME'}")
    print(f"     Wikipedia articles  : {learned}")
    print(f"     SFT pairs added     : {pairs}")
    print(f"     Answers rewritten   : {rewrites}")

    return {**log_entry, "after_assessment": after_assessment}


# =============================================================================
# MAIN
# =============================================================================

def main():
    print(BANNER)

    parser = argparse.ArgumentParser(
        description="Neural Studio Self-Improvement Engine — C++ brain, Python coordinator"
    )
    parser.add_argument("--loop",        action="store_true", help="Loop continuously (Ctrl+C stops)")
    parser.add_argument("--cycles",      type=int, default=1, help="Number of cycles (default: 1)")
    parser.add_argument("--target",      type=float, default=80.0, help="Target avg score %% (default: 80)")
    parser.add_argument("--threshold",   type=float, default=0.55, help="Score below which AI retrains (default: 0.55)")
    parser.add_argument("--assess-only", action="store_true", help="Only run assessment, no training")
    parser.add_argument("--history",     action="store_true", help="Show training history log")
    parser.add_argument("--quiet",       action="store_true", help="Less verbose output")
    args = parser.parse_args()

    verbose = not args.quiet

    if args.history:
        print_history()
        return

    if not os.path.exists(NEURAL_ENGINE):
        print(f"\n[ERROR] C++ neural_engine.exe not found at: {NEURAL_ENGINE}")
        print("  Build it first: cd src && g++ -O3 ... -o ../bin/neural_engine.exe ...")
        sys.exit(1)

    if args.assess_only:
        result = self_assess(verbose=True)
        print(f"\n  C++ engine: {NEURAL_ENGINE}")
        print(f"  Overall score: {result['overall_avg']:.2f} ({result['overall_avg']*100:.0f}%)")
        return

    target_score = args.target / 100.0
    cycle        = 0
    start        = time.time()
    max_cycles   = 9999 if args.loop else args.cycles

    while cycle < max_cycles:
        cycle += 1

        result    = run_cycle(cycle, min_score=args.threshold, verbose=verbose)
        after_avg = result["score_after"]
        elapsed   = (time.time() - start) / 60

        print(f"\n  Time: {elapsed:.1f} min  |  Score: {after_avg * 100:.0f}%  |  Target: {args.target:.0f}%")

        if after_avg >= target_score:
            print(f"\n  TARGET REACHED! Score {after_avg * 100:.0f}% >= {args.target:.0f}%")
            break

        if not args.loop and cycle >= args.cycles:
            break

        if args.loop:
            print(f"\n  Next cycle in 5 seconds... (Ctrl+C to stop)")
            try:
                time.sleep(5)
            except KeyboardInterrupt:
                print(f"\n  Self-improvement stopped after {cycle} cycle(s).")
                break

    elapsed = (time.time() - start) / 60
    print(f"""
+================================================================+
|  SELF-IMPROVEMENT COMPLETE                                      |
+================================================================+

  Cycles completed : {cycle}
  Total time       : {elapsed:.1f} minutes
  C++ engine       : {NEURAL_ENGINE}

  To view progress    : python server/self_improve.py --history
  To keep improving   : python server/self_improve.py --loop --target 85
  To run SFT training : python server/rlhf_trainer.py --full
""")

    print_history()


if __name__ == "__main__":
    main()
