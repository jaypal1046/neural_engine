#!/usr/bin/env python3
"""
Knowledge Distillation: Use LLaMA 3 as a teacher to generate training data
This creates high-quality examples for your smaller model to learn from
"""

import subprocess
import json
import random

def ask_llama3(prompt, max_tokens=500):
    """Query LLaMA 3 via Ollama API"""
    try:
        # Use ollama API for better control
        cmd = ['ollama', 'run', 'llama3', prompt]
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
        return result.stdout.strip()
    except Exception as e:
        print(f"  Error: {e}")
        return ""

print("🎓 Knowledge Distillation: LLaMA 3 → Your Model")
print("=" * 70)
print("")

# Knowledge areas for distillation
knowledge_areas = {
    "Technical Skills": [
        "Explain what a neural network is and how it learns",
        "What is backpropagation in machine learning",
        "How do transformers work in AI",
        "Explain gradient descent optimization",
        "What is the difference between AI, ML, and deep learning",
    ],
    "Science": [
        "Explain photosynthesis in detail",
        "What causes earthquakes",
        "How does DNA replication work",
        "Explain the theory of relativity",
        "What is quantum mechanics",
    ],
    "Problem Solving": [
        "How do you approach debugging complex code",
        "What are effective problem-solving strategies",
        "Explain algorithmic thinking",
        "How do you optimize performance in software",
        "What is computational complexity",
    ],
    "General Knowledge": [
        "What are the major events of World War II",
        "Explain the water cycle",
        "What is democracy and how does it work",
        "Describe the solar system",
        "What is climate change and its causes",
    ],
    "Reasoning": [
        "If you have a 3-gallon and 5-gallon jug, how do you measure exactly 4 gallons",
        "What comes next in this sequence: 2, 6, 12, 20, 30",
        "How would you reverse a string without using built-in functions",
        "What is the best way to learn a new skill",
        "How do you prioritize tasks effectively",
    ],
}

output_file = "llama3_distilled_knowledge.txt"
total_lines = 0

print("📝 Generating distilled knowledge from LLaMA 3...")
print("")

with open(output_file, 'w', encoding='utf-8') as f:
    for category, questions in knowledge_areas.items():
        print(f"📚 Category: {category}")
        print("-" * 70)

        for q in questions:
            print(f"  Q: {q[:60]}...")

            # Get LLaMA 3's answer
            answer = ask_llama3(q)

            if answer and len(answer) > 30:
                # Write multiple formats for better learning

                # Format 1: Q&A
                f.write(f"Question: {q}\n")
                f.write(f"Answer: {answer}\n")
                f.write("\n")
                total_lines += 3

                # Format 2: Direct statement
                f.write(f"{answer}\n")
                f.write("\n")
                total_lines += 2

                # Format 3: Instruction format
                f.write(f"User: {q}\n")
                f.write(f"Assistant: {answer}\n")
                f.write("\n")
                total_lines += 3

                print(f"    ✅ Distilled ({len(answer)} chars, {len(answer.split())} words)")
            else:
                print(f"    ⚠️  Skipped (no response)")

            # Brief pause
            import time
            time.sleep(0.3)

        print("")

print("=" * 70)
print(f"✅ Distillation Complete!")
print(f"📊 Generated {total_lines:,} lines of training data")
print(f"📄 Output: {output_file}")
print("")
print("🎯 This data contains LLaMA 3's knowledge!")
print("🚀 Your smaller model will learn from the teacher!")
print("")
print("Next step:")
print(f"  ./bin/train_llm.exe --corpus {output_file} --epochs 20 --batch 8")
