#!/usr/bin/env python3
"""
Use Ollama LLaMA 3 to generate high-quality training data
This creates diverse, intelligent training examples
"""

import subprocess
import json
import time

def ask_llama3(prompt):
    """Ask LLaMA 3 via Ollama"""
    try:
        result = subprocess.run(
            ['ollama', 'run', 'llama3', prompt],
            capture_output=True,
            text=True,
            timeout=30
        )
        return result.stdout.strip()
    except Exception as e:
        print(f"Error: {e}")
        return ""

# Topics to generate training data for
topics = [
    "artificial intelligence and machine learning",
    "computer science and programming",
    "mathematics and statistics",
    "physics and natural sciences",
    "history and world events",
    "geography and earth science",
    "literature and language",
    "philosophy and ethics",
    "economics and business",
    "technology and innovation",
    "biology and medicine",
    "chemistry and materials",
    "astronomy and space",
    "psychology and neuroscience",
    "art and culture",
]

# Question templates
question_templates = [
    "What is {}?",
    "Explain {} in simple terms.",
    "How does {} work?",
    "What are the key concepts in {}?",
    "Describe the importance of {}.",
    "What are common applications of {}?",
    "How is {} used in real life?",
    "What is the history of {}?",
    "What are the benefits of {}?",
    "How can someone learn about {}?",
]

print("🚀 Generating training data with LLaMA 3...")
print("=" * 60)
print("")

output_file = "llama3_training_data.txt"
lines_generated = 0

with open(output_file, 'w', encoding='utf-8') as f:
    for topic in topics:
        print(f"📚 Generating content for: {topic}")

        # Generate explanations
        for i, template in enumerate(question_templates):
            question = template.format(topic)

            print(f"  Q{i+1}: {question[:50]}...")

            # Ask LLaMA 3
            answer = ask_llama3(question)

            if answer and len(answer) > 20:
                # Write Q&A pair
                f.write(f"Question: {question}\n")
                f.write(f"Answer: {answer}\n")
                f.write("\n")
                lines_generated += 3

                # Also write answer alone for general learning
                f.write(f"{answer}\n")
                f.write("\n")
                lines_generated += 2

                print(f"    ✅ Generated ({len(answer)} chars)")
            else:
                print(f"    ⚠️  Skipped (empty response)")

            # Small delay to avoid overwhelming
            time.sleep(0.5)

        print("")

print("=" * 60)
print(f"✅ Complete! Generated {lines_generated:,} lines")
print(f"📄 Saved to: {output_file}")
print("")
print("Next: Train your model on this LLaMA 3-generated data!")
