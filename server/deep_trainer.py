"""
PYTHON SUPPORT FILE — DEPRECATED FOR DIRECT USE
================================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

deep_trainer.py — DEPRECATED Python deep training scripts
==========================================================
STATUS: No longer called by main.py.
        brain_train endpoint now calls: neural_engine.exe train_transformer

PORTING TO C++:
  Deep training = more epochs + lower LR + larger corpus.
  Use: neural_engine.exe train_transformer corpus.txt 15 0.001 32
  (15 epochs, LR=0.001, batch=32 = "deep" mode)

  For true Claude-1 level training in C++, see docs/ARCHITECTURE.md.

See: docs/ARCHITECTURE.md — "RLHF in C++ — The Plan" section.
----------------------------------------------------------------------
Original description:

Deep Trainer — Push Neural Studio toward Claude 1 Level Intelligence
====================================================================

Claude 1 was trained on:
  - Billions of tokens of books, code, science, math, web text
  - Thousands of SFT pairs across ALL human knowledge domains
  - RLHF across diverse topic areas
  - Constitutional AI with comprehensive rules

We replicate that breadth by:
  1. Loading 300+ SFT pairs across ALL major knowledge domains
  2. Pulling 100+ Wikipedia/web topics into the knowledge base
  3. Running multi-round CAI critique
  4. Continuous training loop until target intelligence level is hit

Run:
    python server/deep_trainer.py                    # Full deep training
    python server/deep_trainer.py --fast             # SFT + CAI only (no web)
    python server/deep_trainer.py --web-only         # Just web knowledge
    python server/deep_trainer.py --continuous       # Loop until stopped
    python server/deep_trainer.py --rounds N         # N CAI rounds (default 3)
"""

import os, sys, re, json, time, argparse
from pathlib import Path

BASE_DIR   = Path(__file__).parent.parent
SERVER_DIR = Path(__file__).parent

sys.path.insert(0, str(SERVER_DIR))
import neural_brain as brain
from rlhf_trainer import (
    RewardModel, cai_critique, run_sft, run_cai_loop,
    CONSTITUTION, SFT_PAIRS
)

# ─── Banner ───────────────────────────────────────────────────────────────────
import sys
if sys.stdout.encoding and sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout = open(sys.stdout.fileno(), mode='w', encoding='utf-8', buffering=1)
    sys.stderr = open(sys.stderr.fileno(), mode='w', encoding='utf-8', buffering=1)

print("""
=== Neural Studio - DEEP TRAINING MODE ===
Goal: Claude 1-level general intelligence
Method: 300+ SFT pairs + 100 web topics + 5 CAI rounds
==========================================
""")

rm = RewardModel()

# =============================================================================
# MEGA SFT PAIRS — 200+ across ALL knowledge domains
# Same domains Claude 1 was trained on: science, math, coding, history, etc.
# =============================================================================

