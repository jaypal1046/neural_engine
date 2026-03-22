import { useState, useEffect, useRef } from 'react'
import { Search, File, Clock, Hash, ArrowRight } from 'lucide-react'
import { readDir } from '../lib/desktopBridge'

interface FileItem {
    name: string
    path: string
    type: 'file' | 'directory'
    relativePath: string
    extension?: string
}

interface Props {
    projectRoot: string
    onOpen: (filePath: string, fileName: string) => void
    onClose: () => void
    mode?: 'file' | 'line' | 'symbol' // Ctrl+P, Ctrl+G, Ctrl+Shift+O
    initialQuery?: string
}

const FILE_ICON_COLORS: Record<string, string> = {
    ts: '#3178C6', tsx: '#3178C6', js: '#F7DF1E', jsx: '#61DAFB',
    css: '#264DE4', html: '#E44D26', json: '#F7DF1E', py: '#3776AB',
    md: '#083FA1', rs: '#CE412B', go: '#00ADD8', java: '#ED8B00',
    cpp: '#659AD2', c: '#A8B9CC', dart: '#0175C2', yaml: '#CB171E',
    sh: '#4EAA25', bat: '#C1F12E', sql: '#E38C00', xml: '#F60',
    svg: '#FFB13B', vue: '#42B883', svelte: '#FF3E00', rb: '#CC342D',
}

function getFileIconColor(ext: string): string {
    return FILE_ICON_COLORS[ext] || '#888'
}

function flattenFileTree(tree: any[], parentPath: string = ''): FileItem[] {
    const result: FileItem[] = []
    for (const node of tree) {
        const relativePath = parentPath ? `${parentPath}/${node.name}` : node.name
        if (node.type === 'file') {
            result.push({
                name: node.name,
                path: node.path,
                type: 'file',
                relativePath,
                extension: node.extension?.replace('.', '') || node.name.split('.').pop() || '',
            })
        }
        if (node.children && node.type === 'directory') {
            result.push(...flattenFileTree(node.children, relativePath))
        }
    }
    return result
}

function fuzzyMatch(query: string, text: string): { match: boolean, score: number } {
    const q = query.toLowerCase()
    const t = text.toLowerCase()

    // Exact substring match (highest priority)
    if (t.includes(q)) return { match: true, score: 100 + (100 - t.indexOf(q)) }

    // Fuzzy match
    let qi = 0
    let score = 0
    let lastMatchIndex = -1
    for (let ti = 0; ti < t.length && qi < q.length; ti++) {
        if (t[ti] === q[qi]) {
            score += 10
            // Consecutive matches get bonus
            if (lastMatchIndex === ti - 1) score += 5
            // Start-of-word bonus
            if (ti === 0 || t[ti - 1] === '/' || t[ti - 1] === '\\' || t[ti - 1] === '.' || t[ti - 1] === '-' || t[ti - 1] === '_') {
                score += 15
            }
            lastMatchIndex = ti
            qi++
        }
    }

    return { match: qi === q.length, score }
}

function highlightMatch(text: string, query: string): React.ReactElement {
    if (!query) return <>{text}</>

    const q = query.toLowerCase()
    const t = text.toLowerCase()
    const idx = t.indexOf(q)

    if (idx >= 0) {
        return (
            <>
                {text.slice(0, idx)}
                <span className="quickopen-highlight">{text.slice(idx, idx + query.length)}</span>
                {text.slice(idx + query.length)}
            </>
        )
    }

    // Fuzzy highlight
    const parts: React.ReactNode[] = []
    let qi = 0
    for (let i = 0; i < text.length; i++) {
        if (qi < q.length && t[i] === q[qi]) {
            parts.push(<span key={i} className="quickopen-highlight">{text[i]}</span>)
            qi++
        } else {
            parts.push(<span key={i}>{text[i]}</span>)
        }
    }
    return <>{parts}</>
}

