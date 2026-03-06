#!/usr/bin/env python3
"""
Download and prepare sample training data for AIZip Brain LLM

This script downloads a small Wikipedia sample for testing the training system.
For production, use full Wikipedia dumps.
"""

import os
import sys
import urllib.request
import re

def download_sample_data():
    """Download a small sample text corpus"""
    print("╔══════════════════════════════════════════════════════════════╗")
    print("║         Download Training Data for AIZip Brain LLM          ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    print()

    output_file = "wiki_clean.txt"

    print(f"Creating sample corpus: {output_file}")
    print()

    # Create sample training data from common knowledge
    sample_texts = [
        "Artificial intelligence is the simulation of human intelligence by machines.",
        "Machine learning is a subset of artificial intelligence that enables systems to learn from data.",
        "Deep learning uses neural networks with multiple layers to learn complex patterns.",
        "Natural language processing helps computers understand and generate human language.",
        "Transformers are a type of neural network architecture used for language tasks.",
        "The attention mechanism allows models to focus on relevant parts of the input.",
        "Training neural networks requires large amounts of data and computational power.",
        "Gradient descent is an optimization algorithm used to minimize loss functions.",
        "Backpropagation computes gradients of the loss with respect to model parameters.",
        "Neural networks consist of interconnected nodes organized in layers.",
        "Computers are electronic devices that process information.",
        "Programming languages allow humans to communicate instructions to computers.",
        "Python is a popular programming language for data science and machine learning.",
        "Algorithms are step-by-step procedures for solving problems.",
        "Data structures organize and store data efficiently.",
        "The internet connects billions of devices worldwide.",
        "Web browsers allow users to access information on the internet.",
        "Operating systems manage computer hardware and software resources.",
        "Databases store and organize large amounts of structured data.",
        "Cloud computing provides on-demand access to computing resources.",
        "Mathematics is the study of numbers, quantities, and shapes.",
        "Physics explores the fundamental laws of nature.",
        "Chemistry studies the properties and interactions of matter.",
        "Biology examines living organisms and life processes.",
        "History records and interprets past human events.",
        "Geography studies the Earth's physical features and human societies.",
        "Literature encompasses written works of artistic value.",
        "Music is the art of organizing sounds in time.",
        "Art creates visual works that express ideas and emotions.",
        "Science uses systematic methods to understand the natural world.",
        "The scientific method involves observation, hypothesis, and experimentation.",
        "Technology applies scientific knowledge for practical purposes.",
        "Engineering designs and builds systems and structures.",
        "Medicine treats diseases and maintains health.",
        "Education transmits knowledge and skills to learners.",
        "Communication is the exchange of information between individuals.",
        "Language is a system of symbols used for communication.",
        "Writing records language in visual form.",
        "Reading interprets written text to extract meaning.",
        "Mathematics helps solve problems using logical reasoning.",
        "Numbers represent quantities and enable calculation.",
        "Addition combines numbers to find their sum.",
        "Subtraction finds the difference between numbers.",
        "Multiplication repeats addition of the same number.",
        "Division splits a number into equal parts.",
        "Fractions represent parts of a whole.",
        "Decimals use place value to represent fractional quantities.",
        "Percentages express proportions out of one hundred.",
        "Geometry studies shapes, sizes, and spatial relationships.",
        "Algebra uses symbols to represent unknown values.",
        "The universe contains all matter, energy, space, and time.",
        "Stars are massive balls of plasma that emit light and heat.",
        "Planets orbit stars and can support different conditions.",
        "The solar system includes the Sun and objects orbiting it.",
        "Earth is the third planet from the Sun.",
        "The Moon orbits Earth and affects tides.",
        "Gravity is a force that attracts objects with mass.",
        "Energy exists in many forms including heat, light, and motion.",
        "Matter is anything that has mass and occupies space.",
        "Atoms are the basic building blocks of matter.",
        "Elements are pure substances made of one type of atom.",
        "Molecules form when atoms bond together.",
        "Chemical reactions transform substances into new products.",
        "Water is essential for life and covers most of Earth.",
        "The atmosphere surrounds Earth and contains the air we breathe.",
        "Weather describes atmospheric conditions at a specific time.",
        "Climate refers to long-term weather patterns.",
        "Ecosystems include living organisms and their environment.",
        "Evolution explains how species change over time.",
        "DNA carries genetic information in living cells.",
        "Cells are the basic units of life.",
        "Photosynthesis converts light energy into chemical energy.",
        "Respiration releases energy from food molecules.",
        "The human brain processes information and controls the body.",
        "Memory stores and retrieves information.",
        "Learning acquires new knowledge or skills.",
        "Thinking involves mental processing of information.",
        "Emotions are complex psychological states.",
        "Behavior is how organisms respond to their environment.",
        "Society is a group of individuals living together.",
        "Culture includes beliefs, customs, and practices of a group.",
        "Government organizes and regulates society.",
        "Economics studies production, distribution, and consumption.",
        "Money facilitates exchange of goods and services.",
        "Trade involves buying and selling between parties.",
        "Transportation moves people and goods from place to place.",
        "Agriculture produces food through farming.",
        "Industry manufactures goods on a large scale.",
        "Energy powers human activities and technology.",
        "Resources are materials used to meet human needs.",
        "The environment includes all living and non-living things.",
        "Conservation protects natural resources and ecosystems.",
        "Pollution harms the environment with contaminants.",
        "Sustainability meets present needs without compromising the future.",
        "Time measures the duration and sequence of events.",
        "Space is the three-dimensional extent where objects exist.",
        "Motion describes changes in position over time.",
        "Speed measures how fast an object moves.",
        "Velocity includes both speed and direction.",
        "Acceleration is the rate of change of velocity.",
    ]

    # Write to file with proper formatting
    with open(output_file, 'w', encoding='utf-8') as f:
        for i, text in enumerate(sample_texts):
            # Add line number for tracking
            f.write(f"{text}\n")

            # Add variations for better training
            if i % 10 == 0:
                f.write(f"This is related to: {text.lower()}\n")
                f.write(f"In other words, {text[0].lower() + text[1:]}\n")

    print(f"✓ Created sample corpus: {output_file}")
    print(f"  Lines: {len(sample_texts) * 3}")
    print(f"  Size: {os.path.getsize(output_file) / 1024:.1f} KB")
    print()

    print("═══════════════════════════════════════════════════════════════")
    print("Next Steps:")
    print("═══════════════════════════════════════════════════════════════")
    print()
    print("1. Build the training system:")
    print("   build_train_llm.bat")
    print()
    print("2. Start training:")
    print("   bin\\train_llm.exe --corpus wiki_clean.txt --epochs 5 --batch 4")
    print()
    print("3. For production, download full Wikipedia:")
    print("   https://dumps.wikimedia.org/enwiki/latest/")
    print()
    print("Note: This is a SMALL sample for testing!")
    print("      Real training needs 100MB - 10GB+ of text data.")
    print()

if __name__ == "__main__":
    try:
        download_sample_data()
    except Exception as e:
        print(f"ERROR: {e}")
        sys.exit(1)
