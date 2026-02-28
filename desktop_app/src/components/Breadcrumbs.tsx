import { useState, useRef, useEffect, useCallback } from 'react'
import { ChevronRight, FileText, Folder, Hash, Box, Braces } from 'lucide-react'

interface Props {
    filePath: string
    language: string
    projectRoot: string
    editorRef?: any
    monacoRef?: any
    cursorLine?: number
    cursorCol?: number
    onNavigateSymbol?: (line: number) => void
}

interface SymbolItem {
    name: string
    kind: string
    line: number
    endLine: number
}

function getSymbolIcon(kind: string) {
    switch (kind) {
        case 'function': return { icon: Braces, color: '#FBBF24' }
        case 'class': return { icon: Box, color: '#4A9EFF' }
        case 'variable': return { icon: Hash, color: '#4ADE80' }
        default: return { icon: Hash, color: '#888' }
    }
}

export function Breadcrumbs({ filePath, language, projectRoot, editorRef, monacoRef, cursorLine, cursorCol, onNavigateSymbol }: Props) {
    const [symbols, setSymbols] = useState<SymbolItem[]>([])
    const [showSymbolDropdown, setShowSymbolDropdown] = useState(false)
    const [showFolderDropdown, setShowFolderDropdown] = useState<number | null>(null)
    const dropdownRef = useRef<HTMLDivElement>(null)

    // Extract symbols from Monaco model
    const extractSymbols = useCallback(() => {
        if (!editorRef?.current || !monacoRef?.current) return
        const model = editorRef.current.getModel()
        if (!model) return

        const text = model.getValue()
        const lines = text.split('\n')
        const found: SymbolItem[] = []

        // Simple regex-based symbol extraction
        const patterns = [
            { regex: /(?:export\s+)?(?:async\s+)?function\s+(\w+)/g, kind: 'function' },
            { regex: /(?:export\s+)?class\s+(\w+)/g, kind: 'class' },
            { regex: /(?:export\s+)?(?:const|let|var)\s+(\w+)\s*=/g, kind: 'variable' },
            { regex: /(?:export\s+)?interface\s+(\w+)/g, kind: 'class' },
            { regex: /(?:export\s+)?type\s+(\w+)\s*=/g, kind: 'class' },
            { regex: /(?:export\s+)?enum\s+(\w+)/g, kind: 'class' },
            { regex: /def\s+(\w+)\s*\(/g, kind: 'function' },  // Python
        ]

        for (let i = 0; i < lines.length; i++) {
            for (const { regex, kind } of patterns) {
                regex.lastIndex = 0
                let m: RegExpExecArray | null
                while ((m = regex.exec(lines[i])) !== null) {
                    found.push({ name: m[1], kind, line: i + 1, endLine: i + 1 })
                }
            }
        }

        setSymbols(found)
    }, [editorRef, monacoRef])

    useEffect(() => {
        extractSymbols()
        // Re-extract when content changes
        const interval = setInterval(extractSymbols, 3000)
        return () => clearInterval(interval)
    }, [extractSymbols, filePath])

    // Close dropdown on click outside
    useEffect(() => {
        const handler = (e: MouseEvent) => {
            if (dropdownRef.current && !dropdownRef.current.contains(e.target as Node)) {
                setShowSymbolDropdown(false)
                setShowFolderDropdown(null)
            }
        }
        document.addEventListener('mousedown', handler)
        return () => document.removeEventListener('mousedown', handler)
    }, [])

    // Build path segments
    const relativePath = filePath.startsWith(projectRoot)
        ? filePath.slice(projectRoot.length).replace(/^[/\\]/, '')
        : filePath
    const parts = relativePath.split(/[/\\]/)
    const fileName = parts[parts.length - 1]
    const folders = parts.slice(0, -1)

    // Find current symbol based on cursor line
    const currentSymbol = cursorLine
        ? symbols.filter(s => s.line <= cursorLine).pop()
        : null

    return (
        <div className="breadcrumbs-bar" ref={dropdownRef}>
            <div className="breadcrumbs-path">
                {/* Folder segments */}
                {folders.map((folder, i) => (
                    <span key={i} className="breadcrumb-segment">
                        <button
                            className="breadcrumb-btn"
                            onClick={() => setShowFolderDropdown(showFolderDropdown === i ? null : i)}
                        >
                            <Folder size={12} style={{ color: '#FBBF24', opacity: 0.7 }} />
                            <span>{folder}</span>
                        </button>
                        <ChevronRight size={10} className="breadcrumb-sep" />
                    </span>
                ))}

                {/* File segment */}
                <span className="breadcrumb-segment">
                    <button className="breadcrumb-btn active">
                        <FileText size={12} style={{ color: '#4A9EFF' }} />
                        <span>{fileName}</span>
                    </button>
                </span>

                {/* Current symbol */}
                {currentSymbol && (
                    <span className="breadcrumb-segment">
                        <ChevronRight size={10} className="breadcrumb-sep" />
                        <button
                            className="breadcrumb-btn symbol"
                            onClick={() => setShowSymbolDropdown(!showSymbolDropdown)}
                        >
                            {(() => {
                                const { icon: Icon, color } = getSymbolIcon(currentSymbol.kind)
                                return <Icon size={12} style={{ color }} />
                            })()}
                            <span>{currentSymbol.name}</span>
                        </button>
                    </span>
                )}
            </div>

            {/* Right side: cursor position */}
            <div className="breadcrumbs-info">
                <span className="breadcrumb-lang-badge">{language}</span>
                {cursorLine && cursorCol && (
                    <span className="breadcrumb-cursor">Ln {cursorLine}, Col {cursorCol}</span>
                )}
            </div>

            {/* Symbol dropdown */}
            {showSymbolDropdown && symbols.length > 0 && (
                <div className="breadcrumb-dropdown symbol-dropdown">
                    <div className="breadcrumb-dropdown-header">Symbols in file</div>
                    {symbols.map((sym, i) => {
                        const { icon: Icon, color } = getSymbolIcon(sym.kind)
                        return (
                            <button
                                key={i}
                                className={`breadcrumb-dropdown-item ${sym === currentSymbol ? 'active' : ''}`}
                                onClick={() => {
                                    onNavigateSymbol?.(sym.line)
                                    setShowSymbolDropdown(false)
                                }}
                            >
                                <Icon size={13} style={{ color }} />
                                <span className="breadcrumb-dropdown-name">{sym.name}</span>
                                <span className="breadcrumb-dropdown-kind">{sym.kind}</span>
                                <span className="breadcrumb-dropdown-line">:{sym.line}</span>
                            </button>
                        )
                    })}
                </div>
            )}
        </div>
    )
}
