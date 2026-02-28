import { useState } from 'react'
import { Search, Download, Star, Check, Puzzle, Zap, Globe, Code2, Palette, GitBranch, Terminal, Brain, Loader2, Trash2 } from 'lucide-react'

interface Extension {
    id: string
    name: string
    publisher: string
    description: string
    icon: any
    iconColor: string
    installed: boolean
    stars: number
    downloads: string
    category: string
    version: string
}

const STORAGE_KEY = 'neural-studio-extensions'

const DEFAULT_EXTENSIONS: Extension[] = [
    {
        id: 'neural-compress', name: 'Neural Compress', publisher: 'Neural Studio',
        description: 'AI-powered file compression using 1,046 neural advisors for extreme compression ratios',
        icon: Zap, iconColor: '#C97FDB', installed: true, stars: 5, downloads: '10K+', category: 'AI', version: '2.1.0'
    },
    {
        id: 'ai-copilot', name: 'AI Copilot', publisher: 'Neural Studio',
        description: 'Intelligent code completion, chat assistant, and code generation powered by Neural Engine V10',
        icon: Brain, iconColor: '#4A9EFF', installed: true, stars: 5, downloads: '50K+', category: 'AI', version: '3.0.1'
    },
    {
        id: 'web-browser', name: 'Built-in Browser', publisher: 'Neural Studio',
        description: 'Browse the web directly inside the IDE with tabs, bookmarks, and full webview support',
        icon: Globe, iconColor: '#22D3EE', installed: true, stars: 4, downloads: '20K+', category: 'Tools', version: '1.4.0'
    },
    {
        id: 'git-lens', name: 'Git Integration', publisher: 'Neural Studio',
        description: 'Full Git support with diff viewer, blame annotations, history, and branch management',
        icon: GitBranch, iconColor: '#F97316', installed: true, stars: 5, downloads: '100K+', category: 'SCM', version: '5.2.3'
    },
    {
        id: 'terminal-pro', name: 'Terminal Pro', publisher: 'Neural Studio',
        description: 'Integrated terminal with multi-tab and split pane support, PowerShell, CMD, Bash',
        icon: Terminal, iconColor: '#4ADE80', installed: true, stars: 4, downloads: '80K+', category: 'Tools', version: '2.0.0'
    },
    {
        id: 'theme-dark', name: 'Neural Dark Theme', publisher: 'Neural Studio',
        description: 'Premium dark theme with carefully crafted syntax colors for optimal readability',
        icon: Palette, iconColor: '#FBBF24', installed: true, stars: 5, downloads: '30K+', category: 'Themes', version: '1.8.0'
    },
    {
        id: 'python-ext', name: 'Python', publisher: 'Microsoft',
        description: 'Rich Python support with IntelliSense, linting, debugging, formatting, and Jupyter notebooks',
        icon: Code2, iconColor: '#3776AB', installed: false, stars: 5, downloads: '80M+', category: 'Languages', version: '2024.1'
    },
    {
        id: 'cpp-ext', name: 'C/C++', publisher: 'Microsoft',
        description: 'C/C++ IntelliSense, debugging, code browsing, cmake integration, and snippets',
        icon: Code2, iconColor: '#659AD2', installed: false, stars: 4, downloads: '40M+', category: 'Languages', version: '1.19.0'
    },
    {
        id: 'prettier', name: 'Prettier', publisher: 'Prettier',
        description: 'Opinionated code formatter supporting JS, TS, CSS, HTML, JSON, YAML, and more',
        icon: Code2, iconColor: '#56B6C2', installed: false, stars: 4, downloads: '35M+', category: 'Formatters', version: '10.3.0'
    },
    {
        id: 'mcp-client', name: 'MCP Client', publisher: 'Neural Studio',
        description: 'Model Context Protocol client for connecting to AI model servers and external tools',
        icon: Puzzle, iconColor: '#DA7555', installed: true, stars: 4, downloads: '5K+', category: 'AI', version: '1.2.0'
    },
    {
        id: 'dart-ext', name: 'Dart', publisher: 'Dart Team',
        description: 'Dart language support with IntelliSense, debugging, refactoring, and code completion',
        icon: Code2, iconColor: '#0175C2', installed: false, stars: 5, downloads: '15M+', category: 'Languages', version: '3.82.0'
    },
    {
        id: 'rust-ext', name: 'Rust Analyzer', publisher: 'rust-lang',
        description: 'Rust language support with auto-complete, inline errors, code actions, and debugging',
        icon: Code2, iconColor: '#CE412B', installed: false, stars: 5, downloads: '10M+', category: 'Languages', version: '0.4.1841'
    },
    {
        id: 'eslint', name: 'ESLint', publisher: 'Microsoft',
        description: 'Integrates ESLint JavaScript into VS Code for real-time code quality checks',
        icon: Code2, iconColor: '#4B32C3', installed: false, stars: 5, downloads: '30M+', category: 'Linters', version: '3.0.5'
    },
    {
        id: 'docker', name: 'Docker', publisher: 'Microsoft',
        description: 'Docker extension for building, managing, and deploying containerized applications',
        icon: Code2, iconColor: '#2496ED', installed: false, stars: 4, downloads: '25M+', category: 'Tools', version: '1.28.0'
    },
]

