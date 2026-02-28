import { useState } from 'react'
import { Palette, Terminal, Globe, BrainCircuit, Zap, Server, GitBranch, Monitor, RotateCcw } from 'lucide-react'

interface SettingSection {
    id: string
    label: string
    icon: any
    settings: SettingItem[]
}

interface SettingItem {
    id: string
    label: string
    description: string
    type: 'toggle' | 'select' | 'text' | 'number'
    defaultValue: any
    options?: string[]
    min?: number
    max?: number
}

const SECTIONS: SettingSection[] = [
    {
        id: 'appearance', label: 'Appearance', icon: Palette,
        settings: [
            { id: 'theme', label: 'Theme', description: 'Select the color theme for the IDE', type: 'select', defaultValue: 'Neural Dark', options: ['Neural Dark', 'Neural Light', 'Midnight Blue', 'Solarized', 'Monokai', 'Dracula'] },
            { id: 'font-size', label: 'Font Size', description: 'Editor font size in pixels', type: 'number', defaultValue: 14, min: 10, max: 32 },
            { id: 'font-family', label: 'Font Family', description: 'Editor font family', type: 'select', defaultValue: 'JetBrains Mono', options: ['JetBrains Mono', 'Fira Code', 'Cascadia Code', 'Consolas', 'Courier New', 'Source Code Pro'] },
            { id: 'minimap', label: 'Show Minimap', description: 'Display a minimap of the code', type: 'toggle', defaultValue: true },
            { id: 'line-numbers', label: 'Line Numbers', description: 'Show line numbers in the editor', type: 'toggle', defaultValue: true },
            { id: 'bracket-pairs', label: 'Bracket Pair Colorization', description: 'Color matching bracket pairs', type: 'toggle', defaultValue: true },
        ]
    },
    {
        id: 'editor', label: 'Editor', icon: Monitor,
        settings: [
            { id: 'tab-size', label: 'Tab Size', description: 'Number of spaces per tab', type: 'number', defaultValue: 4, min: 1, max: 8 },
            { id: 'word-wrap', label: 'Word Wrap', description: 'Wrap long lines in the editor', type: 'toggle', defaultValue: false },
            { id: 'auto-save', label: 'Auto Save', description: 'Automatically save changed files', type: 'select', defaultValue: 'afterDelay', options: ['off', 'afterDelay', 'onFocusChange', 'onWindowChange'] },
            { id: 'format-on-save', label: 'Format on Save', description: 'Format the file when saving', type: 'toggle', defaultValue: true },
            { id: 'auto-indent', label: 'Auto Indent', description: 'Automatically indent new lines', type: 'toggle', defaultValue: true },
            { id: 'trim-trailing', label: 'Trim Trailing Whitespace', description: 'Remove trailing whitespace on save', type: 'toggle', defaultValue: true },
            { id: 'insert-final-newline', label: 'Insert Final Newline', description: 'Ensure file ends with newline', type: 'toggle', defaultValue: true },
        ]
    },
    {
        id: 'terminal', label: 'Terminal', icon: Terminal,
        settings: [
            { id: 'shell', label: 'Default Shell', description: 'Default shell for the integrated terminal', type: 'select', defaultValue: 'PowerShell', options: ['PowerShell', 'CMD', 'Git Bash', 'WSL', 'Bash'] },
            { id: 'term-font-size', label: 'Terminal Font Size', description: 'Font size for the terminal', type: 'number', defaultValue: 13, min: 10, max: 24 },
            { id: 'cursor-style', label: 'Cursor Style', description: 'Terminal cursor style', type: 'select', defaultValue: 'block', options: ['block', 'line', 'underline'] },
            { id: 'scroll-back', label: 'Scrollback Lines', description: 'Number of lines to keep in scrollback', type: 'number', defaultValue: 1000, min: 100, max: 10000 },
        ]
    },
    {
        id: 'ai', label: 'AI Copilot', icon: BrainCircuit,
        settings: [
            { id: 'api-url', label: 'API Endpoint', description: 'Neural Engine API URL', type: 'text', defaultValue: 'http://127.0.0.1:8001' },
            { id: 'auto-suggest', label: 'Auto Suggestions', description: 'Show AI suggestions while typing', type: 'toggle', defaultValue: true },
            { id: 'web-search', label: 'Web Search', description: 'Enable web search in AI responses', type: 'toggle', defaultValue: false },
            { id: 'model', label: 'AI Model', description: 'Neural model for AI responses', type: 'select', defaultValue: 'neural-v10', options: ['neural-v10', 'neural-v9', 'mini-transformer', 'cmix'] },
            { id: 'max-tokens', label: 'Max Response Tokens', description: 'Maximum tokens in AI response', type: 'number', defaultValue: 2048, min: 256, max: 8192 },
        ]
    },
    {
        id: 'mcp', label: 'MCP Servers', icon: Server,
        settings: [
            { id: 'auto-connect', label: 'Auto Connect', description: 'Automatically connect to MCP servers on startup', type: 'toggle', defaultValue: true },
            { id: 'timeout', label: 'Connection Timeout (ms)', description: 'Timeout for MCP server connections', type: 'number', defaultValue: 5000, min: 1000, max: 30000 },
            { id: 'retry-count', label: 'Retry Count', description: 'Number of connection retries', type: 'number', defaultValue: 3, min: 0, max: 10 },
        ]
    },
    {
        id: 'git', label: 'Git', icon: GitBranch,
        settings: [
            { id: 'auto-fetch', label: 'Auto Fetch', description: 'Periodically fetch from remote', type: 'toggle', defaultValue: true },
            { id: 'confirm-sync', label: 'Confirm Sync', description: 'Ask before synchronizing Git', type: 'toggle', defaultValue: true },
            { id: 'auto-stash', label: 'Auto Stash', description: 'Stash uncommitted changes before pull', type: 'toggle', defaultValue: false },
            { id: 'default-branch', label: 'Default Branch', description: 'Default branch name for new repos', type: 'text', defaultValue: 'main' },
        ]
    },
    {
        id: 'browser', label: 'Browser', icon: Globe,
        settings: [
            { id: 'homepage', label: 'Homepage', description: 'Default URL for new browser tabs', type: 'text', defaultValue: 'https://www.google.com' },
            { id: 'javascript', label: 'Enable JavaScript', description: 'Allow JavaScript in webview', type: 'toggle', defaultValue: true },
            { id: 'user-agent', label: 'User Agent', description: 'Custom user agent string', type: 'select', defaultValue: 'Default', options: ['Default', 'Chrome', 'Firefox', 'Safari'] },
        ]
    },
    {
        id: 'compress', label: 'Compression', icon: Zap,
        settings: [
            { id: 'algorithm', label: 'Default Algorithm', description: 'Default compression algorithm', type: 'select', defaultValue: 'neural-cmix', options: ['neural-cmix', 'entropy-v10', 'lz77-enhanced', 'huffman-neural'] },
            { id: 'level', label: 'Compression Level', description: 'Compression level (1-9)', type: 'number', defaultValue: 7, min: 1, max: 9 },
            { id: 'show-progress', label: 'Show Progress', description: 'Display compression progress', type: 'toggle', defaultValue: true },
        ]
    },
]

