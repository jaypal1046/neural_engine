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

    // Cursor position tracking
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

    // AI Inline Generate State (Ctrl+K)
    const [aiOverlayVisible, setAiOverlayVisible] = useState(false)
    const [aiOverlayTop, setAiOverlayTop] = useState(0)
    const [aiOverlayLeft, setAiOverlayLeft] = useState(0)
    const [aiQuery, setAiQuery] = useState('')
    const [aiLoading, setAiLoading] = useState(false)
    const aiInputRef = useRef<HTMLInputElement>(null)

    // AI Inline Edit State (Ctrl+I) — with diff preview
    const [inlineEditVisible, setInlineEditVisible] = useState(false)
    const [inlineEditQuery, setInlineEditQuery] = useState('')
    const [inlineEditLoading, setInlineEditLoading] = useState(false)
    const [inlineEditResult, setInlineEditResult] = useState<string | null>(null)
    const [inlineEditOriginal, setInlineEditOriginal] = useState('')
    const [inlineEditTop, setInlineEditTop] = useState(0)
    const inlineEditInputRef = useRef<HTMLInputElement>(null)

    // AI Autocomplete state
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

    const normalizeFilePath = useCallback((value: string) => {
        return value.replace(/\//g, '\\').toLowerCase()
    }, [])

    const buildNearbySnippet = useCallback((source: string | null, lineNumber: number) => {
        if (!source) return ''
        const lines = source.split('\n')
        const start = Math.max(0, lineNumber - 4)
        const end = Math.min(lines.length, lineNumber + 3)
        return lines.slice(start, end).join('\n').slice(0, 2000)
    }, [])

    // Load file content
    useEffect(() => {
        setLoading(true)
        setContent(null)
        setSelectionInfo(null)
        setCursorLine(1)
        setCursorCol(1)
        setCurrentSymbolName('')

        // Binary file check
        const ext = filePath.split('.').pop()?.toLowerCase() || ''
        const binaryExts = ['exe', 'dll', 'so', 'dylib', 'png', 'jpg', 'jpeg', 'gif', 'webp', 'icns', 'ico', 'zip', 'tar', 'gz', 'mp4', 'webm', 'ogg', 'mp3', 'wav', 'pdf']

        if (binaryExts.includes(ext)) {
            setContent(`// This file (${filePath.split(/[/\\]/).pop()}) is binary or unsupported.\n// It cannot be displayed in the text editor.`)
            setSavedContent('')
            setLoading(false)
            return
        }

        readFile(filePath).then((result: any) => {
            if (typeof result === 'string') {
                setContent(result)
                setSavedContent(result)
            } else {
                setContent(`// Error loading file: ${result?.error || 'Unknown error'}`)
                setSavedContent('')
            }
            setLoading(false)
        }).catch(() => {
            setContent('// File system not available (not running in Electron)')
            setLoading(false)
        })
    }, [filePath])

    // Track modified state
    useEffect(() => {
        if (content !== null) {
            onModified(content !== savedContent)
        }
    }, [content, savedContent, onModified])

    useEffect(() => {
        if (content === null) {
            setCurrentSymbolName('')
            return
        }

        const symbols = extractSymbolsFromSource(content)
        const currentSymbol = symbols.filter(symbol => symbol.line <= cursorLine).pop()
        setCurrentSymbolName(currentSymbol?.name || '')
    }, [content, cursorLine])

    useEffect(() => {
        if (!window.appApi?.updateEditorContext || !filePath || content === null) {
            return
        }

        const timeout = window.setTimeout(() => {
            window.appApi.updateEditorContext({
                filePath,
                fileName: filePath.split(/[\\/]/).pop() || filePath,
                language: getLanguage(filePath),
                cursorLine,
                cursorColumn: cursorCol,
                selection: selectionInfo ? {
                    startLine: selectionInfo.startLine,
                    startColumn: selectionInfo.startColumn,
                    endLine: selectionInfo.endLine,
                    endColumn: selectionInfo.endColumn,
                } : null,
                selectedText: selectionInfo?.selectedText || '',
                currentSymbolName,
                nearbySnippet: buildNearbySnippet(content, cursorLine),
            }).catch(() => {
                // Ignore background context sync failures.
            })
        }, 300)

        return () => window.clearTimeout(timeout)
    }, [filePath, content, cursorLine, cursorCol, selectionInfo, currentSymbolName, buildNearbySnippet])

    // Save file
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

    // Ctrl+S handler
    useEffect(() => {
        const handler = (e: KeyboardEvent) => {
            if ((e.ctrlKey || e.metaKey) && e.key === 's') {
                e.preventDefault()
                saveFile()
            }
        }
        window.addEventListener('keydown', handler)
        return () => window.removeEventListener('keydown', handler)
    }, [saveFile])

    useEffect(() => {
        const handleReviewedPatchApply = async (e: Event) => {
            const customEvent = e as CustomEvent<{
                requestId: string
                targetPath: string
                targetFile?: string
                content: string
            }>
            const detail = customEvent.detail
            const requestId = detail?.requestId

            const respond = (success: boolean, message: string) => {
                window.dispatchEvent(new CustomEvent('ai-apply-reviewed-patch-result', {
                    detail: { requestId, success, message },
                }))
            }

            if (!requestId) return

            if (!detail?.targetPath || typeof detail.content !== 'string') {
                respond(false, 'Patch payload was incomplete.')
                return
            }

            if (normalizeFilePath(detail.targetPath) !== normalizeFilePath(filePath)) {
                respond(false, 'Open the target file before applying the reviewed patch.')
                return
            }

            if (content !== savedContent) {
                respond(false, 'Save or discard your unsaved editor changes before applying the reviewed patch.')
                return
            }

            setSaving(true)
            try {
                const result = await writeFile(detail.targetPath, detail.content)
                if (result?.error) {
                    throw new Error(result.error)
                }
                setContent(detail.content)
                setSavedContent(detail.content)
                setSelectionInfo(null)
                setSaveStatus('Reviewed patch applied ✓')
                setTimeout(() => setSaveStatus(''), 2500)
                respond(true, 'Reviewed patch applied successfully.')
            } catch (error) {
                setSaveStatus('Patch apply failed!')
                setTimeout(() => setSaveStatus(''), 2500)
                respond(false, error instanceof Error ? error.message : 'Failed to write the reviewed patch to disk.')
            } finally {
                setSaving(false)
            }
        }

        window.addEventListener('ai-apply-reviewed-patch', handleReviewedPatchApply)
        return () => window.removeEventListener('ai-apply-reviewed-patch', handleReviewedPatchApply)
    }, [content, filePath, normalizeFilePath, savedContent])

    // Apply Code from AI
    useEffect(() => {
        const handleApplyCode = (e: Event) => {
            const customEvent = e as CustomEvent<{ code: string }>
            if (customEvent.detail?.code && editorRef.current && monacoRef.current) {
                const editor = editorRef.current
                const monaco = monacoRef.current
                const position = editor.getPosition()
                const selection = editor.getSelection()
                if (selection && !selection.isEmpty()) {
                    editor.executeEdits('ai-copilot', [{
                        range: selection,
                        text: customEvent.detail.code,
                        forceMoveMarkers: true
                    }])
                } else if (position) {
                    editor.executeEdits('ai-copilot', [{
                        range: new monaco.Range(position.lineNumber, position.column, position.lineNumber, position.column),
                        text: customEvent.detail.code,
                        forceMoveMarkers: true
                    }])
                }
                editor.focus()
            }
        }
        window.addEventListener('ai-apply-code', handleApplyCode)
        return () => window.removeEventListener('ai-apply-code', handleApplyCode)
    }, [])

    // Go to Line event listener
    useEffect(() => {
        const handleGoToLine = (e: Event) => {
            const ce = e as CustomEvent<{ line: number }>
            if (ce.detail?.line && editorRef.current) {
                editorRef.current.revealLineInCenter(ce.detail.line)
                editorRef.current.setPosition({ lineNumber: ce.detail.line, column: 1 })
                editorRef.current.focus()
            }
        }
        window.addEventListener('editor-go-to-line', handleGoToLine)
        return () => window.removeEventListener('editor-go-to-line', handleGoToLine)
    }, [])

    // Helper for AI context menu actions
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
        window.dispatchEvent(new CustomEvent('ai-chat-query', { detail: { text: prompts[action] || prompts.explain } }))
        window.dispatchEvent(new CustomEvent('open-ai-chat'))
    }, [filePath])

    // On editor mount
    const handleEditorMount: OnMount = (editor, monaco) => {
        editorRef.current = editor
        monacoRef.current = monaco

        // Register Neural Dark theme
        monaco.editor.defineTheme('neural-dark', NEURAL_DARK_THEME)
        monaco.editor.setTheme('neural-dark')

        // Focus the editor
        editor.focus()

        // Track cursor position
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

        // Add Ctrl+S command
        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyS, () => {
            saveFile()
        })

        // ── AI Context Menu Actions ──
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

        addAIAction('explain', '🧠 AI: Explain Code', 1)
        addAIAction('refactor', '✨ AI: Refactor', 2)
        addAIAction('fix', '🔧 AI: Fix Issues', 3)
        addAIAction('tests', '🧪 AI: Generate Tests', 4)
        addAIAction('optimize', '⚡ AI: Optimize', 5)

        // AI: Edit Inline context menu
        editor.addAction({
            id: 'ai-edit-inline',
            label: '🪄 AI: Edit Inline (Ctrl+I)',
            contextMenuGroupId: '9_ai',
            contextMenuOrder: 0,
            keybindings: [monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyI],
            run: () => {
                openInlineEdit()
            }
        })

        // Add Ctrl+K AI Generate Command
        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyK, () => {
            const position = editor.getPosition()
            if (!position) return
            const currentCoords = editor.getScrolledVisiblePosition(position)
            if (currentCoords) {
                setAiOverlayTop(currentCoords.top + 35)
                setAiOverlayLeft(Math.max(20, currentCoords.left))
            } else {
                setAiOverlayTop(40)
                setAiOverlayLeft(40)
            }
            setAiOverlayVisible(true)
            setTimeout(() => aiInputRef.current?.focus(), 50)
        })

        // Add Ctrl+G Go to Line
        editor.addCommand(monaco.KeyMod.CtrlCmd | monaco.KeyCode.KeyG, () => {
            window.dispatchEvent(new CustomEvent('open-quick-open', { detail: { mode: 'line', initialQuery: ':' } }))
        })

        // ── AI Autocomplete (Inline Completions Provider) ──
        if (autocompleteDisposable.current) {
            autocompleteDisposable.current.dispose()
        }

        let autocompleteTimer: ReturnType<typeof setTimeout> | null = null

        autocompleteDisposable.current = monaco.languages.registerInlineCompletionsProvider('*', {
            provideInlineCompletions: async (model: any, position: any, _context: any, token: any) => {
                // Debounce — only trigger after 800ms of inactivity
                if (autocompleteTimer) clearTimeout(autocompleteTimer)

                return new Promise((resolve) => {
                    autocompleteTimer = setTimeout(async () => {
                        if (token.isCancellationRequested) {
                            resolve({ items: [] })
                            return
                        }

                        // Get context: current line + a few lines before
                        const lineCount = model.getLineCount()
                        const startLine = Math.max(1, position.lineNumber - 15)
                        const prefix = model.getValueInRange({
                            startLineNumber: startLine,
                            startColumn: 1,
                            endLineNumber: position.lineNumber,
                            endColumn: position.column,
                        })
                        const suffix = model.getValueInRange({
                            startLineNumber: position.lineNumber,
                            startColumn: position.column,
                            endLineNumber: Math.min(lineCount, position.lineNumber + 5),
                            endColumn: model.getLineMaxColumn(Math.min(lineCount, position.lineNumber + 5)),
                        })

                        // Don't trigger for very short prefixes
                        const trimmed = prefix.trim()
                        if (trimmed.length < 8) {
                            resolve({ items: [] })
                            return
                        }

                        try {
                            const lang = getLanguage(filePath)
                            const res = await fetch('http://127.0.0.1:8001/api/chat', {
                                method: 'POST',
                                headers: { 'Content-Type': 'application/json' },
                                body: JSON.stringify({
                                    message: `You are an IDE autocomplete engine. Complete the code that follows. ` +
                                        `Respond ONLY with the completion text — no markdown, no backticks, no explanations. ` +
                                        `Just the code that should come next. Keep it short (1-3 lines max).\n\n` +
                                        `Language: ${lang}\nFile: ${filePath.split(/[/\\]/).pop()}\n\n` +
                                        `Code before cursor:\n${prefix}\n\n` +
                                        `Code after cursor:\n${suffix}`,
                                }),
                            })

                            if (token.isCancellationRequested) {
                                resolve({ items: [] })
                                return
                            }

                            if (res.ok) {
                                const data = await res.json()
                                let completion = data.response || data.reply || data.content || ''
                                completion = completion.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()

                                if (completion && completion.length > 0 && completion.length < 500) {
                                    resolve({
                                        items: [{
                                            insertText: completion,
                                            range: {
                                                startLineNumber: position.lineNumber,
                                                startColumn: position.column,
                                                endLineNumber: position.lineNumber,
                                                endColumn: position.column,
                                            },
                                        }],
                                    })
                                    return
                                }
                            }
                        } catch {
                            // Silently fail — autocomplete is optional
                        }

                        resolve({ items: [] })
                    }, 800)
                })
            },
            freeInlineCompletions: () => { },
        })
    }

    // ── Ctrl+I Inline Edit Logic ──
    const openInlineEdit = useCallback(() => {
        if (!editorRef.current) return
        const editor = editorRef.current
        const selection = editor.getSelection()
        const position = editor.getPosition()

        // Get selected text or current line
        let original = ''
        if (selection && !selection.isEmpty()) {
            original = editor.getModel()?.getValueInRange(selection) || ''
        } else if (position) {
            original = editor.getModel()?.getLineContent(position.lineNumber) || ''
        }

        setInlineEditOriginal(original)
        setInlineEditResult(null)
        setInlineEditQuery('')

        // Position the widget
        const coords = position ? editor.getScrolledVisiblePosition(position) : null
        setInlineEditTop(coords ? coords.top : 40)
        setInlineEditVisible(true)
        setTimeout(() => inlineEditInputRef.current?.focus(), 50)
    }, [])

    // Handle inline edit submission
    const handleInlineEditSubmit = useCallback(async () => {
        if (!inlineEditQuery.trim() || inlineEditLoading) return
        setInlineEditLoading(true)

        const lang = getLanguage(filePath)
        const fileName = filePath.split(/[/\\]/).pop() || 'file'

        try {
            const res = await fetch('http://127.0.0.1:8001/api/chat', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `You are editing code inline in an IDE. Apply the following edit instruction to the code below. ` +
                        `Respond ONLY with the modified code — no markdown, no backticks, no explanations.\n\n` +
                        `File: ${fileName} (${lang})\n` +
                        `Edit instruction: ${inlineEditQuery}\n\n` +
                        `Original code:\n${inlineEditOriginal}`,
                }),
            })

            if (res.ok) {
                const data = await res.json()
                let result = data.response || data.reply || data.content || ''
                result = result.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()
                setInlineEditResult(result)
            } else {
                setInlineEditResult('// AI service unavailable')
            }
        } catch {
            setInlineEditResult('// Could not reach AI service')
        }

        setInlineEditLoading(false)
    }, [inlineEditQuery, inlineEditOriginal, inlineEditLoading, filePath])

    // Accept inline edit
    const acceptInlineEdit = useCallback(() => {
        if (!inlineEditResult || !editorRef.current || !monacoRef.current) return
        const editor = editorRef.current
        const monaco = monacoRef.current
        const selection = editor.getSelection()
        const position = editor.getPosition()

        if (selection && !selection.isEmpty()) {
            editor.executeEdits('ai-inline-edit', [{ range: selection, text: inlineEditResult, forceMoveMarkers: true }])
        } else if (position) {
            // Replace the entire current line
            const lineLength = editor.getModel()?.getLineMaxColumn(position.lineNumber) || 1
            const range = new monaco.Range(position.lineNumber, 1, position.lineNumber, lineLength)
            editor.executeEdits('ai-inline-edit', [{ range, text: inlineEditResult, forceMoveMarkers: true }])
        }

        setInlineEditVisible(false)
        setInlineEditResult(null)
        setInlineEditQuery('')
        editor.focus()
    }, [inlineEditResult])

    // Reject inline edit
    const rejectInlineEdit = useCallback(() => {
        setInlineEditVisible(false)
        setInlineEditResult(null)
        setInlineEditQuery('')
        editorRef.current?.focus()
    }, [])

    // Cleanup autocomplete on unmount
    useEffect(() => {
        return () => {
            if (autocompleteDisposable.current) {
                autocompleteDisposable.current.dispose()
            }
        }
    }, [])

    // Navigate to symbol from breadcrumb (must be before early return)
    const handleNavigateSymbol = useCallback((line: number) => {
        if (editorRef.current) {
            editorRef.current.revealLineInCenter(line)
            editorRef.current.setPosition({ lineNumber: line, column: 1 })
            editorRef.current.focus()
        }
    }, [])

    if (loading || content === null) {
        return (
            <div style={{
                flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center',
                justifyContent: 'center', gap: 8, background: '#1A1D23', color: '#6B7280'
            }}>
                <div className="spin">
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#4A9EFF" strokeWidth="2">
                        <path d="M21 12a9 9 0 11-6.219-8.56" />
                    </svg>
                </div>
                <span style={{ fontSize: 12 }}>Loading file...</span>
            </div>
        )
    }

    const language = getLanguage(filePath)
    const isModified = content !== savedContent



    return (
        <div style={{ flex: 1, display: 'flex', flexDirection: 'column', background: '#1A1D23', overflow: 'hidden', position: 'relative' }}>
            {/* Breadcrumbs Navigation Bar */}
            <Breadcrumbs
                filePath={filePath}
                language={language}
                projectRoot={projectRoot || ''}
                editorRef={editorRef}
                monacoRef={monacoRef}
                cursorLine={cursorLine}
                cursorCol={cursorCol}
                onNavigateSymbol={handleNavigateSymbol}
            />

            {/* Save status bar */}
            {(isModified || saveStatus) && (
                <div style={{
                    padding: '2px 16px', fontSize: 11, display: 'flex', alignItems: 'center', gap: 8,
                    background: '#1A1D23', borderBottom: '1px solid #2A2D35', flexShrink: 0,
                    justifyContent: 'flex-end'
                }}>
                    {isModified && (
                        <span style={{ fontSize: 10, color: '#4A9EFF', fontWeight: 700 }}>● Modified</span>
                    )}
                    {saveStatus && (
                        <span style={{ fontSize: 11, color: '#4ADE80', fontWeight: 600 }}>{saveStatus}</span>
                    )}
                    <button
                        onClick={saveFile}
                        disabled={saving || !isModified}
                        style={{
                            padding: '2px 10px', borderRadius: 4, fontSize: 10, fontWeight: 600,
                            border: '1px solid', cursor: isModified ? 'pointer' : 'default', fontFamily: 'inherit',
                            background: isModified ? '#4A9EFF' : 'transparent',
                            color: isModified ? '#fff' : '#495162',
                            borderColor: isModified ? '#4A9EFF' : '#2A2D35',
                            transition: 'all 0.15s', opacity: isModified ? 1 : 0.5
                        }}
                    >
                        {saving ? 'Saving...' : 'Save'}
                    </button>
                </div>
            )}

            {/* AI Generate Overlay (Ctrl+K) */}
            {aiOverlayVisible && (
                <div className="ai-overlay ai-generate-overlay" style={{ top: aiOverlayTop, left: aiOverlayLeft }}>
                    <div className="ai-overlay-header">
                        <span className="ai-overlay-icon">✨</span>
                        <span className="ai-overlay-label">AI Generate</span>
                        <kbd className="ai-overlay-kbd">Ctrl+K</kbd>
                    </div>
                    <div className="ai-overlay-input-row">
                        <input
                            ref={aiInputRef}
                            type="text"
                            value={aiQuery}
                            onChange={e => setAiQuery(e.target.value)}
                            placeholder="Describe what code to generate..."
                            className="ai-overlay-input"
                            onKeyDown={async (e) => {
                                if (e.key === 'Escape') {
                                    setAiOverlayVisible(false)
                                    setAiQuery('')
                                    editorRef.current?.focus()
                                } else if (e.key === 'Enter') {
                                    if (!aiQuery.trim() || aiLoading) return
                                    setAiLoading(true)

                                    try {
                                        const selection = editorRef.current?.getSelection()
                                        let contextMsg = ''
                                        if (selection && !selection.isEmpty()) {
                                            contextMsg = "Selected code to edit:\n```\n" + editorRef.current?.getModel()?.getValueInRange(selection) + "\n```"
                                        }

                                        const res = await fetch('http://127.0.0.1:8001/api/chat', {
                                            method: 'POST',
                                            headers: { 'Content-Type': 'application/json' },
                                            body: JSON.stringify({
                                                message: `You are generating code directly into an editor. Respond ONLY with the raw replacement code block. Do NOT include any markdown formatting, no backticks, no explanations. Do not say "here is the code". Your EXACT output will be injected into the file.\n\nTask: ${aiQuery}\n\n${contextMsg}`
                                            })
                                        })

                                        if (res.ok) {
                                            const data = await res.json()
                                            let code = data.response || data.reply || data.content || ''
                                            code = code.replace(/^```[\s\S]*?\n/, '').replace(/\n```$/, '').trim()

                                            const monaco = monacoRef.current
                                            const editor = editorRef.current
                                            if (monaco && editor) {
                                                const sell = editor.getSelection()
                                                const pos = editor.getPosition()
                                                if (sell && !sell.isEmpty()) {
                                                    editor.executeEdits('ai-inline', [{ range: sell, text: code }])
                                                } else if (pos) {
                                                    editor.executeEdits('ai-inline', [{
                                                        range: new monaco.Range(pos.lineNumber, pos.column, pos.lineNumber, pos.column),
                                                        text: code
                                                    }])
                                                }
                                            }
                                        }
                                    } catch (err) {
                                        console.error("AI Generation Failed:", err)
                                    }

                                    setAiLoading(false)
                                    setAiOverlayVisible(false)
                                    setAiQuery('')
                                    editorRef.current?.focus()
                                }
                            }}
                        />
                        {aiLoading && <div className="ai-spinner" />}
                    </div>
                </div>
            )}

            {/* AI Inline Edit Widget (Ctrl+I) */}
            {inlineEditVisible && (
                <div className="ai-overlay ai-inline-edit-overlay" style={{ top: inlineEditTop }}>
                    <div className="ai-overlay-header">
                        <span className="ai-overlay-icon">🪄</span>
                        <span className="ai-overlay-label">AI Edit</span>
                        <kbd className="ai-overlay-kbd">Ctrl+I</kbd>
                        <button className="ai-overlay-close" onClick={rejectInlineEdit}>✕</button>
                    </div>

                    {/* Input row */}
                    {!inlineEditResult && (
                        <div className="ai-overlay-input-row">
                            <input
                                ref={inlineEditInputRef}
                                type="text"
                                value={inlineEditQuery}
                                onChange={e => setInlineEditQuery(e.target.value)}
                                placeholder="Describe the edit to apply..."
                                className="ai-overlay-input"
                                onKeyDown={(e) => {
                                    if (e.key === 'Escape') rejectInlineEdit()
                                    else if (e.key === 'Enter') handleInlineEditSubmit()
                                }}
                            />
                            {inlineEditLoading && <div className="ai-spinner" />}
                        </div>
                    )}

                    {/* Diff preview */}
                    {inlineEditResult && (
                        <div className="ai-inline-diff">
                            <div className="ai-diff-section">
                                <div className="ai-diff-label removed">Original</div>
                                <pre className="ai-diff-code removed">{inlineEditOriginal}</pre>
                            </div>
                            <div className="ai-diff-section">
                                <div className="ai-diff-label added">AI Suggestion</div>
                                <pre className="ai-diff-code added">{inlineEditResult}</pre>
                            </div>
                            <div className="ai-diff-actions">
                                <button className="ai-diff-btn accept" onClick={acceptInlineEdit}>
                                    ✓ Accept
                                </button>
                                <button className="ai-diff-btn reject" onClick={rejectInlineEdit}>
                                    ✕ Reject
                                </button>
                                <button className="ai-diff-btn retry" onClick={() => {
                                    setInlineEditResult(null)
                                    setTimeout(() => inlineEditInputRef.current?.focus(), 50)
                                }}>
                                    ↻ Retry
                                </button>
                            </div>
                        </div>
                    )}
                </div>
            )}

            {/* Monaco Editor */}
            <Editor
                height="100%"
                language={language}
                value={content}
                theme="neural-dark"
                onChange={(value) => setContent(value || '')}
                onMount={handleEditorMount}
                options={{
                    fontSize,
                    fontFamily: `'${fontFamily}', 'Cascadia Code', 'Fira Code', Consolas, monospace`,
                    fontLigatures: true,
                    tabSize,
                    wordWrap: wordWrap as 'on' | 'off',
                    minimap: { enabled: minimap, scale: 2 },
                    lineNumbers: lineNumbers ? 'on' : 'off',
                    bracketPairColorization: { enabled: bracketPairs },
                    renderLineHighlight: 'line',
                    scrollBeyondLastLine: false,
                    smoothScrolling: true,
                    cursorBlinking: 'smooth',
                    cursorSmoothCaretAnimation: 'on',
                    automaticLayout: true,
                    padding: { top: 12, bottom: 12 },
                    overviewRulerLanes: 0,
                    hideCursorInOverviewRuler: true,
                    overviewRulerBorder: false,
                    guides: {
                        indentation: true,
                        bracketPairs: true,
                        highlightActiveIndentation: true,
                    },
                    suggest: {
                        showKeywords: true,
                        showSnippets: true,
                        showClasses: true,
                        showFunctions: true,
                        showVariables: true,
                        showConstants: true,
                    },
                    quickSuggestions: {
                        other: true,
                        comments: false,
                        strings: true,
                    },
                    parameterHints: { enabled: true },
                    folding: true,
                    foldingStrategy: 'indentation',
                    showFoldingControls: 'mouseover',
                    matchBrackets: 'always',
                    renderWhitespace: 'selection',
                    colorDecorators: true,
                    linkedEditing: true,
                    stickyScroll: { enabled: true },
                }}
                loading={
                    <div style={{
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        height: '100%', background: '#1A1D23', color: '#6B7280', fontSize: 12
                    }}>
                        Initializing editor...
                    </div>
                }
            />
        </div>
    )
}
