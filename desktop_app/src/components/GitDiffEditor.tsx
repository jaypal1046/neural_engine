import { useEffect, useState } from 'react'
import { DiffEditor } from '@monaco-editor/react'
import type { DiffOnMount } from '@monaco-editor/react'
import type { editor } from 'monaco-editor'
import { X } from 'lucide-react'

// Reuse the themes and settings helpers from MonacoEditor (or duplicate)
function getEditorSettings(): Record<string, any> {
    try {
        const saved = localStorage.getItem('neural-studio-settings')
        if (saved) return JSON.parse(saved)
    } catch { }
    return {}
}

const NEURAL_DARK_THEME: editor.IStandaloneThemeData = {
    base: 'vs-dark',
    inherit: true,
    rules: [
        { token: 'comment', foreground: '5c6370', fontStyle: 'italic' },
        { token: 'keyword', foreground: 'C97FDB' },
        { token: 'string', foreground: '98C379' },
        { token: 'number', foreground: 'D19A66' },
        { token: 'type', foreground: '4A9EFF' },
        { token: 'class', foreground: 'E5C07B' },
        { token: 'function', foreground: '61AFEF' },
        { token: 'variable', foreground: 'E06C75' },
    ],
    colors: {
        'editor.background': '#1A1D23',
        'editor.foreground': '#ABB2BF',
        'editor.lineHighlightBackground': '#1E2228',
        'editor.selectionBackground': '#3E4451',
        'editor.inactiveSelectionBackground': '#2C313A',
        'diffEditor.insertedTextBackground': '#4ADE8020',
        'diffEditor.removedTextBackground': '#EF444420',
        'diffEditor.insertedLineBackground': '#4ADE8010',
        'diffEditor.removedLineBackground': '#EF444410',
        'diffEditorGutter.insertedLineBackground': '#4ADE8020',
        'diffEditorGutter.removedLineBackground': '#EF444420',
        'diffEditorOverview.insertedForeground': '#4ADE8080',
        'diffEditorOverview.removedForeground': '#EF444480',
    },
}

function getLanguage(filePath: string): string {
    const ext = filePath.split('.').pop()?.toLowerCase() || ''
    const map: Record<string, string> = {
        ts: 'typescript', tsx: 'typescript', js: 'javascript', jsx: 'javascript',
        json: 'json', html: 'html', css: 'css', scss: 'scss', md: 'markdown',
        py: 'python', rs: 'rust', go: 'go', java: 'java', cpp: 'cpp', c: 'c',
        cs: 'csharp', rb: 'ruby', php: 'php', swift: 'swift', kt: 'kotlin',
        dart: 'dart', yaml: 'yaml', yml: 'yaml', xml: 'xml', sql: 'sql', sh: 'shell'
    }
    return map[ext] || 'plaintext'
}

interface Props {
    projectRoot: string
    filePath: string // Relative path inside git project
    absolutePath: string // Absolute path for fs
    onClose: () => void
}

export function GitDiffEditor({ projectRoot, filePath, absolutePath, onClose }: Props) {
    const [original, setOriginal] = useState<string>('')
    const [modified, setModified] = useState<string>('')
    const [loading, setLoading] = useState(true)
    const [settings, setSettings] = useState(getEditorSettings())

    // Load content
    useEffect(() => {
        const load = async () => {
            setLoading(true)
            try {
                // Modified from disk
                if (window.fs?.readFile) {
                    const local = await window.fs.readFile(absolutePath)
                    setModified(typeof local === 'string' ? local : '')
                }
                // Original from git HEAD
                if (window.gitApi?.getFile) {
                    const head = await window.gitApi.getFile(projectRoot, 'HEAD', filePath)
                    setOriginal(head.content || '')
                }
            } catch (e) {
                console.error('Failed to load diff content:', e)
            } finally {
                setLoading(false)
            }
        }
        load()
    }, [projectRoot, filePath, absolutePath])

    useEffect(() => {
        const handleSettingsChange = () => setSettings(getEditorSettings())
        window.addEventListener('settings-changed', handleSettingsChange)
        return () => window.removeEventListener('settings-changed', handleSettingsChange)
    }, [])

    const handleEditorMount: DiffOnMount = (_editor, monaco) => {
        monaco.editor.defineTheme('neural-dark', NEURAL_DARK_THEME)
        monaco.editor.setTheme('neural-dark')
    }

    if (loading) {
        return (
            <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#1A1D23', color: '#6B7280' }}>
                <div className="spin" style={{ marginRight: 8 }}>
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#4A9EFF" strokeWidth="2">
                        <path d="M21 12a9 9 0 11-6.219-8.56" />
                    </svg>
                </div>
                Loading diff...
            </div>
        )
    }

    const language = getLanguage(filePath)

    return (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', background: '#1A1D23', overflow: 'hidden' }}>
            {/* Toolbar */}
            <div style={{
                padding: '4px 16px', fontSize: 12, color: '#ABB2BF',
                borderBottom: '1px solid #2A2D35', display: 'flex', alignItems: 'center', justifyContent: 'space-between',
                background: '#1A1D23', flexShrink: 0, height: 36
            }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 8 }}>
                    <span style={{ fontWeight: 600 }}>Diff:</span>
                    <span style={{ color: '#495162' }}>{filePath}</span>
                    <span style={{ fontSize: 10, padding: '1px 6px', borderRadius: 4, background: '#4A9EFF15', color: '#4A9EFF', textTransform: 'uppercase' }}>
                        {language}
                    </span>
                </div>
                <button
                    onClick={onClose}
                    style={{
                        background: 'transparent', border: 'none', color: '#6B7280', cursor: 'pointer',
                        display: 'flex', alignItems: 'center', justifyContent: 'center', padding: 4, borderRadius: 4
                    }}
                >
                    <X size={16} />
                </button>
            </div>

            {/* Diff Editor */}
            <DiffEditor
                height="100%"
                language={language}
                original={original}
                modified={modified}
                theme="neural-dark"
                onMount={handleEditorMount}
                options={{
                    fontSize: settings['font-size'] || 14,
                    fontFamily: `'${settings['font-family'] || 'JetBrains Mono'}', monospace`,
                    wordWrap: settings['word-wrap'] ? 'on' : 'off',
                    minimap: { enabled: settings['minimap'] !== false },
                    lineNumbers: settings['line-numbers'] !== false ? 'on' : 'off',
                    scrollBeyondLastLine: false,
                    renderSideBySide: true,
                    readOnly: true, // Typically diff editors are read-only for now
                    renderIndicators: true,
                }}
            />
        </div>
    )
}
