#!/usr/bin/env python3
"""
Train Neural Studio C++ Brain from Anthropic Prompt Files.

Python's role (support only):
  - Read and parse .md files from the Anthropic/ directory
  - Split content into topic sections intelligently
  - Write each section to a temp file
  - Call neural_engine.exe learn <file> for each section

C++ Neural Engine does the actual work:
  - Fetches / parses / tokenizes the content
  - Compresses with CMIX 1046-advisor engine
  - Stores in brain/knowledge/ as .aiz files
  - Indexes for fast RAG retrieval

Usage:
    python train_from_anthropic.py
    python train_from_anthropic.py --file claude-sonnet-4.6.md
    python train_from_anthropic.py --dry-run   (shows what would be learned)
"""

import os
import sys
import re
import time
import argparse
import tempfile
import subprocess
from pathlib import Path

# ─── Paths ───────────────────────────────────────────────────────────────────
BASE_DIR      = os.path.dirname(os.path.abspath(__file__))
ANTHROPIC_DIR = os.path.join(BASE_DIR, "Anthropic")
NEURAL_ENGINE = os.path.join(BASE_DIR, "bin", "neural_engine.exe")

# ─── Banner ───────────────────────────────────────────────────────────────────
BANNER = """
+======================================================================+
|    Neural Studio  -  Anthropic Prompt Training Mode                  |
|    Python: parses sections  |  C++: learns, compresses, indexes       |
+======================================================================+
"""

# ─── Section splitters ────────────────────────────────────────────────────────
MIN_CHUNK_CHARS = 100
MAX_CHUNK_CHARS = 8000


def split_into_sections(content: str, filename: str) -> list:
    """
    Intelligently split a prompt file into named topic sections.
    Python does this parsing work to support C++ learning.
    Returns list of {topic, content} dicts.
    """
    sections = []

    # Strategy 1: XML-tag sections (Claude's primary structure)
    xml_pattern = re.compile(
        r'`<([a-zA-Z_][a-zA-Z0-9_]*)>`([\s\S]*?)`</\1>`',
        re.MULTILINE
    )
    xml_matches = list(xml_pattern.finditer(content))
    if xml_matches:
        for m in xml_matches:
            tag   = m.group(1).replace('_', ' ')
            chunk = m.group(2).strip()
            if len(chunk) >= MIN_CHUNK_CHARS:
                topic = f"claude prompt: {tag}"
                sections.extend(_split_large(topic, chunk, filename))
        if sections:
            return sections

    # Strategy 2: Markdown headers
    header_pattern = re.compile(r'^(#{1,3})\s+(.+)$', re.MULTILINE)
    headers = list(header_pattern.finditer(content))
    if len(headers) >= 2:
        for i, hdr in enumerate(headers):
            start = hdr.end()
            end   = headers[i + 1].start() if i + 1 < len(headers) else len(content)
            chunk = content[start:end].strip()
            tag   = hdr.group(2).strip()
            if len(chunk) >= MIN_CHUNK_CHARS:
                topic = f"claude prompt: {tag}"
                sections.extend(_split_large(topic, chunk, filename))
        if sections:
            return sections

    # Strategy 3: Paragraph blocks (fallback)
    paragraphs = re.split(r'\n{3,}', content)
    chunk_buf, chunk_len, part_num = [], 0, 1
    for para in paragraphs:
        para = para.strip()
        if not para:
            continue
        chunk_buf.append(para)
        chunk_len += len(para)
        if chunk_len >= MAX_CHUNK_CHARS:
            text  = '\n\n'.join(chunk_buf)
            topic = f"{filename} part {part_num}"
            sections.append({"topic": topic, "content": text})
            chunk_buf, chunk_len, part_num = [], 0, part_num + 1

    if chunk_buf and chunk_len >= MIN_CHUNK_CHARS:
        text  = '\n\n'.join(chunk_buf)
        topic = f"{filename} part {part_num}"
        sections.append({"topic": topic, "content": text})

    return sections


def _split_large(topic: str, content: str, filename: str) -> list:
    """If a chunk is too large, split it into sub-chunks."""
    if len(content) <= MAX_CHUNK_CHARS:
        return [{"topic": topic, "content": content}]

    chunks, buf, buf_len, part = [], [], 0, 1
    for para in content.split('\n\n'):
        para = para.strip()
        if not para:
            continue
        buf.append(para)
        buf_len += len(para)
        if buf_len >= MAX_CHUNK_CHARS:
            chunks.append({"topic": f"{topic} (part {part})", "content": '\n\n'.join(buf)})
            buf, buf_len, part = [], 0, part + 1

    if buf and buf_len >= MIN_CHUNK_CHARS:
        label = f"{topic} (part {part})" if part > 1 else topic
        chunks.append({"topic": label, "content": '\n\n'.join(buf)})

    return chunks if chunks else [{"topic": topic, "content": content[:MAX_CHUNK_CHARS]}]