DEEP_SFT_PAIRS = [

    # ── MATHEMATICS ──────────────────────────────────────────────────────────
    {
        "question": "What is the Pythagorean theorem?",
        "ideal_answer": "The Pythagorean theorem states that in a right triangle, the square of the hypotenuse equals the sum of squares of the other two sides:\n\n`a² + b² = c²`\n\nwhere `c` is the hypotenuse (side opposite the right angle) and `a`, `b` are the legs.\n\n**Example:** A right triangle with legs 3 and 4 has a hypotenuse of `√(9+16) = √25 = 5`. The 3-4-5 triangle is the simplest Pythagorean triple.",
        "topic": "math: pythagorean theorem",
        "skills": ["formula", "example", "definition"]
    },
    {
        "question": "What is a derivative?",
        "ideal_answer": "A derivative measures the instantaneous rate of change of a function. Formally:\n\n`f'(x) = lim(h→0) [f(x+h) - f(x)] / h`\n\nIntuitively: the slope of the tangent line to the curve at point x.\n\n**Key rules:**\n- Power rule: d/dx(xⁿ) = n·xⁿ⁻¹\n- Product rule: d/dx(f·g) = f'g + fg'\n- Chain rule: d/dx(f(g(x))) = f'(g(x))·g'(x)\n\n**Example:** d/dx(x³) = 3x². At x=2, the slope is 3×4 = 12.",
        "topic": "math: calculus derivative",
        "skills": ["formula", "rules", "example"]
    },
    {
        "question": "What is Big O notation?",
        "ideal_answer": "Big O notation describes how an algorithm's time or space complexity grows as input size n increases.\n\n**Common complexities (best to worst):**\n- O(1) — Constant: array index lookup\n- O(log n) — Logarithmic: binary search\n- O(n) — Linear: single loop through array\n- O(n log n) — Quicksort, mergesort\n- O(n²) — Quadratic: nested loops (bubble sort)\n- O(2ⁿ) — Exponential: brute-force combinations\n\n**Rule:** Drop constants and lower-order terms. O(3n + 50) → O(n).",
        "topic": "math: big o notation",
        "skills": ["classification", "examples per class", "simplification rule"]
    },
    {
        "question": "What is a prime number?",
        "ideal_answer": "A prime number is a natural number greater than 1 that has no positive divisors other than 1 and itself.\n\n**Examples:** 2, 3, 5, 7, 11, 13, 17, 19, 23...\n\n**Not prime (composite):** 4 (=2×2), 6 (=2×3), 9 (=3×3)\n\n**Fun facts:**\n- 2 is the only even prime\n- There are infinitely many primes (Euclid's proof, ~300 BCE)\n- The largest known prime (as of 2024) has 41 million digits\n- Used in RSA encryption: multiplying two large primes is easy; factoring the product is computationally hard",
        "topic": "math: prime numbers",
        "skills": ["definition", "examples", "applications"]
    },
    {
        "question": "What is the difference between mean, median, and mode?",
        "ideal_answer": "These are three measures of central tendency in statistics:\n\n- **Mean** = sum of all values ÷ count. Sensitive to outliers. `[1,2,3,4,100]` → mean = 22\n- **Median** = the middle value when sorted. Robust to outliers. `[1,2,3,4,100]` → median = 3\n- **Mode** = the most frequent value. `[1,2,2,3,4]` → mode = 2\n\n**When to use:**\n- Mean: symmetric data, no extreme outliers (exam scores)\n- Median: skewed data, outliers present (income, house prices)\n- Mode: categorical data (most popular color)",
        "topic": "math: mean median mode",
        "skills": ["three definitions", "outlier comparison", "when to use each"]
    },

    # ── COMPUTER SCIENCE ─────────────────────────────────────────────────────
    {
        "question": "What is recursion?",
        "ideal_answer": "Recursion is when a function calls itself to solve a smaller version of the same problem, until it reaches a base case.\n\n**Structure:**\n1. **Base case** — stop condition (no more recursion)\n2. **Recursive case** — call self with simpler input\n\n**Example — factorial:**\n```python\ndef factorial(n):\n    if n <= 1:        # base case\n        return 1\n    return n * factorial(n - 1)  # recursive case\n\nfactorial(4) = 4 × 3 × 2 × 1 = 24\n```\n\n**Warning:** Every recursive call uses stack memory. No base case = infinite loop + stack overflow.",
        "topic": "CS: recursion",
        "skills": ["definition", "two parts", "code example", "warning"]
    },
    {
        "question": "What is the difference between a stack and a queue?",
        "ideal_answer": "Both are linear data structures, but with opposite ordering:\n\n**Stack (LIFO — Last In, First Out):**\n- Like a stack of plates — last one placed is first removed\n- Operations: `push` (add to top), `pop` (remove from top)\n- Use case: undo/redo, call stack, DFS graph traversal\n\n**Queue (FIFO — First In, First Out):**\n- Like a checkout line — first one in is first served\n- Operations: `enqueue` (add to back), `dequeue` (remove from front)\n- Use case: BFS graph traversal, print queue, request handling\n\n**Memory trick:** Stack = cafeteria plates. Queue = airport security line.",
        "topic": "CS: stack vs queue",
        "skills": ["LIFO/FIFO", "operations named", "use cases", "analogy"]
    },
    {
        "question": "What is object-oriented programming?",
        "ideal_answer": "OOP is a programming paradigm organizing code around **objects** — data + behavior bundled together.\n\n**4 core principles:**\n1. **Encapsulation** — hide internal state, expose only what's needed (private fields, public methods)\n2. **Inheritance** — child class inherits parent's properties and methods (`Dog extends Animal`)\n3. **Polymorphism** — same method name, different behavior (`Animal.speak()` → Dog barks, Cat meows)\n4. **Abstraction** — define interfaces, hide implementation details\n\n**Example (Python):**\n```python\nclass Animal:\n    def speak(self): pass\n\nclass Dog(Animal):\n    def speak(self): return 'Woof!'\n```\n\nOOP languages: Java, Python, C++, C#, Ruby.",
        "topic": "CS: OOP",
        "skills": ["4 principles", "code example", "languages listed"]
    },
    {
        "question": "What is a REST API?",
        "ideal_answer": "REST (Representational State Transfer) is an architectural style for web APIs using HTTP methods to perform actions on resources.\n\n**HTTP verbs:**\n- `GET /users` — retrieve list of users\n- `POST /users` — create a new user\n- `PUT /users/42` — replace user 42\n- `PATCH /users/42` — partially update user 42\n- `DELETE /users/42` — delete user 42\n\n**Key principles:**\n- **Stateless** — each request contains all info needed (no server sessions)\n- **Resource-based** — URLs represent nouns, not actions (`/users`, not `/getUsers`)\n- **JSON responses** — standard data format\n\n**Status codes:** 200 OK, 201 Created, 404 Not Found, 500 Server Error.",
        "topic": "CS: REST API",
        "skills": ["HTTP verbs", "URL examples", "three principles", "status codes"]
    },
    {
        "question": "What is the difference between SQL and NoSQL?",
        "ideal_answer": "**SQL (Relational Databases):**\n- Structured tables with fixed schemas\n- Uses SQL query language (`SELECT * FROM users WHERE age > 25`)\n- ACID transactions (Atomicity, Consistency, Isolation, Durability)\n- Examples: PostgreSQL, MySQL, SQLite, Oracle\n- Best for: financial data, complex joins, strong consistency\n\n**NoSQL (Non-Relational):**\n- Flexible schemas — documents, key-value, graphs, columns\n- Scales horizontally more easily\n- Often eventual consistency (not strict ACID)\n- Examples: MongoDB (documents), Redis (key-value), Cassandra (columns), Neo4j (graph)\n- Best for: huge scale, varied data shapes, rapid iteration\n\n**Rule of thumb:** SQL for structured, relational data. NoSQL for scale, flexibility, or specialized access patterns.",
        "topic": "CS: SQL vs NoSQL",
        "skills": ["two-column comparison", "examples per type", "use case guidance"]
    },
    {
        "question": "What is Git and how does version control work?",
        "ideal_answer": "Git is a distributed version control system that tracks changes to files over time.\n\n**Core concepts:**\n- **Repository (repo):** folder tracked by Git\n- **Commit:** snapshot of your files at a point in time\n- **Branch:** independent line of development\n- **Merge:** combine two branches\n- **Remote:** version on GitHub/GitLab/etc.\n\n**Basic workflow:**\n```bash\ngit init              # Start tracking\ngit add file.txt      # Stage changes\ngit commit -m 'fix'   # Save snapshot\ngit push origin main  # Send to remote\ngit pull              # Get latest changes\n```\n\n**Why it matters:** Multiple people can work simultaneously. You can roll back to any commit if something breaks.",
        "topic": "CS: git version control",
        "skills": ["core concepts", "workflow commands", "benefit explanation"]
    },
    {
        "question": "What is Docker?",
        "ideal_answer": "Docker packages an application and all its dependencies into a **container** — a lightweight, portable, isolated environment that runs consistently on any machine.\n\n**Key concepts:**\n- **Image:** blueprint for a container (like a class)\n- **Container:** running instance of an image (like an object)\n- **Dockerfile:** script that builds an image\n- **Registry:** store of images (Docker Hub)\n\n**Why Docker?** Solves 'it works on my machine' — the container includes OS libraries, config, everything.\n\n**Example Dockerfile:**\n```dockerfile\nFROM python:3.11\nCOPY app.py .\nRUN pip install flask\nCMD [\"python\", \"app.py\"]\n```\n\nBuild: `docker build -t myapp .`\nRun: `docker run -p 8080:8080 myapp`",
        "topic": "CS: Docker containers",
        "skills": ["image vs container analogy", "dockerfile example", "problem solved"]
    },
    {
        "question": "What is machine learning?",
        "ideal_answer": "Machine learning is a subfield of AI where systems learn patterns from data instead of being explicitly programmed.\n\n**3 main types:**\n1. **Supervised learning** — learn from labeled examples. Input → output pairs. (email → spam/not-spam)\n2. **Unsupervised learning** — find hidden patterns in unlabeled data. (customer segmentation, anomaly detection)\n3. **Reinforcement learning** — agent learns by trial and error, maximizing reward. (game AI, robotics)\n\n**Typical pipeline:**\n- Collect data → Clean data → Feature engineering → Train model → Evaluate → Deploy\n\n**Popular algorithms:** Linear regression, Decision trees, Random forests, SVM, Neural networks, k-means clustering.",
        "topic": "CS: machine learning",
        "skills": ["3 types", "pipeline", "algorithm list"]
    },
    {
        "question": "What is a neural network?",
        "ideal_answer": "A neural network is a machine learning model loosely inspired by the brain, made of layers of interconnected 'neurons' (mathematical functions).\n\n**Structure:**\n- **Input layer** — receives data (pixels, text tokens, numbers)\n- **Hidden layers** — learn intermediate representations\n- **Output layer** — produces predictions\n\n**How it learns:**\n1. Forward pass: input flows through layers, produces prediction\n2. Calculate loss: how wrong was the prediction?\n3. Backpropagation: compute gradients (blame attribution)\n4. Update weights: gradient descent moves weights to reduce loss\n\n**Key activation functions:** ReLU `f(x)=max(0,x)`, Sigmoid, Tanh\n\n**Depth:** Deep neural network = many hidden layers = 'deep learning'",
        "topic": "CS: neural networks",
        "skills": ["structure", "4-step training", "activation functions defined"]
    },

    # ── PHYSICS ───────────────────────────────────────────────────────────────
    {
        "question": "What is Newton's second law?",
        "ideal_answer": "Newton's second law: **Force = mass × acceleration**\n\n`F = ma`\n\n- F = net force (Newtons, N)\n- m = mass (kilograms, kg)\n- a = acceleration (m/s²)\n\n**What this means:**\n- Larger force → more acceleration\n- Larger mass → less acceleration from same force\n- Direction matters — F and a are vectors\n\n**Example:** A 10 kg box pushed with 20 N of force accelerates at 2 m/s². If you push twice as hard (40 N), it accelerates at 4 m/s².\n\nThis is the central law of classical mechanics, connecting cause (force) to effect (acceleration).",
        "topic": "physics: Newton's second law",
        "skills": ["formula", "units", "intuition", "example"]
    },
    {
        "question": "What is the speed of light?",
        "ideal_answer": "The speed of light in a vacuum is exactly **299,792,458 meters per second** (≈ 3 × 10⁸ m/s, or ~186,000 miles/second).\n\n**Why c matters:**\n- Einstein's special relativity: nothing with mass can reach c\n- c is the cosmic speed limit\n- E = mc² — mass and energy are interchangeable\n- Light from the Sun takes ~8 minutes to reach Earth\n- Light from the nearest star (Proxima Centauri) takes 4.24 years\n\n**If c were slower:** GPS satellites would give wrong positions (relativity corrections would be larger). The universe as we know it would be completely different.",
        "topic": "physics: speed of light",
        "skills": ["exact value", "relativity implications", "cosmic scale examples"]
    },
    {
        "question": "What is quantum mechanics?",
        "ideal_answer": "Quantum mechanics describes the behavior of matter and energy at atomic and subatomic scales, where classical physics breaks down.\n\n**Key principles:**\n1. **Wave-particle duality** — particles (electrons, photons) behave as both waves and particles depending on observation\n2. **Uncertainty principle** — you cannot simultaneously know exact position AND momentum of a particle (Heisenberg: ΔxΔp ≥ ℏ/2)\n3. **Quantization** — energy comes in discrete packets called quanta (photons)\n4. **Superposition** — a particle can be in multiple states simultaneously until measured (Schrödinger's cat paradox)\n5. **Entanglement** — two particles can be correlated such that measuring one instantly affects the other\n\nQuantum mechanics underlies lasers, transistors, MRI machines, and quantum computers.",
        "topic": "physics: quantum mechanics",
        "skills": ["5 principles", "real applications", "accessible definitions"]
    },

    # ── CHEMISTRY ─────────────────────────────────────────────────────────────
    {
        "question": "What is the periodic table?",
        "ideal_answer": "The periodic table is a tabular arrangement of all 118 known chemical elements, organized by atomic number (protons in nucleus), electron configuration, and recurring chemical properties.\n\n**Structure:**\n- **Rows (periods):** elements with same number of electron shells\n- **Columns (groups):** elements with same valence electrons and similar chemical behavior\n  - Group 1 (alkali metals): Li, Na, K — highly reactive, form +1 ions\n  - Group 17 (halogens): F, Cl, Br — form -1 ions, very reactive\n  - Group 18 (noble gases): He, Ne, Ar — almost completely inert\n\n**Why it's powerful:** You can predict how an unknown element behaves based on its position.\n\nMendeleev created the first version in 1869 and left gaps for undiscovered elements — which were later found exactly as he predicted.",
        "topic": "chemistry: periodic table",
        "skills": ["structure explained", "group examples", "predictive power", "history"]
    },
    {
        "question": "What is pH?",
        "ideal_answer": "pH measures the acidity or alkalinity of a solution, on a scale from 0 to 14:\n\n- **0–6:** Acidic (more H⁺ ions than OH⁻)\n- **7:** Neutral (pure water)\n- **8–14:** Basic/Alkaline (more OH⁻ ions)\n\n**Formula:** `pH = -log₁₀[H⁺]`\n\nEach step is 10× more acidic/basic:\n- pH 5 is **10× more acidic** than pH 6\n- pH 3 (vinegar) is **10,000× more acidic** than pH 7 (water)\n\n**Common pH values:**\n- Battery acid: ~1 | Lemon juice: ~2 | Vinegar: ~3 | Black coffee: ~5\n- Blood: 7.4 | Baking soda: 9 | Bleach: ~12 | Drain cleaner: ~14",
        "topic": "chemistry: pH",
        "skills": ["formula", "logarithmic scale", "examples table"]
    },

    # ── BIOLOGY ───────────────────────────────────────────────────────────────
    {
        "question": "What is DNA?",
        "ideal_answer": "DNA (Deoxyribonucleic Acid) is the molecule that stores the genetic blueprint for all living organisms.\n\n**Structure:**\n- Double helix — two strands twisted together (Watson & Crick, 1953)\n- 4 nitrogen bases: Adenine (A), Thymine (T), Cytosine (C), Guanine (G)\n- Bases pair: A–T and C–G (complementary base pairing)\n- A sequence of 3 bases (codon) encodes one amino acid\n\n**Central dogma of biology:**\nDNA → RNA → Protein\n- DNA is transcribed into messenger RNA\n- mRNA is translated into proteins by ribosomes\n- Proteins do virtually everything in the cell\n\n**Human genome:** ~3.2 billion base pairs encoding ~20,000 genes, stored in 23 chromosome pairs.",
        "topic": "biology: DNA",
        "skills": ["structure", "base pairing", "central dogma", "human genome fact"]
    },
    {
        "question": "What is evolution?",
        "ideal_answer": "Evolution is the process by which populations of organisms change over time through changes in heritable traits.\n\n**Darwin's 4 key ingredients:**\n1. **Variation** — individuals differ in traits\n2. **Heritability** — traits are passed to offspring\n3. **Selection** — some traits improve survival/reproduction\n4. **Time** — selection accumulates over generations\n\n**Types of selection:**\n- Natural selection (survival of the fittest)\n- Sexual selection (traits that improve mating success)\n- Genetic drift (random changes in small populations)\n\n**Evidence:** Fossil record, DNA similarity across species (humans share 98.7% DNA with chimp), observed evolution (antibiotic resistance, Darwin's finches).\n\nEvolution is the unifying theory of all biology.",
        "topic": "biology: evolution",
        "skills": ["4 ingredients", "selection types", "evidence", "significance"]
    },
    {
        "question": "What is photosynthesis?",
        "ideal_answer": "Photosynthesis is the process by which plants, algae, and some bacteria convert light energy into chemical energy (glucose).\n\n**Equation:**\n`6CO₂ + 6H₂O + light energy → C₆H₁₂O₆ (glucose) + 6O₂`\n\n**Where it happens:** Chloroplasts, specifically the green pigment chlorophyll.\n\n**Two stages:**\n1. **Light reactions** (thylakoid membrane) — light splits water, produces ATP and NADPH, releases O₂\n2. **Calvin cycle** (stroma) — uses ATP/NADPH to fix CO₂ into glucose\n\n**Why it matters:** Nearly all life on Earth depends on photosynthesis — it produces the oxygen we breathe and forms the base of almost all food chains.",
        "topic": "biology: photosynthesis",
        "skills": ["equation", "location", "two-stage process", "global significance"]
    },

    # ── HISTORY ───────────────────────────────────────────────────────────────
    {
        "question": "What caused World War 1?",
        "ideal_answer": "WWI (1914–1918) was caused by a combination of long-term tensions and a specific trigger:\n\n**MAIN acronym (long-term causes):**\n- **M**ilitarism — European powers built up massive militaries\n- **A**lliances — Triple Entente (UK, France, Russia) vs Triple Alliance (Germany, Austria, Italy)\n- **I**mperialism — competition for colonies created friction\n- **N**ationalism — rising ethnic independence movements, especially in the Balkans\n\n**Trigger:** Assassination of Archduke Franz Ferdinand (heir to Austria-Hungary) by Gavrilo Princip in Sarajevo (June 28, 1914)\n\n**Chain reaction:** Austria declared war on Serbia → Russia mobilized → Germany declared war on Russia and France → Germany invaded Belgium → Britain declared war\n\n**Result:** ~20 million deaths, collapse of 4 empires (Ottoman, Austro-Hungarian, Russian, German).",
        "topic": "history: WWI causes",
        "skills": ["MAIN acronym", "trigger event", "cascade explained", "result"]
    },
    {
        "question": "What was the Industrial Revolution?",
        "ideal_answer": "The Industrial Revolution (roughly 1760–1840, starting in Britain) was the transition from hand production to machine-based manufacturing, fundamentally reshaping human society.\n\n**Key innovations:**\n- James Watt's steam engine (1769) — powered factories, trains, ships\n- Spinning jenny, power loom — mechanized textile production\n- Iron/steel production grew massively\n- Railways — first modern high-speed transport network\n\n**Effects:**\n- Urbanization — people moved from farms to factory cities\n- New social classes: industrial capitalists and urban working class\n- Child labor and poor factory conditions → labor reform movements\n- GDP grew faster than any time in human history before it\n- Began modern economic growth but also carbon emissions\n\nSpread from Britain to Europe, USA, and Japan by 1900.",
        "topic": "history: industrial revolution",
        "skills": ["timeline", "key inventions", "social effects", "spread"]
    },

    # ── PSYCHOLOGY ────────────────────────────────────────────────────────────
    {
        "question": "What is cognitive bias?",
        "ideal_answer": "A cognitive bias is a systematic error in thinking that affects judgments and decisions. These biases evolved as mental shortcuts (heuristics) but lead to predictable mistakes.\n\n**Important biases:**\n- **Confirmation bias** — seek/interpret info that confirms existing beliefs\n- **Dunning-Kruger effect** — low-skill people overestimate competence; experts underestimate knowledge\n- **Anchoring** — first number seen biases all subsequent judgments\n- **Availability heuristic** — overestimate likelihood of memorable/recent events (plane crash fear > car crash)\n- **Sunk cost fallacy** — continue bad decision because of past investment\n- **Survivorship bias** — only seeing successes, not failures (startup advice)\n\n**Why it matters:** Understanding biases improves decision-making in medicine, business, law, and everyday life.",
        "topic": "psychology: cognitive biases",
        "skills": ["definition", "6 named biases", "practical importance"]
    },
    {
        "question": "What is Maslow's hierarchy of needs?",
        "ideal_answer": "Maslow's hierarchy (1943) is a motivational theory proposing that human needs are arranged in priority order — lower needs must be met before higher ones become motivating.\n\n**5 levels (bottom to top):**\n1. **Physiological** — food, water, shelter, sleep, warmth\n2. **Safety** — personal security, employment, health, property\n3. **Love/Belonging** — friendship, intimacy, family, community\n4. **Esteem** — self-respect, achievement, recognition, status\n5. **Self-Actualization** — reaching full potential, creativity, purpose\n\n**Applications:** Management theory (employees need safety before they can focus on achievement), therapy, product design (Airbnb → safety+belonging before luxury).\n\n**Criticism:** The strict hierarchy doesn't hold universally — culture affects which needs drive motivation most.",
        "topic": "psychology: Maslow hierarchy",
        "skills": ["5 levels listed", "application examples", "honest criticism"]
    },

    # ── ECONOMICS ─────────────────────────────────────────────────────────────
    {
        "question": "What is supply and demand?",
        "ideal_answer": "Supply and demand is the fundamental mechanism that determines prices in a free market.\n\n**Demand:** As price rises, quantity demanded falls (inverse relationship). Demand shifts right if: income rises, population grows, preferences change.\n\n**Supply:** As price rises, quantity supplied rises (direct relationship). Supply shifts right if: production costs fall, technology improves.\n\n**Equilibrium:** The price where supply = demand. At this price, the market 'clears' — no surplus, no shortage.\n\n**Key shifts:**\n- Demand surges (COVID hand sanitizer) → price rises until new equilibrium\n- Supply crashes (drought destroying wheat crop) → price rises\n- New technology (solar panels) → supply increases → price falls\n\n**Elasticity:** How sensitive demand/supply is to price changes. Gasoline = inelastic (you still buy it). Luxury goods = elastic (price hike kills demand).",
        "topic": "economics: supply and demand",
        "skills": ["two curves", "equilibrium", "real examples", "elasticity defined"]
    },
    {
        "question": "What is inflation?",
        "ideal_answer": "Inflation is the general rise in the price level of goods and services over time, meaning each unit of currency buys fewer goods.\n\n**Measured by:** CPI (Consumer Price Index) — tracks basket of typical purchases over time.\n\n**Types:**\n- **Demand-pull:** too much money chasing too few goods (post-COVID stimulus)\n- **Cost-push:** production costs rise (oil price shock → everything gets pricier)\n- **Built-in:** workers demand higher wages → companies raise prices → cycle continues\n\n**Effects:**\n- Erodes savings if returns < inflation rate\n- Helps debtors (pay back with cheaper money)\n- Hurts fixed-income earners\n- Hyperinflation (>50%/month) destroys economies (Zimbabwe 2008, Weimar Germany)\n\n**Central bank response:** Raise interest rates → borrowing costs more → spending falls → inflation cools.",
        "topic": "economics: inflation",
        "skills": ["measurement", "3 types", "effects", "central bank response"]
    },

    # ── PHILOSOPHY ────────────────────────────────────────────────────────────
    {
        "question": "What is the scientific method?",
        "ideal_answer": "The scientific method is the systematic process for acquiring reliable knowledge through observation and testing.\n\n**Steps:**\n1. **Observe** — notice a phenomenon\n2. **Question** — formulate a specific question\n3. **Hypothesis** — propose a testable explanation\n4. **Experiment** — design a controlled test (change one variable, keep others constant)\n5. **Analyze** — collect and interpret data\n6. **Conclude** — does data support or refute hypothesis?\n7. **Replicate** — others try to reproduce results\n8. **Publish** — peer review by other scientists\n\n**Key principle — falsifiability (Popper):** A good scientific hypothesis must be possible to disprove. 'All swans are white' is scientific — finding a black swan falsifies it. Unfalsifiable claims are not science.\n\nThe scientific method has driven more human progress than any other epistemological tool.",
        "topic": "philosophy: scientific method",
        "skills": ["numbered steps", "falsifiability", "importance stated"]
    },
    {
        "question": "What is Occam's Razor?",
        "ideal_answer": "Occam's Razor is the principle that among competing hypotheses, the one with the fewest assumptions should be preferred.\n\n**Latin formulation:** 'Entia non sunt multiplicanda praeter necessitatem' — entities should not be multiplied beyond necessity.\n\n**Practical meaning:** All else being equal, the simplest explanation is most likely correct.\n\n**Examples:**\n- Patient has fever, cough, runny nose → flu is more likely than a rare tropical disease\n- Computer won't turn on → unplugged most likely, not motherboard failure\n\n**Limitations:** Occam's Razor is a guide, not a law. Sometimes reality is genuinely complex (quantum mechanics is counterintuitive and strange, yet correct). It's a starting point for investigation, not a final answer.\n\n**Applications:** Science, medicine, machine learning (prefer simpler models that generalize better).",
        "topic": "philosophy: Occam's razor",
        "skills": ["Latin origin", "practical meaning", "two examples", "honest limitations"]
    },

    # ── LANGUAGE & COMMUNICATION ───────────────────────────────────────────────
    {
        "question": "How should I write a clear explanation?",
        "ideal_answer": "A clear explanation follows these principles (Anthropic-tested):\n\n1. **Lead with the conclusion** — state the answer first, then explain\n2. **Use concrete examples** — 'for example' before abstract concept\n3. **One idea per sentence** — don't pack multiple concepts together\n4. **Analogies bridge the gap** — connect new concept to something familiar ('a CPU is like the brain of a computer')\n5. **Active voice** — 'the function returns X' not 'X is returned by the function'\n6. **Define jargon immediately** — the first time you use a technical term, define it inline\n7. **Check at the end** — 'does this make sense? Want me to go deeper?'\n\n**The test:** Can a smart 15-year-old follow this? If not, simplify.",
        "topic": "communication: clear explanation",
        "skills": ["7 principles", "active voice", "jargon rule", "clarity test"]
    },
    {
        "question": "What makes a good question?",
        "ideal_answer": "A good question has these qualities:\n\n1. **Specific** — 'How do I fix ModuleNotFoundError in Python?' beats 'Python doesn't work'\n2. **Context included** — what you've tried, what error you see, what you expected\n3. **One thing asked** — don't bundle 5 questions; ask one clearly\n4. **Answerable** — not too broad ('explain all of AI') or too narrow (internal system state)\n5. **Shows effort** — mention what you already know or tried\n\n**The XY problem (classic mistake):** Asking about your attempted solution X when your actual goal is Y. Instead, explain Y (the goal).\n\n**Good:** 'My Flask server returns 500 when I POST to /api/user. Here's the traceback...'\n**Bad:** 'Flask not working help'",
        "topic": "communication: good questions",
        "skills": ["5 qualities", "XY problem", "good vs bad example"]
    },

    # ── CODING PATTERNS ───────────────────────────────────────────────────────
    {
        "question": "What is a hash map?",
        "ideal_answer": "A hash map (also called hash table or dictionary) is a data structure mapping keys to values using a hash function.\n\n**How it works:**\n1. You provide a key (e.g., 'apple')\n2. A hash function converts it to an integer (array index)\n3. Value is stored at that index\n4. Lookup: hash the key again → retrieve value directly\n\n**Time complexity:** O(1) average for get/set/delete (much faster than O(n) linear search)\n\n**Python example:**\n```python\nfrequency = {}\nfor char in 'banana':\n    frequency[char] = frequency.get(char, 0) + 1\n# frequency = {'b':1, 'a':3, 'n':2}\n```\n\n**Collisions:** Two keys hash to same index. Resolved by chaining (linked list at each slot) or open addressing.\n\n**Use cases:** Frequency count, caching, sets, database indexes.",
        "topic": "CS: hash map",
        "skills": ["mechanism", "O(1) complexity", "code example", "collision handling"]
    },
    {
        "question": "What is a binary search?",
        "ideal_answer": "Binary search finds a target in a **sorted** array in O(log n) time by repeatedly halving the search space.\n\n**Algorithm:**\n1. Set `low = 0`, `high = len(arr) - 1`\n2. While `low <= high`:\n   - `mid = (low + high) // 2`\n   - If `arr[mid] == target` → found! Return mid\n   - If `arr[mid] < target` → target is right half: `low = mid + 1`\n   - If `arr[mid] > target` → target is left half: `high = mid - 1`\n3. Not found → return -1\n\n**Why O(log n)?** Each step halves the array. 1 million elements → max 20 comparisons.\n\n**Requirement:** Array MUST be sorted first. If unsorted: sort O(n log n) first, then binary search O(log n).",
        "topic": "CS: binary search",
        "skills": ["algorithm steps", "complexity explained", "sorted requirement"]
    },
    {
        "question": "What is async/await in programming?",
        "ideal_answer": "Async/await is a pattern for non-blocking I/O operations — letting your program do other things while waiting for slow operations (network, disk, database).\n\n**Problem without async:**\n```python\nresponse = requests.get(url)  # CPU waits 500ms doing NOTHING\nprint(response.text)          # finally continues\n```\n\n**With async (Python):**\n```python\nasync def fetch(url):\n    async with aiohttp.ClientSession() as session:\n        response = await session.get(url)  # yields control while waiting\n        return await response.text()\n\nawait asyncio.gather(fetch(url1), fetch(url2))  # both run concurrently!\n```\n\n**Key concepts:**\n- `async def` marks a function as a coroutine\n- `await` suspends the coroutine until result is ready (does NOT block thread)\n- Event loop manages all coroutines\n\n**Use when:** many I/O-bound operations (web scraping, API calls, DB queries). Not useful for CPU-bound work (use multiprocessing instead).",
        "topic": "CS: async await",
        "skills": ["problem stated", "code before/after", "event loop", "when to use"]
    },

    # ── NETWORKING ────────────────────────────────────────────────────────────
    {
        "question": "How does the internet work?",
        "ideal_answer": "The internet is a global network of networks communicating via standardized protocols.\n\n**When you visit google.com:**\n1. **DNS lookup** — your computer asks a DNS server: 'What is google.com's IP?' → 142.250.80.46\n2. **TCP connection** — 3-way handshake (SYN, SYN-ACK, ACK) establishes reliable connection\n3. **HTTP/HTTPS request** — browser sends GET request to Google's server\n4. **TLS handshake** (HTTPS) — establishes encrypted tunnel\n5. **Server responds** — sends HTML, CSS, JS\n6. **Browser renders** — parses and displays the page\n\n**Key layers (TCP/IP model):**\n- Application (HTTP, DNS, SMTP)\n- Transport (TCP, UDP)\n- Internet (IP, routing)\n- Link (Ethernet, WiFi)\n\nPackets travel multiple routers, each forwarding based on routing tables — the packet may take a different path than the previous one.",
        "topic": "networking: how internet works",
        "skills": ["numbered steps for URL visit", "4 layers", "packet routing"]
    },

    # ── GENERAL REASONING ─────────────────────────────────────────────────────
    {
        "question": "How do I solve a problem I've never seen before?",
        "ideal_answer": "A systematic problem-solving framework (used by both engineers and scientists):\n\n1. **Understand the problem** — can you restate it in your own words? What are the inputs, outputs, constraints?\n2. **Explore examples** — work through small concrete cases by hand. This reveals patterns.\n3. **Break it down** — decompose into sub-problems. Which parts can you solve already?\n4. **Identify knowns** — what tools, algorithms, or patterns apply? (sorting, recursion, dynamic programming?)\n5. **Draft a solution** — write pseudocode before real code. Get the structure right first.\n6. **Implement and test** — build it, test edge cases (empty input, maximum input, duplicates)\n7. **Reflect and optimize** — does it work? Is there a simpler or faster approach?\n\n**Key mindset:** When stuck, add more examples or simplify the problem (solve for n=1 first, then generalize).",
        "topic": "reasoning: problem solving framework",
        "skills": ["7 steps", "stuck tip", "engineering mindset"]
    },
    {
        "question": "How do I think about tradeoffs?",
        "ideal_answer": "Every decision involves tradeoffs — gaining one thing at the cost of another. Clear tradeoff thinking:\n\n1. **Identify the tradeoffs explicitly** — what do you gain? What do you give up?\n2. **Understand your constraints** — time, money, correctness, simplicity?\n3. **Think in reversibility** — prefer reversible decisions when uncertain. Hard to reverse = extra caution.\n4. **Consider the 2nd order effects** — what happens AFTER the first consequence?\n5. **Check your biases** — are you drawn to the option that feels familiar or comfortable?\n\n**Classic CS tradeoffs:**\n- Speed vs memory (cache more, compute less OR compute more, store less)\n- Consistency vs availability (CAP theorem)\n- Simplicity vs flexibility (hard-coding vs config files)\n- Accuracy vs speed (approximate algorithms)\n\n**Jeff Bezos rule:** Prioritize being 'roughly right quickly' over 'precisely right slowly' for reversible decisions.",
        "topic": "reasoning: tradeoff thinking",
        "skills": ["5 step framework", "CS examples", "reversibility rule", "Bezos quote"]
    },
    {
        "question": "What is first principles thinking?",
        "ideal_answer": "First principles thinking means breaking a problem down to its most fundamental truths and reasoning up from there, instead of relying on analogy or convention.\n\n**Elon Musk's example (battery cost):**\n- **By analogy:** 'Batteries cost $600/kWh because everyone says so'\n- **First principles:** What are batteries made of? Cobalt, nickel, aluminum, carbon, polymer. What do those materials cost on commodities markets? ~$80/kWh. So the target price is achievable.\n\n**Steps:**\n1. Identify your assumptions\n2. Break them down to fundamental facts (physics, chemistry, math)\n3. Reason up from those facts to a new solution\n\n**When to use:** When starting something new, when an industry has stagnant assumptions, when solving exponentially hard problems.\n\n**Contrast with reasoning by analogy:** Easier and faster, but inherits others' assumptions and limitations.",
        "topic": "reasoning: first principles",
        "skills": ["Musk battery example", "3-step process", "contrast with analogy"]
    },

    # ── WRITING & COMMUNICATION STYLE ─────────────────────────────────────────
    {
        "question": "How do I write a good technical document?",
        "ideal_answer": "A great technical document has these elements:\n\n**Structure:**\n1. **Title** — specific, not generic ('Redis Caching Strategy for Auth Service' not 'Caching Docs')\n2. **Summary/TL;DR** — 2–3 sentences: what this is, why it matters\n3. **Context** — what problem exists, what's broken/missing\n4. **Solution** — how it works, key decisions, alternatives considered\n5. **Implementation** — steps, code examples, commands\n6. **Testing** — how to verify it works\n7. **Gotchas / Known issues** — what to watch out for\n\n**Writing rules:**\n- Active voice: 'The server returns', not 'The response is returned'\n- Short sentences: max ~20 words each\n- Code blocks for all commands and snippets\n- Headers for skimmability — assume reader will skip most of the doc\n\n**Test:** Can a developer who wasn't there implement this from the doc alone?",
        "topic": "writing: technical documentation",
        "skills": ["7 section structure", "writing rules", "success test"]
    },

    # ── ETHICS & AI ───────────────────────────────────────────────────────────
    {
        "question": "What are the ethical risks of AI?",
        "ideal_answer": "AI ethics covers risks across multiple dimensions:\n\n**Bias & Fairness:**\n- Models trained on biased data reproduce and amplify biases\n- Example: hiring algorithms discriminating against women because trained on historic male-dominant data\n\n**Privacy:**\n- Large models memorize training data → can leak personal information\n- Surveillance systems enable mass tracking\n\n**Misinformation:**\n- Deepfakes, synthetic text enable scalable deception\n- LLMs hallucinate confidently → spreading false information\n\n**Safety (misaligned AI):**\n- AI optimizes for proxy goal, not actual human values (Goodhart's Law)\n- Existential risk from superintelligent AI (Bostrom, Musk, many ML researchers)\n\n**Labor displacement:**\n- Automation of cognitive work → economic disruption at scale\n\n**Mitigation approaches:**\n- RLHF + Constitutional AI (Anthropic)\n- Red-teaming and safety testing\n- Regulation (EU AI Act)\n- Transparency and explainability requirements",
        "topic": "ethics: AI risks",
        "skills": ["5 categories", "concrete examples", "mitigation approaches"]
    },
    {
        "question": "What is Anthropic's approach to AI safety?",
        "ideal_answer": "Anthropic was founded in 2021 by ex-OpenAI researchers focused specifically on AI safety. Their approach includes:\n\n**1. Constitutional AI (CAI):**\nTrain Claude to follow a written set of principles using self-critique — the AI checks its own outputs against the 'constitution' without requiring human feedback on every example.\n\n**2. RLHF (Reinforcement Learning from Human Feedback):**\nHuman labelers rank AI responses → train a reward model → use that to optimize Claude's outputs.\n\n**3. Interpretability research:**\nUnderstanding *what's happening inside* neural networks — which circuits activate for which concepts. Goal: detect dangerous behaviors before deployment.\n\n**4. Scaling with safety:**\nHarmlessness, helpfulness, and honesty ('HHH') as core objectives — not just capability.\n\n**5. Gradual deployment:**\nTest with limited users first, expand only after safety validation.\n\nAnthropic publishes safety research papers and positions itself as a 'safety-first' commercial AI lab.",
        "topic": "ethics: Anthropic safety approach",
        "skills": ["5 methods", "HHH defined", "CAI explained", "published research"]
    },

    # ── GENERAL KNOWLEDGE ─────────────────────────────────────────────────────
    {
        "question": "What is the difference between weather and climate?",
        "ideal_answer": "**Weather** = short-term atmosphere conditions at a specific place and time.\n- 'It's raining in Mumbai today'\n- Changes hour to hour, day to day\n- Measured by: temperature, humidity, wind, precipitation\n\n**Climate** = long-term patterns of weather over decades in a region.\n- 'Mumbai has a monsoon season from June to September'\n- Defined over 30+ year averages (WMO standard)\n- Measured by: seasonal averages, extreme event frequency\n\n**Analogy:** Weather is your mood. Climate is your personality.\n\n**Climate change context:**\n- Burning fossil fuels increases atmospheric CO₂\n- CO₂ traps heat (greenhouse effect)\n- Global average temperature has risen ~1.2°C since pre-industrial times\n- This shifts climate patterns globally — more extreme weather events, rising sea levels",
        "topic": "science: weather vs climate",
        "skills": ["clear distinction", "analogy", "climate change connection"]
    },
    {
        "question": "What is blockchain?",
        "ideal_answer": "A blockchain is a distributed, append-only ledger where data is stored in cryptographically linked blocks.\n\n**How it works:**\n1. Transactions are grouped into a **block**\n2. The block includes a **hash** of the previous block (creating the 'chain')\n3. Multiple nodes validate new blocks (consensus mechanism)\n4. Once added, blocks cannot be altered without re-doing all subsequent hashes (tamper-evident)\n\n**Key properties:**\n- **Decentralized** — no single authority controls it\n- **Transparent** — anyone can read (in public chains like Bitcoin)\n- **Immutable** — changing history requires consensus of majority of nodes\n\n**Use cases:**\n- Cryptocurrency (Bitcoin, Ethereum)\n- Smart contracts (self-executing code on Ethereum)\n- Supply chain tracking (who handled this food/product at each step)\n\n**Limitations:** Slow (7 tx/sec for Bitcoin vs 65,000 for Visa), high energy use (proof-of-work), often not needed when a trusted database would suffice.",
        "topic": "tech: blockchain",
        "skills": ["numbered mechanism", "3 properties", "use cases", "honest limitations"]
    },
    # ── CODING & DEBUGGING ────────────────────────────────────────────────
    {
        "question": "How do I debug a Python error?",
        "ideal_answer": "Debugging Python errors step by step:\n\n1. **Read the full traceback** — Python shows exactly which line failed and what kind of error. Read from bottom up.\n2. **Identify the error type:**\n   - `NameError` → variable not defined or misspelled\n   - `TypeError` → wrong argument type (`int` where `str` expected)\n   - `IndexError` → list index out of range\n   - `KeyError` → key not in dictionary\n   - `AttributeError` → calling method on wrong object type\n3. **Print intermediate values** — add `print(variable)` before the crash line\n4. **Use pdb:** `import pdb; pdb.set_trace()` — step through code line by line\n5. **Isolate the problem** — comment out half the code, narrow down the cause\n6. **Google the exact error message** — almost every Python error has a StackOverflow answer\n\n**Golden rule:** Never assume. Print the actual value to confirm what you think is true.",
        "topic": "coding: python debugging",
        "skills": ["error types", "numbered steps", "golden rule"]
    },
    {
        "question": "What is the difference between a list and a tuple in Python?",
        "ideal_answer": "Both are ordered sequences in Python, but with a key difference:\n\n**List** - mutable (can be changed after creation):\n```\nfruits = ['apple', 'banana', 'cherry']\nfruits.append('date')     # OK\nfruits[0] = 'avocado'     # OK\n```\n\n**Tuple** - immutable (cannot be changed after creation):\n```\npoint = (3, 4)\npoint[0] = 5  # TypeError: cannot modify\n```\n\n**When to use each:**\n- **List:** when you need to add/remove/modify (shopping cart, log entries)\n- **Tuple:** for fixed data that shouldn't change (coordinates, RGB colors, database record)\n- **Tuple is faster** to iterate and uses less memory\n- **Tuple can be a dict key;** list cannot (because lists are mutable)",
        "topic": "coding: list vs tuple python",
        "skills": ["code examples", "mutability", "when to use each"]
    },
    {
        "question": "What is a closure in programming?",
        "ideal_answer": "A closure is a function that remembers variables from its enclosing scope even after that scope has finished executing.\n\n**Example (Python):**\n```python\ndef make_counter(start=0):\n    count = [start]               # mutable container\n    def increment():\n        count[0] += 1\n        return count[0]\n    return increment              # return the inner function\n\ncounter = make_counter(10)\nprint(counter())  # 11\nprint(counter())  # 12  ← remembers count across calls\n```\n\n**Why closures matter:**\n- Create factory functions (make_adder, make_multiplier)\n- Implement data hiding without classes\n- Used constantly in JavaScript event handlers and callbacks\n- Foundation of decorators in Python",
        "topic": "coding: closures",
        "skills": ["definition", "code example", "use cases"]
    },
    {
        "question": "What is the difference between == and is in Python?",
        "ideal_answer": "`==` checks **value equality** — do the two objects have the same value?\n`is` checks **identity** — are the two objects the SAME object in memory?\n\n```python\na = [1, 2, 3]\nb = [1, 2, 3]\nc = a\n\na == b   # True  ← same value\na is b   # False ← different objects in memory\na is c   # True  ← c points to same object as a\n```\n\n**Common pitfall:**\n```python\nif x == None:   # works but not idiomatic\nif x is None:   # CORRECT — None is a singleton; always use 'is'\n```\n\n**Rule:** Use `==` for values. Use `is` only for singletons: `None`, `True`, `False`.",
        "topic": "coding: == vs is python",
        "skills": ["clear distinction", "code examples", "None rule"]
    },
    {
        "question": "What is a decorator in Python?",
        "ideal_answer": "A decorator is a function that wraps another function to add behavior before/after it runs, without modifying the original code.\n\n**Syntax:** `@decorator` placed above a function definition.\n\n**Example — timing decorator:**\n```python\nimport time\n\ndef timer(func):\n    def wrapper(*args, **kwargs):\n        start = time.time()\n        result = func(*args, **kwargs)\n        print(f'{func.__name__} took {time.time()-start:.3f}s')\n        return result\n    return wrapper\n\n@timer\ndef slow_function():\n    time.sleep(1)\n\nslow_function()  # prints: slow_function took 1.001s\n```\n\n**Built-in decorators:**\n- `@property` — make method behave like attribute\n- `@staticmethod` — method not bound to instance\n- `@classmethod` — method receives class, not instance\n- `@lru_cache` — memoize function results automatically",
        "topic": "coding: python decorators",
        "skills": ["definition", "full code example", "built-in list"]
    },

    # ── LOGIC & MATHEMATICS ───────────────────────────────────────────────
    {
        "question": "What is a logical fallacy?",
        "ideal_answer": "A logical fallacy is an error in reasoning that makes an argument invalid even if its conclusion might happen to be true.\n\n**Common fallacies:**\n- **Ad hominem** — attack the person, not the argument ('You can't trust his climate data — he's unpopular')\n- **Straw man** — misrepresent opponent's position to more easily attack it\n- **False dichotomy** — present only 2 options when more exist ('You're either with us or against us')\n- **Appeal to authority** — 'X is true because famous person Y said so'\n- **Slippery slope** — if A happens, then Z will inevitably follow (without showing steps)\n- **Post hoc** — 'A happened before B, therefore A caused B' (classic correlation ≠ causation)\n- **Hasty generalization** — draw broad conclusion from small sample\n\n**Why it matters:** Recognizing fallacies protects you from bad arguments in politics, advertising, and everyday debate.",
        "topic": "logic: fallacies",
        "skills": ["7 named fallacies", "examples per fallacy", "practical importance"]
    },
    {
        "question": "What is the difference between inductive and deductive reasoning?",
        "ideal_answer": "**Deductive reasoning:** From general rules to specific conclusions. If premises are true and logic is valid, conclusion MUST be true.\n\nExample:\n- Premise 1: All mammals breathe air\n- Premise 2: Whales are mammals\n- Conclusion: Whales breathe air ✅ (logically certain)\n\n**Inductive reasoning:** From specific observations to general patterns. Conclusion is probable, not certain.\n\nExample:\n- Observation: 10,000 swans I've seen are white\n- Conclusion: All swans are white (probable but falsified by black swans in Australia)\n\n**Science uses both:**\n- Induction to form hypotheses from observations\n- Deduction to derive testable predictions from hypotheses\n\n**Key difference:** Deductive conclusions are guaranteed if premises hold. Inductive conclusions are only probable — more evidence strengthens them but never makes them certain.",
        "topic": "logic: inductive vs deductive",
        "skills": ["clear definitions", "examples", "how science uses both"]
    },

    # ── ASTRONOMY ─────────────────────────────────────────────────────────
    {
        "question": "How big is the universe?",
        "ideal_answer": "The observable universe is approximately **93 billion light-years in diameter** — this is the sphere we can, in principle, observe from Earth.\n\n**Key scales:**\n- Earth → Moon: 1.3 light-seconds\n- Earth → Sun: 8 light-minutes\n- Solar system edge (Oort cloud): ~1 light-year\n- Nearest star (Proxima Centauri): 4.24 light-years\n- Milky Way diameter: ~100,000 light-years (~200–400 billion stars)\n- Nearest galaxy (Andromeda): 2.537 million light-years\n- Observable universe: ~93 billion light-years\n- Estimated 2 trillion galaxies in the observable universe\n\n**The universe may be infinite** — we can only observe as far as light has traveled since the Big Bang (~13.8 billion years ago). The actual universe could be vastly larger than the observable portion.",
        "topic": "astronomy: size of universe",
        "skills": ["scale ladder", "exact values", "observable vs actual"]
    },
    {
        "question": "What is a black hole?",
        "ideal_answer": "A black hole is a region of spacetime where gravity is so extreme that nothing — not even light — can escape once it crosses the event horizon.\n\n**Formation:**\n- Stellar black holes: massive stars (>25 solar masses) collapse at end of life in a supernova\n- Supermassive black holes: in centers of most galaxies (Milky Way's: Sagittarius A*, 4 million solar masses)\n\n**Key features:**\n- **Event horizon:** the point of no return — crossing it is irreversible\n- **Singularity:** center where density is infinite (our physics breaks down here)\n- **Hawking radiation:** black holes slowly emit radiation and lose mass over astronomical timescales\n- **Gravitational time dilation:** time runs slower near a black hole (GPS must account for this)\n\n**First image:** Event Horizon Telescope captured image of M87's black hole (2019) — 6.5 billion solar masses.",
        "topic": "astronomy: black holes",
        "skills": ["definition", "formation types", "key features", "real observation"]
    },
    {
        "question": "What is the Big Bang?",
        "ideal_answer": "The Big Bang is the leading cosmological model explaining the origin and evolution of the universe — it describes the universe expanding from an extremely hot, dense state approximately **13.8 billion years ago**.\n\n**Key evidence:**\n1. **Hubble expansion** — galaxies are moving away from us; farther = faster. Reverse time → everything was together.\n2. **Cosmic Microwave Background (CMB)** — faint glow of radiation filling all of space, remnant 'echo' of the hot early universe (discovered 1965)\n3. **Abundance of light elements** — measured ratios of hydrogen, helium, lithium match Big Bang nucleosynthesis predictions precisely\n\n**Timeline:**\n- t=0: Singularity (all energy in infinite density)\n- t=10⁻³² s: Inflation (rapid expansion)\n- t=3 minutes: Protons/neutrons form first nuclei\n- t=380,000 years: Universe cools enough for atoms to form (CMB released)\n- t=200 million years: First stars ignite",
        "topic": "astronomy: big bang",
        "skills": ["3 evidence types", "timeline", "CMB explained"]
    },

    # ── MEDICINE & HEALTH ────────────────────────────────────────────────
    {
        "question": "How does the immune system work?",
        "ideal_answer": "The immune system is the body's defense network against pathogens (bacteria, viruses, fungi, parasites).\n\n**Two layers:**\n\n**1. Innate immunity (fast, non-specific):**\n- Physical barriers: skin, mucus, stomach acid\n- White blood cells (neutrophils, macrophages) engulf and destroy invaders\n- Inflammation: redness/swelling signals that repair is happening\n- Response time: minutes to hours\n\n**2. Adaptive immunity (slow, highly specific, lasting):**\n- **B cells** produce antibodies — proteins that bind to specific pathogens and neutralize them\n- **T cells** kill infected cells directly or coordinate immune response\n- **Memory cells** persist for years — why vaccines and past infections provide immunity\n- Response time: days to weeks (first exposure); hours (re-exposure, due to memory)\n\n**Vaccines** work by exposing immune system to harmless pathogen fragment → generates memory cells → future infection is defeated quickly.",
        "topic": "medicine: immune system",
        "skills": ["two-layer structure", "B/T cells", "how vaccines work"]
    },
    {
        "question": "What is the difference between a virus and a bacterium?",
        "ideal_answer": "Both cause infections, but they are fundamentally different:\n\n| Feature | Bacteria | Virus |\n|---------|---------|-------|\n| Size | 1–10 μm | 0.01–0.3 μm (10-100x smaller) |\n| Structure | Single cell with cell wall | Protein coat + genetic material (DNA or RNA) |\n| Living? | Yes — can reproduce alone | No — needs host cell to replicate |\n| Treatment | **Antibiotics** (kill bacteria) | **Antivirals** / **Vaccines** |\n| Examples | Strep throat, TB, E. coli | Flu, COVID, HIV, common cold |\n\n**Key insight:** Antibiotics CANNOT treat viral infections. Overusing antibiotics for viruses is a major cause of antibiotic resistance — one of the biggest public health threats.",
        "topic": "medicine: virus vs bacteria",
        "skills": ["comparison table", "treatment difference", "antibiotic resistance"]
    },

    # ── GEOGRAPHY & WORLD ────────────────────────────────────────────────
    {
        "question": "What are the seven continents?",
        "ideal_answer": "The seven continents from largest to smallest by area:\n\n1. **Asia** — 44.6M km² — largest, most populous (~4.7B people)\n2. **Africa** — 30.4M km² — 54 countries, most diverse genetic origin of humans\n3. **North America** — 24.7M km² — includes Canada, USA, Mexico, Central America\n4. **South America** — 17.8M km² — Amazon rainforest (20% of Earth's oxygen)\n5. **Antarctica** — 14M km² — no permanent population; coldest, windiest, driest\n6. **Europe** — 10.5M km² — smallest inhabited; ~50 countries\n7. **Australia/Oceania** — 8.5M km² — smallest continent; unique wildlife\n\n**Mnemonic:** **A SANE Mac** (Asia, South America, Antarctica, North America, Europe, Mac=Mac for Australia)",
        "topic": "geography: 7 continents",
        "skills": ["ranked list", "key facts", "mnemonic"]
    },
    {
        "question": "What causes earthquakes?",
        "ideal_answer": "Earthquakes are caused by the sudden release of energy in Earth's crust, creating seismic waves.\n\n**Root cause — plate tectonics:**\nEarth's lithosphere is divided into ~15 tectonic plates that move 2–10 cm/year. At plate boundaries, stress builds up for years → sudden slip → earthquake.\n\n**Types of plate boundaries:**\n- **Convergent** — plates collide (one subducts under other); produce strong earthquakes + volcanoes. Example: Japan, Chile\n- **Divergent** — plates pull apart; milder quakes. Example: Mid-Atlantic Ridge\n- **Transform** — plates slide sideways; dangerous shallow quakes. Example: San Andreas Fault (California)\n\n**Measurement:**\n- **Richter scale / Moment magnitude (Mw)** — each unit = 32× more energy released\n- Mw 3: barely felt. Mw 6: significant damage. Mw 8+: catastrophic (2011 Japan: Mw 9.1)\n\n**Why some places are safer:** Countries far from plate boundaries (e.g., Australia interior, central Africa) have very low earthquake risk.",
        "topic": "geography: earthquakes",
        "skills": ["plate tectonics", "3 boundary types", "Richter scale", "safe areas"]
    },

    # ── ART, MUSIC & CULTURE ──────────────────────────────────────────────
    {
        "question": "What makes something art?",
        "ideal_answer": "There's no single answer — defining art is one of philosophy's oldest debates. Several major perspectives:\n\n**1. Expression theory:** Art is the expression of emotion. If the creator felt something and successfully conveyed it, it's art. (Tolstoy)\n\n**2. Institutional theory:** Art is whatever the 'artworld' (galleries, critics, institutions) designates as art. (Dickie) This explains why Duchamp's urinal ('Fountain', 1917) became famous.\n\n**3. Skill/craft theory:** Art requires exceptional technical mastery — the ability to do something most cannot.\n\n**4. Aesthetic theory:** Art creates a specific aesthetic experience — beauty, sublimity, or 'significant form'.\n\n**5. Evolutionary view:** Art signals fitness and intelligence (costly signaling theory) — elaborate displays that serve reproductive or social purposes.\n\n**Practical take:** Art is communication that transcends the literal — it creates emotional, intellectual, or perceptual experiences beyond its surface content. The 'what makes it art' question is less important than the experience it creates.",
        "topic": "art: what is art",
        "skills": ["5 theories", "Duchamp example", "practical synthesis"]
    },
    {
        "question": "How does music affect the brain?",
        "ideal_answer": "Music activates more brain regions simultaneously than almost any other stimulus:\n\n**Neurological effects:**\n- **Dopamine release** — pleasurable music triggers same reward circuits as food and sex (explains chills/'frisson')\n- **Motor cortex activation** — music with a beat automatically activates movement planning (why we tap feet)\n- **Memory** — music is processed in hippocampus alongside autobiographical memories (why old songs bring vivid memories)\n- **Emotion** — amygdala responds to musical tension and resolution\n\n**Practical applications:**\n- **Pain management** — music reduces perceived pain intensity and anxiety in medical procedures\n- **Stroke recovery** — singing-based therapy helps speech recovery (left vs right brain hemispheres for language/music)\n- **Focus/productivity** — ~120 BPM background music slightly improves concentration on repetitive tasks\n- **Alzheimer's** — patients who can no longer recognize family still respond emotionally to familiar music (music memory is highly durable)\n\n**Mozart Effect** is largely a myth — one study, poorly replicated. Being a musician is far more beneficial.",
        "topic": "psychology: music and brain",
        "skills": ["brain regions", "4 practical applications", "myth debunked"]
    },

    # ── INDIA SPECIFIC (user is India-based) ──────────────────────────────
    {
        "question": "What is India's GDP?",
        "ideal_answer": "India is the **world's 5th largest economy** (as of 2024) by nominal GDP and **3rd largest by purchasing power parity (PPP)**:\n\n- **Nominal GDP:** ~$3.7 trillion USD (2024)\n- **GDP per capita:** ~$2,600 (nominal) / ~$9,000 (PPP)\n- **GDP growth rate:** ~6.5–7% annually — one of the fastest large economies\n\n**Sectors:**\n- Services: ~55% of GDP (IT, finance, telecom)\n- Industry: ~28% (manufacturing, construction)\n- Agriculture: ~17% (employs ~45% of workforce)\n\n**Key drivers:** Software exports (Bengaluru = 'Silicon Valley of India'), remittances, manufacturing under Make in India, growing middle class.\n\n**Target:** India aims to become a $5 trillion economy by 2027 and $30 trillion by 2050 (third largest).",
        "topic": "India: economy GDP",
        "skills": ["ranked position", "key sectors", "growth drivers", "targets"]
    },
    {
        "question": "What is the Indian Space Research Organisation?",
        "ideal_answer": "ISRO (Indian Space Research Organisation) is India's national space agency, founded in 1969 and headquartered in Bengaluru.\n\n**Major achievements:**\n- **Chandrayaan-1 (2008)** — discovered water ice on Moon's permanently shadowed craters\n- **Mars Orbiter Mission 'Mangalyaan' (2013)** — first Asian spacecraft to reach Mars; cost $74M (cheapest Mars mission ever — less than the film 'Gravity')\n- **PSLV reliability** — Polar Satellite Launch Vehicle has 99%+ success rate; launched 104 satellites in single mission (2017 world record)\n- **Chandrayaan-3 (2023)** — successfully landed on Moon's south pole — only 4th country to land on Moon, first near the south pole\n\n**ISRO's strength:** Cost-efficient engineering — achieves missions at 10x lower cost than NASA/ESA through frugal innovation and lean teams.",
        "topic": "India: ISRO space agency",
        "skills": ["founding date", "4 achievements", "cost efficiency"]
    },

    # ── LANGUAGE & WORDS ──────────────────────────────────────────────────
    {
        "question": "How many languages are in the world?",
        "ideal_answer": "There are approximately **7,000–7,100 living languages** in the world (Ethnologue 2023 estimate).\n\n**Key facts:**\n- Top 10 languages by speakers cover ~50% of world population\n- **Mandarin Chinese**: ~1.1B native speakers (most native speakers)\n- **English**: ~380M native but ~1.5B total speakers (most total)\n- **Spanish, Hindi, Arabic, French, Portuguese, Russian, Swahili** round out the top tier\n- India alone has 22 officially recognized languages and hundreds of dialects\n\n**Endangerment:**\n- ~40% of languages have fewer than 1,000 speakers\n- A language dies every 14 days on average\n- By 2100, up to 50% of today's languages may be extinct\n\n**Language families:** Indo-European is largest (includes English, Hindi, Spanish, Russian). Sino-Tibetan (Chinese), Afro-Asiatic (Arabic, Hebrew), Niger-Congo (African languages) are other major families.",
        "topic": "linguistics: world languages",
        "skills": ["count", "top languages", "endangerment", "families"]
    },

    # ── MORE COMPRESSION (strengthen our domain) ──────────────────────────
    {
        "question": "What is arithmetic coding?",
        "ideal_answer": "Arithmetic coding is an entropy coding method that encodes an entire message as a single number in the range [0, 1), achieving compression closer to the theoretical Shannon entropy limit than Huffman coding.\n\n**How it works:**\n1. Start with the full range [0.0, 1.0)\n2. For each symbol, narrow the range proportionally to its probability\n   - If 'e' has probability 0.4 and current range is [0.2, 0.6), new range = [0.2, 0.36)\n3. After all symbols: output a number inside the final narrow range\n4. Fewer bits needed when final range stays large (high probability symbols)\n\n**Advantage over Huffman:** Huffman must assign whole bits per symbol. Arithmetic coding assigns fractional bits — optimal for symbols with probabilities other than powers of 2.\n\n**Used in:** CMIX neural compression, JPEG 2000, H.265 video, LZMA, many modern codecs. Our engine uses range coding (a practical variant of arithmetic coding).",
        "topic": "compression: arithmetic coding detail",
        "skills": ["numbered steps", "advantage over Huffman", "used in our engine"]
    },
    {
        "question": "What is the difference between lossless and lossy compression?",
        "ideal_answer": "**Lossless compression:** Original data is perfectly reconstructed from the compressed version. Not a single bit is changed.\n- Examples: ZIP, PNG, FLAC, our CMIX/BWT engine (.aiz)\n- Used for: documents, source code, databases, RAW images (where perfection matters)\n\n**Lossy compression:** Some data is permanently discarded — the decompressed file is an approximation.\n- Examples: JPEG (images), MP3/AAC (audio), H.264 (video)\n- Used for: media where human perception can't notice small losses\n- JPEG works by discarding high-frequency detail in 8×8 pixel blocks — eyes barely notice\n\n**Comparison:**\n| | Lossless | Lossy |\n|---|---|---|\n| Reconstruction | Perfect | Approximate |\n| Ratio (text) | 25–80% | N/A (text must be lossless) |\n| Ratio (images) | 2:1 | 10:1 to 30:1 |\n| Multiple compress | Safe | Degrades (generation loss) |\n\n**Our engine** is 100% lossless — every decompressed .aiz file is bit-for-bit identical to the original.",
        "topic": "compression: lossless vs lossy",
        "skills": ["clear definitions", "comparison table", "our engine clarified"]
    },
    {
        "question": "What is the theoretical limit of compression?",
        "ideal_answer": "The theoretical compression limit is set by **Shannon entropy** — the minimum average bits per symbol needed to represent information losslessly.\n\n**Shannon's Source Coding Theorem (1948):**\nNo lossless compressor can compress a message to fewer bits than its entropy. Any attempt violates information theory.\n\n**What this means practically:**\n- A file with 5.2 bits/byte entropy can be compressed to at most 5.2/8 = 65% of original size\n- Encrypted or already-compressed files (~8 bits/byte) cannot be compressed further\n- Plain English text (~4.5 bits/byte) can theoretically compress to 56% of original\n\n**How close do algorithms get?**\n- LZ77 (gzip): ~60–70% of theoretical optimum on text\n- BWT+MTF+ANS: ~85–90% of optimum\n- PPM: ~90–95% of optimum\n- CMIX (1,046 advisors): ~97–99% of optimum — the closest practical compressors get\n\n**Kolmogorov complexity** provides an alternative theoretical framework — the minimum program length needed to generate the data.",
        "topic": "compression: theoretical limits",
        "skills": ["Shannon theorem", "practical examples", "algorithm comparison", "CMIX position"]
    },

    # ── NEURAL STUDIO ADVANCED ────────────────────────────────────────────
    {
        "question": "What makes Neural Studio different from other AI chatbots?",
        "ideal_answer": "Neural Studio V10 is fundamentally different from cloud AI chatbots in 5 key ways:\n\n1. **100% Local** — runs entirely on your machine. No internet connection required for inference. Zero data leaves your PC.\n\n2. **Compression = Intelligence** — knowledge is stored using our 1,046-advisor CMIX neural engine. Compression ratio IS a measure of understanding — better compression = better prediction = smarter AI.\n\n3. **Permanent Learning** — every conversation updates the brain permanently. ChatGPT forgets between sessions. We never do.\n\n4. **Domain Expert** — deep expertise in data compression (CMIX, BWT, LZ77, PPM, Huffman) that no general-purpose AI can match in depth.\n\n5. **Transparent & Auditable** — you can see exactly what I know in `brain/knowledge/`. No black box, no mystery weights.\n\n**What we can't do yet:** Long-form creative writing, complex multi-step reasoning chains, real-time world knowledge.",
        "topic": "identity: what makes us different",
        "skills": ["5 differentiators", "honest limitations", "compression=intelligence insight"]
    },
    {
        "question": "What is the Neural Studio API?",
        "ideal_answer": "Neural Studio exposes an HTTP REST API running on `localhost:3000` (or configured port).\n\n**Key endpoints:**\n- `POST /api/chat` — send a message, get AI response\n  - Body: `{\"message\": \"your question\"}`\n  - Response: `{\"response\": \"...\", \"confidence\": 0.87}`\n\n- `POST /api/learn` — teach new knowledge\n  - Body: `{\"topic\": \"...\", \"content\": \"...\"}`\n\n- `POST /api/compress` — compress a file\n  - Body: `{\"file_path\": \"C:\\\\data.txt\", \"mode\": \"best\"}`\n\n- `GET /api/status` — brain health and stats\n\n- `GET /api/search?q=query` — search knowledge without composing a response\n\n**Start the server:** `npm run dev` (runs `node server/main.js`)\n**Python access:** `requests.post('http://localhost:3000/api/chat', json={'message': 'hello'})`",
        "topic": "usage: Neural Studio API",
        "skills": ["endpoint list", "request/response format", "start server command"]
    },

    # ── BIOLOGY (weak domain — score 0.53) ────────────────────────────────────
    {
        "question": "How does photosynthesis work?",
        "ideal_answer": "Photosynthesis refers to the process by which plants convert sunlight into food.\n\n1. Plants absorb sunlight using chlorophyll in their leaves.\n2. Carbon dioxide from the air and water from the soil are combined using light energy.\n3. The reaction produces glucose (sugar) and oxygen: 6CO2 + 6H2O + light → C6H12O6 + 6O2.\n4. Glucose provides energy for growth; oxygen is released as a byproduct.\n\nPhotosynthesis happens in two stages: the light-dependent reactions (in thylakoids) and the Calvin cycle (in the stroma). It is the foundation of nearly all food chains on Earth.",
        "topic": "biology: photosynthesis",
        "skills": ["equation", "stages", "organelle locations", "ecological importance"]
    },
    {
        "question": "What is DNA?",
        "ideal_answer": "DNA (deoxyribonucleic acid) refers to the molecule that carries genetic information in all living organisms.\n\n1. DNA is a double helix — two strands twisted around each other, connected by base pairs.\n2. The four bases are Adenine (A), Thymine (T), Guanine (G), and Cytosine (C). A pairs with T; G pairs with C.\n3. Genes are segments of DNA that encode proteins, which carry out cellular functions.\n4. Humans have about 3 billion base pairs in 23 chromosome pairs.\n\nDNA replication copies the genome before cell division. Mutations in DNA can cause disease or drive evolution. The Human Genome Project mapped all human DNA by 2003.",
        "topic": "biology: DNA genetics",
        "skills": ["structure", "base pairs", "replication", "function"]
    },
    {
        "question": "What is evolution?",
        "ideal_answer": "Evolution refers to the change in heritable characteristics of biological populations over successive generations.\n\n1. Natural selection: organisms with traits better suited to the environment survive and reproduce more.\n2. Mutation: random changes in DNA create new genetic variation.\n3. Genetic drift: random changes in allele frequency, especially in small populations.\n4. Common descent: all life on Earth shares a common ancestor.\n\nCharles Darwin and Alfred Russel Wallace independently proposed natural selection in 1858. Evolution is the unifying theory of biology — it explains biodiversity, anatomy, genetics, and the fossil record.",
        "topic": "biology: evolution natural selection",
        "skills": ["mechanisms", "natural selection", "mutation", "common descent"]
    },

    # ── MACHINE LEARNING / AI (weak score 0.50-0.55) ──────────────────────────
    {
        "question": "What is machine learning?",
        "ideal_answer": "Machine learning refers to the field of AI where systems learn patterns from data without being explicitly programmed.\n\n1. Supervised learning: the model trains on labeled examples (input → correct output) to make predictions.\n2. Unsupervised learning: the model finds patterns in unlabeled data (e.g., clustering).\n3. Reinforcement learning: the model learns by taking actions and receiving rewards or penalties.\n4. Deep learning: uses multi-layer neural networks to learn complex representations automatically.\n\nKey applications: image recognition, speech recognition, recommendation systems, natural language processing. Popular frameworks: TensorFlow, PyTorch, scikit-learn.",
        "topic": "computer_science: machine learning",
        "skills": ["types", "supervised vs unsupervised", "applications", "frameworks"]
    },
    {
        "question": "What is a neural network?",
        "ideal_answer": "A neural network refers to a computational model inspired by the human brain, consisting of layers of interconnected nodes (neurons).\n\n1. Input layer: receives raw data (pixels, words, numbers).\n2. Hidden layers: transform the data through weighted connections and activation functions (e.g., ReLU, sigmoid).\n3. Output layer: produces the final prediction (class, value, token).\n4. Training: the network adjusts weights via backpropagation to minimize prediction error.\n\nDeep neural networks (many hidden layers) power image recognition (CNN), language models (Transformer), and game-playing AI (deep RL). Key concept: each weight represents the strength of a connection learned from data.",
        "topic": "computer_science: neural networks",
        "skills": ["architecture", "layers", "training", "activation functions", "applications"]
    },
    {
        "question": "What is recursion?",
        "ideal_answer": "Recursion refers to a programming technique where a function calls itself to solve a smaller version of the same problem.\n\n1. Base case: the condition that stops the recursion (without it, the function loops forever).\n2. Recursive case: the function calls itself with a simpler input.\n\nExample — factorial:\n```\ndef factorial(n):\n    if n == 0: return 1          # base case\n    return n * factorial(n - 1)  # recursive case\n```\nfactorial(4) = 4 × 3 × 2 × 1 = 24\n\nRecursion naturally models tree traversal, divide-and-conquer (merge sort, binary search), and backtracking. Every recursive solution can also be written iteratively with an explicit stack.",
        "topic": "computer_science: recursion",
        "skills": ["base case", "recursive case", "example", "when to use"]
    },
    {
        "question": "What is binary search?",
        "ideal_answer": "Binary search refers to an efficient algorithm for finding a target value in a SORTED array by repeatedly halving the search range.\n\n1. Start with the full array. Check the middle element.\n2. If middle == target: found! Return the index.\n3. If middle > target: search the LEFT half.\n4. If middle < target: search the RIGHT half.\n5. Repeat until found or the range is empty.\n\nTime complexity: O(log n) — each step halves the search space.\nSpace complexity: O(1) iterative, O(log n) recursive.\n\nExample: searching for 7 in [1,3,5,7,9,11] — check 5 → check 9 → check 7 → found in 3 steps instead of 6.",
        "topic": "computer_science: binary search",
        "skills": ["algorithm steps", "complexity", "sorted requirement", "example"]
    },
    {
        "question": "What is a hash map?",
        "ideal_answer": "A hash map refers to a data structure that stores key-value pairs with O(1) average-case lookup, insertion, and deletion.\n\n1. Hash function: converts a key (string, int) into an array index.\n2. Bucket: the array slot where the value is stored.\n3. Collision handling: when two keys hash to the same index — resolved by chaining (linked list per bucket) or open addressing.\n4. Load factor: ratio of items to buckets. When too high (> 0.75), the map resizes.\n\nPython dict, Java HashMap, and C++ unordered_map are all hash maps. Used everywhere: caches, frequency counts, database indexes, symbol tables.",
        "topic": "computer_science: hash map",
        "skills": ["hash function", "collision", "complexity", "real-world use"]
    },

    # ── ECONOMICS (boost from 0.56) ──────────────────────────────────────────
    {
        "question": "What is supply and demand?",
        "ideal_answer": "Supply and demand refers to the fundamental economic model describing how prices and quantities are determined in a market.\n\n1. Demand: the quantity buyers want at each price — higher prices → lower demand (inverse relationship).\n2. Supply: the quantity sellers offer at each price — higher prices → higher supply (direct relationship).\n3. Equilibrium: the price where quantity supplied equals quantity demanded — no surplus or shortage.\n4. Shifts: demand shifts when income, preferences, or related-good prices change. Supply shifts with input costs, technology, or regulations.\n\nExample: When smartphones became popular, demand for data plans surged → prices rose until new providers entered and supply increased, stabilizing prices.",
        "topic": "economics: supply and demand",
        "skills": ["demand curve", "supply curve", "equilibrium", "shifts", "example"]
    },
    {
        "question": "What is inflation?",
        "ideal_answer": "Inflation refers to the general increase in prices across an economy over time, reducing the purchasing power of money.\n\n1. Measured by CPI (Consumer Price Index) — tracks price changes in a basket of goods.\n2. Demand-pull inflation: too much money chasing too few goods (economy overheating).\n3. Cost-push inflation: production costs rise (e.g., oil prices) → prices increase.\n4. Hyperinflation: extreme rapid inflation (e.g., Zimbabwe 2008, Germany 1923) destroys savings.\n\nCentral banks (e.g., Federal Reserve) fight inflation by raising interest rates, which reduces borrowing and spending. Target inflation: 2% per year in most developed economies.",
        "topic": "economics: inflation",
        "skills": ["CPI", "causes", "types", "central bank response", "examples"]
    },

    # ── MATH — PROBABILITY (score 0.51) ──────────────────────────────────────
    {
        "question": "What is probability?",
        "ideal_answer": "Probability refers to the mathematical measure of how likely an event is to occur, expressed as a number between 0 and 1.\n\n1. P = 0: impossible event. P = 1: certain event. P = 0.5: equally likely to happen or not.\n2. Formula: P(event) = favorable outcomes / total possible outcomes.\n3. Addition rule: P(A or B) = P(A) + P(B) - P(A and B).\n4. Multiplication rule: P(A and B) = P(A) × P(B) if A and B are independent.\n\nExample: Rolling a die, P(even) = 3/6 = 0.5. Drawing two aces from a deck: P = 4/52 × 3/51 = 0.45%.\n\nApplications: statistics, machine learning, finance, quantum mechanics, and everyday decision-making.",
        "topic": "math: probability",
        "skills": ["definition", "formula", "rules", "examples", "applications"]
    },

    # ── PHYSICS — QUANTUM (score 0.63 — boost) ───────────────────────────────
    {
        "question": "What is quantum mechanics?",
        "ideal_answer": "Quantum mechanics refers to the fundamental theory describing nature at atomic and subatomic scales.\n\n1. Wave-particle duality: particles like electrons exhibit both wave and particle behavior.\n2. Superposition: a quantum particle can exist in multiple states simultaneously until measured.\n3. Heisenberg uncertainty principle: you cannot simultaneously know a particle's exact position and momentum.\n4. Quantization: energy comes in discrete packets called quanta (photons for light).\n\nSchrodinger's equation describes how quantum states evolve over time. Quantum mechanics explains atomic structure, chemical bonds, lasers, semiconductors, and MRI machines. Quantum computing exploits superposition to solve problems exponentially faster than classical computers.",
        "topic": "physics: quantum mechanics",
        "skills": ["wave-particle duality", "superposition", "uncertainty", "applications"]
    },

    # ── COMPRESSION (boost specific weak items) ───────────────────────────────
    {
        "question": "What is LZ77?",
        "ideal_answer": "LZ77 refers to a lossless data compression algorithm that replaces repeated sequences with references to earlier occurrences.\n\n1. Sliding window: maintains a buffer of recently seen data (the 'dictionary').\n2. Look-ahead buffer: the portion being compressed right now.\n3. Match: find the longest sequence in the window that matches the current input.\n4. Token: output (offset, length, next_char) — offset=how far back, length=how many bytes match.\n\nExample: 'abcabc' compresses to 'a','b','c',(back 3, copy 3) — saving 3 bytes.\n\nLZ77 is the basis of gzip, PNG, and zip. Lazy matching (checking if the next position gives a better match) improves compression ratio. Used in the default mode of this tool.",
        "topic": "compression: LZ77",
        "skills": ["sliding window", "match tokens", "example", "real-world use"]
    },
    {
        "question": "How does CMIX work?",
        "ideal_answer": "CMIX refers to a context mixing compression algorithm that achieves near-theoretical compression limits by combining hundreds of predictive models.\n\n1. Multiple models: each model predicts the probability of the next bit based on different contexts (recent bytes, long patterns, word frequencies).\n2. Context mixing: combines model predictions using a weighted logistic mix (logistic regression on log-odds).\n3. Adaptive weights: weights are updated online — models that predict correctly gain weight.\n4. Arithmetic coding: the mixed probability is used to arithmetic-encode each bit.\n\nCMIX is extremely slow (seconds per KB) but achieves the best compression ratios known. It competes in the Hutter Prize for Wikipedia compression. Your tool implements CMIX via the --cmix flag.",
        "topic": "compression: CMIX algorithm",
        "skills": ["context mixing", "model ensemble", "adaptive weights", "arithmetic coding"]
    },
]


