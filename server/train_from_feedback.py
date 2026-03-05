"""
Auto-training from User Feedback - Phase I1
============================================
Automatically trains the AI from user feedback (👍👎).

Process:
1. Load positive feedback → SFT training pairs
2. Load negative feedback → corrections queue
3. Run SFT training on positive examples
4. Run auto-learning on negative examples
5. Retrain transformer

Usage:
    python train_from_feedback.py                    # Train from all feedback
    python train_from_feedback.py --positive-only    # Train only on positive feedback
    python train_from_feedback.py --threshold=10     # Minimum feedback count to train
"""

import os
import sys
import json
import subprocess
from datetime import datetime

BASE_DIR = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
NEURAL_ENGINE_EXE = os.path.join(BASE_DIR, "bin", "neural_engine.exe")


def load_feedback_data():
    """Load all user feedback from JSONL file."""
    feedback_file = os.path.join(BASE_DIR, "brain", "feedback", "user_feedback.jsonl")
    if not os.path.exists(feedback_file):
        print("No feedback data found.")
        return []

    feedback = []
    with open(feedback_file, 'r', encoding='utf-8') as f:
        for line in f:
            if line.strip():
                try:
                    feedback.append(json.loads(line))
                except:
                    pass
    return feedback


def get_positive_feedback(feedback):
    """Extract all positive feedback examples."""
    return [f for f in feedback if f.get('feedback') == 'positive']


def get_negative_feedback(feedback):
    """Extract all negative feedback examples."""
    return [f for f in feedback if f.get('feedback') == 'negative']


def train_on_positive_feedback(positive_examples):
    """Train on positive feedback - convert to corpus and train transformer."""
    if not positive_examples:
        print("No positive feedback to train on.")
        return False

    print(f"\n=== Training on {len(positive_examples)} Positive Examples ===")

    # Create training corpus from positive feedback
    # Each example becomes a Q&A pair in the corpus
    corpus_lines = []
    for ex in positive_examples:
        corpus_lines.append(f"Q: {ex['question']}")
        corpus_lines.append(f"A: {ex['answer']}")
        corpus_lines.append("")  # Blank line separator

    # Write to corpus file
    sft_corpus_file = os.path.join(BASE_DIR, "brain", "training", "sft_from_feedback.txt")
    os.makedirs(os.path.dirname(sft_corpus_file), exist_ok=True)
    with open(sft_corpus_file, 'w', encoding='utf-8') as f:
        f.write('\n'.join(corpus_lines))

    print(f"Created SFT corpus file: {sft_corpus_file}")
    print(f"Training transformer on {len(positive_examples)} examples...")

    # Run transformer training (no buffering - stream output)
    try:
        cmd = [NEURAL_ENGINE_EXE, "train_transformer", sft_corpus_file, "5", "0.001", "8"]
        # Use Popen for real-time output instead of run()
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=BASE_DIR,
            bufsize=1  # Line buffered
        )

        # Stream output in real-time
        for line in process.stdout:
            print(line.rstrip())

        process.wait(timeout=600)  # 10 minute timeout

        if process.returncode == 0:
            print(f"✓ SFT training completed successfully!")
            return True
        else:
            print(f"✗ SFT training failed with exit code {process.returncode}")
            return False
    except subprocess.TimeoutExpired:
        print("✗ SFT training timeout (> 10 minutes)")
        process.kill()
        return False
    except Exception as e:
        print(f"✗ SFT training error: {e}")
        return False


def improve_negative_feedback(negative_examples):
    """Learn from negative feedback - auto-correct and retrain."""
    if not negative_examples:
        print("No negative feedback to improve on.")
        return False

    print(f"\n=== Improving {len(negative_examples)} Negative Examples ===")

    corrections = []
    for ex in negative_examples:
        question = ex['question']
        bad_answer = ex['answer']

        print(f"\n[Negative Feedback] Q: {question[:60]}...")
        print(f"  Bad Answer: {bad_answer[:60]}...")

        # Try to get better answer by learning from Wikipedia
        # Extract key concepts from question
        words = question.lower().split()
        key_concepts = [w for w in words if len(w) > 4 and w not in [
            'what', 'where', 'when', 'which', 'explain', 'describe', 'about', 'does', 'should'
        ]]

        if key_concepts:
            topic = ' '.join(key_concepts[:3])  # Use first 3 key words
            print(f"  Learning from Wikipedia: {topic}")

            # Learn from Wikipedia
            try:
                url = f"https://en.wikipedia.org/wiki/{topic.replace(' ', '_')}"
                cmd = [NEURAL_ENGINE_EXE, "learn", url]
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=30, cwd=BASE_DIR)
                if result.returncode == 0:
                    print(f"  ✓ Learned from: {url}")
                else:
                    print(f"  ✗ Could not learn from Wikipedia")
            except:
                pass

            # Get improved answer
            try:
                cmd = [NEURAL_ENGINE_EXE, "ai_ask", question]
                result = subprocess.run(cmd, capture_output=True, text=True, timeout=20, cwd=BASE_DIR)
                if result.returncode == 0:
                    improved_answer = result.stdout.strip()
                    print(f"  Improved Answer: {improved_answer[:60]}...")

                    corrections.append({
                        "question": question,
                        "bad_answer": bad_answer,
                        "improved_answer": improved_answer,
                        "topic": topic
                    })
                else:
                    print(f"  ✗ Could not generate improved answer")
            except:
                pass

    # Save corrections
    if corrections:
        corrections_file = os.path.join(BASE_DIR, "brain", "self_learning", "feedback_corrections.json")
        os.makedirs(os.path.dirname(corrections_file), exist_ok=True)
        with open(corrections_file, 'w', encoding='utf-8') as f:
            json.dump(corrections, f, indent=2)
        print(f"\n✓ Saved {len(corrections)} corrections to: {corrections_file}")
        return True
    else:
        print("\n✗ No corrections generated")
        return False


