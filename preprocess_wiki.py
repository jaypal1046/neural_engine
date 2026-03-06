#!/usr/bin/env python3
"""
Wikipedia XML to Clean Text Preprocessor
Extracts clean text from Wikipedia XML dumps for LLM training

Usage:
    python preprocess_wiki.py input.xml output.txt [--min-length 50]
"""

import re
import sys
import xml.etree.ElementTree as ET
from html.parser import HTMLParser
import argparse

class WikiHTMLParser(HTMLParser):
    """Parse HTML tags in Wikipedia articles"""
    def __init__(self):
        super().__init__()
        self.text = []

    def handle_data(self, data):
        self.text.append(data)

    def get_text(self):
        return ''.join(self.text)

def clean_wiki_text(text):
    """Clean Wikipedia markup and formatting"""

    # Remove XML/HTML tags
    text = re.sub(r'<[^>]+>', '', text)

    # Remove Wikipedia markup
    text = re.sub(r'\[\[Category:[^\]]+\]\]', '', text)  # Categories
    text = re.sub(r'\[\[File:[^\]]+\]\]', '', text)      # Files
    text = re.sub(r'\[\[Image:[^\]]+\]\]', '', text)     # Images
    text = re.sub(r'\{\{[^\}]+\}\}', '', text)           # Templates
    text = re.sub(r'\[\[[^\]]*\|([^\]]+)\]\]', r'\1', text)  # Links with text
    text = re.sub(r'\[\[([^\]]+)\]\]', r'\1', text)      # Simple links

    # Remove references and citations
    text = re.sub(r'<ref[^>]*>.*?</ref>', '', text, flags=re.DOTALL)
    text = re.sub(r'<ref[^>]*/?>', '', text)

    # Remove URLs
    text = re.sub(r'http[s]?://(?:[a-zA-Z]|[0-9]|[$-_@.&+]|[!*\\(\\),]|(?:%[0-9a-fA-F][0-9a-fA-F]))+', '', text)

    # Remove special Wikipedia syntax
    text = re.sub(r"'{2,}", '', text)  # Bold/italic markers
    text = re.sub(r'\*+\s*', '', text)  # List markers
    text = re.sub(r'^\s*#+\s*', '', text, flags=re.MULTILINE)  # Headers
    text = re.sub(r'^\s*=+\s*', '', text, flags=re.MULTILINE)  # Section headers
    text = re.sub(r'\s*=+\s*$', '', text, flags=re.MULTILINE)

    # Remove multiple spaces and newlines
    text = re.sub(r'\s+', ' ', text)
    text = re.sub(r'\n\s*\n+', '\n', text)

    # Remove special characters but keep basic punctuation
    text = re.sub(r'[^\w\s.,!?;:\'\"-]', '', text)

    return text.strip()

def extract_articles(xml_file, output_file, min_length=50, max_articles=None):
    """
    Extract articles from Wikipedia XML dump

    Args:
        xml_file: Path to Wikipedia XML dump
        output_file: Path to output text file
        min_length: Minimum article length in characters
        max_articles: Maximum articles to process (None = all)
    """

    print(f"Processing Wikipedia dump: {xml_file}")
    print(f"Output file: {output_file}")
    print(f"Minimum article length: {min_length} characters")
    if max_articles:
        print(f"Maximum articles: {max_articles}")
    print()

    article_count = 0
    filtered_count = 0
    total_chars = 0

    # XML namespace for Wikipedia dumps
    namespaces = {
        'mw': 'http://www.mediawiki.org/xml/export-0.10/'
    }

    try:
        with open(output_file, 'w', encoding='utf-8') as out:
            # Parse XML iteratively to handle large files
            context = ET.iterparse(xml_file, events=('end',))

            for event, elem in context:
                if elem.tag == '{http://www.mediawiki.org/xml/export-0.10/}page':
                    # Extract title
                    title_elem = elem.find('mw:title', namespaces)
                    title = title_elem.text if title_elem is not None else ''

                    # Skip special pages
                    if any(title.startswith(prefix) for prefix in
                           ['Wikipedia:', 'File:', 'Template:', 'Category:',
                            'Help:', 'Portal:', 'Draft:', 'MediaWiki:', 'Talk:']):
                        elem.clear()
                        continue

                    # Extract text content
                    text_elem = elem.find('.//mw:text', namespaces)
                    if text_elem is not None and text_elem.text:
                        text = text_elem.text

                        # Clean the text
                        cleaned = clean_wiki_text(text)

                        # Filter by length
                        if len(cleaned) >= min_length:
                            # Write to file (one sentence per line for better training)
                            sentences = re.split(r'[.!?]+\s+', cleaned)
                            for sentence in sentences:
                                sentence = sentence.strip()
                                if len(sentence) >= 20:  # Minimum sentence length
                                    out.write(sentence + '.\n')

                            article_count += 1
                            total_chars += len(cleaned)

                            # Progress update
                            if article_count % 100 == 0:
                                print(f"Processed {article_count} articles, "
                                      f"~{total_chars // 1024 // 1024} MB", end='\r')
                        else:
                            filtered_count += 1

                    # Clear element to free memory
                    elem.clear()

                    # Check if we've reached max articles
                    if max_articles and article_count >= max_articles:
                        break

            print()  # New line after progress updates

    except ET.ParseError as e:
        print(f"\nWarning: XML parsing error: {e}")
        print("This is normal for large files - continuing from last good position")
    except KeyboardInterrupt:
        print("\n\nInterrupted by user!")

    # Summary
    print("\n" + "="*60)
    print("Processing Complete!")
    print("="*60)
    print(f"Articles processed:  {article_count:,}")
    print(f"Articles filtered:   {filtered_count:,} (too short)")
    print(f"Total characters:    {total_chars:,}")
    print(f"Total size:          ~{total_chars // 1024 // 1024} MB")
    print(f"Output file:         {output_file}")
    print()

