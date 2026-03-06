#!/usr/bin/env python3
"""
Create additional specialized training corpora for diverse AI training
"""

import random

# Conversational AI training data
conversations = [
    ("Hello!", "Hello! How can I help you today?"),
    ("What is AI?", "AI stands for Artificial Intelligence, which refers to computer systems designed to perform tasks that typically require human intelligence."),
    ("How does machine learning work?", "Machine learning works by training algorithms on data, allowing them to learn patterns and make predictions without being explicitly programmed for every scenario."),
    ("What are neural networks?", "Neural networks are computing systems inspired by biological neural networks in the brain, consisting of interconnected nodes that process information."),
    ("Explain deep learning", "Deep learning is a subset of machine learning that uses multi-layered neural networks to learn hierarchical representations of data."),
    ("What is natural language processing?", "Natural language processing is a field of AI that focuses on enabling computers to understand, interpret, and generate human language."),
    ("How do transformers work?", "Transformers are neural network architectures that use attention mechanisms to process sequential data efficiently, particularly effective for language tasks."),
    ("What is supervised learning?", "Supervised learning is a machine learning approach where models learn from labeled training data to make predictions on new, unseen data."),
    ("Explain reinforcement learning", "Reinforcement learning is a type of machine learning where agents learn to make decisions by receiving rewards or penalties for their actions."),
    ("What are embeddings?", "Embeddings are dense vector representations of discrete objects like words or images, capturing semantic relationships in a continuous space."),
]

# Question-Answer pairs for knowledge
qa_pairs = [
    ("What is the speed of light?", "The speed of light in vacuum is approximately 299,792,458 meters per second, often denoted as c."),
    ("Who discovered gravity?", "Isaac Newton formulated the law of universal gravitation, though the concept was understood by earlier scientists."),
    ("What is photosynthesis?", "Photosynthesis is the process by which plants convert light energy into chemical energy, producing oxygen and glucose from carbon dioxide and water."),
    ("When did World War II end?", "World War II ended in 1945, with Germany surrendering in May and Japan in August after atomic bombs were dropped."),
    ("What is DNA?", "DNA, or deoxyribonucleic acid, is the molecule that carries genetic instructions for life, consisting of a double helix structure."),
    ("Who painted the Mona Lisa?", "Leonardo da Vinci painted the Mona Lisa during the Renaissance period, likely between 1503 and 1519."),
    ("What is quantum mechanics?", "Quantum mechanics is the branch of physics that describes the behavior of matter and energy at atomic and subatomic scales."),
    ("When was the internet invented?", "The internet evolved from ARPANET in the 1960s, with the modern World Wide Web created by Tim Berners-Lee in 1989."),
    ("What causes earthquakes?", "Earthquakes are caused by the sudden release of energy in Earth's crust, typically from the movement of tectonic plates."),
    ("What is climate change?", "Climate change refers to long-term shifts in global temperatures and weather patterns, primarily driven by human greenhouse gas emissions."),
]

# Instruction-following data
instructions = [
    ("Summarize this text in one sentence", "I can help summarize text into concise single sentences that capture the main ideas."),
    ("Translate to simple terms", "I can explain complex concepts using simple, easy-to-understand language."),
    ("List the key points", "I can extract and organize the main points from any given text or topic."),
    ("Explain step by step", "I can break down processes into clear, sequential steps that are easy to follow."),
    ("Compare and contrast", "I can analyze similarities and differences between concepts, ideas, or objects."),
    ("Provide examples", "I can give concrete examples to illustrate abstract concepts and make them clearer."),
    ("Define this term", "I can provide clear, accurate definitions for technical terms and concepts."),
    ("Analyze this problem", "I can examine problems systematically to identify causes, effects, and potential solutions."),
    ("Generate ideas", "I can brainstorm creative ideas and suggestions for various topics and challenges."),
    ("Correct this text", "I can identify and fix errors in grammar, spelling, and clarity."),
]

# Create comprehensive training file
print("Creating specialized training corpora...")

output_file = "wiki_training_qa.txt"
lines_written = 0

with open(output_file, 'w', encoding='utf-8') as f:
    # Add conversations (repeated with variations)
    for i in range(50):  # 50 repetitions
        for q, a in conversations:
            f.write(f"Question: {q}\n")
            f.write(f"Answer: {a}\n")
            f.write("\n")
            lines_written += 3

            # Variations
            f.write(f"User: {q}\n")
            f.write(f"Assistant: {a}\n")
            f.write("\n")
            lines_written += 3

            f.write(f"Q: {q}\n")
            f.write(f"A: {a}\n")
            f.write("\n")
            lines_written += 3

    # Add Q&A pairs
    for i in range(50):
        for q, a in qa_pairs:
            f.write(f"{q}\n")
            f.write(f"{a}\n")
            f.write("\n")
            lines_written += 3

            # With labels
            f.write(f"Question: {q}\n")
            f.write(f"Answer: {a}\n")
            f.write("\n")
            lines_written += 3

    # Add instruction data
    for i in range(50):
        for inst, resp in instructions:
            f.write(f"Instruction: {inst}\n")
            f.write(f"Response: {resp}\n")
            f.write("\n")
            lines_written += 3

            # Variations
            f.write(f"Task: {inst}\n")
            f.write(f"Output: {resp}\n")
            f.write("\n")
            lines_written += 3

print(f"Created {output_file}")
print(f"Total lines: {lines_written:,}")
print("")

# Create a coding-focused corpus
coding_examples = [
    "Python is a high-level programming language known for its readability and versatility.",
    "Functions in programming are reusable blocks of code that perform specific tasks.",
    "Variables store data values that can be referenced and manipulated in a program.",
    "Loops allow code to be executed repeatedly based on specified conditions.",
    "Conditional statements enable programs to make decisions based on different scenarios.",
    "Object-oriented programming organizes code into objects that contain data and methods.",
    "Algorithms are step-by-step procedures for solving problems or performing tasks.",
    "Data structures organize and store data efficiently for different types of operations.",
    "APIs (Application Programming Interfaces) allow different software systems to communicate.",
    "Debugging is the process of finding and fixing errors in computer programs.",
]

output_file2 = "wiki_training_code.txt"
lines_written2 = 0

with open(output_file2, 'w', encoding='utf-8') as f:
    for i in range(100):  # 100 repetitions
        for example in coding_examples:
            f.write(example + "\n")
            lines_written2 += 1

            # Add variations
            f.write(f"Explanation: {example}\n")
            lines_written2 += 1

            f.write(f"Concept: {example}\n")
            lines_written2 += 1

print(f"Created {output_file2}")
print(f"Total lines: {lines_written2:,}")
print("")
print("All specialized corpora created!")
print("")
print("Ready for training!")
