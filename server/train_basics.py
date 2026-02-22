"""
Neural Brain - Initial Training Script
Gives the brain basic understanding before the user starts interacting.

Run once: python train_basics.py
"""

import os
import sys
import time

# Add parent dir so we can import neural_brain
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from neural_brain import store_knowledge, ensure_brain, brain_stats

def train():
    ensure_brain()
    print("\n  Neural Brain - Basic Training")
    print("  =============================\n")

    lessons = [
        # ─── About Itself ───
        ("neural studio", """
Neural Studio is a high-performance compression application built with a C++ compression engine and a React/Electron desktop frontend.
It uses a Python FastAPI server to bridge the frontend and the compression engine (myzip.exe).
The system has 1,046 neural network advisors that work together using context mixing to achieve world-class compression ratios.
The AI assistant is the Neural Brain - our own custom-built intelligence that learns, compresses knowledge, and retrieves it to answer questions.
All knowledge is stored compressed using the CMIX neural compression engine, so the brain takes up minimal disk space.
The Neural Brain does not use any external LLM or API - it is entirely self-contained and learns from user interactions and web content.
        """),

        ("compression algorithms", """
There are four main compression algorithms in Neural Studio:
1. Default (LZ77+delta) - Fast general-purpose compression using sliding window dictionary matching.
2. BWT (--best flag) - Burrows-Wheeler Transform followed by Move-to-Front, Run-Length Encoding, and rANS coding. Good balance of speed and ratio for text.
3. PPM (--ultra flag) - Prediction by Partial Matching with order-4 context and arithmetic coding. Slow but excellent ratio.
4. CMIX (--cmix flag) - Neural Network Context Mixing with 1,046 advisors using gradient descent. Slowest but world-class compression ratio.
Each algorithm has strengths: LZ77 is fast, BWT is balanced, PPM is thorough, CMIX is the most intelligent.
        """),

        ("context mixing", """
Context Mixing is the most advanced compression technique. It works by running many prediction models simultaneously, each looking at different patterns in the data.
Each model (called an advisor) makes a prediction about what the next byte or bit will be.
A neural network then combines all these predictions using weighted averaging, where the weights are adjusted by gradient descent - the same technique used in deep learning.
The V10 engine has 1,046 advisors organized into groups: order-0 through order-7 byte context models, bit-level models, word boundary models, and skip/long-range pattern models.
The key insight: compression IS prediction. If you can predict the next byte perfectly, you can compress it to zero bits. The better the prediction, the better the compression.
This is why compression and AI are fundamentally the same thing - both are about learning patterns in data.
        """),

        ("shannon entropy", """
Shannon Entropy measures information density in bits per byte (bpb). It ranges from 0 to 8:
- 0 bpb: Perfectly predictable data (e.g., all zeros). Can be compressed to almost nothing.
- 1-3 bpb: Low entropy, highly compressible. Common in natural language text, source code, and structured data.
- 3-5 bpb: Moderate entropy. Mixed data with some patterns. Decent compression possible.
- 5-7 bpb: High entropy. Dense data with few patterns. Limited compression possible.
- 8 bpb: Maximum entropy, pure random data. Cannot be compressed at all (Shannon's theorem).
Already-compressed files (ZIP, JPEG, MP3) have entropy near 8 bpb because the patterns have already been removed.
Encrypting data also produces near-maximum entropy since encryption is designed to remove all patterns.
Shannon proved in 1948 that no lossless compression algorithm can beat the entropy limit.
        """),

        ("file types", """
Different file types have different compression characteristics:
- Text files (.txt, .md, .csv, .log): Usually 1-4 bpb entropy. Highly compressible because natural language has strong patterns (word frequencies, grammar, common phrases).
- Source code (.py, .js, .cpp, .ts): Usually 2-4 bpb. Compressible due to keywords, indentation patterns, and syntactic repetition.
- Data files (.json, .xml, .html): Usually 2-5 bpb. Structured formats with repeated tags and keys compress well.
- Binary executables (.exe, .dll): Usually 5-7 bpb. Some patterns in code sections but data sections are dense.
- Images (.png, .jpg): Already compressed, 7-8 bpb. Re-compression makes them larger.
- Archives (.zip, .gz, .7z): Already compressed, near 8 bpb. Never re-compress an archive.
- Audio/Video (.mp3, .mp4): Already compressed with lossy algorithms. Near maximum entropy.
The CMIX engine excels on text and code because the neural advisors learn linguistic patterns.
        """),

        ("how computers work", """
Computers process data as binary - sequences of 0s and 1s called bits. Eight bits make a byte.
A byte can represent 256 different values (0-255), which is enough for one character of text (ASCII/UTF-8).
Files are sequences of bytes stored on disk. When we compress a file, we find patterns in these bytes and represent them more efficiently.
CPUs execute instructions sequentially at billions of operations per second. RAM provides fast temporary storage. Disk provides permanent storage.
Programming languages like C++ compile to native machine code for maximum performance - this is why our compression engine (myzip.exe) is written in C++.
Python is used for the server because it is flexible and has excellent libraries for web APIs (FastAPI).
TypeScript/React is used for the frontend because it provides a rich interactive user interface.
Electron wraps the web frontend into a desktop application with access to the local file system.
        """),

        ("neural networks", """
Neural networks are computing systems inspired by biological brains. They consist of layers of interconnected nodes (neurons).
Each connection has a weight that determines how much influence one neuron has on another.
Learning happens through gradient descent: the network makes a prediction, measures the error, and adjusts weights to reduce the error.
Over many iterations, the network learns to recognize patterns in data.
In our CMIX compression engine, each of the 1,046 advisors is essentially a simple neural network that learns patterns in the data being compressed.
The mixing network combines all advisor predictions using weighted averaging with gradient descent.
During decompression, the exact same neural network is rebuilt from scratch - it learns the same patterns in the same order (mirror mode).
This is why CMIX compression is lossless: the decompressor creates an identical copy of the compressor's neural network.
Key concepts: weights, biases, activation functions, loss functions, backpropagation, learning rate.
        """),

        ("command line usage", """
The myzip.exe compression tool supports these commands:
Compress: myzip compress input.txt -o output.myzip --cmix
Decompress: myzip decompress archive.myzip -o recovered.txt
Benchmark: myzip benchmark input.txt
Flags: --cmix (neural), --best (BWT), --ultra (PPM), default (LZ77)
The -o flag sets the output path. Without it, compression adds .myzip extension and decompression adds .recovered.
The -v flag enables verbose output showing internal processing details.
The benchmark command compresses and decompresses a file and reports speed and ratio statistics.
        """),

        ("data structures", """
Key data structures used in compression:
- Hash Table: O(1) lookup for LZ77 dictionary matching. Maps byte sequences to positions.
- Suffix Array: Sorted array of all suffixes, used in BWT for efficient transformation.
- Binary Tree: Used in Huffman coding to build optimal prefix-free codes.
- Context Table: Maps byte contexts (previous N bytes) to probability distributions. Core of PPM and context mixing.
- Ring Buffer: Circular buffer for sliding window in LZ77. Efficient memory usage.
- Priority Queue: Used in Huffman tree construction to always merge lowest-frequency nodes.
- Trie: Prefix tree for fast context lookup in PPM models.
Understanding these structures helps understand why different algorithms have different speed/ratio tradeoffs.
        """),

        ("mathematics of compression", """
Key mathematical concepts in compression:
- Information Theory: Founded by Claude Shannon in 1948. Defines entropy as the lower bound on compression.
- Probability: Compression models predict byte probabilities. Better predictions = better compression.
- Logarithm: Entropy formula uses log2. The number of bits needed to encode an event with probability p is -log2(p).
- Arithmetic Coding: Encodes entire message as a single number between 0 and 1. Near-optimal encoding.
- ANS (Asymmetric Numeral Systems): Modern alternative to arithmetic coding. Used in our rANS implementation.
- Gradient Descent: The neural network weight update rule. Moves weights in the direction that reduces prediction error.
- Cross-Entropy Loss: Measures how well the model's predictions match reality. Minimized during training.
- Bayes Theorem: Updates probabilities based on new evidence. Foundation of adaptive compression.
        """),
    ]

    for i, (topic, content) in enumerate(lessons):
        content = content.strip()
        print(f"  [{i+1}/{len(lessons)}] Learning: {topic}...")
        result = store_knowledge(topic, content, source="basic_training")
        savings = result.get("savings_pct", 0)
        tokens = result.get("token_count", 0)
        print(f"           {tokens} tokens, {savings}% compressed")

    print("\n  Training complete!\n")
    stats = brain_stats()
    print(f"  Knowledge items: {stats['total_knowledge_items']}")
    print(f"  Vocabulary size: {stats['vocabulary_size']} words")
    print(f"  Raw size: {stats['total_raw_human']}")
    print(f"  Compressed: {stats['total_compressed_human']}")
    print(f"  Savings: {stats['compression_savings_pct']}%")
    print()

if __name__ == "__main__":
    train()
