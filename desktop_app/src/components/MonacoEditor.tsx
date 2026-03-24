import { useRef, useEffect, useState, useCallback } from 'react'
import Editor from '@monaco-editor/react'
import type { OnMount } from '@monaco-editor/react'
import type { editor } from 'monaco-editor'
import { Breadcrumbs } from './Breadcrumbs'
import { readFile, writeFile } from '../lib/desktopBridge'

interface EditorSymbol {
    name: string
    kind: string
    line: number
}

// Load settings from localStorage
function getEditorSettings(): Record<string, any> {
    try {
        const saved = localStorage.getItem('neural-studio-settings')
        if (saved) return JSON.parse(saved)
    } catch { }
    return {}
}

// Map file extensions to Monaco language IDs
export function getLanguage(filePath: string): string {
    const ext = filePath.split('.').pop()?.toLowerCase() || ''
    const map: Record<string, string> = {
        ts: 'typescript',
        tsx: 'typescript',
        js: 'javascript',
        jsx: 'javascript',
        mjs: 'javascript',
        cjs: 'javascript',
        json: 'json',
        html: 'html',
        htm: 'html',
        css: 'css',
        scss: 'scss',
        less: 'less',
        md: 'markdown',
        py: 'python',
        rs: 'rust',
        go: 'go',
        java: 'java',
        cpp: 'cpp',
        c: 'c',
        h: 'cpp',
        hpp: 'cpp',
        cs: 'csharp',
        rb: 'ruby',
        php: 'php',
        swift: 'swift',
        kt: 'kotlin',
        dart: 'dart',
        yaml: 'yaml',
        yml: 'yaml',
        xml: 'xml',
        sql: 'sql',
        sh: 'shell',
        bash: 'shell',
        bat: 'bat',
        ps1: 'powershell',
        r: 'r',
        lua: 'lua',
        toml: 'ini',
        ini: 'ini',
        env: 'ini',
        dockerfile: 'dockerfile',
        makefile: 'makefile',
        graphql: 'graphql',
        vue: 'html',
        svelte: 'html',
    }
    return map[ext] || 'plaintext'
}