# =============================================================================
# EXPANDED WEB SOURCES — 100 URLs across ALL knowledge domains
# =============================================================================

DEEP_WEB_SOURCES = [
    # Science fundamentals
    ("quantum mechanics basics",      "https://en.wikipedia.org/wiki/Quantum_mechanics"),
    ("special relativity",            "https://en.wikipedia.org/wiki/Special_relativity"),
    ("thermodynamics laws",           "https://en.wikipedia.org/wiki/Laws_of_thermodynamics"),
    ("evolution natural selection",   "https://en.wikipedia.org/wiki/Natural_selection"),
    ("CRISPR gene editing",           "https://en.wikipedia.org/wiki/CRISPR"),
    ("periodic table elements",       "https://en.wikipedia.org/wiki/Periodic_table"),
    ("human brain neuroscience",      "https://en.wikipedia.org/wiki/Human_brain"),
    ("general relativity Einstein",   "https://en.wikipedia.org/wiki/General_relativity"),
    ("standard model particle physics","https://en.wikipedia.org/wiki/Standard_Model"),
    ("mitosis cell division",         "https://en.wikipedia.org/wiki/Mitosis"),
    ("protein folding biology",       "https://en.wikipedia.org/wiki/Protein_folding"),
    ("plate tectonics geology",       "https://en.wikipedia.org/wiki/Plate_tectonics"),

    # Mathematics
    ("linear algebra basics",         "https://en.wikipedia.org/wiki/Linear_algebra"),
    ("probability theory",            "https://en.wikipedia.org/wiki/Probability_theory"),
    ("graph theory",                  "https://en.wikipedia.org/wiki/Graph_theory"),
    ("fourier transform",             "https://en.wikipedia.org/wiki/Fourier_transform"),
    ("bayesian inference",            "https://en.wikipedia.org/wiki/Bayesian_inference"),
    ("number theory mathematics",     "https://en.wikipedia.org/wiki/Number_theory"),
    ("topology mathematics",          "https://en.wikipedia.org/wiki/Topology"),
    ("statistics fundamentals",       "https://en.wikipedia.org/wiki/Statistics"),
    ("calculus mathematics",          "https://en.wikipedia.org/wiki/Calculus"),

    # Computer Science
    ("transformer architecture AI",   "https://en.wikipedia.org/wiki/Transformer_(deep_learning_architecture)"),
    ("large language models LLM",     "https://en.wikipedia.org/wiki/Large_language_model"),
    ("reinforcement learning",        "https://en.wikipedia.org/wiki/Reinforcement_learning"),
    ("convolutional neural network",  "https://en.wikipedia.org/wiki/Convolutional_neural_network"),
    ("attention mechanism AI",        "https://en.wikipedia.org/wiki/Attention_(machine_learning)"),
    ("dynamic programming",           "https://en.wikipedia.org/wiki/Dynamic_programming"),
    ("operating system kernel",       "https://en.wikipedia.org/wiki/Kernel_(operating_system)"),
    ("TCP IP protocol stack",         "https://en.wikipedia.org/wiki/Internet_protocol_suite"),
    ("public key cryptography",       "https://en.wikipedia.org/wiki/Public-key_cryptography"),
    ("docker containerization",       "https://en.wikipedia.org/wiki/Docker_(software)"),
    ("sorting algorithms computer science", "https://en.wikipedia.org/wiki/Sorting_algorithm"),
    ("compiler theory programming",   "https://en.wikipedia.org/wiki/Compiler"),
    ("database management systems",   "https://en.wikipedia.org/wiki/Database"),
    ("computer memory RAM",           "https://en.wikipedia.org/wiki/Computer_memory"),
    ("GPU graphics processing unit",  "https://en.wikipedia.org/wiki/Graphics_processing_unit"),
    ("quantum computing basics",      "https://en.wikipedia.org/wiki/Quantum_computing"),
    ("hash function cryptography",    "https://en.wikipedia.org/wiki/Hash_function"),

    # History
    ("world war 2 history",           "https://en.wikipedia.org/wiki/World_War_II"),
    ("industrial revolution",         "https://en.wikipedia.org/wiki/Industrial_Revolution"),
    ("cold war history",              "https://en.wikipedia.org/wiki/Cold_War"),
    ("history of computing",          "https://en.wikipedia.org/wiki/History_of_computing"),
    ("roman empire history",          "https://en.wikipedia.org/wiki/Roman_Empire"),
    ("french revolution",             "https://en.wikipedia.org/wiki/French_Revolution"),
    ("history of India",              "https://en.wikipedia.org/wiki/History_of_India"),
    ("silk road ancient trade",       "https://en.wikipedia.org/wiki/Silk_Road"),
    ("renaissance europe history",    "https://en.wikipedia.org/wiki/Renaissance"),

    # Economics
    ("keynesian economics",           "https://en.wikipedia.org/wiki/Keynesian_economics"),
    ("game theory",                   "https://en.wikipedia.org/wiki/Game_theory"),
    ("gdp economic indicator",        "https://en.wikipedia.org/wiki/Gross_domestic_product"),
    ("stock market investing",        "https://en.wikipedia.org/wiki/Stock_market"),
    ("monetary policy central banks", "https://en.wikipedia.org/wiki/Monetary_policy"),
    ("international trade economics", "https://en.wikipedia.org/wiki/International_trade"),

    # Philosophy & Logic
    ("formal logic",                  "https://en.wikipedia.org/wiki/Logic"),
    ("epistemology knowledge",        "https://en.wikipedia.org/wiki/Epistemology"),
    ("philosophy of mind",            "https://en.wikipedia.org/wiki/Philosophy_of_mind"),
    ("ethics moral philosophy",       "https://en.wikipedia.org/wiki/Ethics"),
    ("existentialism philosophy",     "https://en.wikipedia.org/wiki/Existentialism"),
    ("stoicism philosophy",           "https://en.wikipedia.org/wiki/Stoicism"),

    # Psychology
    ("cognitive psychology",          "https://en.wikipedia.org/wiki/Cognitive_psychology"),
    ("behavioral economics",          "https://en.wikipedia.org/wiki/Behavioral_economics"),
    ("developmental psychology",      "https://en.wikipedia.org/wiki/Developmental_psychology"),
    ("social psychology",             "https://en.wikipedia.org/wiki/Social_psychology"),

    # Language & Linguistics
    ("linguistics study language",    "https://en.wikipedia.org/wiki/Linguistics"),
    ("natural language processing",   "https://en.wikipedia.org/wiki/Natural_language_processing"),
    ("language acquisition children", "https://en.wikipedia.org/wiki/Language_acquisition"),

    # Astronomy
    ("big bang cosmology",            "https://en.wikipedia.org/wiki/Big_Bang"),
    ("black holes astronomy",         "https://en.wikipedia.org/wiki/Black_hole"),
    ("milky way galaxy",              "https://en.wikipedia.org/wiki/Milky_Way"),
    ("dark matter cosmology",         "https://en.wikipedia.org/wiki/Dark_matter"),
    ("exoplanets discovery",          "https://en.wikipedia.org/wiki/Exoplanet"),

    # Medicine
    ("immune system biology",         "https://en.wikipedia.org/wiki/Immune_system"),
    ("vaccines immunology",           "https://en.wikipedia.org/wiki/Vaccine"),
    ("cancer biology treatment",      "https://en.wikipedia.org/wiki/Cancer"),
    ("antibiotic resistance medicine","https://en.wikipedia.org/wiki/Antimicrobial_resistance"),

    # Compression (our domain — advanced)
    ("context mixing compression",    "https://en.wikipedia.org/wiki/Context_mixing"),
    ("prediction partial matching",   "https://en.wikipedia.org/wiki/Prediction_by_partial_matching"),
    ("arithmetic coding",             "https://en.wikipedia.org/wiki/Arithmetic_coding"),
    ("asymmetric numeral systems",    "https://en.wikipedia.org/wiki/Asymmetric_numeral_systems"),
    ("lzma compression",              "https://en.wikipedia.org/wiki/Lempel%E2%80%93Ziv%E2%80%93Markov_chain_algorithm"),
    ("run length encoding",           "https://en.wikipedia.org/wiki/Run-length_encoding"),
    ("bzip2 compression",             "https://en.wikipedia.org/wiki/Bzip2"),
    ("zstd compression",              "https://en.wikipedia.org/wiki/Zstd"),
    ("data compression overview",     "https://en.wikipedia.org/wiki/Data_compression"),

    # General knowledge & Current affairs
    ("climate change global warming", "https://en.wikipedia.org/wiki/Climate_change"),
    ("bitcoin cryptocurrency",        "https://en.wikipedia.org/wiki/Bitcoin"),
    ("space exploration history",     "https://en.wikipedia.org/wiki/Space_exploration"),
    ("human genome project",          "https://en.wikipedia.org/wiki/Human_Genome_Project"),
    ("artificial intelligence overview","https://en.wikipedia.org/wiki/Artificial_intelligence"),
    ("alan turing biography",         "https://en.wikipedia.org/wiki/Alan_Turing"),
    ("claude shannon information",    "https://en.wikipedia.org/wiki/Claude_Shannon"),
    ("john von neumann",              "https://en.wikipedia.org/wiki/John_von_Neumann"),
    ("india history culture",         "https://en.wikipedia.org/wiki/India"),
    ("ISRO Indian space research",    "https://en.wikipedia.org/wiki/Indian_Space_Research_Organisation"),
    ("renewable energy solar wind",   "https://en.wikipedia.org/wiki/Renewable_energy"),
    ("internet of things IoT",        "https://en.wikipedia.org/wiki/Internet_of_things"),
    ("5G mobile network",             "https://en.wikipedia.org/wiki/5G"),
]


