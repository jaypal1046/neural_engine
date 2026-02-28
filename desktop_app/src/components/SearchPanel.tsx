import { useState, useCallback, useRef } from 'react'
import { Search, FileText, ChevronDown, ChevronRight, Replace } from 'lucide-react'

interface SearchMatch {
    line: number
    text: string
    column: number
}

interface SearchResult {
    filePath: string
    fileName: string
    relativePath: string
    matches: SearchMatch[]
}

interface Props {
    projectRoot: string
    onFileOpen: (filePath: string, fileName: string) => void
}

export function SearchPanel({ projectRoot, onFileOpen }: Props) {
    const [query, setQuery] = useState('')
    const [replaceText, setReplaceText] = useState('')
    const [showReplace, setShowReplace] = useState(false)
    const [results, setResults] = useState<SearchResult[]>([])
    const [searching, setSearching] = useState(false)
    const [expandedFiles, setExpandedFiles] = useState<Set<string>>(new Set())
    const [totalMatches, setTotalMatches] = useState(0)
    const [caseSensitive, setCaseSensitive] = useState(false)
    const [wholeWord, setWholeWord] = useState(false)
    const [useRegex, setUseRegex] = useState(false)
    const searchTimeout = useRef<any>(null)

    const doSearch = useCallback(async (q: string) => {
        if (!q || q.length < 2 || !projectRoot) {
            setResults([])
            setTotalMatches(0)
            return
        }

        setSearching(true)
        try {
            const searchApi = (window as any).searchApi
            if (searchApi?.searchFiles) {
                const res = await searchApi.searchFiles(projectRoot, q)
                setResults(res || [])
                setTotalMatches(res?.reduce((sum: number, r: SearchResult) => sum + r.matches.length, 0) || 0)
                // Auto-expand all results
                setExpandedFiles(new Set(res?.map((r: SearchResult) => r.filePath) || []))
            }
        } catch (err) {
            console.error('Search failed:', err)
        }
        setSearching(false)
    }, [projectRoot])

    const handleQueryChange = (value: string) => {
        setQuery(value)
        if (searchTimeout.current) clearTimeout(searchTimeout.current)
        searchTimeout.current = setTimeout(() => doSearch(value), 400)
    }

    const toggleExpand = (filePath: string) => {
        setExpandedFiles(prev => {
            const next = new Set(prev)
            if (next.has(filePath)) next.delete(filePath)
            else next.add(filePath)
            return next
        })
    }

    const highlightMatch = (text: string, query: string) => {
        if (!query) return <span>{text}</span>
        const idx = text.toLowerCase().indexOf(query.toLowerCase())
        if (idx === -1) return <span>{text}</span>
        return (
            <span>
                {text.substring(0, idx)}
                <span style={{ background: 'rgba(255,204,0,0.3)', color: '#FFD700', borderRadius: 2, padding: '0 1px' }}>
                    {text.substring(idx, idx + query.length)}
                </span>
                {text.substring(idx + query.length)}
            </span>
        )
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>Search</span>
                <span style={{ fontSize: 10, color: 'var(--text-faint)', fontWeight: 400, textTransform: 'none', letterSpacing: 0 }}>
                    {totalMatches > 0 ? `${totalMatches} results in ${results.length} files` : ''}
                </span>
            </div>

            <div style={{ padding: '4px 12px 8px', flexShrink: 0 }}>
                {/* Search input */}
                <div style={{
                    display: 'flex', alignItems: 'center', gap: 4,
                    background: 'var(--bg-surface)', border: '1px solid var(--border)',
                    borderRadius: 6, padding: '0 6px', marginBottom: 4
                }}>
                    <Search size={13} style={{ color: 'var(--text-faint)', flexShrink: 0 }} />
                    <input
                        type="text"
                        placeholder="Search"
                        value={query}
                        onChange={e => handleQueryChange(e.target.value)}
                        onKeyDown={e => { if (e.key === 'Enter') doSearch(query) }}
                        style={{
                            flex: 1, background: 'transparent', border: 'none', outline: 'none',
                            color: 'var(--text)', fontSize: 12, padding: '7px 0', fontFamily: 'inherit'
                        }}
                    />
                    {/* Toggle options */}
                    <button onClick={() => setCaseSensitive(!caseSensitive)}
                        title="Match Case"
                        style={{
                            padding: '2px 4px', border: 'none', borderRadius: 3, fontSize: 11, fontWeight: 700,
                            cursor: 'pointer', fontFamily: 'monospace',
                            background: caseSensitive ? 'var(--accent-soft)' : 'transparent',
                            color: caseSensitive ? 'var(--accent)' : 'var(--text-faint)',
                        }}>Aa</button>
                    <button onClick={() => setWholeWord(!wholeWord)}
                        title="Match Whole Word"
                        style={{
                            padding: '2px 4px', border: 'none', borderRadius: 3, fontSize: 10, fontWeight: 700,
                            cursor: 'pointer', fontFamily: 'monospace',
                            background: wholeWord ? 'var(--accent-soft)' : 'transparent',
                            color: wholeWord ? 'var(--accent)' : 'var(--text-faint)',
                        }}>ab</button>
                    <button onClick={() => setUseRegex(!useRegex)}
                        title="Use Regular Expression"
                        style={{
                            padding: '2px 4px', border: 'none', borderRadius: 3, fontSize: 10, fontWeight: 700,
                            cursor: 'pointer', fontFamily: 'monospace',
                            background: useRegex ? 'var(--accent-soft)' : 'transparent',
                            color: useRegex ? 'var(--accent)' : 'var(--text-faint)',
                        }}>.*</button>
                    <button onClick={() => setShowReplace(!showReplace)}
                        title="Toggle Replace"
                        style={{
                            padding: '2px', border: 'none', borderRadius: 3,
                            cursor: 'pointer', background: 'transparent',
                            color: showReplace ? 'var(--accent)' : 'var(--text-faint)',
                            display: 'flex'
                        }}>
                        <Replace size={13} />
                    </button>
                </div>

                {/* Replace input */}
                {showReplace && (
                    <div style={{
                        display: 'flex', alignItems: 'center', gap: 4,
                        background: 'var(--bg-surface)', border: '1px solid var(--border)',
                        borderRadius: 6, padding: '0 6px'
                    }}>
                        <Replace size={13} style={{ color: 'var(--text-faint)', flexShrink: 0 }} />
                        <input
                            type="text"
                            placeholder="Replace"
                            value={replaceText}
                            onChange={e => setReplaceText(e.target.value)}
                            style={{
                                flex: 1, background: 'transparent', border: 'none', outline: 'none',
                                color: 'var(--text)', fontSize: 12, padding: '7px 0', fontFamily: 'inherit'
                            }}
                        />
                    </div>
                )}
            </div>

            {/* Loading */}
            {searching && (
                <div style={{ padding: '8px 16px', fontSize: 11, color: 'var(--accent)', display: 'flex', alignItems: 'center', gap: 6 }}>
                    <div className="spin" style={{ width: 12, height: 12 }}>
                        <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2.5">
                            <path d="M21 12a9 9 0 11-6.219-8.56" />
                        </svg>
                    </div>
                    Searching...
                </div>
            )}

            {/* Results */}
            <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
                {results.map(result => (
                    <div key={result.filePath}>
                        {/* File header */}
                        <div
                            onClick={() => toggleExpand(result.filePath)}
                            style={{
                                display: 'flex', alignItems: 'center', gap: 4, padding: '4px 12px',
                                cursor: 'pointer', fontSize: 12, fontWeight: 600, color: 'var(--text)',
                                transition: 'background 0.1s'
                            }}
                            onMouseEnter={e => (e.currentTarget.style.background = 'var(--bg-hover)')}
                            onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                        >
                            {expandedFiles.has(result.filePath) ?
                                <ChevronDown size={12} style={{ flexShrink: 0 }} /> :
                                <ChevronRight size={12} style={{ flexShrink: 0 }} />
                            }
                            <FileText size={12} style={{ flexShrink: 0, color: 'var(--accent)' }} />
                            <span style={{ overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                {result.relativePath}
                            </span>
                            <span style={{
                                marginLeft: 'auto', fontSize: 10, color: 'var(--text-faint)',
                                background: 'var(--bg-hover)', padding: '1px 5px', borderRadius: 8, flexShrink: 0
                            }}>
                                {result.matches.length}
                            </span>
                        </div>
                        {/* Matches */}
                        {expandedFiles.has(result.filePath) && result.matches.map((match, i) => (
                            <div
                                key={i}
                                onClick={() => onFileOpen(result.filePath, result.fileName)}
                                style={{
                                    display: 'flex', alignItems: 'flex-start', gap: 6, padding: '3px 12px 3px 36px',
                                    cursor: 'pointer', fontSize: 11, color: 'var(--text-muted)',
                                    transition: 'background 0.1s'
                                }}
                                onMouseEnter={e => (e.currentTarget.style.background = 'var(--bg-hover)')}
                                onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                            >
                                <span style={{ color: 'var(--text-faint)', fontFamily: "'JetBrains Mono', monospace", fontSize: 10, minWidth: 28, textAlign: 'right', flexShrink: 0 }}>
                                    {match.line}
                                </span>
                                <span style={{ fontFamily: "'JetBrains Mono', monospace", fontSize: 11, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>
                                    {highlightMatch(match.text, query)}
                                </span>
                            </div>
                        ))}
                    </div>
                ))}
                {!searching && query.length >= 2 && results.length === 0 && (
                    <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
                        No results found for "{query}"
                    </div>
                )}
                {!query && (
                    <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
                        Type at least 2 characters to search across your project files.
                    </div>
                )}
            </div>
        </div>
    )
}
