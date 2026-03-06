#!/usr/bin/env python3
"""
Continuous Learning Pipeline:
1. Generate data with LLaMA 3
2. Train your model
3. Test your model
4. Identify weak areas
5. Generate more data for weak areas
6. Repeat!
"""

import subprocess
import os
import time

def run_ollama(prompt):
    """Query LLaMA 3"""
    try:
        result = subprocess.run(
            ['ollama', 'run', 'llama3', prompt],
            capture_output=True,
            text=True,
            timeout=60
        )
        return result.stdout.strip()
    except:
        return ""

def generate_dataset(topic, num_examples=50):
    """Generate training examples for a specific topic"""
    print(f"\n🎯 Generating {num_examples} examples for: {topic}")

    filename = f"training_{topic.replace(' ', '_')}.txt"
    count = 0

    with open(filename, 'w', encoding='utf-8') as f:
        prompts = [
            f"Explain {topic} in detail",
            f"What are the key concepts in {topic}",
            f"How does {topic} work",
            f"Give examples of {topic}",
            f"What are common misconceptions about {topic}",
            f"How is {topic} applied in practice",
            f"What is the history of {topic}",
            f"Why is {topic} important",
            f"How can someone learn {topic}",
            f"What are advanced concepts in {topic}",
        ]

        for i in range(num_examples):
            prompt = prompts[i % len(prompts)]

            print(f"  Example {i+1}/{num_examples}...", end="")

            response = run_ollama(prompt)

            if response and len(response) > 30:
                f.write(f"{prompt}\n")
                f.write(f"{response}\n")
                f.write("\n")
                count += 1
                print(f" ✅ ({len(response)} chars)")
            else:
                print(" ⚠️  skipped")

            time.sleep(0.2)

    print(f"✅ Created {filename} with {count} examples")
    return filename

# Topics to focus on
priority_topics = [
    "artificial intelligence",
    "machine learning algorithms",
    "deep neural networks",
    "natural language processing",
    "computer vision",
    "reinforcement learning",
    "transformer architectures",
    "optimization techniques",
    "data structures",
    "algorithm design",
]

print("=" * 70)
print("🚀 Continuous Learning with LLaMA 3 Teacher")
print("=" * 70)

# Generate datasets for each topic
generated_files = []

for topic in priority_topics[:5]:  # Start with first 5 topics
    filename = generate_dataset(topic, num_examples=20)
    generated_files.append(filename)

# Combine all generated data
print("\n📦 Combining all generated datasets...")

combined_file = "llama3_continuous_learning.txt"
total_lines = 0

with open(combined_file, 'w', encoding='utf-8') as outfile:
    for fname in generated_files:
        if os.path.exists(fname):
            with open(fname, 'r', encoding='utf-8') as infile:
                content = infile.read()
                outfile.write(content)
                total_lines += content.count('\n')

print(f"✅ Combined dataset: {combined_file}")
print(f"📊 Total lines: {total_lines:,}")

print("\n" + "=" * 70)
print("🎓 Knowledge Distillation Complete!")
print("=" * 70)
print("")
print("📚 Generated training data from LLaMA 3 teacher")
print(f"📄 Output file: {combined_file}")
print(f"📊 Total lines: {total_lines:,}")
print("")
print("🚀 Next step: Train your model on this data!")
print("")
print(f"Command:")
print(f"  ./bin/train_llm.exe --corpus {combined_file} --epochs 25 --batch 8 --output model_llama3_distilled.bin")
print("")
print("💡 Your smaller model will learn from LLaMA 3's knowledge!")
