"""
generate_hard_samples.py - Generate challenging test files for the compressor.

Files generated:
  3d/   mesh_ascii.obj       - ASCII OBJ 3D model (repetitive vertex data)
  3d/   mesh_binary.stl      - Binary STL 3D model (structured binary)
  video/ raw_frames.yuv      - Raw uncompressed video frames (YUV420)
  video/ motion_pattern.yuv  - Repeating motion pattern (very compressible)
  audio/ pcm_audio.raw       - Raw PCM audio (16-bit, 44100 Hz)
  code/  source_code.c       - Generated C source (lots of repeated patterns)
  dna/   genome.txt          - DNA sequence (only 4 symbols: ACGT)
  mixed/ structured_data.bin - Structured binary records (like a DB dump)

Usage:
    python samples/generate_hard_samples.py
"""

import os
import sys
import math
import struct
import random

ROOT    = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SAMPLES = os.path.dirname(os.path.abspath(__file__))

def mkdir(path):
    os.makedirs(path, exist_ok=True)

def write(path, data):
    with open(path, "wb") as f:
        f.write(data)
    kb = len(data) / 1024
    print("  wrote %-45s  %7.1f KB" % (os.path.relpath(path, ROOT), kb))

# -----------------------------------------------------------------------------
# 3D MODEL — ASCII OBJ
# Very compressible: "v 1.234567 2.345678 3.456789" repeated thousands of times
# Real .obj files look exactly like this
# -----------------------------------------------------------------------------

def gen_obj_ascii():
    rng = random.Random(42)
    lines = []
    lines.append("# Generated 3D mesh for compression testing")
    lines.append("# Vertices, normals, UVs, and faces")
    lines.append("mtllib mesh.mtl")
    lines.append("o TestMesh")

    # 2000 vertices — each line is "v x y z"
    verts = []
    for i in range(2000):
        x = rng.uniform(-10.0, 10.0)
        y = rng.uniform(-10.0, 10.0)
        z = rng.uniform(-10.0, 10.0)
        lines.append("v %.6f %.6f %.6f" % (x, y, z))
        verts.append((x, y, z))

    # 2000 UV coordinates
    for i in range(2000):
        u = rng.uniform(0.0, 1.0)
        v = rng.uniform(0.0, 1.0)
        lines.append("vt %.6f %.6f" % (u, v))

    # 2000 normals (mostly pointing up with slight variation)
    for i in range(2000):
        nx = rng.uniform(-0.1, 0.1)
        ny = rng.uniform(0.9, 1.0)
        nz = rng.uniform(-0.1, 0.1)
        mag = math.sqrt(nx*nx + ny*ny + nz*nz)
        lines.append("vn %.6f %.6f %.6f" % (nx/mag, ny/mag, nz/mag))

    lines.append("usemtl Material")
    lines.append("s 1")

    # 1800 triangular faces
    for i in range(1800):
        a = rng.randint(1, 2000)
        b = rng.randint(1, 2000)
        c = rng.randint(1, 2000)
        lines.append("f %d/%d/%d %d/%d/%d %d/%d/%d" % (a,a,a, b,b,b, c,c,c))

    return "\n".join(lines).encode("ascii")


# -----------------------------------------------------------------------------
# 3D MODEL — Binary STL
# Binary format: 80-byte header + 4-byte count + N * 50-byte triangles
# Each triangle: 12-byte normal + 3 * 12-byte vertex + 2-byte attribute
# Structured binary — good test for LZ77 on binary data
# -----------------------------------------------------------------------------

def gen_stl_binary():
    rng = random.Random(42)
    num_triangles = 2000

    out = bytearray()
    # 80-byte header
    header = b"Binary STL generated for compression testing - myzip"
    out.extend(header.ljust(80, b"\x00"))
    # Triangle count
    out.extend(struct.pack("<I", num_triangles))

    for _ in range(num_triangles):
        # Normal vector
        nx, ny, nz = rng.uniform(-1,1), rng.uniform(-1,1), rng.uniform(-1,1)
        mag = math.sqrt(nx*nx+ny*ny+nz*nz) or 1.0
        out.extend(struct.pack("<fff", nx/mag, ny/mag, nz/mag))
        # 3 vertices
        for _ in range(3):
            x = rng.uniform(-100.0, 100.0)
            y = rng.uniform(-100.0, 100.0)
            z = rng.uniform(-100.0, 100.0)
            out.extend(struct.pack("<fff", x, y, z))
        # Attribute (usually 0)
        out.extend(struct.pack("<H", 0))

    return bytes(out)


