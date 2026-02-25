#!/usr/bin/env python3
"""
PYTHON SUPPORT SCRIPT — Corpus Assembly Only
=============================================
ARCHITECTURE RULE: Python = support layer only. C++ = THE ONE BRAIN.

create_training_corpus.py — Download Wikipedia articles, clean text,
                             assemble a corpus.txt file for C++ training.
Role: Python fetches Wikipedia, strips HTML, writes plain text to a file.
      Then YOU call C++: neural_engine.exe train_transformer corpus.txt 7 0.002 16
Does NOT: train anything itself, learn anything, store any knowledge.

Usage:
  python create_training_corpus.py                → creates training_corpus.txt
  Then: neural_engine.exe train_transformer training_corpus.txt 7 0.002 16

See: docs/ARCHITECTURE.md for the complete system design.

Original description:
Create a high-quality training corpus from Wikipedia articles
Downloads and cleans text for transformer training
"""

import requests
import re
import sys

def clean_text(text):
    """Clean Wikipedia text for training"""
    # Remove citation markers like [1], [2], etc.
    text = re.sub(r'\[\d+\]', '', text)

    # Remove multiple spaces
    text = re.sub(r'\s+', ' ', text)

    # Remove URLs
    text = re.sub(r'https?://\S+', '', text)

    # Keep only readable ASCII + common punctuation
    text = ''.join(c for c in text if ord(c) < 128 or c in 'àáâãäåèéêëìíîïòóôõöùúûüýÿñçÀÁÂÃÄÅÈÉÊËÌÍÎÏÒÓÔÕÖÙÚÛÜÝŸÑÇ')

    return text.strip()

def get_wikipedia_article(title):
    """Fetch Wikipedia article text"""
    url = f"https://en.wikipedia.org/w/api.php"
    params = {
        'action': 'query',
        'format': 'json',
        'titles': title,
        'prop': 'extracts',
        'explaintext': True,
        'exsectionformat': 'plain'
    }

    try:
        response = requests.get(url, params=params, timeout=10)
        data = response.json()

        page = next(iter(data['query']['pages'].values()))
        if 'extract' in page:
            return clean_text(page['extract'])
        return None
    except Exception as e:
        print(f"Error fetching {title}: {e}", file=sys.stderr)
        return None

def create_corpus(output_file, target_lines=5000):
    """Create training corpus from diverse topics"""

    # Diverse topics for well-rounded AI knowledge
    topics = [
        # Tech & Computer Science
        "Data compression", "Artificial intelligence", "Machine learning",
        "Programming language", "Computer science", "Algorithm",
        "Software engineering", "Computer network", "Database",

        # Science
        "Physics", "Chemistry", "Biology", "Mathematics",
        "Astronomy", "Geology", "Evolution", "Scientific method",

        # General Knowledge
        "History", "Geography", "Literature", "Philosophy",
        "Psychology", "Economics", "Politics", "Art",

        # Practical
        "Technology", "Engineering", "Medicine", "Education",
        "Communication", "Transportation", "Energy", "Environment"
    ]

    print(f"Creating training corpus with ~{target_lines} lines...")
    print(f"Downloading {len(topics)} Wikipedia articles...\n")

    corpus_text = []
    total_lines = 0

    for i, topic in enumerate(topics, 1):
        print(f"[{i}/{len(topics)}] Fetching: {topic}...", end=" ")

        text = get_wikipedia_article(topic)
        if text:
            # Split into sentences
            sentences = re.split(r'[.!?]\s+', text)

            # Add sentences until we reach target
            for sentence in sentences:
                if sentence.strip() and len(sentence) > 20:  # Skip very short sentences
                    corpus_text.append(sentence.strip() + ".")
                    total_lines += 1

                    if total_lines >= target_lines:
                        break

            print(f"OK ({len(sentences)} sentences, total: {total_lines} lines)")
        else:
            print("Failed")

        if total_lines >= target_lines:
            print(f"\nReached target of {target_lines} lines!")
            break

    # Write to file
    with open(output_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(corpus_text))

    print(f"\n{'='*60}")
    print(f"Created: {output_file}")
    print(f"  Lines: {total_lines}")
    print(f"  Size: {len(''.join(corpus_text)) / 1024:.1f} KB")
    print(f"{'='*60}\n")

    return total_lines

if __name__ == "__main__":
    target = int(sys.argv[1]) if len(sys.argv) > 1 else 5000
    output = sys.argv[2] if len(sys.argv) > 2 else "training_large.txt"

    create_corpus(output, target)

    print("\nNext steps:")
    print(f"  1. Train: ./bin/neural_engine.exe train_transformer {output} 7 0.002 16")
    print(f"  2. Time: ~6-8 minutes (same fast training!)")
    print(f"  3. Quality: Level 3-4 (much better than Level 2.2)")
    print(f"  4. Result: Actual readable answers!")
