"""
Sample File Downloader + Generator
====================================
Downloads real public-domain files AND generates large synthetic samples
for every file type, so we have meaningful compression test data.

Real downloads (fallback to synthetic if offline):
  text/   - Pride and Prejudice (~700 KB)  from Project Gutenberg
  text/   - King James Bible (~4 MB)       from Project Gutenberg
  image/  - NASA public domain TIFF (raw, uncompressed)

Synthetic (always generated, realistic statistical properties):
  text/   - server log file  (~300 KB)
  json/   - API response dump (~400 KB)
  csv/    - sales database    (~350 KB)
  image/  - BMP gradient      (~2 MB, 24-bit uncompressed)
  binary/ - mixed ELF-like    (~200 KB)
  binary/ - already-compressed (~100 KB, near-random — worst case)

Usage:
    python samples/download_samples.py
"""

import os
import sys
import math
import json
import struct
import random
import urllib.request
import urllib.error

SAMPLES_DIR = os.path.dirname(os.path.abspath(__file__))

def download(url: str, dest: str, label: str) -> bool:
    if os.path.exists(dest) and os.path.getsize(dest) > 1000:
        print(f"  [SKIP]  {label}  (already exists)")
        return True
    try:
        print(f"  [DOWN]  {label}  <- {url}")
        req = urllib.request.Request(url, headers={"User-Agent": "Mozilla/5.0"})
        with urllib.request.urlopen(req, timeout=15) as resp, open(dest, "wb") as f:
            f.write(resp.read())
        size = os.path.getsize(dest)
        print(f"          Saved {size:,} bytes -> {os.path.basename(dest)}")
        return True
    except Exception as e:
        print(f"  [FAIL]  {label}  ({e})  — will use synthetic instead")
        return False


def generate_text_log(path: str, size_kb: int = 500):
    """Realistic server log file — highly repetitive structure, variable content."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    random.seed(10)
    levels   = ["INFO", "INFO", "INFO", "WARN", "ERROR", "DEBUG"]
    services = ["api-gateway", "auth-service", "db-pool", "cache", "scheduler"]
    actions  = [
        "Request received: GET /api/v2/users/{uid}",
        "Request received: POST /api/v2/orders",
        "Request received: GET /api/v2/products?page={pg}",
        "Cache HIT  key=session:{uid}  ttl={ttl}s",
        "Cache MISS key=product:{uid}  fetching from DB",
        "DB query OK  table=users  rows={rows}  time={ms}ms",
        "DB query SLOW  table=orders  rows={rows}  time={ms}ms  (threshold=100ms)",
        "Auth token validated  user={uid}  scope=read",
        "Rate limit check  ip=192.168.{a}.{b}  count={cnt}/60s",
        "Response sent  status={code}  size={sz}B  time={ms}ms",
    ]
    lines = []
    for i in range(size_kb * 8):
        lvl  = levels[i % len(levels)]
        svc  = services[i % len(services)]
        act  = actions[i % len(actions)].format(
            uid=random.randint(1000, 9999), pg=random.randint(1,50),
            ttl=random.randint(60,3600), rows=random.randint(1,500),
            ms=random.randint(1,2000), a=random.randint(1,254),
            b=random.randint(1,254), cnt=random.randint(1,60),
            code=random.choice([200,200,200,201,400,404,500]),
            sz=random.randint(100,50000)
        )
        ts = f"2026-02-19T10:{(i//3600)%60:02d}:{(i//60)%60:02d}.{i%1000:03d}Z"
        lines.append(f"{ts}  [{lvl:<5}]  {svc:<15}  {act}")
    with open(path, "w", encoding="utf-8") as f:
        f.write("\n".join(lines))
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


def generate_json(path: str, records: int = 2000):
    """API-style JSON dump — structured, repetitive keys, variable values."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    random.seed(20)
    statuses  = ["active", "inactive", "pending", "suspended"]
    plans     = ["free", "basic", "pro", "enterprise"]
    countries = ["US", "IN", "GB", "DE", "FR", "JP", "CA", "AU"]
    data = {"meta": {"total": records, "page": 1, "generated": "2026-02-19"}, "users": []}
    for i in range(records):
        data["users"].append({
            "id": 100000 + i,
            "email": f"user{i}@example-{i%20}.com",
            "name": {"first": f"First{i%50}", "last": f"Last{i%100}"},
            "plan": plans[i % len(plans)],
            "status": statuses[i % len(statuses)],
            "country": countries[i % len(countries)],
            "score": round(random.uniform(0.0, 100.0), 4),
            "logins": random.randint(0, 5000),
            "created": f"2024-{(i%12)+1:02d}-{(i%28)+1:02d}",
            "tags": [f"tag_{j}" for j in range(i % 5)],
        })
    with open(path, "w", encoding="utf-8") as f:
        json.dump(data, f, indent=2)
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


