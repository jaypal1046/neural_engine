#!/usr/bin/env python3
"""
Download Wikipedia dumps for LLM training

Downloads different sizes of Wikipedia based on your needs:
- Small (Simple English): ~100 MB, good for testing
- Medium (English chunk): ~1 GB, good for basic training
- Large (Full English): ~20 GB, production quality
"""

import os
import sys
import urllib.request
import argparse

WIKIPEDIA_DUMPS = {
    'small': {
        'name': 'Simple English Wikipedia',
        'url': 'https://dumps.wikimedia.org/simplewiki/latest/simplewiki-latest-pages-articles.xml.bz2',
        'size': '~100 MB compressed',
        'expanded': '~400 MB',
        'articles': '~200K',
        'training_time': '2-4 hours',
        'quality': 'Good for testing'
    },
    'medium': {
        'name': 'English Wikipedia (chunk 1)',
        'url': 'https://dumps.wikimedia.org/enwiki/latest/enwiki-latest-pages-articles1.xml-p1p41242.bz2',
        'size': '~1 GB compressed',
        'expanded': '~4 GB',
        'articles': '~500K',
        'training_time': '12-24 hours',
        'quality': 'Good quality'
    },
    'large': {
        'name': 'Full English Wikipedia',
        'url': 'https://dumps.wikimedia.org/enwiki/latest/enwiki-latest-pages-articles.xml.bz2',
        'size': '~20 GB compressed',
        'expanded': '~80 GB',
        'articles': '~6M',
        'training_time': '2-3 days',
        'quality': 'Excellent quality'
    }
}

def download_file(url, output_path):
    """Download file with progress bar"""

    def progress_hook(block_num, block_size, total_size):
        downloaded = block_num * block_size
        if total_size > 0:
            percent = min(downloaded * 100 / total_size, 100)
            mb_downloaded = downloaded / (1024 * 1024)
            mb_total = total_size / (1024 * 1024)
            print(f'\rDownloading: {percent:.1f}% ({mb_downloaded:.1f}/{mb_total:.1f} MB)', end='')
        else:
            mb_downloaded = downloaded / (1024 * 1024)
            print(f'\rDownloaded: {mb_downloaded:.1f} MB', end='')

    print(f"Downloading from: {url}")
    print(f"Saving to: {output_path}\n")

    try:
        urllib.request.urlretrieve(url, output_path, reporthook=progress_hook)
        print("\n\n✓ Download complete!")
        return True
    except KeyboardInterrupt:
        print("\n\nDownload interrupted by user!")
        if os.path.exists(output_path):
            os.remove(output_path)
        return False
    except Exception as e:
        print(f"\n\nError: {e}")
        return False

def extract_bz2(input_path, output_path):
    """Extract bz2 compressed file"""
    import bz2
    import shutil

    print(f"\nExtracting: {input_path}")
    print(f"Output: {output_path}\n")

    try:
        with bz2.open(input_path, 'rb') as f_in:
            with open(output_path, 'wb') as f_out:
                file_size = os.path.getsize(input_path)
                bytes_read = 0

                chunk_size = 1024 * 1024  # 1 MB chunks
                while True:
                    chunk = f_in.read(chunk_size)
                    if not chunk:
                        break

                    f_out.write(chunk)
                    bytes_read += len(chunk)

                    # Progress
                    mb_read = bytes_read / (1024 * 1024)
                    print(f'\rExtracted: {mb_read:.1f} MB', end='')

        print("\n\n✓ Extraction complete!")
        return True
    except KeyboardInterrupt:
        print("\n\nExtraction interrupted!")
        if os.path.exists(output_path):
            os.remove(output_path)
        return False
    except Exception as e:
        print(f"\n\nError: {e}")
        return False

def main():
    parser = argparse.ArgumentParser(
        description='Download Wikipedia dumps for LLM training'
    )
    parser.add_argument('size', nargs='?', choices=['small', 'medium', 'large'],
                       help='Wikipedia dump size: small/medium/large')
    parser.add_argument('--output', '-o', help='Output directory (default: current)')
    parser.add_argument('--no-extract', action='store_true',
                       help='Do not extract after download')

    args = parser.parse_args()

    # Show available options if no size specified
    if not args.size:
        print("╔══════════════════════════════════════════════════════════════╗")
        print("║         Wikipedia Download for LLM Training                 ║")
        print("╚══════════════════════════════════════════════════════════════╝")
        print("\nAvailable Wikipedia dumps:\n")

        for size, info in WIKIPEDIA_DUMPS.items():
            print(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
            print(f"Size: {size.upper()}")
            print(f"━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━")
            print(f"  Name:          {info['name']}")
            print(f"  Download size: {info['size']}")
            print(f"  Expanded size: {info['expanded']}")
            print(f"  Articles:      {info['articles']}")
            print(f"  Training time: {info['training_time']}")
            print(f"  Quality:       {info['quality']}")
            print()

        print("Usage:")
        print("  python download_wikipedia.py small     # Recommended for testing")
        print("  python download_wikipedia.py medium    # Good for basic training")
        print("  python download_wikipedia.py large     # Best quality")
        print()
        return

    # Get dump info
    dump = WIKIPEDIA_DUMPS[args.size]
    output_dir = args.output or '.'

    print("╔══════════════════════════════════════════════════════════════╗")
    print("║         Wikipedia Download for LLM Training                 ║")
    print("╚══════════════════════════════════════════════════════════════╝")
    print()
    print(f"Selected: {dump['name']}")
    print(f"Size: {dump['size']} (compressed)")
    print(f"Articles: {dump['articles']}")
    print(f"Expected training time: {dump['training_time']}")
    print()

    # Confirm
    response = input("Continue with download? (y/n): ")
    if response.lower() != 'y':
        print("Download cancelled.")
        return

    # Download
    filename = os.path.basename(dump['url'])
    output_path = os.path.join(output_dir, filename)

    if os.path.exists(output_path):
        print(f"\nFile already exists: {output_path}")
        response = input("Re-download? (y/n): ")
        if response.lower() != 'y':
            print("Skipping download.")
        else:
            if not download_file(dump['url'], output_path):
                return
    else:
        if not download_file(dump['url'], output_path):
            return

    # Extract
    if not args.no_extract:
        xml_path = output_path.replace('.bz2', '')

        if os.path.exists(xml_path):
            print(f"\nExtracted file already exists: {xml_path}")
            response = input("Re-extract? (y/n): ")
            if response.lower() != 'y':
                print("Skipping extraction.")
            else:
                if not extract_bz2(output_path, xml_path):
                    return
        else:
            if not extract_bz2(output_path, xml_path):
                return

        # Next steps
        print("\n" + "="*60)
        print("Next Steps:")
        print("="*60)
        print("\n1. Preprocess Wikipedia XML to clean text:")
        print(f"   python preprocess_wiki.py {xml_path} wiki_clean.txt")
        print("\n2. Start training:")
        print("   bin\\train_llm.exe --corpus wiki_clean.txt --epochs 10")
        print("\n3. Wait for training to complete (see estimated time above)")
        print()

    else:
        print("\n" + "="*60)
        print("Next Steps:")
        print("="*60)
        print("\n1. Extract the file:")
        print(f"   bunzip2 {output_path}")
        print("\n2. Preprocess:")
        print(f"   python preprocess_wiki.py <extracted.xml> wiki_clean.txt")
        print("\n3. Train:")
        print("   bin\\train_llm.exe --corpus wiki_clean.txt --epochs 10")
        print()

if __name__ == '__main__':
    main()