function extractSymbolsFromSource(text: string): EditorSymbol[] {
    const lines = text.split('\n')
    const found: EditorSymbol[] = []
    const patterns = [
        { regex: /(?:export\s+)?(?:async\s+)?function\s+(\w+)/g, kind: 'function', captureIndex: 1 },
        { regex: /(?:export\s+)?class\s+(\w+)/g, kind: 'class', captureIndex: 1 },
        { regex: /(?:export\s+)?(?:const|let|var)\s+(\w+)\s*=/g, kind: 'variable', captureIndex: 1 },
        { regex: /(?:export\s+)?interface\s+(\w+)/g, kind: 'class', captureIndex: 1 },
        { regex: /(?:export\s+)?type\s+(\w+)\s*=/g, kind: 'class', captureIndex: 1 },
        { regex: /(?:export\s+)?enum\s+(\w+)/g, kind: 'class', captureIndex: 1 },
        { regex: /def\s+(\w+)\s*\(/g, kind: 'function', captureIndex: 1 },
        { regex: /class\s+(\w+)\s*[:(]/g, kind: 'class', captureIndex: 1 },
        { regex: /\b([A-Za-z_][A-Za-z0-9_:<>~]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*\([^;]*\)\s*\{/g, kind: 'function', captureIndex: 2 },
    ]

    for (let i = 0; i < lines.length; i++) {
        for (const pattern of patterns) {
            pattern.regex.lastIndex = 0
            let match: RegExpExecArray | null
            while ((match = pattern.regex.exec(lines[i])) !== null) {
                const name = match[pattern.captureIndex]
                if (name) {
                    found.push({ name, kind: pattern.kind, line: i + 1 })
                }
            }
        }
    }

    return found
}

// Neural Studio dark theme definition
const NEURAL_DARK_THEME: editor.IStandaloneThemeData = {
    base: 'vs-dark',
    inherit: true,
    rules: [
        { token: 'comment', foreground: '5c6370', fontStyle: 'italic' },
        { token: 'keyword', foreground: 'C97FDB' },
        { token: 'string', foreground: '98C379' },
        { token: 'number', foreground: 'D19A66' },
        { token: 'regexp', foreground: 'E5C07B' },
        { token: 'type', foreground: '4A9EFF' },
        { token: 'class', foreground: 'E5C07B' },
        { token: 'function', foreground: '61AFEF' },
        { token: 'variable', foreground: 'E06C75' },
        { token: 'constant', foreground: 'D19A66' },
        { token: 'parameter', foreground: 'ABB2BF' },
        { token: 'tag', foreground: 'E06C75' },
        { token: 'attribute.name', foreground: 'D19A66' },
        { token: 'attribute.value', foreground: '98C379' },
        { token: 'delimiter', foreground: 'ABB2BF' },
        { token: 'operator', foreground: '56B6C2' },
    ],
    colors: {
        'editor.background': '#1A1D23',
        'editor.foreground': '#ABB2BF',
        'editor.lineHighlightBackground': '#1E2228',
        'editor.selectionBackground': '#3E4451',
        'editor.inactiveSelectionBackground': '#2C313A',
        'editorCursor.foreground': '#4A9EFF',
        'editorWhitespace.foreground': '#3B4048',
        'editorIndentGuide.background': '#3B4048',
        'editorIndentGuide.activeBackground': '#4B5263',
        'editor.selectionHighlightBackground': '#3E445180',
        'editorBracketMatch.background': '#3E445180',
        'editorBracketMatch.border': '#4A9EFF40',
        'editorLineNumber.foreground': '#495162',
        'editorLineNumber.activeForeground': '#6B7280',
        'editorGutter.background': '#1A1D23',
        'minimap.background': '#1A1D23',
        'scrollbar.shadow': '#00000000',
        'scrollbarSlider.background': '#ffffff10',
        'scrollbarSlider.hoverBackground': '#ffffff20',
        'scrollbarSlider.activeBackground': '#ffffff30',
        'editorOverviewRuler.border': '#00000000',
        'editor.findMatchBackground': '#FFD70040',
        'editor.findMatchHighlightBackground': '#FFD70020',
        'editorWidget.background': '#21252B',
        'editorWidget.border': '#3E4451',
        'editorSuggestWidget.background': '#21252B',
        'editorSuggestWidget.border': '#3E4451',
        'editorSuggestWidget.selectedBackground': '#2C313A',
        'editorHoverWidget.background': '#21252B',
        'editorHoverWidget.border': '#3E4451',
        'peekViewEditor.background': '#1B1D23',
        'peekViewResult.background': '#21252B',
    },
}

interface Props {
    filePath: string
    onModified: (modified: boolean) => void
    projectRoot?: string
}

export function MonacoEditor({ filePath, onModified, projectRoot }: Props) {
    const [content, setContent] = useState<string | null>(null)
    const [savedContent, setSavedContent] = useState<string>('')
    const [loading, setLoading] = useState(true)
    const [saving, setSaving] = useState(false)
    const [saveStatus, setSaveStatus] = useState('')
    const editorRef = useRef<editor.IStandaloneCodeEditor | null>(null)
    const monacoRef = useRef<any>(null)

    const [cursorLine, setCursorLine] = useState(1)
    const [cursorCol, setCursorCol] = useState(1)
    const [currentSymbolName, setCurrentSymbolName] = useState('')
    const [selectionInfo, setSelectionInfo] = useState<{
        startLine: number
        startColumn: number
        endLine: number
        endColumn: number
        selectedText: string
    } | null>(null)

    // AI Generate State (Ctrl+K)
    const [aiOverlayVisible, setAiOverlayVisible] = useState(false)
    const [aiOverlayTop, setAiOverlayTop] = useState(0)
    const [aiOverlayLeft, setAiOverlayLeft] = useState(0)
    const [aiQuery, setAiQuery] = useState('')
    const [aiLoading, setAiLoading] = useState(false)
    const aiInputRef = useRef<HTMLInputElement>(null)

    // AI Popup State (Ctrl+I for edit, Ctrl+Shift+E for explain)
    const [aiPopupVisible, setAiPopupVisible] = useState(false)
    const [aiPopupMode, setAiPopupMode] = useState<'edit' | 'explain'>('edit')
    const [aiPopupQuery, setAiPopupQuery] = useState('')
    const [aiPopupLoading, setAiPopupLoading] = useState(false)
    const [aiPopupResult, setAiPopupResult] = useState<string | null>(null)
    const [aiPopupOriginal, setAiPopupOriginal] = useState('')
    const [aiPopupTop, setAiPopupTop] = useState(0)
    const aiPopupInputRef = useRef<HTMLInputElement>(null)

    const autocompleteDisposable = useRef<any>(null)
    const [settings, setSettings] = useState(getEditorSettings())

    useEffect(() => {
        const handleSettingsChange = () => setSettings(getEditorSettings())
        window.addEventListener('settings-changed', handleSettingsChange)
        return () => window.removeEventListener('settings-changed', handleSettingsChange)
    }, [])

    const fontSize = settings['font-size'] || 14
    const fontFamily = settings['font-family'] || 'JetBrains Mono'
    const tabSize = settings['tab-size'] || 4
    const wordWrap = settings['word-wrap'] ? 'on' : 'off'
    const minimap = settings['minimap'] !== false
    const lineNumbers = settings['line-numbers'] !== false
    const bracketPairs = settings['bracket-pairs'] !== false

    const normalizeFilePath = useCallback((value: string) => value.replace(/\//g, '\\').toLowerCase(), [])

    const buildNearbySnippet = useCallback((source: string | null, lineNumber: number) => {
        if (!source) return ''
        const lines = source.split('\n')
        const start = Math.max(0, lineNumber - 4)
        const end = Math.min(lines.length, lineNumber + 3)
        return lines.slice(start, end).join('\n').slice(0, 2000)
    }, [])

    // ── AI Popup (Edit/Explain) Logic ──
    const openAiPopup = useCallback((mode: 'edit' | 'explain' = 'edit') => {
        if (!editorRef.current) return
        const editor = editorRef.current
        const selection = editor.getSelection()
        const position = editor.getPosition()

        let original = ''
        if (selection && !selection.isEmpty()) {
            original = editor.getModel()?.getValueInRange(selection) || ''
        } else if (position) {
            original = editor.getModel()?.getLineContent(position.lineNumber) || ''
        }

        setAiPopupMode(mode)
        setAiPopupOriginal(original)
        setAiPopupResult(null)
        setAiPopupQuery('')

        const coords = position ? editor.getScrolledVisiblePosition(position) : null
        setAiPopupTop(coords ? coords.top : 40)
        setAiPopupVisible(true)
        setTimeout(() => aiPopupInputRef.current?.focus(), 50)
    }, [])

    const rejectAiPopup = useCallback(() => {
        setAiPopupVisible(false)
        setAiPopupResult(null)
        setAiPopupQuery('')
        editorRef.current?.focus()
    }, [])

    const handleAiPopupSubmit = useCallback(async () => {
        if (!aiPopupQuery.trim() || aiPopupLoading) return
        setAiPopupLoading(true)

        const lang = getLanguage(filePath)
        const fileName = filePath.split(/[/\\]/).pop() || 'file'

        const editorContext = {
            filePath,
            fileName,
            language: lang,
            cursorLine,
            cursorColumn: cursorCol,
            selection: selectionInfo ? {
                startLine: selectionInfo.startLine,
                startColumn: selectionInfo.startColumn,
                endLine: selectionInfo.endLine,
                endColumn: selectionInfo.endColumn,
            } : null,
            selectedText: aiPopupOriginal,
            nearbySnippet: buildNearbySnippet(content, cursorLine),
        }

        try {
            const res = await fetch('http://127.0.0.1:8001/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: aiPopupMode === 'edit'
                        ? `Edit this code: ${aiPopupQuery}`
                        : `Explain this code: ${aiPopupQuery}`,
                    workspace_root: projectRoot,
                    editor_context: editorContext
                }),
            })

            if (res.ok) {
                const data = await res.json()
                let result = data.response || data.reply || data.content || ''
                if (aiPopupMode === 'edit') {
                    result = result.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()
                }
                setAiPopupResult(result)
            } else {
                setAiPopupResult('// AI service unavailable')
            }
        } catch {
            setAiPopupResult('// Could not reach AI service')
        }
        setAiPopupLoading(false)
    }, [aiPopupQuery, aiPopupLoading, filePath, cursorLine, cursorCol, selectionInfo, aiPopupOriginal, content, buildNearbySnippet, aiPopupMode, projectRoot])

    const acceptAiPopup = useCallback(() => {
        if (!aiPopupResult || !editorRef.current || !monacoRef.current || aiPopupMode !== 'edit') return
        const editor = editorRef.current
        const monaco = monacoRef.current
        const selection = editor.getSelection()
        const position = editor.getPosition()

        if (selection && !selection.isEmpty()) {
            editor.executeEdits('ai-popup-edit', [{ range: selection, text: aiPopupResult, forceMoveMarkers: true }])
        } else if (position) {
            const lineLength = editor.getModel()?.getLineMaxColumn(position.lineNumber) || 1
            const range = new monaco.Range(position.lineNumber, 1, position.lineNumber, lineLength)
            editor.executeEdits('ai-popup-edit', [{ range, text: aiPopupResult, forceMoveMarkers: true }])
        }

        setAiPopupVisible(false)
        setAiPopupResult(null)
        setAiPopupQuery('')
        editor.focus()
    }, [aiPopupResult, aiPopupMode])

    const sendToAI = useCallback((text: string, action: string) => {
        const fileName = filePath.split(/[/\\]/).pop() || 'file'
        const lang = getLanguage(filePath)
        const codeBlock = `\`\`\`${lang}\n${text}\n\`\`\``
        const prompts: Record<string, string> = {
            explain: `Explain this code from \`${fileName}\`:\n\n${codeBlock}`,
            refactor: `Refactor this code from \`${fileName}\` for better readability and performance:\n\n${codeBlock}`,
            tests: `Generate comprehensive unit tests for this code from \`${fileName}\`:\n\n${codeBlock}`,
            fix: `Find and fix any bugs or issues in this code from \`${fileName}\`:\n\n${codeBlock}`,
            optimize: `Optimize this code from \`${fileName}\` for performance:\n\n${codeBlock}`,
        }
        if (action === 'explain') {
            openAiPopup('explain')
        } else {
            window.dispatchEvent(new CustomEvent('ai-chat-query', { detail: { text: prompts[action] || prompts.explain } }))
            window.dispatchEvent(new CustomEvent('open-ai-chat'))
        }
    }, [filePath, openAiPopup])

    const handleEditorMount: OnMount = (editor, monaco) => {
        editorRef.current = editor
        monacoRef.current = monaco
        monaco.editor.defineTheme('neural-dark', NEURAL_DARK_THEME)
        monaco.editor.setTheme('neural-dark')
        editor.focus()

        editor.onDidChangeCursorPosition((e) => {
            setCursorLine(e.position.lineNumber)
            setCursorCol(e.position.column)
        })

        editor.onDidChangeCursorSelection((e) => {
            const selection = e.selection
            if (!selection || selection.isEmpty()) {
                setSelectionInfo(null)
                return
            }
            const selectedText = editor.getModel()?.getValueInRange(selection) || ''
            setSelectionInfo({
                startLine: selection.startLineNumber,
                startColumn: selection.startColumn,
                endLine: selection.endLineNumber,
                endColumn: selection.endColumn,
                selectedText: selectedText.slice(0, 4000),
            })
        })

        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
            saveFile()
        })

        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyK, () => {
            const position = editor.getPosition()
            if (!position) return
            const coords = editor.getScrolledVisiblePosition(position)
            setAiOverlayTop(coords ? coords.top + 35 : 40)
            setAiOverlayLeft(coords ? Math.max(20, coords.left) : 40)
            setAiOverlayVisible(true)
            setTimeout(() => aiInputRef.current?.focus(), 50)
        })

        editor.addAction({
            id: 'ai-edit-inline',
            label: '🪄 AI: Edit Inline (Ctrl+I)',
            contextMenuGroupId: '9_ai',
            contextMenuOrder: 0,
            keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyI],
            run: () => openAiPopup('edit')
        })

        editor.addAction({
            id: 'ai-explain-selection',
            label: '🧠 AI: Explain Selection (Ctrl+Shift+E)',
            contextMenuGroupId: '9_ai',
            contextMenuOrder: 1,
            keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyMod.Shift | monaco.KeyCode.KeyE],
            run: () => openAiPopup('explain')
        })

        const addAIAction = (id: string, label: string, order: number) => {
            editor.addAction({
                id: `ai-${id}`,
                label,
                contextMenuGroupId: '9_ai',
                contextMenuOrder: order,
                run: (ed) => {
                    const selection = ed.getSelection()
                    if (!selection) return
                    const text = ed.getModel()?.getValueInRange(selection)
                    if (text && text.trim().length > 0) {
                        sendToAI(text, id)
                    } else {
                        window.dispatchEvent(new CustomEvent('open-ai-chat'))
                    }
                }
            })
        }
        addAIAction('explain', '🧠 AI: Explain Code', 2)
        addAIAction('refactor', '✨ AI: Refactor', 3)
        addAIAction('fix', '🔧 AI: Fix Issues', 4)
        addAIAction('tests', '🧪 AI: Generate Tests', 5)
        addAIAction('optimize', '⚡ AI: Optimize', 6)

        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyG, () => {
            window.dispatchEvent(new CustomEvent('open-quick-open', { detail: { mode: 'line', initialQuery: ':' } }))
        })

        if (autocompleteDisposable.current) autocompleteDisposable.current.dispose()
        let autocompleteTimer: any = null
        autocompleteDisposable.current = monaco.languages.registerInlineCompletionsProvider('*', {
            provideInlineCompletions: async (model, position, _context, token) => {
                if (autocompleteTimer) clearTimeout(autocompleteTimer)
                return new Promise((resolve) => {
                    autocompleteTimer = setTimeout(async () => {
                        if (token.isCancellationRequested) return resolve({ items: [] })
                        const prefix = model.getValueInRange({ startLineNumber: Math.max(1, position.lineNumber - 15), startColumn: 1, endLineNumber: position.lineNumber, endColumn: position.column })
                        const suffix = model.getValueInRange({ startLineNumber: position.lineNumber, startColumn: position.column, endLineNumber: Math.min(model.getLineCount(), position.lineNumber + 5), endColumn: 999 })
                        if (prefix.trim().length < 8) return resolve({ items: [] })
                        try {
                            const res = await fetch('http://127.0.0.1:8001/api/chat', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({
                                    message: `Complete this code: ${prefix}`,
                                    editor_context: { filePath, language: getLanguage(filePath), selectedText: prefix }
                                })
                            })
                            if (res.ok) {
                                const data = await res.json()
                                let completion = data.response || data.reply || data.content || ''
                                completion = completion.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()
                                if (completion) return resolve({ items: [{ insertText: completion, range: new monaco.Range(position.lineNumber, position.column, position.lineNumber, position.column) }] })
                            }
                        } catch { }
                        resolve({ items: [] })
                    }, 800)
                })
            },
            freeInlineCompletions: () => { }
        })
    }

    const saveFile = useCallback(async () => {
        if (content === null) return
        setSaving(true)
        try {
            await writeFile(filePath, content)
            setSavedContent(content)
            setSaveStatus('Saved ✓')
            setTimeout(() => setSaveStatus(''), 2000)
        } catch {
            setSaveStatus('Save failed!')
        }
        setSaving(false)
    }, [filePath, content])

    useEffect(() => {
        setLoading(true)
        setContent(null)
        setSelectionInfo(null)
        readFile(filePath).then((res: any) => {
            const data = typeof res === 'string' ? res : '// Error loading'
            setContent(data)
            setSavedContent(data)
            setLoading(false)
        })
    }, [filePath])

    useEffect(() => {
        if (content !== null) onModified(content !== savedContent)
    }, [content, savedContent, onModified])

    useEffect(() => {
        if (!window.appApi?.updateEditorContext || !filePath || content === null) return
        const timer = setTimeout(() => {
            window.appApi.updateEditorContext({
                filePath,
                fileName: filePath.split(/[\\/]/).pop() || filePath,
                language: getLanguage(filePath),
                cursorLine, cursorColumn: cursorCol,
                selection: selectionInfo ? { startLine: selectionInfo.startLine, startColumn: selectionInfo.startColumn, endLine: selectionInfo.endLine, endColumn: selectionInfo.endColumn } : null,
                selectedText: selectionInfo?.selectedText || '',
                currentSymbolName,
                nearbySnippet: buildNearbySnippet(content, cursorLine),
            }).catch(() => { })
        }, 300)
        return () => clearTimeout(timer)
    }, [filePath, content, cursorLine, cursorCol, selectionInfo, currentSymbolName, buildNearbySnippet])

    useEffect(() => {
        const handleApply = (e: any) => {
            if (!e.detail?.code || !editorRef.current || !monacoRef.current) return
            const editor = editorRef.current
            const monaco = monacoRef.current
            const sel = editor.getSelection()
            if (sel && !sel.isEmpty()) editor.executeEdits('ai', [{ range: sel, text: e.detail.code, forceMoveMarkers: true }])
            else {
                const pos = editor.getPosition()
                if (pos) editor.executeEdits('ai', [{ range: new monaco.Range(pos.lineNumber, pos.column, pos.lineNumber, pos.column), text: e.detail.code, forceMoveMarkers: true }])
            }
            editor.focus()
        }
        window.addEventListener('ai-apply-code', handleApply)
        return () => window.removeEventListener('ai-apply-code', handleApply)
    }, [])

    const handleNavigateSymbol = useCallback((line: number) => {
        if (editorRef.current) {
            editorRef.current.revealLineInCenter(line)
            editorRef.current.setPosition({ lineNumber: line, column: 1 })
            editorRef.current.focus()
        }
    }, [])

    if (loading || content === null) return <div className="editor-loading">Loading...</div>

    const language = getLanguage(filePath)
    const isModified = content !== savedContent

    return (
        <div className="monaco-editor-container" style={{ flex: 1, display: 'flex', flexDirection: 'column', position: 'relative', overflow: 'hidden' }}>
            <Breadcrumbs filePath={filePath} language={language} projectRoot={projectRoot || ''} editorRef={editorRef} monacoRef={monacoRef} cursorLine={cursorLine} cursorCol={cursorCol} onNavigateSymbol={handleNavigateSymbol} />
            
            {(isModified || saveStatus) && (
                <div className="editor-status-bar">
                    {isModified && <span className="modified-dot">● Modified</span>}
                    {saveStatus && <span className="save-status">{saveStatus}</span>}
                    <button onClick={saveFile} disabled={saving || !isModified} className="save-btn">{saving ? 'Saving...' : 'Save'}</button>
                </div>
            )}

            {/* AI Generate Overlay (Ctrl+K) */}
            {aiOverlayVisible && (
                <div className="ai-overlay ai-generate-overlay" style={{ top: aiOverlayTop, left: aiOverlayLeft }}>
                    <div className="ai-overlay-header">✨ AI Generate <kbd>Ctrl+K</kbd></div>
                    <div className="ai-overlay-input-row">
                        <input ref={aiInputRef} type="text" value={aiQuery} onChange={e => setAiQuery(e.target.value)} placeholder="Describe code to generate..." className="ai-overlay-input" onKeyDown={async (e) => {
                            if (e.key === 'Escape') { setAiOverlayVisible(false); editorRef.current?.focus(); }
                            else if (e.key === 'Enter') {
                                if (!aiQuery.trim() || aiLoading) return
                                setAiLoading(true)
                                try {
                                    const res = await fetch('http://127.0.0.1:8001/api/chat', {
                                        method: 'POST',
                                        headers: { 'Content-Type': 'application/json' },
                                        body: JSON.stringify({ message: aiQuery, editor_context: { filePath, language: getLanguage(filePath) } })
                                    })
                                    if (res.ok) {
                                        const data = await res.json()
                                        let code = data.response || data.reply || data.content || ''
                                        code = code.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()
                                        const editor = editorRef.current
                                        if (editor) {
                                            const sel = editor.getSelection()
                                            if (sel && !sel.isEmpty()) editor.executeEdits('ai', [{ range: sel, text: code }])
                                            else {
                                                const pos = editor.getPosition()
                                                if (pos) editor.executeEdits('ai', [{ range: new monacoRef.current.Range(pos.lineNumber, pos.column, pos.lineNumber, pos.column), text: code }])
                                            }
                                        }
                                    }
                                } catch { }
                                setAiLoading(false); setAiOverlayVisible(false); setAiQuery(''); editorRef.current?.focus();
                            }
                        }} />
                        {aiLoading && <div className="ai-spinner" />}
                    </div>
                </div>
            )}

            {/* Unified AI Popup */}
            {aiPopupVisible && (
                <div className="ai-overlay ai-inline-edit-overlay" style={{ top: aiPopupTop }}>
                    <div className="ai-overlay-header">
                        <span>{aiPopupMode === 'edit' ? '🪄 AI Edit' : '🧠 AI Explain'}</span>
                        <kbd>{aiPopupMode === 'edit' ? 'Ctrl+I' : 'Ctrl+Shift+E'}</kbd>
                        <button onClick={rejectAiPopup}>✕</button>
                    </div>
                    {!aiPopupResult && (
                        <div className="ai-overlay-input-row">
                            <input ref={aiPopupInputRef} type="text" value={aiPopupQuery} onChange={e => setAiPopupQuery(e.target.value)} placeholder={aiPopupMode === 'edit' ? "Describe edit..." : "Ask..."} className="ai-overlay-input" onKeyDown={e => { if (e.key === 'Escape') rejectAiPopup(); else if (e.key === 'Enter') handleAiPopupSubmit(); }} />
                            {aiPopupLoading && <div className="ai-spinner" />}
                        </div>
                    )}
                    {aiPopupResult && (
                        <div className="ai-popup-result">
                            {aiPopupMode === 'edit' ? (
                                <div className="ai-inline-diff">
                                    <pre className="removed">{aiPopupOriginal}</pre>
                                    <pre className="added">{aiPopupResult}</pre>
                                    <div className="actions">
                                        <button onClick={acceptAiPopup}>Accept</button>
                                        <button onClick={rejectAiPopup}>Reject</button>
                                    </div>
                                </div>
                            ) : (
                                <div className="ai-explanation">
                                    <div className="content">{aiPopupResult}</div>
                                    <button onClick={rejectAiPopup}>Close</button>
                                </div>
                            )}
                        </div>
                    )}
                </div>
            )}

            <Editor height="100%" language={language} value={content} theme="neural-dark" onChange={val => setContent(val || '')} onMount={handleEditorMount} options={{ fontSize, fontFamily, tabSize, wordWrap, minimap: { enabled: minimap }, lineNumbers, bracketPairColorization: { enabled: bracketPairs }, automaticLayout: true, padding: { top: 12, bottom: 12 } }} />
        </div>
    )
}