# -----------------------------------------------------------------------------
# RAW VIDEO — YUV420 frames
# 320x240 @ 10 frames — uncompressed video
# Frame N is a slow pan: background + moving square
# Very compressible between frames (temporal redundancy)
# -----------------------------------------------------------------------------

def gen_yuv_video():
    W, H    = 320, 240
    FRAMES  = 20
    out     = bytearray()

    for f in range(FRAMES):
        # Y plane (luma) — grey gradient background
        y_plane = bytearray(W * H)
        for row in range(H):
            for col in range(W):
                # slow horizontal gradient that shifts slightly each frame
                y_plane[row * W + col] = (col + f * 3) % 200 + 28

        # Moving white square (80x80), position shifts each frame
        sq_x = (f * 12) % (W - 80)
        sq_y = (f * 8)  % (H - 80)
        for row in range(sq_y, sq_y + 80):
            for col in range(sq_x, sq_x + 80):
                y_plane[row * W + col] = 235  # near-white luma

        out.extend(y_plane)

        # U and V planes (chroma, half resolution each)
        chroma_w, chroma_h = W // 2, H // 2
        u_plane = bytes([128] * (chroma_w * chroma_h))  # neutral chroma
        v_plane = bytes([128] * (chroma_w * chroma_h))
        out.extend(u_plane)
        out.extend(v_plane)

    return bytes(out)


# -----------------------------------------------------------------------------
# RAW AUDIO — PCM 16-bit mono 44100 Hz
# 5 seconds of a 440 Hz sine wave with slight noise
# -----------------------------------------------------------------------------

def gen_pcm_audio():
    SAMPLE_RATE = 44100
    DURATION    = 3       # seconds
    FREQUENCY   = 440.0  # Hz (A4 note)
    rng         = random.Random(42)

    samples = []
    for i in range(SAMPLE_RATE * DURATION):
        t    = i / SAMPLE_RATE
        # Sine wave + harmonics (like a real instrument)
        val  = math.sin(2 * math.pi * FREQUENCY * t) * 0.6
        val += math.sin(2 * math.pi * FREQUENCY * 2 * t) * 0.2
        val += math.sin(2 * math.pi * FREQUENCY * 3 * t) * 0.1
        # Small noise
        val += rng.uniform(-0.02, 0.02)
        # Clamp and convert to int16
        val = max(-1.0, min(1.0, val))
        samples.append(int(val * 32767))

    return struct.pack("<%dh" % len(samples), *samples)


# -----------------------------------------------------------------------------
# C SOURCE CODE — generated with lots of structural repetition
# Function signatures, variable declarations, loops — typical real code patterns
# -----------------------------------------------------------------------------

def gen_c_source():
    rng   = random.Random(42)
    lines = []
    types = ["int", "float", "double", "uint8_t", "uint32_t", "size_t"]
    ops   = ["+=", "-=", "*=", "/=", "="]

    lines.append("#include <stdio.h>")
    lines.append("#include <stdlib.h>")
    lines.append("#include <string.h>")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("#define MAX_SIZE 1024")
    lines.append("#define BUFFER_SIZE 4096")
    lines.append("")

    for fn_idx in range(40):
        ret  = rng.choice(types)
        name = "process_data_%03d" % fn_idx
        arg1 = rng.choice(types)
        arg2 = rng.choice(types)
        lines.append("%s %s(%s input, %s count) {" % (ret, name, arg1, arg2))
        lines.append("    %s result = 0;" % ret)
        lines.append("    %s temp = 0;" % rng.choice(types))
        lines.append("    int i, j;")
        lines.append("")
        lines.append("    for (i = 0; i < count; i++) {")
        lines.append("        for (j = 0; j < MAX_SIZE; j++) {")
        lines.append("            temp = input * j;")
        lines.append("            result %s temp;" % rng.choice(ops))
        lines.append("            if (result > BUFFER_SIZE) {")
        lines.append("                result = result %% BUFFER_SIZE;" )
        lines.append("            }")
        lines.append("        }")
        lines.append("    }")
        lines.append("")
        lines.append("    return result;")
        lines.append("}")
        lines.append("")

    lines.append("int main(int argc, char *argv[]) {")
    lines.append("    printf(\"myzip compression test\\n\");")
    for fn_idx in range(40):
        lines.append("    process_data_%03d(argc, 10);" % fn_idx)
    lines.append("    return 0;")
    lines.append("}")

    return "\n".join(lines).encode("ascii")


