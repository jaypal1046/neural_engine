"""
File Format Converters — Extract readable text from any file type.
Supports: PDF, DOCX, XLSX, CSV, JSON, code files, and plain text.
Falls back gracefully if optional libraries aren't installed.
"""

import os
import json
import csv
import io

# =============================================================================
# Text extraction for various formats
# =============================================================================

# Supported text extensions (read directly)
TEXT_EXTENSIONS = {
    '.txt', '.md', '.py', '.js', '.ts', '.tsx', '.jsx', '.cpp', '.c', '.h',
    '.hpp', '.java', '.cs', '.go', '.rs', '.rb', '.php', '.swift', '.kt',
    '.html', '.htm', '.css', '.scss', '.less', '.xml', '.yaml', '.yml',
    '.toml', '.ini', '.cfg', '.conf', '.env', '.sh', '.bat', '.ps1',
    '.sql', '.r', '.m', '.pl', '.lua', '.vim', '.dockerfile',
    '.gitignore', '.editorconfig', '.log', '.csv', '.tsv',
    '.json', '.jsonl', '.ndjson',
}

def can_read(filepath: str) -> bool:
    """Check if we can extract text from this file type."""
    ext = os.path.splitext(filepath)[1].lower()
    if ext in TEXT_EXTENSIONS:
        return True
    if ext in {'.pdf', '.docx', '.xlsx', '.xls', '.doc', '.pptx'}:
        return True
    # Try to detect text vs binary
    try:
        with open(filepath, 'rb') as f:
            chunk = f.read(1024)
        # If >80% printable ASCII, probably text
        printable = sum(1 for b in chunk if 32 <= b <= 126 or b in (9, 10, 13))
        return len(chunk) > 0 and printable / len(chunk) > 0.8
    except:
        return False

def extract_text(filepath: str, max_chars: int = 200_000) -> str:
    """
    Extract readable text from any supported file format.
    Returns extracted text string or empty string on failure.
    """
    if not os.path.exists(filepath):
        return ""
    
    ext = os.path.splitext(filepath)[1].lower()
    fname = os.path.basename(filepath)
    
    try:
        # ─── PDF ───
        if ext == '.pdf':
            return _extract_pdf(filepath, max_chars)
        
        # ─── Word DOCX ───
        if ext == '.docx':
            return _extract_docx(filepath, max_chars)
        
        # ─── Old Word DOC ───
        if ext == '.doc':
            return _extract_doc(filepath, max_chars)
        
        # ─── Excel XLSX ───
        if ext in ('.xlsx', '.xls'):
            return _extract_xlsx(filepath, max_chars)
        
        # ─── PowerPoint ───
        if ext == '.pptx':
            return _extract_pptx(filepath, max_chars)
        
        # ─── CSV / TSV ───
        if ext in ('.csv', '.tsv'):
            return _extract_csv(filepath, max_chars)
        
        # ─── JSON ───
        if ext in ('.json', '.jsonl', '.ndjson'):
            return _extract_json(filepath, max_chars)
        
        # ─── All text-readable files ───
        if ext in TEXT_EXTENSIONS or _is_text_file(filepath):
            return _extract_text_file(filepath, max_chars)
        
        return ""
        
    except Exception as e:
        return f"[Error extracting {fname}: {str(e)}]"


# =============================================================================
# Format-specific extractors
# =============================================================================