def create_sample_corpus(output_file='wiki_sample.txt', num_articles=1000):
    """Create a sample corpus without downloading Wikipedia"""

    print("Creating sample training corpus...")
    print(f"Target: {num_articles} synthetic articles")
    print()

    # Sample topics and templates
    topics = [
        ("Artificial Intelligence", [
            "Artificial intelligence is the simulation of human intelligence by machines.",
            "Machine learning is a subset of AI that learns from data.",
            "Deep learning uses neural networks with multiple layers.",
            "Natural language processing helps computers understand text.",
            "Computer vision enables machines to interpret images.",
        ]),
        ("Computer Science", [
            "Computer science studies algorithms and computation.",
            "Programming languages provide instructions to computers.",
            "Data structures organize information efficiently.",
            "Algorithms are step-by-step problem-solving procedures.",
            "Software engineering develops reliable applications.",
        ]),
        ("Mathematics", [
            "Mathematics is the study of numbers and patterns.",
            "Algebra uses symbols to represent unknown values.",
            "Geometry studies shapes and spatial relationships.",
            "Calculus analyzes rates of change and accumulation.",
            "Statistics collects and analyzes numerical data.",
        ]),
        ("Physics", [
            "Physics explores the fundamental laws of nature.",
            "Mechanics studies motion and forces.",
            "Thermodynamics examines heat and energy transfer.",
            "Electromagnetism describes electric and magnetic phenomena.",
            "Quantum mechanics governs behavior at atomic scales.",
        ]),
        ("Technology", [
            "Technology applies scientific knowledge for practical purposes.",
            "The internet connects billions of devices worldwide.",
            "Cloud computing provides on-demand computing resources.",
            "Mobile devices enable communication and information access.",
            "Cybersecurity protects systems from digital threats.",
        ]),
    ]

    with open(output_file, 'w', encoding='utf-8') as f:
        articles_written = 0

        # Generate variations of each topic
        for topic, sentences in topics:
            for variation in range(num_articles // len(topics)):
                # Write topic variations
                for sentence in sentences:
                    f.write(sentence + '\n')

                    # Add variations
                    f.write(f"In other words, {sentence[0].lower() + sentence[1:]}\n")
                    f.write(f"This means that {sentence[0].lower() + sentence[1:]}\n")

                articles_written += 1

                if articles_written % 100 == 0:
                    print(f"Generated {articles_written}/{num_articles} articles...", end='\r')

        print()

    file_size = open(output_file, 'rb').seek(0, 2)
    print(f"\n✓ Created {output_file}")
    print(f"  Articles: {articles_written}")
    print(f"  Size: {file_size // 1024} KB")
    print()

def main():
    parser = argparse.ArgumentParser(
        description='Extract clean text from Wikipedia XML dumps for LLM training'
    )
    parser.add_argument('input', nargs='?', help='Wikipedia XML dump file')
    parser.add_argument('output', nargs='?', help='Output text file')
    parser.add_argument('--min-length', type=int, default=50,
                       help='Minimum article length (default: 50)')
    parser.add_argument('--max-articles', type=int, default=None,
                       help='Maximum articles to process (default: all)')
    parser.add_argument('--sample', action='store_true',
                       help='Create sample corpus without Wikipedia dump')
    parser.add_argument('--sample-size', type=int, default=1000,
                       help='Number of sample articles (default: 1000)')

    args = parser.parse_args()

    if args.sample:
        # Create sample corpus
        output = args.output or 'wiki_sample.txt'
        create_sample_corpus(output, args.sample_size)
    elif args.input and args.output:
        # Process Wikipedia dump
        extract_articles(args.input, args.output, args.min_length, args.max_articles)
    else:
        parser.print_help()
        print("\n" + "="*60)
        print("Quick Start Examples:")
        print("="*60)
        print("\n1. Create sample training data (no download needed):")
        print("   python preprocess_wiki.py --sample --sample-size 1000")
        print("\n2. Process Wikipedia dump:")
        print("   python preprocess_wiki.py wiki.xml wiki_clean.txt")
        print("\n3. Process first 10,000 articles only:")
        print("   python preprocess_wiki.py wiki.xml output.txt --max-articles 10000")
        print()

if __name__ == '__main__':
    main()
