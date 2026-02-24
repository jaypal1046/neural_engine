import os
import subprocess
import hashlib
import time
import sys
import random

# Configuration
EXE_PATH = os.path.abspath(os.path.join(os.path.dirname(__file__), "../src/myzip.exe"))
TEST_DIR = os.path.dirname(os.path.abspath(__file__))

def get_sha256(path):
    """Compute SHA-256 of a file."""
    sha = hashlib.sha256()
    with open(path, 'rb') as f:
        while True:
            chunk = f.read(1024 * 1024)
            if not chunk: break
            sha.update(chunk)
    return sha.hexdigest()

def run_command(args):
    """Run a shell command and return (returncode, stdout, stderr)."""
    try:
        r = subprocess.run(args, capture_output=True, text=True)
        return r.returncode, r.stdout, r.stderr
    except FileNotFoundError:
        return -1, "", "Executable not found"

def generate_test_file(path, size_mb, type='text'):
    """Generate a synthetic test file."""
    print(f"Generating {size_mb}MB {type} file: {os.path.basename(path)}...")
    with open(path, 'wb') as f:
        if type == 'text':
            words = [b"latency", b"throughput", b"server", b"error", b"debug", b"info", b"warning", b"critical"]
            written = 0
            while written < size_mb * 1024 * 1024:
                line = b" ".join(random.choices(words, k=10)) + b"\n"
                f.write(line)
                written += len(line)
        elif type == 'binary':
            f.write(os.urandom(size_mb * 1024 * 1024))
        elif type == 'zeros':
            f.write(b'\x00' * (size_mb * 1024 * 1024))

def test_round_trip(input_path, mode_flag=None, mode_name="Default"):
    """Compress -> Decompress -> Verify SHA-256."""
    input_base = os.path.basename(input_path)
    output_path = input_path + ".aiz"
    recovered_path = input_path + ".recovered"

    # Cleanup previous run
    if os.path.exists(output_path): os.remove(output_path)
    if os.path.exists(recovered_path): os.remove(recovered_path)

    # 1. Compress
    cmd = [EXE_PATH, "compress", input_path]
    if mode_flag: cmd.append(mode_flag)
    
    start = time.time()
    rc, out, err = run_command(cmd)
    dur = time.time() - start
    
    if rc != 0:
        print(f"  [FAIL] {mode_name} Compress: {err.strip()}")
        return False

    if not os.path.exists(output_path):
        # Maybe output wasn't named .aiz? Attempt to guess or rely on default
        # But our tool defaults to .aiz
        print(f"  [FAIL] {mode_name} Output file not created.")
        return False
        
    comp_size = os.path.getsize(output_path)
    orig_size = os.path.getsize(input_path)
    ratio = comp_size / orig_size if orig_size > 0 else 1.0

    # 2. Decompress
    cmd_d = [EXE_PATH, "decompress", output_path]
    start_d = time.time()
    rc_d, out_d, err_d = run_command(cmd_d)
    dur_d = time.time() - start_d

    if rc_d != 0:
        print(f"  [FAIL] {mode_name} Decompress: {err_d.strip()}")
        return False

    # 3. Verify
    if not os.path.exists(recovered_path):
        print(f"  [FAIL] {mode_name} Recovered file missing.")
        return False

    sha_orig = get_sha256(input_path)
    sha_rec = get_sha256(recovered_path)

    if sha_orig != sha_rec:
        print(f"  [FAIL] {mode_name} SHA-256 MISMATCH!")
        print(f"    Orig: {sha_orig}")
        print(f"    Rec : {sha_rec}")
        return False

    print(f"  [PASS] {mode_name:<8} | Ratio: {ratio:.3f} | Comp: {dur:.2f}s | Decomp: {dur_d:.2f}s")
    
    # Cleanup success
    os.remove(output_path)
    os.remove(recovered_path)
    return True

def main():
    print("=== myzip Regression Test Suite ===")
    if not os.path.exists(EXE_PATH):
        print(f"Error: myzip.exe not found at {EXE_PATH}")
        print("Please build it first.")
        sys.exit(1)

    # Ensure clean state
    samples = []
    
    # 1. Small Text (Log structure)
    f1 = os.path.join(TEST_DIR, "test_log_1mb.txt")
    generate_test_file(f1, 1, 'text')
    samples.append(f1)

    # 2. Binary (High entropy)
    f2 = os.path.join(TEST_DIR, "test_random_1mb.bin")
    generate_test_file(f2, 1, 'binary')
    samples.append(f2)

    # 3. Zeros (Highly compressible)
    f3 = os.path.join(TEST_DIR, "test_zeros_1mb.bin")
    generate_test_file(f3, 1, 'zeros')
    samples.append(f3)

    print("\nStarting Tests...")
    all_pass = True

    for f in samples:
        print(f"\nFile: {os.path.basename(f)}")
        # v7 Default
        if not test_round_trip(f, None, "v7-Def"): all_pass = False
        # v8 Best
        if not test_round_trip(f, "--best", "v8-Best"): all_pass = False
        # v9 Ultra
        if not test_round_trip(f, "--ultra", "v9-Ultra"): all_pass = False

    print("\n" + "="*40)
    if all_pass:
        print("✅ ALL TESTS PASSED")
    else:
        print("❌ SOME TESTS FAILED")
    
    # Cleanup samples
    for f in samples:
        if os.path.exists(f): os.remove(f)

if __name__ == "__main__":
    main()