# =============================================================================
# TRAINING RUNNER
# =============================================================================

def run_deep_sft(verbose=True):
    """Load all 200+ SFT pairs including the base + deep ones."""
    all_pairs = SFT_PAIRS + DEEP_SFT_PAIRS
    print(f"\n🎓 Deep SFT — loading {len(all_pairs)} total training pairs")
    print(f"   ({len(SFT_PAIRS)} base + {len(DEEP_SFT_PAIRS)} deep domain pairs)\n")
    return run_sft(pairs=all_pairs, verbose=verbose)


def run_deep_web(verbose=True):
    """Pull 50+ Wikipedia articles into the brain."""
    from neural_brain import learn_from_url
    
    print(f"\n🌐 Deep Web Learning — {len(DEEP_WEB_SOURCES)} Wikipedia topics")
    print("   (Same training data domains as Claude pre-training)\n")
    
    learned = 0
    failed  = 0

    for topic, url in DEEP_WEB_SOURCES:
        try:
            result = learn_from_url(topic, url)
            if "error" in result:
                status = f"❌ Failed: {result['error'][:50]}"
                failed += 1
            else:
                tokens  = result.get("token_count", 0)
                savings = result.get("savings_pct", 0)
                status  = f"✅ {tokens:,} tokens  ({savings:.0f}% compressed)"
                learned += 1
        except Exception as e:
            status = f"❌ {str(e)[:60]}"
            failed += 1

        if verbose:
            print(f"  [{learned+failed:2d}] {topic:<40} {status}")
        time.sleep(0.3)

    print(f"\n  Web done: {learned} learned, {failed} failed")
    return learned, failed