# -----------------------------------------------------------------------------
# DNA SEQUENCE
# Only 4 symbols (A, C, G, T) — entropy is only 2 bits/byte
# Compressors that handle this well can get very high ratios
# Real genomes also have lots of repeated subsequences
# -----------------------------------------------------------------------------

def gen_dna():
    rng   = random.Random(42)
    bases = b"ACGT"
    # Realistic: mostly random but with some repeated motifs
    motifs = [
        b"ATCGATCGATCG",
        b"GCGCGCGCGCGC",
        b"AAATTTGGGCCC",
        b"TATAATATATAA",
        b"GCTAGCTAGCTA",
    ]

    chunks = []
    total  = 0
    target = 200 * 1024  # 200 KB

    while total < target:
        if rng.random() < 0.3:
            # insert a repeated motif
            m = rng.choice(motifs) * rng.randint(5, 30)
            chunks.append(m)
            total += len(m)
        else:
            # random bases
            n = rng.randint(50, 200)
            chunks.append(bytes(rng.choices(bases, k=n)))
            total += n

    return b"".join(chunks)[:target]


# -----------------------------------------------------------------------------
# STRUCTURED BINARY — like a database dump
# Fixed-size records: ID(4) + timestamp(8) + value(8) + flags(1) + name(32)
# Record size = 53 bytes, thousands of records
# Very regular structure — good test for LZ77 on binary patterns
# -----------------------------------------------------------------------------

def gen_structured_binary():
    rng     = random.Random(42)
    RECORD  = 53   # bytes per record
    COUNT   = 3000
    out     = bytearray()

    # File header
    out.extend(b"MYDB")           # magic
    out.extend(struct.pack("<I", COUNT))
    out.extend(struct.pack("<I", RECORD))
    out.extend(b"\x00" * 20)     # reserved

    names = [
        b"temperature", b"pressure", b"humidity",
        b"voltage",     b"current",  b"power",
        b"speed",       b"position", b"status",
    ]

    for i in range(COUNT):
        record_id = i + 1
        timestamp = 1700000000 + i * 60        # one record per minute
        value     = rng.uniform(0.0, 1000.0)
        flags     = rng.randint(0, 7)
        name      = rng.choice(names)

        out.extend(struct.pack("<I",  record_id))
        out.extend(struct.pack("<Q",  timestamp))
        out.extend(struct.pack("<d",  value))
        out.extend(struct.pack("<B",  flags))
        name_field = name[:31].ljust(32, b"\x00")
        out.extend(name_field)

    return bytes(out)


# -----------------------------------------------------------------------------
# MAIN
# -----------------------------------------------------------------------------

def main():
    print()
    print("Generating hard sample files...")
    print()

    dirs = {
        "3d":    os.path.join(SAMPLES, "3d"),
        "video": os.path.join(SAMPLES, "video"),
        "audio": os.path.join(SAMPLES, "audio"),
        "code":  os.path.join(SAMPLES, "code"),
        "dna":   os.path.join(SAMPLES, "dna"),
        "mixed": os.path.join(SAMPLES, "mixed"),
    }
    for d in dirs.values():
        mkdir(d)

    write(os.path.join(dirs["3d"],    "mesh_ascii.obj"),       gen_obj_ascii())
    write(os.path.join(dirs["3d"],    "mesh_binary.stl"),      gen_stl_binary())
    write(os.path.join(dirs["video"], "raw_frames.yuv"),       gen_yuv_video())
    write(os.path.join(dirs["audio"], "pcm_audio.raw"),        gen_pcm_audio())
    write(os.path.join(dirs["code"],  "source_code.c"),        gen_c_source())
    write(os.path.join(dirs["dna"],   "genome.txt"),           gen_dna())
    write(os.path.join(dirs["mixed"], "structured_data.bin"),  gen_structured_binary())

    print()
    print("Done.")

if __name__ == "__main__":
    main()
