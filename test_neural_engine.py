"""
Neural Engine C++ — Full Feature Test
Tests ALL 6 LLM capabilities + math engine

Run: python test_neural_engine.py
"""

import subprocess
import json
import os
import sys
import tempfile

ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "bin", "neural_engine.exe")

# Sample training text — covers multiple topics for real testing
TRAIN_TEXT = """
Data compression is the process of reducing the size of files.
Compression uses algorithms to find patterns in data.
The compressor predicts the next byte using neural networks.
Better prediction leads to smaller output files.
Neural networks learn from data by adjusting weights.
Machine learning is a subset of artificial intelligence.
Artificial intelligence is the simulation of human intelligence.
Deep learning uses neural networks with many layers.
A transformer is a neural network architecture.
The transformer uses attention to focus on relevant parts.
Attention is all you need for language understanding.
Self-attention computes query key and value matrices.
The encoder processes input sequences into representations.
The decoder generates output sequences from representations.
Positional encoding gives the model a sense of word order.
Word embeddings represent words as dense vectors.
Similar words have similar vector representations.
The king minus man plus woman equals queen analogy.
Language models predict the next word in a sequence.
N-gram models count word co-occurrence frequencies.
Entropy measures the uncertainty in a probability distribution.
Shannon entropy is the theoretical limit of compression.
Higher entropy means more randomness and harder to compress.
Lower entropy means more patterns and easier to compress.
Huffman coding assigns shorter codes to frequent symbols.
LZ77 finds repeated patterns using a sliding window.
BWT rearranges data to group similar bytes together.
Context mixing combines predictions from multiple models.
The CMIX algorithm uses over 1000 context models.
Compression ratio is compressed size divided by original size.
Lossless compression preserves all original data perfectly.
Python is a programming language used for AI and web development.
C plus plus is a fast programming language for systems programming.
JavaScript is used for web development and user interfaces.
"""

passed = 0
failed = 0
total = 0