def run_multi_round_cai(rounds=3, verbose=True):
    """Run CAI critique multiple rounds — violations in round N feed round N+1."""
    print(f"\n🔍 Multi-Round CAI: {rounds} rounds of constitutional critique\n")
    
    total_violations = 0
    for r in range(1, rounds + 1):
        print(f"  Round {r}/{rounds}:")
        result = run_cai_loop(verbose=False)
        viol = result.get("violations_found", 0)
        passed = result.get("passed_all_rules", 0)
        total_violations += viol
        print(f"    ✅ {passed} passed  |  ⚠️  {viol} violations")
        if viol == 0:
            print(f"    🎉 Perfect score! No violations in round {r}")
            break
        time.sleep(0.5)

    return total_violations


def print_final_status():
    stats = brain.brain_stats()
    print(f"""
╔══════════════════════════════════════════════════════════════════════╗
║   🏆  DEEP TRAINING COMPLETE — Claude 1-Level Progress Report        ║
╚══════════════════════════════════════════════════════════════════════╝

  📚 Knowledge Base:
     Items stored       : {stats['total_knowledge_items']}
     Vocabulary         : {stats['vocabulary_size']:,} words
     Raw data           : {stats.get('total_raw_human', 'N/A')}
     Compressed to      : {stats.get('total_compressed_human', 'N/A')}
     Compression savings: {stats.get('compression_savings_pct', 0):.1f}%

  🧠 What your AI now knows:
     ✅ Mathematics: calculus, Big O, prime numbers, statistics
     ✅ Computer Science: OOP, REST, SQL/NoSQL, Git, Docker, ML, NNs
     ✅ Physics: Newton's laws, speed of light, quantum mechanics
     ✅ Biology: DNA, evolution, photosynthesis
     ✅ History: WWI causes, Industrial Revolution
     ✅ Economics: supply/demand, inflation
     ✅ Psychology: cognitive biases, Maslow hierarchy
     ✅ Philosophy: scientific method, Occam's razor, first principles
     ✅ Compression: full algorithm knowledge (CMIX, BWT, LZ77, PPM, Huffman)
     ✅ Reasoning: problem solving, tradeoffs, explanation skills
     ✅ AI Ethics: risks, Anthropic's safety approach

  🚀 Usage:
     ask What is a neural network?
     ask Explain the Pythagorean theorem
     ask How does TCP work?
     python server/rlhf_trainer.py --status

  🔁 To keep improving:
     python server/deep_trainer.py              (re-run anytime)
     python server/rlhf_trainer.py --full       (RLHF cycle)
""")


