#!/usr/bin/env python3
"""
Create advanced training data inspired by LLaMA 3's knowledge
Even without direct API calls, we can create high-quality training data
"""

# High-quality training examples (LLaMA 3 style responses)
training_examples = {
    "AI & ML": [
        ("What is artificial intelligence?",
         "Artificial Intelligence (AI) refers to computer systems designed to perform tasks that typically require human intelligence. These tasks include learning, reasoning, problem-solving, perception, and language understanding. AI can be categorized into narrow AI (designed for specific tasks) and general AI (theoretical systems with human-like cognitive abilities). Modern AI primarily uses machine learning techniques, where systems learn from data rather than being explicitly programmed."),

        ("Explain machine learning",
         "Machine learning is a subset of AI that enables systems to automatically learn and improve from experience without being explicitly programmed. It works by training algorithms on data, identifying patterns, and making predictions or decisions. There are three main types: supervised learning (learning from labeled data), unsupervised learning (finding patterns in unlabeled data), and reinforcement learning (learning through trial and error with rewards)."),

        ("How do neural networks work?",
         "Neural networks are computing systems inspired by biological neural networks in the brain. They consist of interconnected nodes (neurons) organized in layers: input layer, hidden layers, and output layer. Each connection has a weight that adjusts during training. Information flows forward through the network, and learning occurs through backpropagation, where errors are propagated backward to adjust weights. Deep neural networks with multiple hidden layers enable deep learning, capable of learning hierarchical representations of data."),

        ("What are transformers in AI?",
         "Transformers are a revolutionary neural network architecture introduced in 2017 that revolutionized natural language processing. Unlike previous sequential models, transformers process entire sequences simultaneously using self-attention mechanisms. This allows them to capture long-range dependencies and relationships between words regardless of distance. Key components include multi-head attention, positional encoding, and feedforward networks. Transformers power modern language models like GPT, BERT, and LLaMA."),

        ("Explain deep learning",
         "Deep learning is a branch of machine learning that uses artificial neural networks with multiple layers (deep neural networks) to progressively extract higher-level features from raw input. Each layer learns to transform its input into a slightly more abstract representation. For example, in image recognition, early layers might detect edges, middle layers detect shapes, and deeper layers recognize specific objects. Deep learning has achieved remarkable success in computer vision, natural language processing, speech recognition, and many other domains."),
    ],

    "Programming": [
        ("What is an algorithm?",
         "An algorithm is a step-by-step procedure or formula for solving a problem or performing a task. It's a finite sequence of well-defined instructions that, when executed, produces a result or achieves a specific goal. Algorithms are fundamental to computer science and programming. Good algorithms are efficient, correct, and clearly defined. They can be expressed in pseudocode, flowcharts, or programming languages. Algorithm analysis involves studying their time and space complexity."),

        ("Explain data structures",
         "Data structures are specialized formats for organizing, storing, and managing data in computers. They define the relationship between data elements and the operations that can be performed on them. Common data structures include arrays (contiguous memory storage), linked lists (connected nodes), stacks (LIFO), queues (FIFO), trees (hierarchical), graphs (networks), and hash tables (key-value mapping). Choosing the right data structure significantly impacts program efficiency and performance."),

        ("What is object-oriented programming?",
         "Object-Oriented Programming (OOP) is a programming paradigm based on the concept of objects, which contain data (attributes) and code (methods). Key principles include encapsulation (bundling data and methods), inheritance (creating new classes from existing ones), polymorphism (objects taking multiple forms), and abstraction (hiding complex implementation details). OOP promotes code reusability, modularity, and maintainability. Popular OOP languages include Python, Java, C++, and C#."),
    ],

    "Science": [
        ("How does photosynthesis work?",
         "Photosynthesis is the process by which plants, algae, and some bacteria convert light energy into chemical energy stored in glucose. It occurs primarily in chloroplasts and involves two main stages: light-dependent reactions and light-independent reactions (Calvin cycle). In light reactions, chlorophyll absorbs light energy, splitting water molecules to release oxygen and generate ATP and NADPH. The Calvin cycle uses these energy carriers to fix carbon dioxide into glucose. The overall equation is: 6CO₂ + 6H₂O + light energy → C₆H₁₂O₆ + 6O₂."),

        ("What causes earthquakes?",
         "Earthquakes are caused by the sudden release of energy in Earth's crust, creating seismic waves. Most earthquakes occur along tectonic plate boundaries where plates interact. The main causes include: plate collision (convergent boundaries), plate separation (divergent boundaries), and plates sliding past each other (transform boundaries). As plates move, stress builds up in rocks. When stress exceeds rock strength, it fractures, releasing energy as seismic waves. The focus is where rupture begins; the epicenter is the surface point directly above."),
    ],
}

print("🎓 Creating Advanced Training Data (LLaMA 3 Style)")
print("=" * 70)

output_file = "llama3_style_training.txt"
total_lines = 0

with open(output_file, 'w', encoding='utf-8') as f:
    for category, examples in training_examples.items():
        print(f"\n📚 Category: {category}")
        print(f"   Examples: {len(examples)}")

        for question, answer in examples:
            # Format 1: Q&A
            f.write(f"Question: {question}\n")
            f.write(f"Answer: {answer}\n")
            f.write("\n")
            total_lines += 3

            # Format 2: Direct statement
            f.write(f"{answer}\n")
            f.write("\n")
            total_lines += 2

            # Format 3: Instruction
            f.write(f"User: {question}\n")
            f.write(f"Assistant: {answer}\n")
            f.write("\n")
            total_lines += 3

            # Format 4: Explain format
            f.write(f"Explain: {question}\n")
            f.write(f"Explanation: {answer}\n")
            f.write("\n")
            total_lines += 3

print("\n" + "=" * 70)
print(f"✅ Created {output_file}")
print(f"📊 Total lines: {total_lines:,}")
print("")
print("🚀 This data contains high-quality, detailed explanations!")
print("💡 Your model will learn from expert-level responses!")
print("")
print("Next: Train on this data!")
print(f"  ./bin/train_llm.exe --corpus {output_file} --epochs 25 --batch 8")