def generate_csv(path: str, rows: int = 5000):
    """Sales transaction CSV — structured columns, repetitive values."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    random.seed(30)
    products = ["Widget-Alpha","Widget-Beta","Gadget-X1","Gadget-X2","Doohickey-Pro",
                "Thingamajig","Gizmo-3000","Contraption-A","Device-Z","Module-7"]
    regions  = ["North-America","South-America","Europe","Asia-Pacific","Middle-East","Africa"]
    channels = ["online","retail","wholesale","partner","direct"]
    with open(path, "w", encoding="utf-8") as f:
        f.write("txn_id,date,product,region,channel,units,unit_price,revenue,discount_pct,tax\n")
        for i in range(rows):
            date      = f"2025-{(i%12)+1:02d}-{(i%28)+1:02d}"
            product   = products[i % len(products)]
            region    = regions[i % len(regions)]
            channel   = channels[i % len(channels)]
            units     = random.randint(1, 1000)
            price     = round(random.uniform(4.99, 999.99), 2)
            disc      = random.choice([0, 5, 10, 15, 20, 25])
            revenue   = round(units * price * (1 - disc/100), 2)
            tax       = round(revenue * 0.18, 2)
            f.write(f"TXN{1000000+i},{date},{product},{region},{channel},{units},{price},{revenue},{disc},{tax}\n")
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


def generate_bmp(path: str, width: int = 800, height: int = 600):
    """24-bit uncompressed BMP — best case for compression (visual structure)."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    row_size         = ((width * 3 + 3) & ~3)
    pixel_data_size  = row_size * height
    file_size        = 54 + pixel_data_size
    bmp = bytearray()
    # File header (14 bytes)
    bmp += b'BM'
    bmp += struct.pack('<I', file_size)
    bmp += struct.pack('<HH', 0, 0)
    bmp += struct.pack('<I', 54)
    # DIB header (40 bytes)
    bmp += struct.pack('<I', 40)
    bmp += struct.pack('<ii', width, -height)
    bmp += struct.pack('<H', 1)
    bmp += struct.pack('<H', 24)
    bmp += struct.pack('<I', 0)
    bmp += struct.pack('<I', pixel_data_size)
    bmp += struct.pack('<ii', 2835, 2835)
    bmp += struct.pack('<II', 0, 0)
    # Pixel data — photo-realistic with regions
    for y in range(height):
        row = bytearray()
        for x in range(width):
            # Sky gradient (top), ground (bottom), bands
            if y < height // 3:
                r = int(100 + (y / (height//3)) * 50)
                g = int(150 + (y / (height//3)) * 50)
                b = int(200 + (y / (height//3)) * 55)
            elif y < height * 2 // 3:
                r = int(80 + (x / width) * 60)
                g = int(120 + math.sin(x * 0.05) * 30)
                b = int(60 + (y / height) * 40)
            else:
                stripe = (x // 40 + y // 40) % 4
                r = [80, 60, 100, 70][stripe]
                g = [60, 80, 50, 90][stripe]
                b = [40, 50, 60, 30][stripe]
            row += bytes([min(255, max(0, r)), min(255, max(0, g)), min(255, max(0, b))])
        row += b'\x00' * (row_size - width * 3)
        bmp += row
    with open(path, "wb") as f:
        f.write(bmp)
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


def generate_binary_mixed(path: str):
    """Mixed binary — like a compiled executable: headers + code + data."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    random.seed(40)
    buf = bytearray()
    # ELF-like header (null-padded — very compressible)
    buf += b'\x7fELF\x02\x01\x01\x00' + b'\x00' * 8
    buf += struct.pack('<HHI', 2, 62, 1)
    buf += b'\x00' * 500
    # String table section (repetitive text strings)
    strings = [b".text\x00", b".data\x00", b".bss\x00", b".rodata\x00",
               b"main\x00", b"printf\x00", b"malloc\x00", b"free\x00"]
    for _ in range(300):
        buf += random.choice(strings)
    buf += b'\x00' * 200
    # Code section — pseudo-random x86-like opcodes (low entropy)
    opcodes = [0x55, 0x48, 0x89, 0xe5, 0x48, 0x83, 0xec, 0x20,
               0x48, 0x8d, 0x05, 0x00, 0x5d, 0xc3, 0x90, 0x66]
    for _ in range(10000):
        buf += bytes([opcodes[random.randint(0, len(opcodes)-1)]])
    # Data section (structured records)
    for i in range(1000):
        buf += struct.pack('<IIffHH',
            i, i * 7 % 256,
            float(i) * 1.5, float(i) * 0.001,
            i % 65535, i % 255)
    with open(path, "wb") as f:
        f.write(buf)
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


def generate_already_compressed(path: str, size_kb: int = 150):
    """Simulate already-compressed data (near-random bytes) — worst case for any compressor."""
    if os.path.exists(path) and os.path.getsize(path) > 1000:
        print(f"  [SKIP]  {os.path.basename(path)}"); return
    import zlib
    # Build up compressed chunks until we reach size_kb
    buf = bytearray()
    chunk = b"compressed_content_repetitive_data_" * 5000
    while len(buf) < size_kb * 1024:
        buf.extend(zlib.compress(chunk, level=9))
    with open(path, "wb") as f:
        f.write(bytes(buf[:size_kb * 1024]))
    print(f"  [GEN]   {os.path.basename(path)}  ({os.path.getsize(path)//1024} KB)")


if __name__ == "__main__":
    print("\n" + "=" * 60)
    print("  SAMPLE FILE SETUP")
    print("  Downloading real files + generating synthetic samples")
    print("=" * 60)

    # ── Real downloads ────────────────────────────────────────────────────────
    print("\n  --- Real downloads (public domain) ---")
    download(
        "https://www.gutenberg.org/files/1342/1342-0.txt",
        os.path.join(SAMPLES_DIR, "text", "pride_and_prejudice.txt"),
        "Pride and Prejudice (~780 KB, text)"
    )
    download(
        "https://www.gutenberg.org/files/10/10-0.txt",
        os.path.join(SAMPLES_DIR, "text", "king_james_bible.txt"),
        "King James Bible (~4 MB, text)"
    )
    download(
        "https://www.gutenberg.org/files/84/84-0.txt",
        os.path.join(SAMPLES_DIR, "text", "frankenstein.txt"),
        "Frankenstein (~430 KB, text)"
    )

    # ── Synthetic generation ──────────────────────────────────────────────────
    print("\n  --- Generating synthetic samples ---")
    generate_text_log(os.path.join(SAMPLES_DIR, "text", "server_log_500kb.txt"), size_kb=500)
    generate_json(os.path.join(SAMPLES_DIR,     "json", "api_users_2k.json"),   records=2000)
    generate_csv(os.path.join(SAMPLES_DIR,      "csv",  "sales_5k.csv"),        rows=5000)
    generate_bmp(os.path.join(SAMPLES_DIR,      "image","photo_800x600.bmp"),   800, 600)
    generate_binary_mixed(os.path.join(SAMPLES_DIR, "binary", "program_mixed.bin"))
    generate_already_compressed(os.path.join(SAMPLES_DIR, "binary", "already_compressed.bin"))

    # ── Summary ───────────────────────────────────────────────────────────────
    print("\n  --- Sample Files Ready ---")
    total_size = 0
    for root, dirs, files in os.walk(SAMPLES_DIR):
        for name in files:
            if name == os.path.basename(__file__):
                continue
            fp   = os.path.join(root, name)
            size = os.path.getsize(fp)
            rel  = os.path.relpath(fp, SAMPLES_DIR)
            total_size += size
            print(f"  {rel:<45}  {size//1024:>6} KB")
    print(f"\n  Total: {total_size//1024} KB  ({total_size//1024//1024} MB)")
    print("=" * 60 + "\n")