def retrain_transformer():
    """Retrain transformer on all learned knowledge."""
    print("\n=== Retraining Transformer ===")

    # Create corpus from all knowledge files
    knowledge_dir = os.path.join(BASE_DIR, "brain", "knowledge")
    if not os.path.exists(knowledge_dir):
        print("No knowledge directory found")
        return False

    corpus_file = os.path.join(BASE_DIR, "brain", "training", "feedback_corpus.txt")
    os.makedirs(os.path.dirname(corpus_file), exist_ok=True)

    # Merge all knowledge files
    file_count = 0
    with open(corpus_file, 'w', encoding='utf-8') as out:
        for filename in os.listdir(knowledge_dir):
            if filename.endswith('.txt'):
                filepath = os.path.join(knowledge_dir, filename)
                try:
                    with open(filepath, 'r', encoding='utf-8') as f:
                        content = f.read().strip()
                        if content:
                            out.write(content + '\n\n')
                            file_count += 1
                except:
                    pass

    print(f"Created corpus from {file_count} knowledge files: {corpus_file}")
    print(f"Training transformer (7 epochs, LR=0.002, batch=16)...")
    print("This may take several minutes - streaming output below:")
    print("-" * 60)

    try:
        cmd = [NEURAL_ENGINE_EXE, "train_transformer", corpus_file, "7", "0.002", "16"]
        # Use Popen for real-time streaming output
        process = subprocess.Popen(
            cmd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            cwd=BASE_DIR,
            bufsize=1  # Line buffered
        )

        # Stream output in real-time to prevent buffer overflow
        for line in process.stdout:
            print(line.rstrip())

        process.wait(timeout=1200)  # 20 minute timeout for large corpus

        print("-" * 60)
        if process.returncode == 0:
            print(f"✓ Transformer training completed!")
            return True
        else:
            print(f"✗ Transformer training failed with exit code {process.returncode}")
            return False
    except subprocess.TimeoutExpired:
        print("✗ Transformer training timeout (> 20 minutes)")
        process.kill()
        return False
    except Exception as e:
        print(f"✗ Transformer training error: {e}")
        return False


def archive_processed_feedback():
    """Archive feedback that has been processed."""
    feedback_file = os.path.join(BASE_DIR, "brain", "feedback", "user_feedback.jsonl")
    if not os.path.exists(feedback_file):
        return

    # Move to archive
    archive_dir = os.path.join(BASE_DIR, "brain", "feedback", "archive")
    os.makedirs(archive_dir, exist_ok=True)

    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    archive_file = os.path.join(archive_dir, f"feedback_{timestamp}.jsonl")

    import shutil
    shutil.move(feedback_file, archive_file)
    print(f"\n✓ Archived processed feedback to: {archive_file}")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Train AI from user feedback")
    parser.add_argument('--positive-only', action='store_true', help="Train only on positive feedback")
    parser.add_argument('--threshold', type=int, default=5, help="Minimum feedback count to train (default: 5)")
    args = parser.parse_args()

    print("=" * 60)
    print("AUTO-TRAINING FROM USER FEEDBACK (Phase I1)")
    print("=" * 60)

    # Load all feedback
    all_feedback = load_feedback_data()
    if len(all_feedback) == 0:
        print("\nNo feedback data to process. Exiting.")
        return

    print(f"\nTotal feedback entries: {len(all_feedback)}")

    positive = get_positive_feedback(all_feedback)
    negative = get_negative_feedback(all_feedback)

    print(f"  Positive (👍): {len(positive)}")
    print(f"  Negative (👎): {len(negative)}")

    # Check threshold
    if len(all_feedback) < args.threshold:
        print(f"\nNot enough feedback (< {args.threshold}). Waiting for more feedback.")
        return

    success = False

    # Train on positive feedback
    if positive:
        if train_on_positive_feedback(positive):
            success = True

    # Improve on negative feedback (unless --positive-only)
    if not args.positive_only and negative:
        if improve_negative_feedback(negative):
            success = True

    # Retrain transformer if we made changes
    if success:
        retrain_transformer()

        # Archive processed feedback
        archive_processed_feedback()

        print("\n" + "=" * 60)
        print("✓ TRAINING FROM FEEDBACK COMPLETE!")
        print("=" * 60)
    else:
        print("\n✗ No training performed (no successful learning)")


if __name__ == "__main__":
    main()