def get_md_files(specific_file: str = None) -> list:
    """Return list of .md files to process."""
    if specific_file:
        p = Path(os.path.join(ANTHROPIC_DIR, specific_file))
        if not p.exists():
            print(f"[ERROR] File not found: {p}")
            sys.exit(1)
        return [p]

    files = sorted(Path(ANTHROPIC_DIR).glob("*.md"))
    if not files:
        print(f"[ERROR] No .md files found in {ANTHROPIC_DIR}")
        print(f"        Create the Anthropic/ folder and put .md files there.")
        sys.exit(1)
    return files


def learn_section_cpp(topic: str, content: str) -> bool:
    """
    Python support function: write section to temp file, then call C++ learn.
    C++ neural_engine.exe handles all learning, compression, and indexing.
    """
    # Prepend topic as a header so C++ can identify it
    full_text = f"# {topic}\n\n{content}"

    with tempfile.NamedTemporaryFile(
        mode='w', suffix='.txt', delete=False,
        encoding='utf-8', prefix='anthropic_train_'
    ) as tmp:
        tmp.write(full_text)
        tmp_path = tmp.name

    try:
        result = subprocess.run(
            [NEURAL_ENGINE, "learn", tmp_path],
            capture_output=True, text=True, timeout=30,
            cwd=BASE_DIR
        )
        log = (result.stdout + result.stderr).strip()
        return "SUCCESS" in log.upper() or "success" in log.lower() or result.returncode == 0
    except subprocess.TimeoutExpired:
        return False
    finally:
        try:
            os.unlink(tmp_path)
        except Exception:
            pass


def train(files: list, dry_run: bool = False):
    """Main training loop — Python parses, C++ learns."""
    print(BANNER)

    if not os.path.exists(NEURAL_ENGINE):
        print(f"[ERROR] C++ Neural Engine not found: {NEURAL_ENGINE}")
        print("        Run build_smart_brain.bat first!")
        sys.exit(1)

    print(f"[OK] C++ Neural Engine: {NEURAL_ENGINE}")
    print(f"[OK] Processing {len(files)} file(s) from: {ANTHROPIC_DIR}")
    print()

    total_sections = 0
    total_learned  = 0
    total_skipped  = 0
    total_chars    = 0

    for md_file in files:
        fname = md_file.name
        stem  = md_file.stem
        raw   = md_file.read_text(encoding='utf-8', errors='replace')

        print(f"[FILE] {fname}  ({len(raw):,} chars)")

        sections = split_into_sections(raw, stem)
        print(f"       Found {len(sections)} sections to learn")

        file_learned = 0
        file_skipped = 0

        for i, sec in enumerate(sections, 1):
            topic   = sec["topic"]
            content = sec["content"]
            total_sections += 1

            if dry_run:
                print(f"  [{i:3d}] [DRY-RUN] [{len(content):6,} chars] {topic[:70]}")
                total_skipped += 1
                file_skipped  += 1
                continue

            # Python writes temp file → C++ learns it
            ok = learn_section_cpp(topic, content)
            if ok:
                print(f"  [{i:3d}] [OK] {topic[:70]}")
                total_learned += 1
                file_learned  += 1
                total_chars   += len(content)
            else:
                print(f"  [{i:3d}] [FAIL] {topic[:70]}")
                total_skipped += 1
                file_skipped  += 1

            time.sleep(0.02)  # slight delay to avoid disk hammering

        print(f"       {fname}: {file_learned} learned, {file_skipped} skipped\n")

    # Summary
    print("=" * 70)
    print("TRAINING COMPLETE")
    print("=" * 70)
    print(f"  Files processed     : {len(files)}")
    print(f"  Total sections      : {total_sections}")
    print(f"  Successfully learned: {total_learned}")
    print(f"  Skipped / errors    : {total_skipped}")
    print(f"  Total chars fed     : {total_chars:,}")
    print()
    if not dry_run:
        print("  C++ Neural Engine has learned all Anthropic prompt patterns.")
        print("  Query with: bin/neural_engine.exe ai_ask 'your question'")
        print("  Or via API: POST /api/brain/ask")
    else:
        print("  (Dry-run: nothing stored)")


def main():
    parser = argparse.ArgumentParser(
        description="Train C++ Neural Engine from Anthropic system prompts (Python is support only)"
    )
    parser.add_argument("--file", "-f", default=None,
                        help="Specific .md file (default: all .md in Anthropic/)")
    parser.add_argument("--dry-run", "-n", action="store_true",
                        help="Show what would be learned without storing")
    parser.add_argument("--list", "-l", action="store_true",
                        help="List available .md files and exit")
    args = parser.parse_args()

    if args.list:
        files = list(Path(ANTHROPIC_DIR).glob("*.md"))
        print(f"Files in {ANTHROPIC_DIR}:")
        for f in sorted(files):
            print(f"  {f.name:<45}  {f.stat().st_size // 1024:6,} KB")
        return

    files = get_md_files(args.file)
    train(files, dry_run=args.dry_run)


if __name__ == "__main__":
    main()