export function QuickOpen({ projectRoot, onOpen, onClose, mode = 'file', initialQuery = '' }: Props) {
    const [query, setQuery] = useState(initialQuery)
    const [files, setFiles] = useState<FileItem[]>([])
    const [activeIndex, setActiveIndex] = useState(0)
    const [loading, setLoading] = useState(true)
    const [recentFiles] = useState<string[]>(() => {
        try {
            return JSON.parse(localStorage.getItem('neural-studio-recent-files') || '[]')
        } catch { return [] }
    })

    const inputRef = useRef<HTMLInputElement>(null)
    const listRef = useRef<HTMLDivElement>(null)

    // Load file tree
    useEffect(() => {
        if (!projectRoot) {
            setLoading(false)
            return
        }
        readDir(projectRoot).then((tree: any[]) => {
            setFiles(flattenFileTree(tree))
            setLoading(false)
        }).catch(() => setLoading(false))
    }, [projectRoot])

    useEffect(() => {
        inputRef.current?.focus()
    }, [])

    useEffect(() => {
        setActiveIndex(0)
    }, [query])

    // Check for Go to Line mode (query starts with :)
    const isGoToLine = query.startsWith(':')
    const isGoToSymbol = query.startsWith('@')

    // Filter and sort
    const filtered = (() => {
        if (isGoToLine || isGoToSymbol) return []
        if (!query.trim()) {
            // Show recent files first
            const recent = recentFiles
                .map(p => files.find(f => f.path === p))
                .filter(Boolean) as FileItem[]
            return recent.length > 0 ? recent : files.slice(0, 20)
        }
        return files
            .map(f => ({ file: f, ...fuzzyMatch(query, f.name + ' ' + f.relativePath) }))
            .filter(f => f.match)
            .sort((a, b) => b.score - a.score)
            .slice(0, 30)
            .map(f => f.file)
    })()

    const handleSelect = (file: FileItem) => {
        // Save to recent
        const recent = [file.path, ...recentFiles.filter(p => p !== file.path)].slice(0, 10)
        localStorage.setItem('neural-studio-recent-files', JSON.stringify(recent))
        onOpen(file.path, file.name)
        onClose()
    }

    const handleKeyDown = (e: React.KeyboardEvent) => {
        if (e.key === 'Escape') {
            onClose()
        } else if (e.key === 'ArrowDown') {
            e.preventDefault()
            setActiveIndex(prev => Math.min(prev + 1, filtered.length - 1))
            // Scroll into view
            const el = listRef.current?.children[Math.min(activeIndex + 1, filtered.length - 1)] as HTMLElement
            el?.scrollIntoView({ block: 'nearest' })
        } else if (e.key === 'ArrowUp') {
            e.preventDefault()
            setActiveIndex(prev => Math.max(prev - 1, 0))
            const el = listRef.current?.children[Math.max(activeIndex - 1, 0)] as HTMLElement
            el?.scrollIntoView({ block: 'nearest' })
        } else if (e.key === 'Enter') {
            if (isGoToLine) {
                const lineNum = parseInt(query.slice(1))
                if (lineNum > 0) {
                    // Dispatch go-to-line event
                    window.dispatchEvent(new CustomEvent('editor-go-to-line', { detail: { line: lineNum } }))
                    onClose()
                }
            } else if (filtered[activeIndex]) {
                handleSelect(filtered[activeIndex])
            }
        }
    }

    const placeholderText = mode === 'line'
        ? 'Go to line number...'
        : 'Search files by name (prefix : for line, @ for symbol)'

    return (
        <div className="quickopen-overlay" onClick={onClose}>
            <div className="quickopen-dialog" onClick={e => e.stopPropagation()}>
                <div className="quickopen-input-row">
                    <Search size={15} style={{ color: 'var(--text-muted)', flexShrink: 0 }} />
                    <input
                        ref={inputRef}
                        type="text"
                        className="quickopen-input"
                        placeholder={placeholderText}
                        value={query}
                        onChange={e => setQuery(e.target.value)}
                        onKeyDown={handleKeyDown}
                    />
                    <kbd className="quickopen-kbd">ESC</kbd>
                </div>

                <div className="quickopen-list" ref={listRef}>
                    {loading && (
                        <div className="quickopen-loading">
                            <div className="spin" style={{ width: 14, height: 14 }}>
                                <svg width="14" height="14" viewBox="0 0 24 24" fill="none" stroke="#4A9EFF" strokeWidth="2.5">
                                    <path d="M21 12a9 9 0 11-6.219-8.56" />
                                </svg>
                            </div>
                            <span>Indexing files...</span>
                        </div>
                    )}

                    {isGoToLine && (
                        <div className="quickopen-go-to-line">
                            <Hash size={15} style={{ color: 'var(--accent)' }} />
                            <span>Go to line <strong>{query.slice(1) || '...'}</strong></span>
                            <ArrowRight size={13} style={{ color: 'var(--text-faint)', marginLeft: 'auto' }} />
                            <span style={{ fontSize: 11, color: 'var(--text-faint)' }}>Press Enter</span>
                        </div>
                    )}

                    {!isGoToLine && !loading && !query && recentFiles.length > 0 && filtered.length > 0 && (
                        <div className="quickopen-section-label">
                            <Clock size={11} /> Recently Opened
                        </div>
                    )}

                    {!isGoToLine && filtered.map((file, i) => {
                        const ext = file.extension || ''
                        const iconColor = getFileIconColor(ext)
                        return (
                            <button
                                key={file.path}
                                className={`quickopen-item ${i === activeIndex ? 'active' : ''}`}
                                onClick={() => handleSelect(file)}
                                onMouseEnter={() => setActiveIndex(i)}
                            >
                                <File size={14} style={{ color: iconColor, flexShrink: 0 }} />
                                <span className="quickopen-item-name">
                                    {highlightMatch(file.name, query)}
                                </span>
                                <span className="quickopen-item-path">
                                    {file.relativePath}
                                </span>
                            </button>
                        )
                    })}

                    {!isGoToLine && !loading && query && filtered.length === 0 && (
                        <div className="quickopen-empty">
                            No matching files found
                        </div>
                    )}
                </div>
            </div>
        </div>
    )
}
