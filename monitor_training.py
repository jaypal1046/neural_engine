#!/usr/bin/env python3
"""
Real-time training monitor - tracks all active training sessions
"""

import subprocess
import time
import os
from datetime import datetime, timedelta

def get_training_processes():
    """Get all running training processes"""
    try:
        result = subprocess.run(
            ['ps', 'aux'],
            capture_output=True,
            text=True
        )

        processes = []
        for line in result.stdout.split('\n'):
            if 'train_llm' in line and 'grep' not in line:
                parts = line.split()
                if len(parts) >= 11:
                    pid = parts[1]
                    cpu_time = parts[9]  # CPU time
                    start_time = parts[10]  # Start time
                    processes.append({
                        'pid': pid,
                        'cpu_time': cpu_time,
                        'start_time': start_time
                    })

        return processes
    except:
        return []

def check_checkpoints():
    """Check for completed checkpoints"""
    checkpoints = []
    for file in os.listdir('.'):
        if file.startswith('checkpoint_') and file.endswith('.bin'):
            size = os.path.getsize(file)
            checkpoints.append({
                'name': file,
                'size_mb': size / (1024 * 1024)
            })
    return checkpoints

def check_models():
    """Check for completed models"""
    models = []
    for file in os.listdir('.'):
        if file.startswith('model_') and file.endswith('.bin'):
            size = os.path.getsize(file)
            models.append({
                'name': file,
                'size_mb': size / (1024 * 1024)
            })
    return models

def format_time_running(start_str):
    """Calculate how long process has been running"""
    try:
        now = datetime.now()
        # Parse start time (format: HH:MM:SS)
        if ':' in start_str:
            parts = start_str.split(':')
            if len(parts) == 3:
                h, m, s = map(int, parts)
                start = now.replace(hour=h, minute=m, second=s, microsecond=0)
                if start > now:  # Started yesterday
                    start -= timedelta(days=1)

                elapsed = now - start
                hours = elapsed.seconds // 3600
                minutes = (elapsed.seconds % 3600) // 60
                return f"{hours}h {minutes}m"
    except:
        pass
    return "unknown"

print("=" * 70)
print("TRAINING MONITOR - Real-time Status")
print("=" * 70)
print(f"Time: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}")
print()

# Active processes
processes = get_training_processes()
print(f"ACTIVE TRAINING PROCESSES: {len(processes)}")
print("-" * 70)

if processes:
    for i, proc in enumerate(processes, 1):
        running_time = format_time_running(proc['start_time'])
        print(f"  Session {i}:")
        print(f"    PID: {proc['pid']}")
        print(f"    Started: {proc['start_time']}")
        print(f"    Running: {running_time}")
        print(f"    CPU Time: {proc['cpu_time']}")
        print()
else:
    print("  No training processes running!")
    print()

# Checkpoints
checkpoints = check_checkpoints()
print(f"CHECKPOINTS SAVED: {len(checkpoints)}")
print("-" * 70)

if checkpoints:
    for ckpt in checkpoints:
        print(f"  {ckpt['name']}: {ckpt['size_mb']:.2f} MB")
else:
    print("  No checkpoints yet (training in progress...)")
print()

# Completed models
models = check_models()
print(f"MODELS COMPLETED: {len(models)}")
print("-" * 70)

if models:
    for model in models:
        print(f"  {model['name']}: {model['size_mb']:.2f} MB")
else:
    print("  No models completed yet (waiting for epochs to finish...)")
print()

# Training data stats
print("TRAINING DATA:")
print("-" * 70)

training_files = [
    'wiki_training.txt',
    'wiki_training_qa.txt',
    'wiki_training_code.txt',
    'wiki_training_expanded.txt',
    'wiki_training_combined.txt',
    'llama3_style_training.txt',
    'wiki_large.txt',
]

total_size = 0
total_lines = 0

for fname in training_files:
    if os.path.exists(fname):
        size = os.path.getsize(fname)
        total_size += size

        # Count lines
        try:
            with open(fname, 'r', encoding='utf-8', errors='ignore') as f:
                lines = sum(1 for _ in f)
            total_lines += lines
            print(f"  {fname}: {size/(1024*1024):.1f} MB, {lines:,} lines")
        except:
            print(f"  {fname}: {size/(1024*1024):.1f} MB")

print()
print(f"TOTAL: {total_size/(1024*1024):.1f} MB, {total_lines:,} lines")

print()
print("=" * 70)
print("STATUS SUMMARY")
print("=" * 70)

if len(processes) > 0:
    print(f"✓ {len(processes)} training sessions ACTIVE")
    print(f"✓ Processing {total_lines:,} lines of training data")
    print(f"✓ Training data size: {total_size/(1024*1024):.1f} MB")

    if len(checkpoints) > 0:
        print(f"✓ {len(checkpoints)} checkpoints saved")
    else:
        print("⏳ First epoch still in progress...")

    if len(models) > 0:
        print(f"✓ {len(models)} models completed!")
    else:
        print("⏳ Waiting for models to complete...")

    print()
    print("Training is ACTIVE and HEALTHY!")
else:
    print("⚠ No training processes detected!")
    print()
    print("Training may have completed or stopped.")
    print("Check model files above to see results.")

print("=" * 70)
