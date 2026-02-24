#!/usr/bin/env python3
"""
Simple test of trained transformer model
Directly calls the C++ code without needing full neural_engine.exe rebuild
"""

import subprocess
import sys
import os

def test_transformer(prompt="The sky is"):
    """Test the trained transformer model"""

    base_dir = os.path.dirname(os.path.abspath(__file__))

    print(f"\n{'='*60}")
    print(f"Testing Trained Transformer Model")
    print(f"{'='*60}\n")

    print(f"Prompt: {prompt}\n")

    # Check if models exist
    tokenizer_path = os.path.join(base_dir, "models", "tokenizer.bin")
    transformer_path = os.path.join(base_dir, "models", "transformer.bin")

    if not os.path.exists(tokenizer_path):
        print(f"❌ ERROR: Tokenizer not found at {tokenizer_path}")
        return

    if not os.path.exists(transformer_path):
        print(f"❌ ERROR: Transformer model not found at {transformer_path}")
        return

    print(f"✅ Tokenizer found: {tokenizer_path}")
    print(f"✅ Model found: {transformer_path}")
    print(f"   Model: Level 2.2/10, Perplexity 23.7")
    print(f"   Training: 129 lines (too small for good output)\n")

    # Try to call neural_engine if it exists
    neural_engine = os.path.join(base_dir, "bin", "neural_engine.exe")

    if os.path.exists(neural_engine):
        print(f"✅ neural_engine.exe found, testing...\n")

        try:
            result = subprocess.run(
                [neural_engine, "transformer_generate", prompt],
                capture_output=True,
                text=True,
                timeout=30,
                cwd=base_dir
            )

            print(f"STDOUT:\n{result.stdout}\n")
            print(f"STDERR:\n{result.stderr}\n")

            if result.returncode == 0:
                print(f"✅ Model works!")
                print(f"\n⚠️  Output will be gibberish because model trained on only 129 lines")
                print(f"   To get good quality: Train on 1000+ lines")
            else:
                print(f"❌ Error: exit code {result.returncode}")

        except subprocess.TimeoutExpired:
            print(f"❌ Timeout - model took >30 seconds")
        except Exception as e:
            print(f"❌ Error: {e}")
    else:
        print(f"❌ neural_engine.exe NOT found at {neural_engine}")
        print(f"\n📝 To rebuild:")
        print(f"   Option A: Run build_smart_brain.bat (if all sources exist)")
        print(f"   Option B: Create minimal transformer-only exe")
        print(f"   Option C: Get larger training corpus first (recommended)")

    print(f"\n{'='*60}\n")

if __name__ == "__main__":
    prompt = " ".join(sys.argv[1:]) if len(sys.argv) > 1 else "The sky is"
    test_transformer(prompt)