function loadExtensions(): Extension[] {
    try {
        const saved = localStorage.getItem(STORAGE_KEY)
        if (saved) {
            const savedData = JSON.parse(saved) as Record<string, boolean>
            return DEFAULT_EXTENSIONS.map(ext => ({
                ...ext,
                installed: savedData[ext.id] ?? ext.installed
            }))
        }
    } catch { }
    return DEFAULT_EXTENSIONS
}

function saveExtensionState(extensions: Extension[]) {
    try {
        const state: Record<string, boolean> = {}
        extensions.forEach(ext => { state[ext.id] = ext.installed })
        localStorage.setItem(STORAGE_KEY, JSON.stringify(state))
    } catch { }
}

export function ExtensionsPanel() {
    const [searchQuery, setSearchQuery] = useState('')
    const [extensions, setExtensions] = useState(loadExtensions)
    const [filter, setFilter] = useState<'all' | 'installed'>('all')
    const [installing, setInstalling] = useState<string | null>(null)
    const [toast, setToast] = useState('')
    const [selectedExt, setSelectedExt] = useState<Extension | null>(null)

    const showToast = (msg: string) => {
        setToast(msg)
        setTimeout(() => setToast(''), 3000)
    }

    const filtered = extensions.filter(ext => {
        if (filter === 'installed' && !ext.installed) return false
        if (searchQuery) {
            const q = searchQuery.toLowerCase()
            return ext.name.toLowerCase().includes(q) || ext.description.toLowerCase().includes(q) ||
                ext.publisher.toLowerCase().includes(q) || ext.category.toLowerCase().includes(q)
        }
        return true
    })

    const toggleInstall = async (id: string) => {
        const ext = extensions.find(e => e.id === id)
        if (!ext) return

        setInstalling(id)
        // Simulate brief install delay
        await new Promise(r => setTimeout(r, 600))

        setExtensions(prev => {
            const updated = prev.map(e =>
                e.id === id ? { ...e, installed: !e.installed } : e
            )
            saveExtensionState(updated)
            return updated
        })
        setInstalling(null)
        showToast(ext.installed ? `Uninstalled: ${ext.name}` : `Installed: ${ext.name} v${ext.version} ✓`)
    }

    const installedCount = extensions.filter(e => e.installed).length

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>Extensions</span>
                <span style={{ fontSize: 10, color: 'var(--text-faint)', fontWeight: 400, textTransform: 'none', letterSpacing: 0 }}>
                    {installedCount} installed
                </span>
            </div>

            {/* Toast */}
            {toast && (
                <div style={{
                    margin: '0 8px 6px', padding: '6px 10px', borderRadius: 6,
                    background: 'rgba(74,222,128,0.1)', border: '1px solid rgba(74,222,128,0.2)',
                    color: '#4ADE80', fontSize: 11, fontWeight: 600, flexShrink: 0,
                    animation: 'menuSlide 0.15s ease-out'
                }}>{toast}</div>
            )}

            {/* Search */}
            <div style={{ padding: '4px 12px 8px', flexShrink: 0 }}>
                <div style={{
                    display: 'flex', alignItems: 'center', gap: 6,
                    background: 'var(--bg-surface)', border: '1px solid var(--border)',
                    borderRadius: 6, padding: '0 8px'
                }}>
                    <Search size={13} style={{ color: 'var(--text-faint)', flexShrink: 0 }} />
                    <input
                        type="text"
                        placeholder="Search extensions..."
                        value={searchQuery}
                        onChange={e => setSearchQuery(e.target.value)}
                        style={{
                            flex: 1, background: 'transparent', border: 'none', outline: 'none',
                            color: 'var(--text)', fontSize: 12, padding: '7px 0', fontFamily: 'inherit'
                        }}
                    />
                </div>
            </div>

            {/* Filter tabs */}
            <div style={{ display: 'flex', padding: '0 12px 8px', gap: 4, flexShrink: 0 }}>
                {(['all', 'installed'] as const).map(f => (
                    <button key={f} onClick={() => setFilter(f)} style={{
                        padding: '3px 10px', borderRadius: 12, fontSize: 11, fontWeight: 600,
                        border: '1px solid var(--border)', cursor: 'pointer', fontFamily: 'inherit',
                        background: filter === f ? 'var(--accent-soft)' : 'transparent',
                        color: filter === f ? 'var(--accent)' : 'var(--text-muted)',
                        borderColor: filter === f ? 'rgba(74,158,255,0.3)' : 'var(--border)',
                        transition: 'all 0.15s'
                    }}>
                        {f === 'all' ? `Marketplace (${extensions.length})` : `Installed (${installedCount})`}
                    </button>
                ))}
            </div>

            {/* Extension list */}
            <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden', paddingBottom: 16 }}>
                {filtered.map(ext => (
                    <div key={ext.id} style={{
                        display: 'flex', gap: 10, padding: '8px 12px',
                        cursor: 'pointer', transition: 'background 0.1s',
                        background: selectedExt?.id === ext.id ? 'var(--bg-hover)' : 'transparent'
                    }}
                        onClick={() => setSelectedExt(selectedExt?.id === ext.id ? null : ext)}
                        onMouseEnter={e => { if (selectedExt?.id !== ext.id) e.currentTarget.style.background = 'rgba(255,255,255,0.02)' }}
                        onMouseLeave={e => { if (selectedExt?.id !== ext.id) e.currentTarget.style.background = 'transparent' }}
                    >
                        <div style={{
                            width: 36, height: 36, borderRadius: 8, flexShrink: 0,
                            background: `color-mix(in srgb, ${ext.iconColor} 12%, transparent)`,
                            border: `1px solid color-mix(in srgb, ${ext.iconColor} 25%, transparent)`,
                            display: 'flex', alignItems: 'center', justifyContent: 'center',
                            color: ext.iconColor
                        }}>
                            <ext.icon size={18} />
                        </div>
                        <div style={{ flex: 1, minWidth: 0 }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                                <span style={{ fontSize: 13, fontWeight: 600, color: 'var(--text)' }}>{ext.name}</span>
                                {ext.installed && <Check size={12} style={{ color: 'var(--green)' }} />}
                                <span style={{ fontSize: 9, color: 'var(--text-faint)', fontWeight: 600 }}>v{ext.version}</span>
                            </div>
                            <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 1 }}>{ext.publisher}</div>
                            <div style={{
                                fontSize: 11, color: 'var(--text-faint)', marginTop: 3,
                                overflow: 'hidden', textOverflow: 'ellipsis',
                                display: '-webkit-box', WebkitLineClamp: selectedExt?.id === ext.id ? 3 : 1,
                                WebkitBoxOrient: 'vertical' as any, lineHeight: '1.4'
                            }}>{ext.description}</div>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginTop: 4 }}>
                                <span style={{ fontSize: 10, color: '#FBBF24', display: 'flex', alignItems: 'center', gap: 2 }}>
                                    {Array.from({ length: ext.stars }).map((_, i) => (
                                        <Star key={i} size={9} fill="currentColor" />
                                    ))}
                                </span>
                                <span style={{ fontSize: 10, color: 'var(--text-faint)', display: 'flex', alignItems: 'center', gap: 2 }}>
                                    <Download size={9} /> {ext.downloads}
                                </span>
                                <span style={{
                                    fontSize: 9, padding: '1px 5px', borderRadius: 8,
                                    background: 'var(--bg-hover)', color: 'var(--text-faint)', fontWeight: 600
                                }}>{ext.category}</span>
                                <button
                                    onClick={(e) => { e.stopPropagation(); toggleInstall(ext.id) }}
                                    disabled={installing === ext.id}
                                    style={{
                                        marginLeft: 'auto', padding: '3px 10px', borderRadius: 4, fontSize: 11, fontWeight: 600,
                                        border: '1px solid', cursor: installing === ext.id ? 'wait' : 'pointer', fontFamily: 'inherit',
                                        background: ext.installed ? 'transparent' : 'var(--accent)',
                                        color: ext.installed ? 'var(--text-muted)' : '#fff',
                                        borderColor: ext.installed ? 'var(--border)' : 'var(--accent)',
                                        transition: 'all 0.15s', display: 'flex', alignItems: 'center', gap: 4
                                    }}
                                >
                                    {installing === ext.id ? (
                                        <><Loader2 size={11} className="spin" /> {ext.installed ? 'Removing...' : 'Installing...'}</>
                                    ) : (
                                        ext.installed ? <><Trash2 size={10} /> Uninstall</> : <><Download size={10} /> Install</>
                                    )}
                                </button>
                            </div>
                        </div>
                    </div>
                ))}
                {filtered.length === 0 && (
                    <div style={{ padding: 20, textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
                        No extensions found
                    </div>
                )}
            </div>
        </div>
    )
}