def _extract_text_file(filepath: str, max_chars: int) -> str:
    """Read plain text files."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        return f.read(max_chars)

def _extract_csv(filepath: str, max_chars: int) -> str:
    """Extract text from CSV/TSV files."""
    delimiter = '\t' if filepath.endswith('.tsv') else ','
    result = []
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        reader = csv.reader(f, delimiter=delimiter)
        for i, row in enumerate(reader):
            if i > 1000:
                result.append("... (truncated)")
                break
            result.append(' | '.join(str(cell) for cell in row))
    text = '\n'.join(result)
    return text[:max_chars]

def _extract_json(filepath: str, max_chars: int) -> str:
    """Extract text from JSON files."""
    with open(filepath, 'r', encoding='utf-8', errors='replace') as f:
        content = f.read(max_chars)
    
    try:
        data = json.loads(content)
        # Flatten JSON to readable text
        return _flatten_json(data, max_chars)
    except json.JSONDecodeError:
        return content[:max_chars]

def _flatten_json(data, max_chars: int, prefix: str = "") -> str:
    """Flatten nested JSON into readable key-value pairs."""
    parts = []
    
    if isinstance(data, dict):
        for key, value in data.items():
            path = f"{prefix}.{key}" if prefix else key
            if isinstance(value, (dict, list)):
                parts.append(_flatten_json(value, max_chars, path))
            else:
                parts.append(f"{path}: {value}")
    elif isinstance(data, list):
        for i, item in enumerate(data[:50]):  # Limit list items
            path = f"{prefix}[{i}]"
            if isinstance(item, (dict, list)):
                parts.append(_flatten_json(item, max_chars, path))
            else:
                parts.append(f"{path}: {item}")
        if len(data) > 50:
            parts.append(f"... ({len(data)} items total)")
    else:
        parts.append(str(data))
    
    text = '\n'.join(parts)
    return text[:max_chars]

def _extract_pdf(filepath: str, max_chars: int) -> str:
    """Extract text from PDF files."""
    # Try PyPDF2 first
    try:
        import PyPDF2
        text_parts = []
        with open(filepath, 'rb') as f:
            reader = PyPDF2.PdfReader(f)
            for page in reader.pages:
                page_text = page.extract_text()
                if page_text:
                    text_parts.append(page_text)
                if sum(len(t) for t in text_parts) > max_chars:
                    break
        text = '\n\n'.join(text_parts)
        if text.strip():
            return text[:max_chars]
    except ImportError:
        pass
    except Exception:
        pass
    
    # Try pdfplumber
    try:
        import pdfplumber
        text_parts = []
        with pdfplumber.open(filepath) as pdf:
            for page in pdf.pages:
                page_text = page.extract_text()
                if page_text:
                    text_parts.append(page_text)
                if sum(len(t) for t in text_parts) > max_chars:
                    break
        text = '\n\n'.join(text_parts)
        if text.strip():
            return text[:max_chars]
    except ImportError:
        pass
    except Exception:
        pass
    
    # Try pdfminer
    try:
        from pdfminer.high_level import extract_text as pdfminer_extract
        text = pdfminer_extract(filepath)
        if text.strip():
            return text[:max_chars]
    except ImportError:
        pass
    except Exception:
        pass
    
    return "[PDF detected but no PDF library installed. Install one: pip install PyPDF2 pdfplumber pdfminer.six]"

def _extract_docx(filepath: str, max_chars: int) -> str:
    """Extract text from DOCX (Word) files."""
    try:
        import docx
        doc = docx.Document(filepath)
        text_parts = []
        for para in doc.paragraphs:
            text_parts.append(para.text)
        
        # Also get text from tables
        for table in doc.tables:
            for row in table.rows:
                cells = [cell.text for cell in row.cells]
                text_parts.append(' | '.join(cells))
        
        text = '\n'.join(text_parts)
        return text[:max_chars]
    except ImportError:
        pass
    except Exception as e:
        return f"[DOCX error: {e}]"
    
    # Fallback: DOCX is a ZIP of XML files, try raw extraction
    try:
        import zipfile
        import re
        with zipfile.ZipFile(filepath) as z:
            # word/document.xml contains the main text
            if 'word/document.xml' in z.namelist():
                xml_content = z.read('word/document.xml').decode('utf-8', errors='replace')
                # Strip XML tags
                text = re.sub(r'<[^>]+>', ' ', xml_content)
                text = re.sub(r'\s+', ' ', text).strip()
                return text[:max_chars]
    except Exception:
        pass
    
    return "[DOCX detected but python-docx not installed. Install: pip install python-docx]"

def _extract_doc(filepath: str, max_chars: int) -> str:
    """Extract text from old .doc files."""
    try:
        import textract
        text = textract.process(filepath).decode('utf-8', errors='replace')
        return text[:max_chars]
    except ImportError:
        pass
    except Exception:
        pass
    
    # Try antiword on Windows
    try:
        import subprocess
        result = subprocess.run(['antiword', filepath], capture_output=True, text=True, timeout=30)
        if result.returncode == 0 and result.stdout.strip():
            return result.stdout[:max_chars]
    except Exception:
        pass
    
    return "[Old .doc format detected. Install textract: pip install textract, or convert to .docx]"

def _extract_xlsx(filepath: str, max_chars: int) -> str:
    """Extract text from Excel files."""
    try:
        import openpyxl
        wb = openpyxl.load_workbook(filepath, read_only=True, data_only=True)
        text_parts = []
        
        for sheet_name in wb.sheetnames:
            ws = wb[sheet_name]
            text_parts.append(f"=== Sheet: {sheet_name} ===")
            row_count = 0
            for row in ws.iter_rows(values_only=True):
                cells = [str(cell) if cell is not None else '' for cell in row]
                text_parts.append(' | '.join(cells))
                row_count += 1
                if row_count > 500:
                    text_parts.append("... (truncated)")
                    break
        
        wb.close()
        text = '\n'.join(text_parts)
        return text[:max_chars]
    except ImportError:
        pass
    except Exception as e:
        return f"[Excel error: {e}]"
    
    return "[Excel detected but openpyxl not installed. Install: pip install openpyxl]"

def _extract_pptx(filepath: str, max_chars: int) -> str:
    """Extract text from PowerPoint files."""
    try:
        from pptx import Presentation
        prs = Presentation(filepath)
        text_parts = []
        
        for i, slide in enumerate(prs.slides):
            text_parts.append(f"=== Slide {i+1} ===")
            for shape in slide.shapes:
                if hasattr(shape, "text") and shape.text.strip():
                    text_parts.append(shape.text)
        
        text = '\n'.join(text_parts)
        return text[:max_chars]
    except ImportError:
        pass
    except Exception as e:
        return f"[PPTX error: {e}]"
    
    return "[PowerPoint detected but python-pptx not installed. Install: pip install python-pptx]"

# =============================================================================
# Helpers
# =============================================================================

def _is_text_file(filepath: str) -> bool:
    """Detect if a file is text by sampling bytes."""
    try:
        with open(filepath, 'rb') as f:
            chunk = f.read(1024)
        if len(chunk) == 0:
            return True
        printable = sum(1 for b in chunk if 32 <= b <= 126 or b in (9, 10, 13))
        return printable / len(chunk) > 0.8
    except:
        return False

def get_supported_formats() -> dict:
    """Return info about supported file formats and which libraries are available."""
    formats = {
        "always_supported": list(TEXT_EXTENSIONS),
        "pdf": {"supported": False, "install": "pip install PyPDF2"},
        "docx": {"supported": False, "install": "pip install python-docx"},
        "xlsx": {"supported": False, "install": "pip install openpyxl"},
        "pptx": {"supported": False, "install": "pip install python-pptx"},
    }
    
    try:
        import PyPDF2
        formats["pdf"]["supported"] = True
    except ImportError:
        try:
            import pdfplumber
            formats["pdf"]["supported"] = True
        except ImportError:
            pass
    
    try:
        import docx
        formats["docx"]["supported"] = True
    except ImportError:
        pass
    
    try:
        import openpyxl
        formats["xlsx"]["supported"] = True
    except ImportError:
        pass
    
    try:
        from pptx import Presentation
        formats["pptx"]["supported"] = True
    except ImportError:
        pass
    
    return formats