const STORAGE_KEY = 'neural-studio-settings'

function loadSettings(): Record<string, any> {
    try {
        const saved = localStorage.getItem(STORAGE_KEY)
        if (saved) return JSON.parse(saved)
    } catch { }
    // Return defaults
    const defaults: Record<string, any> = {}
    SECTIONS.forEach(s => s.settings.forEach(item => { defaults[item.id] = item.defaultValue }))
    return defaults
}

function saveSettings(settings: Record<string, any>) {
    try {
        localStorage.setItem(STORAGE_KEY, JSON.stringify(settings))
        window.dispatchEvent(new Event('settings-changed'))
    } catch { }
}

export function SettingsPanel() {
    const [activeSection, setActiveSection] = useState('appearance')
    const [settings, setSettings] = useState<Record<string, any>>(loadSettings)
    const [searchQuery, setSearchQuery] = useState('')

    const currentSection = SECTIONS.find(s => s.id === activeSection)

    const updateSetting = (id: string, value: any) => {
        setSettings(prev => {
            const next = { ...prev, [id]: value }
            saveSettings(next)
            return next
        })
    }

    const resetDefaults = () => {
        if (!confirm('Reset all settings to defaults?')) return
        const defaults: Record<string, any> = {}
        SECTIONS.forEach(s => s.settings.forEach(item => { defaults[item.id] = item.defaultValue }))
        setSettings(defaults)
        saveSettings(defaults)
    }

    // Filter all settings by search query
    const filteredSections = searchQuery
        ? SECTIONS.map(section => ({
            ...section,
            settings: section.settings.filter(item =>
                item.label.toLowerCase().includes(searchQuery.toLowerCase()) ||
                item.description.toLowerCase().includes(searchQuery.toLowerCase())
            )
        })).filter(s => s.settings.length > 0)
        : currentSection ? [currentSection] : []

    return (
        <div style={{ display: 'flex', flex: 1, overflow: 'hidden', background: 'var(--bg-surface)' }}>
            {/* Settings sidebar */}
            <div style={{
                width: 220, flexShrink: 0, borderRight: '1px solid var(--border-subtle)',
                overflow: 'auto', background: 'var(--bg-dark)', padding: '12px 0',
                display: 'flex', flexDirection: 'column'
            }}>
                <div style={{
                    padding: '8px 16px', fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)',
                    textTransform: 'uppercase', letterSpacing: '0.8px', flexShrink: 0
                }}>
                    Settings
                </div>
                {/* Search */}
                <div style={{ padding: '4px 12px 8px', flexShrink: 0 }}>
                    <input
                        type="text"
                        placeholder="Search settings..."
                        value={searchQuery}
                        onChange={e => setSearchQuery(e.target.value)}
                        style={{
                            width: '100%', background: 'var(--bg-surface)', border: '1px solid var(--border)',
                            borderRadius: 6, padding: '5px 8px', color: 'var(--text)', fontSize: 11,
                            outline: 'none', fontFamily: 'inherit'
                        }}
                    />
                </div>
                <div style={{ flex: 1, overflow: 'auto' }}>
                    {SECTIONS.map(section => (
                        <button
                            key={section.id}
                            onClick={() => { setActiveSection(section.id); setSearchQuery('') }}
                            style={{
                                display: 'flex', alignItems: 'center', gap: 8, padding: '8px 16px',
                                width: '100%', border: 'none', cursor: 'pointer', fontSize: 13,
                                fontFamily: 'inherit', transition: 'all 0.1s', textAlign: 'left',
                                background: activeSection === section.id && !searchQuery ? 'var(--accent-soft)' : 'transparent',
                                color: activeSection === section.id && !searchQuery ? 'var(--accent)' : 'var(--text-secondary)',
                            }}
                            onMouseEnter={e => { if (activeSection !== section.id) e.currentTarget.style.background = 'var(--bg-hover)' }}
                            onMouseLeave={e => { if (activeSection !== section.id) e.currentTarget.style.background = 'transparent' }}
                        >
                            <section.icon size={15} />
                            {section.label}
                        </button>
                    ))}
                </div>
                {/* Reset button */}
                <div style={{ padding: '8px 12px', borderTop: '1px solid var(--border-subtle)', flexShrink: 0 }}>
                    <button onClick={resetDefaults} style={{
                        display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 6,
                        width: '100%', padding: '6px', borderRadius: 6, border: '1px solid var(--border)',
                        background: 'transparent', color: 'var(--text-muted)', fontSize: 11,
                        cursor: 'pointer', fontFamily: 'inherit', transition: 'all 0.12s'
                    }}
                        onMouseEnter={e => { e.currentTarget.style.background = 'var(--bg-hover)'; e.currentTarget.style.color = 'var(--text)' }}
                        onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--text-muted)' }}
                    >
                        <RotateCcw size={12} /> Reset All Defaults
                    </button>
                </div>
            </div>

            {/* Settings content */}
            <div style={{ flex: 1, overflow: 'auto', padding: '24px 32px' }}>
                {filteredSections.map(section => (
                    <div key={section.id} style={{ marginBottom: 32 }}>
                        <h2 style={{ fontSize: 20, fontWeight: 700, marginBottom: 4, display: 'flex', alignItems: 'center', gap: 8 }}>
                            <section.icon size={20} style={{ color: 'var(--accent)' }} />
                            {section.label}
                        </h2>
                        <div style={{ color: 'var(--text-muted)', fontSize: 13, marginBottom: 24 }}>
                            Configure {section.label.toLowerCase()} settings
                        </div>

                        {section.settings.map(item => (
                            <div key={item.id} style={{
                                padding: '16px 0', borderBottom: '1px solid var(--border-subtle)',
                                display: 'flex', alignItems: 'center', gap: 16
                            }}>
                                <div style={{ flex: 1 }}>
                                    <div style={{ fontSize: 13, fontWeight: 600, color: 'var(--text)' }}>
                                        {item.label}
                                    </div>
                                    <div style={{ fontSize: 12, color: 'var(--text-muted)', marginTop: 2 }}>
                                        {item.description}
                                    </div>
                                </div>

                                {item.type === 'toggle' && (
                                    <div
                                        onClick={() => updateSetting(item.id, !settings[item.id])}
                                        style={{
                                            width: 40, height: 22, borderRadius: 11, cursor: 'pointer',
                                            background: settings[item.id] ? 'var(--accent)' : 'var(--bg-hover)',
                                            border: `1px solid ${settings[item.id] ? 'var(--accent)' : 'var(--border)'}`,
                                            position: 'relative', transition: 'all 0.2s', flexShrink: 0,
                                        }}
                                    >
                                        <div style={{
                                            width: 16, height: 16, borderRadius: '50%', background: '#fff',
                                            position: 'absolute', top: 2,
                                            left: settings[item.id] ? 20 : 2,
                                            transition: 'left 0.2s',
                                            boxShadow: '0 1px 3px rgba(0,0,0,0.3)'
                                        }} />
                                    </div>
                                )}

                                {item.type === 'select' && (
                                    <select
                                        value={settings[item.id]}
                                        onChange={e => updateSetting(item.id, e.target.value)}
                                        style={{
                                            padding: '6px 10px', borderRadius: 6, border: '1px solid var(--border)',
                                            background: 'var(--bg-card)', color: 'var(--text)', fontSize: 12,
                                            outline: 'none', cursor: 'pointer', fontFamily: 'inherit', minWidth: 150,
                                        }}
                                    >
                                        {item.options?.map(opt => (
                                            <option key={opt} value={opt}>{opt}</option>
                                        ))}
                                    </select>
                                )}

                                {item.type === 'text' && (
                                    <input
                                        type="text"
                                        value={settings[item.id]}
                                        onChange={e => updateSetting(item.id, e.target.value)}
                                        style={{
                                            padding: '6px 10px', borderRadius: 6, border: '1px solid var(--border)',
                                            background: 'var(--bg-card)', color: 'var(--text)', fontSize: 12,
                                            outline: 'none', fontFamily: 'inherit', minWidth: 200,
                                        }}
                                    />
                                )}

                                {item.type === 'number' && (
                                    <input
                                        type="number"
                                        value={settings[item.id]}
                                        onChange={e => updateSetting(item.id, parseInt(e.target.value) || 0)}
                                        min={item.min}
                                        max={item.max}
                                        style={{
                                            padding: '6px 10px', borderRadius: 6, border: '1px solid var(--border)',
                                            background: 'var(--bg-card)', color: 'var(--text)', fontSize: 12,
                                            outline: 'none', fontFamily: 'inherit', width: 80,
                                        }}
                                    />
                                )}
                            </div>
                        ))}
                    </div>
                ))}
            </div>
        </div>
    )
}