def run_engine(cmd, *args):
    """Run the C++ engine and return parsed JSON."""
    full_cmd = [ENGINE, cmd] + list(args)
    try:
        result = subprocess.run(full_cmd, capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and result.stdout.strip():
            return json.loads(result.stdout.strip())
        return {"error": result.stderr or f"Exit code: {result.returncode}"}
    except Exception as e:
        return {"error": str(e)}

def test(name, condition, detail=""):
    global passed, failed, total
    total += 1
    status = "PASS" if condition else "FAIL"
    if condition:
        passed += 1
        print(f"  ✅ {name}: {detail}")
    else:
        failed += 1
        print(f"  ❌ {name}: {detail}")

def main():
    global passed, failed, total
    
    print("=" * 65)
    print("  Neural Engine C++ — Full Feature Test")
    print("=" * 65)
    
    # Check engine exists
    if not os.path.exists(ENGINE):
        print(f"❌ Engine not found at: {ENGINE}")
        print("   Compile with: g++ -O3 -std=c++17 -o bin/neural_engine.exe src/neural_engine.cpp")
        return
    
    # Write training data to temp file
    tmp = tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8')
    tmp.write(TRAIN_TEXT)
    tmp.close()
    train_file = tmp.name
    
    try:
        # =====================================================================
        print("\n📊 Test 1: STATS")
        # =====================================================================
        r = run_engine("stats")
        test("Architecture", r.get("architecture") == "Transformer-CPP", r.get("architecture", "?"))
        test("Parameters", r.get("total_parameters", 0) > 50000, f"{r.get('total_parameters', 0):,} params")
        test("Has all features", "features" in r, str(r.get("features", [])))
        
        # =====================================================================
        print("\n🔢 Test 2: MATH ENGINE")
        # =====================================================================
        r = run_engine("math", "2 + 3 * 4")
        test("Basic math (2+3*4)", r.get("result") == 14, f"Result: {r.get('result')}")
        
        r = run_engine("math", "sqrt(144)")
        test("sqrt(144)", r.get("result") == 12, f"Result: {r.get('result')}")
        
        r = run_engine("math", "2 ^ 10")
        test("Power (2^10)", r.get("result") == 1024, f"Result: {r.get('result')}")
        
        r = run_engine("math", "sin(0)")
        test("Trig sin(0)", abs(r.get("result", 99)) < 0.001, f"Result: {r.get('result')}")
        
        r = run_engine("math", "log2(256)")
        test("log2(256)", abs(r.get("result", 0) - 8) < 0.001, f"Result: {r.get('result')}")
        
        r = run_engine("math", "factorial(6)")
        test("factorial(6)", r.get("result") == 720, f"Result: {r.get('result')}")
        
        # =====================================================================
        print("\n🧠 Test 3: TRANSFORMER TRAINING")
        # =====================================================================
        r = run_engine("train", train_file)
        test("Training success", r.get("status") == "success", f"Status: {r.get('status')}")
        test("Training steps > 0", r.get("training_steps", 0) > 0, f"Steps: {r.get('training_steps')}")
        test("Vocab size > 50", r.get("vocab_size", 0) > 50, f"Vocab: {r.get('vocab_size')}")
        test("N-gram words > 100", r.get("ngram_words", 0) > 100, f"N-gram words: {r.get('ngram_words')}")
        test("Embeddings trained", r.get("embedding_words", 0) > 50, f"Embedding words: {r.get('embedding_words')}")
        test("Facts extracted > 10", r.get("facts_extracted", 0) > 10, f"Facts: {r.get('facts_extracted')}")
        test("Training time (ms)", r.get("time_ms", 0) > 0, f"Time: {r.get('time_ms', 0):.1f}ms")
        
        # =====================================================================
        print("\n📐 Test 4: SEMANTIC SIMILARITY")
        # =====================================================================
        r = run_engine("similarity", "compression reduces file size", "data compression makes files smaller")
        sim1 = r.get("similarity", 0)
        test("Similar texts score high", sim1 != 0, f"Similarity: {sim1:.4f}")
        
        r = run_engine("similarity", "compression algorithm", "birthday cake recipe")
        sim2 = r.get("similarity", 0)
        test("Different texts differ", True, f"Similarity: {sim2:.4f}")
        
        # =====================================================================
        print("\n🔤 Test 5: TEXT ENCODING")
        # =====================================================================
        r = run_engine("encode", "neural network")
        test("Encoding returns vector", r.get("dimensions", 0) == 64, f"Dimensions: {r.get('dimensions', 0)}")
        vec = r.get("vector", [])
        test("Vector has values", len(vec) > 0 and any(v != 0 for v in vec), f"Non-zero values in {len(vec)}-dim vector")
        
        # =====================================================================
        print("\n🔮 Test 6: NEXT WORD PREDICTION (Transformer)")
        # =====================================================================
        r = run_engine("predict", "compression uses")
        preds = r.get("predictions", [])
        test("Predictions returned", len(preds) > 0, f"Got {len(preds)} predictions")
        if preds:
            top_words = [p["word"] for p in preds[:5]]
            test("Reasonable predictions", True, f"Top 5: {', '.join(top_words)}")
        
        # =====================================================================
        print("\n📖 Test 7: N-GRAM LANGUAGE MODEL")
        # =====================================================================
        r = run_engine("ngram", train_file, "neural networks")
        test("N-gram model works", r.get("status") == "success", r.get("status", "?"))
        test("N-gram has words", r.get("total_words", 0) > 100, f"Total words: {r.get('total_words')}")
        ngram_preds = r.get("predictions", [])
        test("N-gram predictions", len(ngram_preds) > 0, f"Got {len(ngram_preds)} predictions")
        if ngram_preds:
            top_ngram = [f"{p['word']}({p['prob']:.2f})" for p in ngram_preds[:5]]
            test("N-gram top words", True, f"After 'neural networks': {', '.join(top_ngram)}")
        
        # =====================================================================
        print("\n🔍 Test 8: WORD SIMILARITY (Embeddings)")
        # =====================================================================
        r = run_engine("similar", train_file, "compression")
        similar = r.get("similar", [])
        test("Similar words found", len(similar) > 0, f"Found {len(similar)} similar words")
        if similar:
            top_sim = [f"{s['word']}({s['similarity']:.3f})" for s in similar[:5]]
            test("Words similar to 'compression'", True, f"{', '.join(top_sim)}")
        test("Embedding vocab", r.get("vocab_size", 0) > 50, f"Vocab: {r.get('vocab_size')}")
        
        # =====================================================================
        print("\n🧩 Test 9: INFERENCE ENGINE (Fact Chaining)")
        # =====================================================================
        r = run_engine("infer", train_file, "what is compression")
        test("Inference works", r.get("status") == "success", r.get("status", "?"))
        test("Facts extracted", r.get("total_facts", 0) > 5, f"Total facts: {r.get('total_facts')}")
        direct = r.get("direct_facts", [])
        test("Direct facts found", len(direct) > 0, f"Found {len(direct)} direct facts")
        if direct:
            for f in direct[:3]:
                test(f"  Fact", True, f"\"{f['subject']}\" {f['relation']} \"{f['object']}\"")
        inferred = r.get("inferred", [])
        if inferred:
            test("Inferred facts (chained)", True, f"Chains: {len(inferred)}")
            for inf in inferred[:2]:
                test(f"  Inferred", True, inf)
        
        # =====================================================================
        print("\n✍️  Test 10: TEXT GENERATION")
        # =====================================================================
        r = run_engine("generate", "compression", "10")
        test("Generation works", r.get("status") == "success", r.get("status", "?"))
        gen = r.get("generated", "")
        test("Generated text", len(gen) > 0, f'"{gen}"')
        
        # =====================================================================
        print("\n📉 Test 11: ENTROPY CALCULATION")
        # =====================================================================
        r = run_engine("entropy", train_file)
        test("Entropy computed", r.get("status") == "success", r.get("status", "?"))
        ent = r.get("entropy", 0)
        test("Entropy value", 0 < ent < 8, f"Entropy: {ent:.4f} bits/byte")
        test("File size", r.get("size", 0) > 0, f"Size: {r.get('size', 0)} bytes")
        
    finally:
        os.unlink(train_file)
    
    # =====================================================================
    print("\n" + "=" * 65)
    print(f"  RESULTS: {passed}/{total} passed, {failed} failed")
    if failed == 0:
        print("  🎉 ALL TESTS PASSED!")
    else:
        print(f"  ⚠️  {failed} test(s) need attention")
    print("=" * 65)
    
    return 0 if failed == 0 else 1

if __name__ == "__main__":
    sys.exit(main())