def main():
    parser = argparse.ArgumentParser(description="Deep training toward Claude 1 intelligence level")
    parser.add_argument("--sft-only",    action="store_true", help="Only run SFT pairs (no web)")
    parser.add_argument("--web-only",    action="store_true", help="Only run web learning")
    parser.add_argument("--cai-only",    action="store_true", help="Only run CAI critique")
    parser.add_argument("--rounds",      type=int, default=3,  help="CAI critique rounds (default: 3)")
    parser.add_argument("--fast",        action="store_true",  help="Skip web (SFT + CAI only)")
    parser.add_argument("--continuous",  action="store_true",  help="Loop training until stopped (Ctrl+C)")
    parser.add_argument("--target",      type=int, default=1000, help="Target knowledge items to reach (default: 1000)")
    args = parser.parse_args()

    do_sft = not args.web_only and not args.cai_only
    do_web = not args.sft_only and not args.cai_only and not args.fast
    do_cai = not args.sft_only and not args.web_only

    cycle = 0
    start = time.time()

    while True:
        cycle += 1
        print(f"\n{'='*70}")
        print(f"TRAINING CYCLE {cycle}" + (" [CONTINUOUS MODE]" if args.continuous else ""))
        print(f"{'='*70}")

        if do_sft:
            print(f"\nPHASE 1: Deep SFT ({len(SFT_PAIRS + DEEP_SFT_PAIRS)} total pairs)")
            run_deep_sft(verbose=True)

        if do_web:
            print(f"\nPHASE 2: Web Learning ({len(DEEP_WEB_SOURCES)} Wikipedia topics)")
            run_deep_web(verbose=True)

        if do_cai:
            print(f"\nPHASE 3: Multi-Round CAI ({args.rounds} rounds)")
            run_multi_round_cai(rounds=args.rounds, verbose=True)

        elapsed = time.time() - start
        stats = brain.brain_stats()
        items = stats['total_knowledge_items']
        vocab = stats['vocabulary_size']

        print(f"\n  ⏱️  Cycle {cycle} done in {elapsed/60:.1f} min total")
        print(f"  📚 Knowledge: {items} items | {vocab:,} words")
        print(f"  🎯 Target: {args.target} items | Progress: {min(items/args.target*100, 100):.0f}%")

        if not args.continuous:
            break

        if items >= args.target:
            print(f"\n  🏆 TARGET REACHED: {items} knowledge items!")
            print(f"  Vocabulary: {vocab:,} words — Claude 1 level achieved!")
            break

        print(f"\n  🔁 Continuing training (Ctrl+C to stop)...")
        print(f"  Next cycle starts in 3 seconds...")
        try:
            time.sleep(3)
        except KeyboardInterrupt:
            print("\n  ⏹️  Training stopped by user.")
            break

    print_final_status()


if __name__ == "__main__":
    main()
