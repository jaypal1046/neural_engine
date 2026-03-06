#!/usr/bin/env python3
"""
Simple Wikipedia text extractor
Extracts clean text from Wikipedia XML dump
"""

import re
import sys

def clean_text(text):
    """Clean Wikipedia markup and HTML"""
    if not text:
        return ""

    # Remove XML/HTML tags
    text = re.sub(r'<[^>]+>', '', text)

    # Remove Wikipedia templates {{...}}
    text = re.sub(r'\{\{[^\}]+\}\}', '', text)

    # Remove references [[File:...]], [[Image:...]]
    text = re.sub(r'\[\[(File|Image):[^\]]+\]\]', '', text)

    # Convert wiki links [[text]] or [[link|text]] to just text
    text = re.sub(r'\[\[([^\]|]+)\|([^\]]+)\]\]', r'\2', text)
    text = re.sub(r'\[\[([^\]]+)\]\]', r'\1', text)

    # Remove URLs
    text = re.sub(r'http[s]?://[^\s]+', '', text)

    # Remove special characters
    text = re.sub(r'[&lt;&gt;&amp;]', '', text)

    # Remove extra whitespace
    text = re.sub(r'\s+', ' ', text)

    return text.strip()

def extract_simple(input_file, output_file):
    """Simple extraction - just get text between <text> tags"""
    print(f"Extracting from: {input_file}")
    print(f"Output to: {output_file}")
    print("")

    articles = 0
    lines_written = 0

    with open(input_file, 'r', encoding='utf-8', errors='ignore') as f_in:
        with open(output_file, 'w', encoding='utf-8') as f_out:
            in_text = False
            current_text = []

            for line_num, line in enumerate(f_in):
                if line_num % 10000 == 0:
                    print(f'\rProcessed {line_num:,} lines, {articles:,} articles, {lines_written:,} sentences', end='')

                # Check for text start
                if '<text' in line:
                    in_text = True
                    # Get text after <text...> tag
                    match = re.search(r'<text[^>]*>(.*)', line)
                    if match:
                        current_text.append(match.group(1))
                    continue

                # Check for text end
                if '</text>' in line:
                    in_text = False
                    # Get text before </text>
                    match = re.search(r'(.*)</text>', line)
                    if match:
                        current_text.append(match.group(1))

                    # Process accumulated text
                    full_text = ' '.join(current_text)
                    cleaned = clean_text(full_text)

                    if len(cleaned) > 50:
                        # Split into sentences
                        sentences = re.split(r'[.!?]+\s+', cleaned)
                        for sent in sentences:
                            sent = sent.strip()
                            if len(sent) > 20:
                                f_out.write(sent + '.\n')
                                lines_written += 1

                        articles += 1

                    current_text = []
                    continue

                # Accumulate text
                if in_text:
                    current_text.append(line)

    print('')
    print('')
    print('='*60)
    print('Extraction Complete!')
    print('='*60)
    print(f'Articles processed: {articles:,}')
    print(f'Sentences written: {lines_written:,}')
    print(f'Output file: {output_file}')
    print('')

if __name__ == '__main__':
    if len(sys.argv) != 3:
        print("Usage: python extract_wiki_simple.py input.xml output.txt")
        sys.exit(1)

    extract_simple(sys.argv[1], sys.argv[2])
