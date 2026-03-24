import { useEffect, useRef, useState } from 'react'
import {
    ArrowUp, Brain, BrainCircuit, Check, ChevronDown, ChevronUp, Copy, Globe, Paperclip, Square,
    ThumbsDown, ThumbsUp, Wand2, Zap
} from 'lucide-react'
import { readFile, selectFile } from '../lib/desktopBridge'
import { Mermaid } from './Mermaid'
import { WebViewPanel } from './WebViewPanel'
import { apiClient } from '../lib/apiClient'

const API = 'http://127.0.0.1:8001'

interface Message {
    role: 'user' | 'assistant' | 'system'
    content: string
    timestamp: Date
    tool?: string
    feedback?: 'positive' | 'negative' | null
    messageId?: string
    command?: string
    browserUrl?: string
    confidence?: number
    sources?: string[]
    flowSections?: FlowSection[]
    analysis?: AnalysisDetails
    findings?: ReviewFinding[]
    testGaps?: string[]
    proposedCode?: string
    targetFile?: string
    targetPath?: string
    appliedContent?: string
    validation?: {
        status?: string
        summary?: string
        command?: string
    }
}

interface FlowEntry {
    label: string
    path: string
    line_start?: number
    line_end?: number
    kind?: string
    snippet?: string
}

interface FlowSection {
    type: 'symbol' | 'file' | 'impact'
    title: string
    summary?: string
    entries: FlowEntry[]
}

interface ReviewFinding {
    title: string
    severity: 'high' | 'medium' | 'low' | 'info'
    file: string
    line_start: number
    line_end?: number
    body: string
    confidence?: number
}

interface AnalysisStep {
    name: string
    category?: string
    status?: string
    reason?: string
    command?: string
    durationMs?: number
    summary?: string[]
    preferred?: boolean
}

interface AnalysisDetails {
    status?: string
    lastRunAt?: string
    preferenceIntent?: string
    preferredCommands?: string[]
    steps: AnalysisStep[]
    alternatives?: AnalysisStep[]
}

interface ChatAttachment {
    name: string
    path: string
    content: string
}

interface AIChatPanelProps {
    serverStatus: string
    projectRoot?: string
    activeFilePath?: string
}

function FlowSectionsBlock({ sections }: { sections: FlowSection[] }) {
    const openEntry = (entry: FlowEntry) => {
        window.dispatchEvent(new CustomEvent('ai-open-file', {
            detail: {
                path: entry.path,
                line: entry.line_start || 1,
            },
        }))
    }

    return (
        <div style={{ marginTop: 12, display: 'flex', flexDirection: 'column', gap: 10 }}>
            {sections.map((section, index) => (
                <div
                    key={`${section.type}-${section.title}-${index}`}
                    style={{
                        border: '1px solid var(--border)',
                        borderRadius: 8,
                        padding: 10,
                        background: 'rgba(0,0,0,0.15)',
                    }}
                >
                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 6, flexWrap: 'wrap' }}>
                        <span style={{
                            fontSize: 10,
                            fontWeight: 700,
                            padding: '2px 8px',
                            borderRadius: 999,
                            border: '1px solid rgba(74,158,255,0.25)',
                            background: 'rgba(74,158,255,0.08)',
                            color: 'var(--accent)',
                            textTransform: 'uppercase',
                        }}>
                            {section.type} flow
                        </span>
                        <span style={{ fontSize: 12, fontWeight: 700, color: 'var(--text)' }}>{section.title}</span>
                    </div>
                    {section.summary && (
                        <div style={{ fontSize: 12, color: 'var(--text-muted)', marginBottom: 8 }}>
                            {section.summary}
                        </div>
                    )}
                    <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
                        {section.entries.map((entry, entryIndex) => {
                            const location = `${entry.path}:${entry.line_start || 1}${entry.line_end && entry.line_end !== entry.line_start ? `-${entry.line_end}` : ''}`
                            return (
                                <button
                                    key={`${entry.path}-${entry.line_start || 1}-${entryIndex}`}
                                    onClick={() => openEntry(entry)}
                                    style={{
                                        textAlign: 'left',
                                        background: 'rgba(255,255,255,0.02)',
                                        border: '1px solid var(--border)',
                                        borderRadius: 8,
                                        padding: 10,
                                        cursor: 'pointer',
                                        color: 'inherit',
                                    }}
                                >
                                    <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap', marginBottom: 4 }}>
                                        <span style={{ fontSize: 10, fontWeight: 700, color: '#4A9EFF' }}>{entry.label}</span>
                                        {entry.kind && (
                                            <span style={{ fontSize: 10, color: 'var(--text-muted)', textTransform: 'uppercase' }}>
                                                {entry.kind}
                                            </span>
                                        )}
                                    </div>
                                    <div style={{ fontSize: 11, color: 'var(--text)' }}>{location}</div>
                                    {entry.snippet && (
                                        <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 4, whiteSpace: 'pre-wrap' }}>
                                            {entry.snippet}
                                        </div>
                                    )}
                                </button>
                            )
                        })}
                    </div>
                </div>
            ))}
        </div>
    )
}

const QUICK_ACTIONS = [
    { label: 'Explain', cmd: '/explain', color: '#4A9EFF' },
    { label: 'Review', cmd: '/review', color: '#4ADE80' },
    { label: 'Modify', cmd: '/modify', color: '#FBBF24' },
    { label: 'Generate', cmd: '/generate', color: '#FF6B9D' },
    { label: 'Research', cmd: '/research', color: '#C97FDB' },
]

function ActionPreview({
    label,
    value,
    buttonLabel,
    onRun,
}: {
    label: string
    value: string
    buttonLabel: string
    onRun: () => void
}) {
    const [done, setDone] = useState(false)

    const handleRun = () => {
        onRun()
        setDone(true)
    }

    return (
        <div style={{ marginTop: 12, padding: 12, background: 'rgba(0,0,0,0.2)', borderRadius: 8, border: '1px solid var(--border)' }}>
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 8, gap: 8 }}>
                <div style={{ fontSize: 11, fontWeight: 600, color: 'var(--text-muted)' }}>
                    {label}
                </div>
                {done ? (
                    <span style={{ fontSize: 11, color: 'var(--green)', display: 'flex', alignItems: 'center', gap: 4 }}>
                        <Check size={12} /> Ready
                    </span>
                ) : (
                    <button
                        onClick={handleRun}
                        style={{
                            padding: '4px 12px',
                            borderRadius: 4,
                            background: 'var(--accent)',
                            color: '#fff',
                            border: 'none',
                            fontSize: 11,
                            cursor: 'pointer',
                        }}
                    >
                        {buttonLabel}
                    </button>
                )}
            </div>
            <code style={{ fontSize: 12, color: 'var(--text)', wordBreak: 'break-all', fontFamily: 'monospace', background: '#000', padding: '2px 4px', borderRadius: 4 }}>
                {value}
            </code>
        </div>
    )
}

function ReviewFindingsBlock({ findings }: { findings: ReviewFinding[] }) {
    const colors: Record<string, string> = {
        high: '#EF4444',
        medium: '#FBBF24',
        low: '#4A9EFF',
        info: '#A0A0A0',
    }

    return (
        <div style={{ marginTop: 12, display: 'flex', flexDirection: 'column', gap: 8 }}>
            {findings.map((finding, index) => {
                const color = colors[finding.severity] || '#A0A0A0'
                const location = `${finding.file}:${finding.line_start}${finding.line_end && finding.line_end !== finding.line_start ? `-${finding.line_end}` : ''}`
                return (
                    <div
                        key={`${finding.file}-${finding.line_start}-${index}`}
                        style={{
                            border: '1px solid var(--border)',
                            borderLeft: `3px solid ${color}`,
                            borderRadius: 8,
                            padding: 10,
                            background: 'rgba(0,0,0,0.15)',
                        }}
                    >
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 6, flexWrap: 'wrap' }}>
                            <span style={{
                                fontSize: 10,
                                fontWeight: 700,
                                color,
                                border: `1px solid ${color}44`,
                                background: `${color}12`,
                                borderRadius: 999,
                                padding: '2px 8px',
                                textTransform: 'uppercase',
                            }}>
                                {finding.severity}
                            </span>
                            <span style={{ fontSize: 12, fontWeight: 700, color: 'var(--text)' }}>{finding.title}</span>
                        </div>
                        <div style={{ fontSize: 11, color: 'var(--text-muted)', marginBottom: 6 }}>{location}</div>
                        <div style={{ fontSize: 12, color: 'var(--text)', whiteSpace: 'pre-wrap' }}>{finding.body}</div>
                    </div>
                )
            })}
        </div>
    )
}

function AnalysisBlock({ analysis, workspaceRoot }: { analysis: AnalysisDetails; workspaceRoot?: string }) {
    if (!analysis.steps.length) return null
    const [preferredCommands, setPreferredCommands] = useState<string[]>(analysis.preferredCommands || [])

    const runCommand = (command?: string) => {
        if (!command) return
        window.dispatchEvent(new CustomEvent('open-bottom-panel', { detail: { tab: 'terminal' } }))
        window.setTimeout(() => {
            window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd: command } }))
        }, 150)
    }

    const savePreference = async (commandName?: string) => {
        if (!commandName || !analysis.preferenceIntent) return
        try {
            const res = await apiClient.fetch(`${API}/api/analysis/preferences`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    intent: analysis.preferenceIntent,
                    command_name: commandName,
                    workspace_root: workspaceRoot || '',
                }),
            })
            const data = await res.json()
            if (res.ok && data.status === 'ok') {
                const next = Array.isArray(data.preferences?.[analysis.preferenceIntent]?.preferred_commands)
                    ? data.preferences[analysis.preferenceIntent].preferred_commands
                    : [commandName]
                setPreferredCommands(next)
            }
        } catch {
            // Ignore preference save errors in the chat UI
        }
    }

    const statusColor = analysis.status === 'ok'
        ? 'var(--green)'
        : analysis.status === 'warning'
            ? '#FBBF24'
            : analysis.status === 'error'
                ? '#EF4444'
                : 'var(--text-muted)'

    return (
        <div style={{ marginTop: 12, display: 'flex', flexDirection: 'column', gap: 8 }}>
            <div
                style={{
                    border: '1px solid var(--border)',
                    borderRadius: 8,
                    padding: 10,
                    background: 'rgba(0,0,0,0.15)',
                }}
            >
                <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 8, flexWrap: 'wrap' }}>
                    <span style={{
                        fontSize: 10,
                        fontWeight: 700,
                        padding: '2px 8px',
                        borderRadius: 999,
                        border: `1px solid ${statusColor}44`,
                        background: `${statusColor}12`,
                        color: statusColor,
                        textTransform: 'uppercase',
                    }}>
                        Local checks
                    </span>
                    {analysis.status && (
                        <span style={{ fontSize: 11, color: 'var(--text-muted)' }}>
                            {analysis.status}
                        </span>
                    )}
                </div>
                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                    {analysis.steps.map((step, index) => (
                        <div
                            key={`${step.name}-${index}`}
                            style={{
                                border: '1px solid var(--border)',
                                borderRadius: 8,
                                padding: 10,
                                background: 'rgba(255,255,255,0.02)',
                            }}
                        >
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, flexWrap: 'wrap', marginBottom: 4 }}>
                                <span style={{ fontSize: 11, fontWeight: 700, color: 'var(--text)' }}>{step.name}</span>
                                {(step.preferred || preferredCommands.includes(step.name)) && (
                                    <span style={{ fontSize: 10, color: 'var(--green)', textTransform: 'uppercase' }}>
                                        preferred
                                    </span>
                                )}
                                {step.category && (
                                    <span style={{ fontSize: 10, color: 'var(--accent)', textTransform: 'uppercase' }}>
                                        {step.category}
                                    </span>
                                )}
                                {step.status && (
                                    <span style={{ fontSize: 10, color: statusColor }}>
                                        {step.status}
                                    </span>
                                )}
                                {typeof step.durationMs === 'number' && (
                                    <span style={{ fontSize: 10, color: 'var(--text-muted)' }}>
                                        {step.durationMs} ms
                                    </span>
                                )}
                            </div>
                            {step.reason && (
                                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginBottom: 4 }}>
                                    {step.reason}
                                </div>
                            )}
                            {step.command && (
                                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginBottom: 4 }}>
                                    Command: <code className="inline-code">{step.command}</code>
                                    <button
                                        onClick={() => runCommand(step.command)}
                                        style={{
                                            marginLeft: 8,
                                            padding: '2px 8px',
                                            borderRadius: 999,
                                            border: '1px solid var(--border)',
                                            background: 'transparent',
                                            color: 'var(--accent)',
                                            fontSize: 10,
                                            cursor: 'pointer',
                                        }}
                                    >
                                        Run
                                    </button>
                                    {analysis.preferenceIntent && (
                                        <button
                                            onClick={() => { void savePreference(step.name) }}
                                            style={{
                                                marginLeft: 6,
                                                padding: '2px 8px',
                                                borderRadius: 999,
                                                border: '1px solid var(--border)',
                                                background: 'transparent',
                                                color: preferredCommands.includes(step.name) ? 'var(--green)' : 'var(--text-muted)',
                                                fontSize: 10,
                                                cursor: 'pointer',
                                            }}
                                        >
                                            {preferredCommands.includes(step.name) ? 'Preferred' : 'Prefer'}
                                        </button>
                                    )}
                                </div>
                            )}
                            {Array.isArray(step.summary) && step.summary.length > 0 && (
                                <div style={{ fontSize: 11, color: 'var(--text)', whiteSpace: 'pre-wrap' }}>
                                    {step.summary[step.summary.length - 1]}
                                </div>
                            )}
                        </div>
                    ))}
                </div>
                {analysis.alternatives && analysis.alternatives.length > 0 && (
                    <div style={{ marginTop: 10 }}>
                        <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', marginBottom: 6 }}>
                            Other useful checks
                        </div>
                        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6 }}>
                            {analysis.alternatives.map((step, index) => (
                                <div
                                    key={`${step.name}-${index}`}
                                    style={{
                                        display: 'flex',
                                        alignItems: 'center',
                                        gap: 4,
                                        padding: '4px 8px',
                                        borderRadius: 999,
                                        border: '1px solid var(--border)',
                                        background: 'rgba(255,255,255,0.02)',
                                    }}
                                    title={step.reason || step.command || step.name}
                                >
                                    <button
                                        onClick={() => runCommand(step.command)}
                                        style={{
                                            border: 'none',
                                            background: 'transparent',
                                            color: 'var(--text)',
                                            fontSize: 11,
                                            cursor: 'pointer',
                                        }}
                                    >
                                        {step.name}
                                    </button>
                                    {analysis.preferenceIntent && (
                                        <button
                                            onClick={() => { void savePreference(step.name) }}
                                            style={{
                                                border: 'none',
                                                background: 'transparent',
                                                color: preferredCommands.includes(step.name) ? 'var(--green)' : 'var(--text-muted)',
                                                fontSize: 10,
                                                cursor: 'pointer',
                                            }}
                                        >
                                            {preferredCommands.includes(step.name) ? 'Preferred' : 'Prefer'}
                                        </button>
                                    )}
                                </div>
                            ))}
                        </div>
                    </div>
                )}
            </div>
        </div>
    )
}

function ThoughtBlock({ analysis, isThinking, workspaceRoot }: { analysis?: AnalysisDetails; isThinking?: boolean; workspaceRoot?: string }) {
    const [isExpanded, setIsExpanded] = useState(isThinking)

    return (
        <div className="thought-container">
            <button className="thought-header" onClick={() => setIsExpanded(!isExpanded)}>
                <Brain size={14} className="thought-header-icon" />
                <span className="thought-header-text">
                    {isThinking ? 'Analyzing & Thinking...' : 'Technical Analysis'}
                </span>
                {isExpanded ? <ChevronUp size={14} className="thought-header-toggle" /> : <ChevronDown size={14} className="thought-header-toggle" />}
            </button>
            {isExpanded && (
                <div className="thought-content">
                    {isThinking && (
                        <div className="ai-thinking-inline-minimal">
                            <span style={{ fontSize: 11, color: 'var(--text-muted)' }}>Processing</span>
                            <div className="dot-flashing" />
                        </div>
                    )}
                    {analysis && <AnalysisBlock analysis={analysis} workspaceRoot={workspaceRoot} />}
                </div>
            )}
        </div>
    )
}

const CodeBlock = ({
    code,
    language,
    onApply,
    showApplyButton = true,
    applyLabel = 'Apply',
    applyTitle = 'Apply to Active File',
}: {
    code: string
    language: string
    onApply?: () => Promise<boolean> | boolean
    showApplyButton?: boolean
    applyLabel?: string
    applyTitle?: string
}) => {
    const [copied, setCopied] = useState(false)
    const [applied, setApplied] = useState(false)
    const [applying, setApplying] = useState(false)

    const copy = () => {
        navigator.clipboard.writeText(code)
        setCopied(true)
        setTimeout(() => setCopied(false), 2000)
    }

    const apply = async () => {
        setApplying(true)
        try {
            let ok = true
            if (onApply) {
                ok = await onApply()
            } else {
                window.dispatchEvent(new CustomEvent('ai-apply-code', { detail: { code } }))
            }

            if (ok) {
                setApplied(true)
                setTimeout(() => setApplied(false), 2000)
            }
        } finally {
            setApplying(false)
        }
    }

    return (
        <div style={{ margin: '12px 0', borderRadius: 8, overflow: 'hidden', border: '1px solid var(--border)', background: 'rgba(0,0,0,0.2)' }}>
            <div style={{ 
                display: 'flex', 
                justifyContent: 'space-between', 
                alignItems: 'center',
                padding: '6px 12px', 
                background: 'rgba(0,0,0,0.4)', 
                borderBottom: '1px solid var(--border)', 
                fontSize: 11,
                fontWeight: 600,
                color: 'var(--text-muted)' 
            }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                    <div style={{ width: 8, height: 8, borderRadius: '50%', background: 'var(--accent)', opacity: 0.6 }} />
                    <span style={{ textTransform: 'uppercase', letterSpacing: '0.05em' }}>{language || 'code'}</span>
                </div>
                <div style={{ display: 'flex', gap: 12 }}>
                    <button 
                        onClick={copy} 
                        style={{ 
                            background: 'none', 
                            border: 'none', 
                            color: copied ? 'var(--green)' : 'inherit', 
                            cursor: 'pointer', 
                            display: 'flex', 
                            alignItems: 'center', 
                            gap: 5,
                            transition: 'all 0.2s',
                            padding: '2px 4px'
                        }}
                        title="Copy to clipboard"
                    >
                        {copied ? <Check size={13} /> : <Copy size={13} />}
                        <span>{copied ? 'Copied' : 'Copy'}</span>
                    </button>
                    {showApplyButton && (
                        <button
                            onClick={() => { void apply() }}
                            disabled={applying}
                            style={{ 
                                background: 'none', 
                                border: 'none', 
                                color: applied ? 'var(--green)' : 'var(--accent)', 
                                cursor: applying ? 'wait' : 'pointer', 
                                display: 'flex', 
                                alignItems: 'center', 
                                gap: 5,
                                transition: 'all 0.2s',
                                padding: '2px 4px',
                                opacity: applying ? 0.7 : 1 
                            }}
                            title={applyTitle}
                        >
                            {applied ? <Check size={13} /> : <Wand2 size={13} />}
                            <span>{applied ? 'Applied' : applying ? 'Applying...' : applyLabel}</span>
                        </button>
                    )}
                </div>
            </div>
            <pre style={{ 
                margin: 0, 
                padding: '12px 16px', 
                background: 'transparent', 
                color: '#ABB2BF', 
                fontSize: 12, 
                lineHeight: 1.5,
                overflowX: 'auto', 
                whiteSpace: 'pre-wrap', 
                wordBreak: 'break-word',
                fontFamily: 'var(--font-code, "JetBrains Mono", monospace)'
            }}>
                <code style={{ fontFamily: 'inherit' }}>{code}</code>
            </pre>
        </div>
    )
}

export function AIChatPanel({ serverStatus, projectRoot = '', activeFilePath = '' }: AIChatPanelProps) {
    const STORAGE_KEY = `neural-chat-${projectRoot || 'default'}`
    const fetchedRootRef = useRef<string | null>(null)


    const defaultMessages: Message[] = [
        {
            role: 'system',
            content: 'Neural Studio AI Copilot connected to the local brain',
            timestamp: new Date(),
        },
        {
            role: 'assistant',
            content: "Welcome to Neural Studio AI Copilot.\n\nI can help explain the project, review code, and suggest safe next steps while keeping the code local.",
            timestamp: new Date(),
        },
    ]

    // Load messages from localStorage on mount, fallback to defaults
    const [projectContext, setProjectContext] = useState<any>(null)
    const [projectIndexPath, setProjectIndexPath] = useState<string>('')
    const [messages, setMessages] = useState<Message[]>(() => {
        try {
            const saved = localStorage.getItem(STORAGE_KEY)
            if (saved) {
                const parsed = JSON.parse(saved) as Message[]
                if (Array.isArray(parsed) && parsed.length > 0) {
                    // Restore Date objects (JSON stringify converts them to strings)
                    return parsed.map(m => ({ ...m, timestamp: new Date(m.timestamp) }))
                }
            }
        } catch {
            // Ignore parse errors, fall through to defaults
        }
        return defaultMessages
    })

    const [input, setInput] = useState('')
    const [isTyping, setIsTyping] = useState(false)
    const [availableModels, setAvailableModels] = useState<string[]>([])
    const [selectedModel, setSelectedModel] = useState<string>(() => {
        return localStorage.getItem('neural-selected-model') || 'qwen2.5-coder:7b'
    })
    const [webEnabled, setWebEnabled] = useState(() => {
        try {
            const saved = localStorage.getItem(`${STORAGE_KEY}-webEnabled`)
            return saved ? JSON.parse(saved) : false
        } catch {
            return false
        }
    })
    const [attachedFiles, setAttachedFiles] = useState<ChatAttachment[]>([])
    
    // Split pane browser state
    const [showBrowser, setShowBrowser] = useState(() => {
        try {
            const saved = localStorage.getItem(`${STORAGE_KEY}-showBrowser`)
            return saved ? JSON.parse(saved) : false
        } catch {
            return false
        }
    })
    const [browserUrl, setBrowserUrl] = useState('https://github.com')

    const historyRef = useRef<HTMLDivElement>(null)

    // Debounce save messages, webEnabled, and showBrowser to localStorage
    useEffect(() => {
        const handler = setTimeout(() => {
            try {
                localStorage.setItem(STORAGE_KEY, JSON.stringify(messages))
                localStorage.setItem(`${STORAGE_KEY}-webEnabled`, JSON.stringify(webEnabled))
                localStorage.setItem(`${STORAGE_KEY}-showBrowser`, JSON.stringify(showBrowser))
            } catch (error) {
                console.error("Failed to save state to localStorage", error)
            }
        }, 500) // Debounce for 500ms

        return () => {
            clearTimeout(handler)
        }
    }, [messages, webEnabled, showBrowser, STORAGE_KEY])

    const attachFile = async (filePath: string) => {
        if (!filePath) return
        if (attachedFiles.some((file) => file.path === filePath)) return

        try {
            const result = await readFile(filePath)
            if (typeof result !== 'string') {
                throw new Error(result?.error || 'Could not read file')
            }

            const normalized = result.length > 12000
                ? `${result.slice(0, 12000)}\n\n[Truncated for chat context]`
                : result

            setAttachedFiles((prev) => [
                ...prev,
                {
                    name: filePath.split(/[\\/]/).pop() || filePath,
                    path: filePath,
                    content: normalized,
                },
            ])
        } catch (error) {
            setMessages((prev) => [
                ...prev,
                {
                    role: 'assistant',
                    content: `I could not attach \`${filePath}\`: ${error instanceof Error ? error.message : 'Unknown error'}.`,
                    timestamp: new Date(),
                    tool: 'attachment_error',
                },
            ])
        }
    }

    const handleAttachFile = async () => {
        const preferredPath = activeFilePath || await selectFile()
        if (!preferredPath) return
        await attachFile(preferredPath)
    }

    const removeAttachment = (filePath: string) => {
        setAttachedFiles((prev) => prev.filter((file) => file.path !== filePath))
    }

    useEffect(() => {
        if (historyRef.current) {
            historyRef.current.scrollTop = historyRef.current.scrollHeight
        }
    }, [messages, isTyping])

    useEffect(() => {
        const handleQuery = (e: Event) => {
            const customEvent = e as CustomEvent<{ text: string }>
            if (customEvent.detail?.text) {
                setInput(prev => prev ? `${prev}\n\n${customEvent.detail.text}` : customEvent.detail.text)
            }
        }
        window.addEventListener('ai-chat-query', handleQuery)
        return () => window.removeEventListener('ai-chat-query', handleQuery)
    }, [])

    // --- Project Context Awareness ---
    useEffect(() => {
        if (!projectRoot || fetchedRootRef.current === projectRoot) {
            if (!projectRoot) {
                setProjectContext(null)
                setProjectIndexPath('')
                fetchedRootRef.current = null
            }
            return
        }

        const fetchContext = async () => {
            try {
                // 1. Get structured context
                const ctxRes = await apiClient.fetch(`${API}/api/project/context?workspace_root=${encodeURIComponent(projectRoot)}`)
                if (ctxRes.ok) {
                    const ctx = await ctxRes.json()
                    setProjectContext(ctx)
                    fetchedRootRef.current = projectRoot // Mark as fetched for THIS project

                    // Only show context card if the first message isn't a context card
                    setMessages(prev => {
                        const hasContextCard = prev.some(m => m.role === 'system' && m.content.includes('Project Context Loaded'))
                        if (!hasContextCard && prev.length <= 2) { 
                            return [
                                ...prev,
                                {
                                    role: 'system',
                                    content: `📂 **Project Context Loaded: ${ctx.name || 'Current Workspace'}**\nStack: ${(ctx.stack || []).join(', ') || 'Unknown'} • ${ctx.total_files || 0} files • ${((ctx.total_size || 0) / 1024 / 1024).toFixed(2)} MB\nUse \`/tree\` to see files or \`/review <file>\` for analysis.`,
                                    timestamp: new Date()
                                }
                            ]
                        }
                        return prev
                    })
                }

                // 2. Get/Generate long-form index path
                const idxRes = await apiClient.fetch(`${API}/api/project/index?workspace_root=${encodeURIComponent(projectRoot)}`)
                if (idxRes.ok) {
                    const idxData = await idxRes.json()
                    setProjectIndexPath(idxData.path)
                }
            } catch (err) {
                console.error('Failed to fetch project context:', err)
            }
        }

        fetchContext()
    }, [projectRoot])

    // --- Model Management ---
    useEffect(() => {
        const fetchModels = async () => {
            try {
                const res = await apiClient.fetch(`${API}/api/setup/status`)
                if (res.ok) {
                    const data = await res.json()
                    if (data.models && Array.isArray(data.models)) {
                        setAvailableModels(data.models)
                    }
                }
            } catch (e) {
                console.error('Failed to fetch models:', e)
            }
        }
        fetchModels()
    }, [])


    // ── Browser Webview Bridge ─────────────────────────────────────────────────
    // Listen for results coming back FROM the webview
    useEffect(() => {
        const onWebviewContent = (e: Event) => {
            const ce = e as CustomEvent<{ type: string; content: string; error?: string }>
            const { type, content } = ce.detail || {}
            const isScreenshot = type === 'screenshot' && content?.startsWith('data:image')
            setMessages(prev => [...prev, {
                role: 'assistant',
                content: isScreenshot
                    ? `🌐 **Browser screenshot captured**`
                    : `🌐 **Browser result (${type}):**\n\n${content}`,
                timestamp: new Date(),
                tool: `browser_${type}`,
            }])
        }
        window.addEventListener('webview:content', onWebviewContent)
        return () => window.removeEventListener('webview:content', onWebviewContent)
    }, [])

    // Scan the latest AI message for [BROWSER: cmd args] directives and execute them
    useEffect(() => {
        const lastMsg = messages[messages.length - 1]
        if (!lastMsg || lastMsg.role !== 'assistant' || !lastMsg.content) return

        // Match all [BROWSER: <command> <optional-arg>] blocks
        const browserCmdRegex = /\[BROWSER:\s*(\w+)(?:\s+([^\]]+))?\]/gi
        let match: RegExpExecArray | null
        const commands: Array<{ cmd: string; arg?: string }> = []
        while ((match = browserCmdRegex.exec(lastMsg.content)) !== null) {
            commands.push({ cmd: match[1].toLowerCase(), arg: match[2]?.trim() })
        }
        if (commands.length === 0) return

        setShowBrowser(true)

        // Execute each command with a small stagger
        commands.forEach(({ cmd, arg }, i) => {
            setTimeout(() => {
                switch (cmd) {
                    case 'navigate':
                        // Update the split pane URL
                        setBrowserUrl(arg || 'https://github.com')
                        setTimeout(() => {
                            window.dispatchEvent(new CustomEvent('webview:navigate', { detail: { url: arg || '' } }))
                        }, 400)
                        break
                    case 'getcontent':
                        window.dispatchEvent(new CustomEvent('webview:getContent'))
                        break
                    case 'gethtml':
                        window.dispatchEvent(new CustomEvent('webview:getHTML'))
                        break
                    case 'getelement':
                        window.dispatchEvent(new CustomEvent('webview:getElement', { detail: { selector: arg || 'body' } }))
                        break
                    case 'consolelogs':
                        window.dispatchEvent(new CustomEvent('webview:consoleLogs'))
                        break
                    case 'screenshot':
                        window.dispatchEvent(new CustomEvent('webview:screenshot'))
                        break
                }
            }, i * 300)
        })
    }, [messages])


    const sendMessage = async () => {
        const text = input.trim()
        if (!text || isTyping) return

        // --- Slash Command Handling ---
        if (text.startsWith('/')) {
            const parts = text.split(' ')
            const cmd = parts[0].toLowerCase()
            const arg = parts.slice(1).join(' ').trim()

            if (cmd === '/tree') {
                if (!projectContext) {
                    setMessages(prev => [...prev, { role: 'assistant', content: 'No project context loaded. Open a folder first.', timestamp: new Date() }])
                    setInput('')
                    return
                }
                const tree = projectContext.files.slice(0, 100).map((f: any) => `${f.key ? '* ' : '  '}${f.path}`).join('\n')
                setMessages(prev => [...prev, { role: 'user', content: text, timestamp: new Date() }, { role: 'assistant', content: `**Project File Tree (top 100):**\n\`\`\`\n${tree}\n\`\`\``, timestamp: new Date() }])
                setInput('')
                return
            }

            if (cmd === '/open' && arg) {
                window.dispatchEvent(new CustomEvent('open-file', { detail: { path: arg } }))
                setMessages(prev => [...prev, { role: 'user', content: text, timestamp: new Date() }, { role: 'assistant', content: `Opening \`${arg}\` in editor...`, timestamp: new Date() }])
                setInput('')
                return
            }

            if (cmd === '/review' || cmd === '/explain' || cmd === '/modify') {
                if (!arg && activeFilePath) {
                    // Default to active file if no arg provided
                    setInput(`${cmd} ${activeFilePath}`)
                    // Recursive call with updated input
                    setTimeout(sendMessage, 0)
                    return
                }
                // Continue to LLM with these commands
            }
        }

        const isSlashCommand = text.trim().startsWith('/')
        const historyPayload = isSlashCommand ? [] : messages.filter(m => {
            const content = m.content.trim();
            const startsWithSlash = content.startsWith('/');
            const isFindings = content.includes('Findings:');
            const isAssistedReview = m.role === 'assistant' && (content.includes('Finding') || content.includes('review pipeline'));
            return m.role !== 'system' && !startsWithSlash && !isFindings && !isAssistedReview;
        }).map(m => ({ role: m.role, content: m.content }));




        const attachmentContext = attachedFiles.length > 0
            ? '\n\nAttached local file context:\n' + attachedFiles.map((file) => (
                `File: ${file.path}\n\`\`\`\n${file.content}\n\`\`\``
            )).join('\n\n')
            : ''
        
        // Inject project index path if available
        const projectIndexHint = projectIndexPath ? `\n\n[Project Index available at: ${projectIndexPath}]` : ''
        const requestMessage = `${text}${attachmentContext}${projectIndexHint}`
        
        const visibleUserMessage = attachedFiles.length > 0
            ? `${text}\n\n[Attached files: ${attachedFiles.map((file) => file.name).join(', ')}]`
            : text

        const userMsg: Message = { role: 'user', content: visibleUserMessage, timestamp: new Date() }
        setMessages(prev => [...prev, userMsg])
        setInput('')
        setIsTyping(true)

        const assistantMsgId = `msg-${Date.now()}`
        setMessages(prev => [...prev, {
            role: 'assistant',
            content: '',
            timestamp: new Date(),
            messageId: assistantMsgId,
        }])

        try {
            const res = await apiClient.fetch(`${API}/api/chat_stream`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: requestMessage,
                    history: historyPayload,
                    web_search: webEnabled,
                    workspace_root: projectRoot || undefined,
                    project_index_path: projectIndexPath || undefined,
                    model: selectedModel,
                    editor_context: activeFilePath ? {
                        activeFilePath,
                        relativePath: activeFilePath.replace(projectRoot || '', '').replace(/^[\\/]/, '').replace(/\\/g, '/'),
                        language: activeFilePath.split('.').pop() || '',
                        // Add selection if available via props (though AIChatPanel doesn't currently track it deeply)
                        selection: null,
                        selectedText: ''
                    } : undefined,
                }),
            })
            if (!res.ok) {
                setMessages(prev => prev.map(m => m.messageId === assistantMsgId ? {
                    ...m,
                    content: `The local connector returned status ${res.status}. Check the Python connector and C++ brain processes.`
                } : m))
                return
            }

            const reader = res.body?.getReader()
            const decoder = new TextDecoder()
            let partialContent = ''
            let finalData: any = null
            let sseBuffer = ''

            if (reader) {
                while (true) {
                    const { done, value } = await reader.read()
                    if (done) break
                    sseBuffer += decoder.decode(value, { stream: true })

                    let newlineIndex
                    while ((newlineIndex = sseBuffer.indexOf('\n\n')) >= 0) {
                        const line = sseBuffer.slice(0, newlineIndex).trim()
                        sseBuffer = sseBuffer.slice(newlineIndex + 2)

                        if (line.startsWith('data: ')) {
                            const jsonStr = line.slice(6).trim()
                            if (jsonStr) {
                                try {
                                    const event = JSON.parse(jsonStr)
                                    if (event.type === 'chunk' && event.content) {
                                        if (isTyping) setIsTyping(false)
                                        partialContent += event.content
                                        setMessages(prev => prev.map(m =>
                                            m.messageId === assistantMsgId ? { ...m, content: partialContent } : m
                                        ))
                                    } else if (event.type === 'final' && event.data) {
                                        if (isTyping) setIsTyping(false)
                                        finalData = event.data
                                    } else if (event.type === 'error') {
                                        if (isTyping) setIsTyping(false)
                                        partialContent += `\n[Error: ${event.error}]`
                                        setMessages(prev => prev.map(m =>
                                            m.messageId === assistantMsgId ? { ...m, content: partialContent } : m
                                        ))
                                    }
                                } catch (e) {
                                    console.error('SSE parse error:', e, jsonStr)
                                }
                            }
                        }
                    }
                }
            }

            const data = finalData || {}
            let replyContent = data.response || data.reply || data.content || partialContent || JSON.stringify(data)
            let toolUsed = data.tool
            let confidence = data.confidence || 100
            let proposedCommand = ''
            let proposedUrl = ''
            const findings: ReviewFinding[] = Array.isArray(data.findings) ? data.findings : []
            const testGaps: string[] = Array.isArray(data.test_gaps) ? data.test_gaps : []
            const proposedCode = typeof data.proposed_code === 'string' ? data.proposed_code : ''
            const targetFile = typeof data.target_file === 'string' ? data.target_file : ''
            const targetPath = typeof data.target_path === 'string' ? data.target_path : ''
            const appliedContent = typeof data.applied_content === 'string' ? data.applied_content : ''
            const validation = data.validation && typeof data.validation === 'object' ? data.validation : undefined
            const analysis: AnalysisDetails | undefined = data.analysis && typeof data.analysis === 'object' && Array.isArray(data.analysis.steps)
                ? {
                    status: typeof data.analysis.status === 'string' ? data.analysis.status : undefined,
                    lastRunAt: typeof data.analysis.last_run_at === 'string' ? data.analysis.last_run_at : undefined,
                    preferenceIntent: typeof data.analysis.preference_intent === 'string' ? data.analysis.preference_intent : undefined,
                    preferredCommands: Array.isArray(data.analysis.preferred_commands)
                        ? data.analysis.preferred_commands.filter((item: any) => typeof item === 'string')
                        : undefined,
                    steps: data.analysis.steps
                        .filter((step: any) => step && typeof step.name === 'string')
                        .map((step: any) => ({
                            name: step.name,
                            category: typeof step.category === 'string' ? step.category : undefined,
                            status: typeof step.status === 'string' ? step.status : undefined,
                            reason: typeof step.reason === 'string' ? step.reason : undefined,
                            command: typeof step.command === 'string' ? step.command : undefined,
                            durationMs: typeof step.duration_ms === 'number' ? step.duration_ms : undefined,
                            summary: Array.isArray(step.summary) ? step.summary.filter((item: any) => typeof item === 'string') : undefined,
                            preferred: Boolean(step.preferred),
                        })),
                    alternatives: Array.isArray(data.analysis.alternatives)
                        ? data.analysis.alternatives
                            .filter((step: any) => step && typeof step.name === 'string')
                            .map((step: any) => ({
                                name: step.name,
                                category: typeof step.category === 'string' ? step.category : undefined,
                                status: typeof step.status === 'string' ? step.status : undefined,
                                reason: typeof step.reason === 'string' ? step.reason : undefined,
                                command: typeof step.command === 'string' ? step.command : undefined,
                                preferred: Boolean(step.preferred),
                            }))
                        : undefined,
                }
                : undefined
            const flowSections: FlowSection[] = Array.isArray(data.flow_sections)
                ? data.flow_sections
                    .filter((section: any) => section && typeof section === 'object' && Array.isArray(section.entries))
                    .map((section: any) => ({
                        type: section.type === 'file' ? 'file' : section.type === 'impact' ? 'impact' : 'symbol',
                        title: typeof section.title === 'string' ? section.title : 'Flow',
                        summary: typeof section.summary === 'string' ? section.summary : '',
                        entries: section.entries
                            .filter((entry: any) => entry && typeof entry.path === 'string')
                            .map((entry: any) => ({
                                label: typeof entry.label === 'string' ? entry.label : 'Reference',
                                path: entry.path,
                                line_start: typeof entry.line_start === 'number' ? entry.line_start : undefined,
                                line_end: typeof entry.line_end === 'number' ? entry.line_end : undefined,
                                kind: typeof entry.kind === 'string' ? entry.kind : undefined,
                                snippet: typeof entry.snippet === 'string' ? entry.snippet : undefined,
                            })),
                    }))
                    .filter((section: FlowSection) => section.entries.length > 0)
                : []
            let sourceLines: string[] = Array.isArray(data.sources)
                ? data.sources.map((s: any) => {
                    if (typeof s === 'string') return s
                    const path = s.path || s.file || 'unknown'
                    const start = s.line_start || s.lineStart
                    const end = s.line_end || s.lineEnd
                    return start ? `${path}:${start}${end ? `-${end}` : ''}` : path
                })
                : []

            try {
                // Only try to parse as JSON if it looks like a structured response
                if (replyContent.trim().startsWith('{') && replyContent.trim().endsWith('}')) {
                    const parsed = JSON.parse(replyContent.trim())
                    if (parsed.answer) {
                        replyContent = parsed.answer
                        confidence = parsed.confidence !== undefined ? parsed.confidence : confidence
                        if (parsed.sources && parsed.sources.length > 0) {
                            replyContent += '\n\nSources:\n' + parsed.sources.map((s: string) => `- ${s}`).join('\n')
                        }
                    } else if (parsed.response) {
                        replyContent = parsed.response
                    }
                }
            } catch {
                // Plain text response, keep as-is.
            }

            const runMatch = replyContent.match(/^\[ACTION:\s*RUN_COMMAND\]\s*([^\n]+)/im)
            if (runMatch) {
                const cmd = runMatch[1].trim()
                if (cmd && cmd !== 'command' && !cmd.includes('[') && !cmd.includes(']')) {
                    proposedCommand = cmd
                    toolUsed = `Proposed Command`
                }
                replyContent = replyContent.replace(runMatch[0], '')
            }

            const browseMatch = replyContent.match(/^\[ACTION:\s*OPEN_BROWSER\]\s*([^\n]+)/im)
            if (browseMatch) {
                const url = browseMatch[1].trim()
                if (url.startsWith('http://') || url.startsWith('https://')) {
                    proposedUrl = url
                    toolUsed = proposedCommand ? `${toolUsed} + Link` : 'Proposed Link'
                }
                replyContent = replyContent.replace(browseMatch[0], '')
            }

            const searchMatch = replyContent.match(/^\[ACTION:\s*SEARCH\]\s*([^\n]+)/im)
            if (searchMatch) {
                const query = searchMatch[1].trim()
                if (query && query !== 'query' && !query.includes('[') && !query.includes(']')) {
                    proposedUrl = 'https://google.com/search?q=' + encodeURIComponent(query)
                    toolUsed = proposedCommand ? `${toolUsed} + Search` : 'Proposed Search'
                }
                replyContent = replyContent.replace(searchMatch[0], '')
            }

            replyContent = replyContent.replace(/User request:\n/gi, '').trim()
            if (!replyContent) {
                replyContent = proposedCommand || proposedUrl ? 'Proposed action:' : 'Task complete.'
            }

            if (sourceLines.length > 0) {
                replyContent += '\n\nSources:\n' + sourceLines.map(line => `- ${line}`).join('\n')
            }

            setMessages(prev => prev.map(m => m.messageId === assistantMsgId ? {
                ...m,
                content: replyContent,
                tool: toolUsed,
                feedback: null,
                command: proposedCommand,
                browserUrl: proposedUrl,
                confidence,
                sources: sourceLines,
                flowSections,
                analysis,
                findings,
                testGaps,
                proposedCode,
                targetFile,
                targetPath,
                appliedContent,
                validation,
            } : m))
            setAttachedFiles([])
        } catch (err) {
            setMessages(prev => {
                const exists = prev.some(m => m.messageId === assistantMsgId)
                if (exists) {
                    return prev.map(m => m.messageId === assistantMsgId ? {
                        ...m,
                        content: `Could not reach the local connector at ${API}. Make sure the Python connector is running.`
                    } : m)
                } else {
                    return [...prev, {
                        role: 'assistant',
                        content: `Could not reach the local connector at ${API}. Make sure the Python connector is running.`,
                        timestamp: new Date(),
                    }]
                }
            })
        } finally {
            setIsTyping(false)
        }
    }

    const applyReviewedPatch = async (msg: Message): Promise<boolean> => {
        if (!msg.targetPath || !msg.appliedContent) {
            setMessages(prev => [...prev, {
                role: 'assistant',
                content: 'This patch is missing the final staged file content, so it cannot be applied safely.',
                timestamp: new Date(),
                tool: 'patch_apply',
            }])
            return false
        }

        const requestId = `patch-${Date.now()}-${Math.random().toString(36).slice(2, 8)}`

        return await new Promise<boolean>((resolve) => {
            let settled = false
            const handleResult = (event: Event) => {
                const customEvent = event as CustomEvent<{ requestId: string, success: boolean, message?: string }>
                if (customEvent.detail?.requestId !== requestId) return
                settled = true
                window.clearTimeout(timeoutId)
                window.removeEventListener('ai-apply-reviewed-patch-result', handleResult)
                if (!customEvent.detail?.success) {
                    setMessages(prev => [...prev, {
                        role: 'assistant',
                        content: customEvent.detail?.message || 'The reviewed patch could not be applied.',
                        timestamp: new Date(),
                        tool: 'patch_apply',
                    }])
                }
                resolve(Boolean(customEvent.detail?.success))
            }

            window.addEventListener('ai-apply-reviewed-patch-result', handleResult)
            window.dispatchEvent(new CustomEvent('ai-apply-reviewed-patch', {
                detail: {
                    requestId,
                    targetPath: msg.targetPath,
                    targetFile: msg.targetFile,
                    content: msg.appliedContent,
                },
            }))

            const timeoutId = window.setTimeout(() => {
                if (settled) return
                window.removeEventListener('ai-apply-reviewed-patch-result', handleResult)
                setMessages(prev => [...prev, {
                    role: 'assistant',
                    content: 'The editor did not confirm the patch apply request in time. Try again with the target file open.',
                    timestamp: new Date(),
                    tool: 'patch_apply',
                }])
                resolve(false)
            }, 4000)
        })
    }

    const handleQuickAction = (action: any) => {
        const cmd = action.cmd || `/${action.label.toLowerCase()} `
        setInput(cmd)
        // Give time for state update or pass directly
        setTimeout(() => {
            const sendBtn = document.querySelector('.ai-send-btn') as HTMLButtonElement
            sendBtn?.click()
        }, 50)
    }

    const formatTime = (d: Date) => d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })

    const handleFeedback = async (messageIndex: number, feedback: 'positive' | 'negative') => {
        const msg = messages[messageIndex]
        if (msg.role !== 'assistant') return

        const settings = localStorage.getItem('neural-studio-settings')
        const collectFeedback = settings ? JSON.parse(settings)['collect-feedback'] !== false : true
        if (!collectFeedback) return

        setMessages(prev => prev.map((m, i) => i === messageIndex ? { ...m, feedback } : m))

        try {
            const userMsg = messages[messageIndex - 1]
            await apiClient.fetch(`${API}/api/feedback`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    question: userMsg?.content || '',
                    answer: msg.content,
                    feedback,
                    timestamp: msg.timestamp.toISOString(),
                    messageId: msg.messageId || `msg-${Date.now()}`,
                }),
            })
        } catch (err) {
            console.error('Failed to send feedback:', err)
        }
    }

    const formatContent = (text: string) => {
        const blocks = text.split(/(```[\s\S]*?```)/g)
        return blocks.map((block, idx) => {
            if (block.startsWith('```') && block.endsWith('```')) {
                const lines = block.slice(3, -3).split('\n')
                const lang = lines[0].trim()
                const code = lines.slice(1).join('\n')
                if (lang === 'mermaid' || (lang === '' && code.trim().startsWith('graph') || code.trim().startsWith('sequenceDiagram') || code.trim().startsWith('classDiagram'))) {
                    return <Mermaid key={idx} chart={code} />
                }
                return <CodeBlock key={idx} language={lang} code={code} />
            }

            const parts = block.split(/(\*\*[^*]+\*\*)/g)
            return parts.map((part, i) => {
                if (part.startsWith('**') && part.endsWith('**')) {
                    return <strong key={`${idx}-${i}`} style={{ color: 'var(--accent)' }}>{part.slice(2, -2)}</strong>
                }

                const codeParts = part.split(/(`[^`]+`)/g)
                return codeParts.map((cp, j) => {
                    if (cp.startsWith('`') && cp.endsWith('`')) {
                        return <code key={`${idx}-${i}-${j}`} className="inline-code">{cp.slice(1, -1)}</code>
                    }
                    return <span key={`${idx}-${i}-${j}`} style={{ whiteSpace: 'pre-wrap' }}>{cp}</span>
                })
            })
        })
    }

    return (
        <div style={{ display: 'flex', width: '100%', height: '100%' }}>
            <div className="ai-chat-panel" style={{ flex: 1, borderRight: showBrowser ? '1px solid var(--border)' : 'none' }}>
                <div className="ai-chat-header">
                    <div className="ai-chat-header-left">
                        <div className="ai-chat-avatar">
                        <BrainCircuit size={16} />
                    </div>
                    <div>
                        <div className="ai-chat-name">AI Copilot</div>
                        <div className="ai-chat-status">
                            <div className="ai-status-dot" style={{
                                background: serverStatus === 'online' ? 'var(--green)' : 'var(--red)',
                                boxShadow: `0 0 6px ${serverStatus === 'online' ? 'var(--green)' : 'var(--red)'}`,
                            }} />
                            {serverStatus === 'online' ? 'Connected to Neural Engine' : 'Engine offline'}
                        </div>
                        <div style={{ fontSize: 10, color: 'var(--text-faint)', marginTop: 2 }}>
                            {projectRoot
                                ? `Workspace: ${projectRoot.split(/[\\/]/).pop() || projectRoot}${activeFilePath ? ` · Active file: ${activeFilePath.split(/[\\/]/).pop() || activeFilePath}` : ''}`
                                : 'Open a folder to give chat full workspace context'}
                        </div>
                        
                        <div style={{ marginTop: 6, display: 'flex', alignItems: 'center', gap: 6 }}>
                            <span style={{ fontSize: 10, color: 'var(--text-faint)' }}>Model:</span>
                            <select
                                value={selectedModel}
                                onChange={(e) => {
                                    const val = e.target.value
                                    setSelectedModel(val)
                                    localStorage.setItem('neural-selected-model', val)
                                }}
                                style={{
                                    background: 'rgba(255,255,255,0.05)',
                                    border: '1px solid var(--border)',
                                    borderRadius: 4,
                                    color: 'var(--text-muted)',
                                    fontSize: 10,
                                    padding: '1px 4px',
                                    outline: 'none',
                                    cursor: 'pointer',
                                    fontFamily: 'inherit'
                                }}
                            >
                                {availableModels.length > 0 ? (
                                    availableModels.map(m => (
                                        <option key={m} value={m} style={{ background: 'var(--bg-dark)' }}>{m}</option>
                                    ))
                                ) : (
                                    <option value={selectedModel}>{selectedModel}</option>
                                )}
                            </select>
                        </div>
                    </div>
                </div>
                <button
                    onClick={() => setWebEnabled(!webEnabled)}
                    style={{
                        display: 'flex', alignItems: 'center', gap: 5, padding: '4px 10px',
                        borderRadius: 16, border: '1px solid', cursor: 'pointer',
                        fontSize: 11, fontWeight: 600, fontFamily: 'inherit', transition: 'all 0.15s',
                        background: webEnabled ? 'rgba(74,158,255,0.08)' : 'transparent',
                        color: webEnabled ? 'var(--accent)' : 'var(--text-faint)',
                        borderColor: webEnabled ? 'rgba(74,158,255,0.3)' : 'var(--border)',
                    }}
                >
                    <Globe size={12} /> Web
                </button>
                <button
                    onClick={() => {
                        try { localStorage.removeItem(STORAGE_KEY) } catch {}
                        setMessages(defaultMessages)
                        setAttachedFiles([])
                    }}
                    title="Clear chat history"
                    style={{
                        display: 'flex', alignItems: 'center', gap: 5, padding: '4px 10px',
                        borderRadius: 16, border: '1px solid var(--border)', cursor: 'pointer',
                        fontSize: 11, fontWeight: 600, fontFamily: 'inherit', transition: 'all 0.15s',
                        background: 'transparent', color: 'var(--text-faint)',
                    }}
                >
                    Clear
                </button>
            </div>

            <div className="ai-chat-history" ref={historyRef}>
                {messages.map((msg, i) => {
                    if (msg.role === 'system') {
                        return (
                            <div key={i} style={{
                                textAlign: 'center', fontSize: 11, color: 'var(--text-faint)',
                                background: 'var(--bg-dark)', border: '1px solid var(--border)',
                                borderRadius: 16, padding: '4px 12px', margin: '0 auto', width: 'fit-content'
                            }}>
                                {msg.content}
                            </div>
                        )
                    }

                    return (
                        <div key={i} className={`ai-msg-row ${msg.role}`}>
                            <div className={`ai-msg-avatar-sm ${msg.role}`}>
                                {msg.role === 'assistant' ? 'AI' : 'U'}
                            </div>
                            <div className="ai-msg-content">
                                {msg.tool && (
                                    <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 4, flexWrap: 'wrap' }}>
                                        <div style={{
                                            fontSize: 10, fontWeight: 700, padding: '1px 6px', borderRadius: 8,
                                            border: '1px solid rgba(201,127,219,0.3)', background: 'rgba(201,127,219,0.08)',
                                            color: '#C97FDB', display: 'inline-block'
                                        }}>
                                            {msg.tool}
                                        </div>
                                        {typeof msg.confidence === 'number' && (
                                            <div style={{
                                                fontSize: 10,
                                                fontWeight: 700,
                                                padding: '1px 6px',
                                                borderRadius: 8,
                                                border: '1px solid rgba(74,158,255,0.25)',
                                                background: 'rgba(74,158,255,0.08)',
                                                color: 'var(--accent)',
                                            }}>
                                                {msg.confidence}% confidence
                                            </div>
                                        )}
                                    </div>
                                )}
                                <div className={`ai-bubble ${msg.role}`}>
                                    {msg.role === 'assistant' && (() => {
                                        const isCurrentlyThinking = isTyping && i === messages.length - 1 && !msg.content;
                                        const hasAnalysis = msg.analysis && msg.analysis.steps.length > 0;
                                        return (
                                            <>
                                                {(isCurrentlyThinking || hasAnalysis) && (
                                                    <ThoughtBlock 
                                                        isThinking={isCurrentlyThinking} 
                                                        analysis={msg.analysis} 
                                                        workspaceRoot={projectRoot} 
                                                    />
                                                )}
                                                {msg.content ? (
                                                    <div style={{ marginTop: 8 }}>
                                                        {formatContent(msg.content)}
                                                    </div>
                                                ) : null}
                                            </>
                                        );
                                    })()}
                                    {msg.role !== 'assistant' && msg.content && formatContent(msg.content)}
                                    {msg.proposedCode && (
                                        <div style={{ marginTop: 12 }}>
                                            <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', marginBottom: 6 }}>
                                                Proposed Code {msg.targetFile ? `for ${msg.targetFile}` : ''}
                                            </div>
                                            <CodeBlock
                                                code={msg.proposedCode}
                                                language={msg.targetFile?.split('.').pop() || 'code'}
                                                showApplyButton={Boolean(msg.validation?.status === 'ok' && msg.targetPath && msg.appliedContent)}
                                                onApply={msg.validation?.status === 'ok' && msg.targetPath && msg.appliedContent
                                                    ? () => applyReviewedPatch(msg)
                                                    : undefined}
                                                applyLabel={msg.validation?.status === 'ok' && msg.targetPath && msg.appliedContent ? 'Apply Reviewed Patch' : 'Apply'}
                                                applyTitle={msg.validation?.status === 'ok' && msg.targetPath && msg.appliedContent
                                                    ? 'Apply the validated and reviewed patch to the target file'
                                                    : 'Apply to Active File'}
                                            />
                                        </div>
                                    )}
                                    {msg.validation && (
                                        <div style={{
                                            marginTop: 12,
                                            border: '1px solid var(--border)',
                                            borderRadius: 8,
                                            padding: 10,
                                            background: 'rgba(0,0,0,0.15)',
                                        }}>
                                            <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', marginBottom: 6 }}>
                                                Validation
                                            </div>
                                            <div style={{ fontSize: 12, color: 'var(--text)' }}>
                                                Status: {msg.validation.status || 'unknown'}
                                            </div>
                                            {msg.validation.summary && (
                                                <div style={{ fontSize: 12, color: 'var(--text)', marginTop: 4, whiteSpace: 'pre-wrap' }}>
                                                    {msg.validation.summary}
                                                </div>
                                            )}
                                            {msg.validation.command && (
                                                <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 6 }}>
                                                    Command: <code className="inline-code">{msg.validation.command}</code>
                                                </div>
                                            )}
                                        </div>
                                    )}
                                    {msg.flowSections && msg.flowSections.length > 0 && (
                                        <FlowSectionsBlock sections={msg.flowSections} />
                                    )}
                                    {msg.findings && msg.findings.length > 0 && (
                                        <ReviewFindingsBlock findings={msg.findings} />
                                    )}
                                    {msg.testGaps && msg.testGaps.length > 0 && (
                                        <div style={{ marginTop: 12 }}>
                                            <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', marginBottom: 6 }}>
                                                Test Gaps
                                            </div>
                                            <div style={{ display: 'flex', flexDirection: 'column', gap: 6 }}>
                                                {msg.testGaps.map((gap, gapIndex) => (
                                                    <div
                                                        key={`${gap}-${gapIndex}`}
                                                        style={{
                                                            fontSize: 12,
                                                            color: 'var(--text)',
                                                            background: 'rgba(0,0,0,0.15)',
                                                            border: '1px solid var(--border)',
                                                            borderRadius: 8,
                                                            padding: 10,
                                                        }}
                                                    >
                                                        {gap}
                                                    </div>
                                                ))}
                                            </div>
                                        </div>
                                    )}
                                    {msg.command && (
                                        <ActionPreview
                                            label="PROPOSED COMMAND"
                                            value={msg.command}
                                            buttonLabel="Send to Terminal"
                                            onRun={() => window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd: msg.command } }))}
                                        />
                                    )}
                                    {msg.browserUrl && (
                                        <ActionPreview
                                            label="PROPOSED LINK"
                                            value={msg.browserUrl}
                                            buttonLabel="Open Browser"
                                            onRun={() => {
                                                setBrowserUrl(msg.browserUrl || 'https://github.com')
                                                setShowBrowser(true)
                                            }}
                                        />
                                    )}
                                </div>
                                <div style={{
                                    display: 'flex', alignItems: 'center', gap: 8, marginTop: 4,
                                    justifyContent: msg.role === 'user' ? 'flex-end' : 'flex-start'
                                }}>
                                    <div className="ai-msg-time">
                                        {formatTime(msg.timestamp)}
                                    </div>
                                    {msg.role === 'assistant' && (
                                        <div style={{ display: 'flex', gap: 4 }}>
                                            <button
                                                onClick={() => handleFeedback(i, 'positive')}
                                                style={{
                                                    background: msg.feedback === 'positive' ? 'rgba(74,222,128,0.15)' : 'transparent',
                                                    border: '1px solid',
                                                    borderColor: msg.feedback === 'positive' ? 'rgba(74,222,128,0.5)' : 'var(--border)',
                                                    borderRadius: 4, padding: '2px 6px', cursor: 'pointer',
                                                    display: 'flex', alignItems: 'center', color: msg.feedback === 'positive' ? '#4ADE80' : 'var(--text-muted)',
                                                    transition: 'all 0.15s', fontSize: 10
                                                }}
                                                title="Good response"
                                            >
                                                <ThumbsUp size={11} />
                                            </button>
                                            <button
                                                onClick={() => handleFeedback(i, 'negative')}
                                                style={{
                                                    background: msg.feedback === 'negative' ? 'rgba(239,68,68,0.15)' : 'transparent',
                                                    border: '1px solid',
                                                    borderColor: msg.feedback === 'negative' ? 'rgba(239,68,68,0.5)' : 'var(--border)',
                                                    borderRadius: 4, padding: '2px 6px', cursor: 'pointer',
                                                    display: 'flex', alignItems: 'center', color: msg.feedback === 'negative' ? '#EF4444' : 'var(--text-muted)',
                                                    transition: 'all 0.15s', fontSize: 10
                                                }}
                                                title="Bad response"
                                            >
                                                <ThumbsDown size={11} />
                                            </button>
                                        </div>
                                    )}
                                </div>
                            </div>
                        </div>
                    )
                })}

                {isTyping && messages[messages.length - 1]?.role !== 'assistant' && (
                    <div className="ai-msg-row">
                        <div className="ai-msg-avatar-sm assistant">AI</div>
                        <div className="ai-typing">
                            <div className="dot" /><div className="dot" /><div className="dot" />
                        </div>
                    </div>
                )}
            </div>

            <div style={{
                display: 'flex', gap: 4, padding: '6px 16px', overflowX: 'auto',
                flexShrink: 0, scrollbarWidth: 'none'
            }}>
                {QUICK_ACTIONS.map(action => (
                    <button key={action.label} onClick={() => handleQuickAction(action)} style={{
                        display: 'flex', alignItems: 'center', gap: 4, padding: '4px 10px',
                        borderRadius: 16, border: '1px solid var(--border)', background: 'var(--bg-dark)',
                        color: 'var(--text-muted)', fontSize: 11, fontWeight: 600, cursor: 'pointer',
                        whiteSpace: 'nowrap', fontFamily: 'inherit', transition: 'all 0.12s'
                    }}
                        onMouseEnter={e => {
                            e.currentTarget.style.borderColor = action.color
                            e.currentTarget.style.color = action.color
                        }}
                        onMouseLeave={e => {
                            e.currentTarget.style.borderColor = 'var(--border)'
                            e.currentTarget.style.color = 'var(--text-muted)'
                        }}
                    >
                        <Zap size={10} /> {action.label}
                    </button>
                ))}
            </div>

            <div className="ai-input-area">
                {attachedFiles.length > 0 && (
                    <div style={{
                        display: 'flex',
                        gap: 6,
                        flexWrap: 'wrap',
                        margin: '0 16px 8px',
                    }}>
                        {attachedFiles.map((file) => (
                            <div
                                key={file.path}
                                style={{
                                    display: 'flex',
                                    alignItems: 'center',
                                    gap: 6,
                                    padding: '4px 8px',
                                    borderRadius: 999,
                                    border: '1px solid var(--border)',
                                    background: 'var(--bg-dark)',
                                    color: 'var(--text-muted)',
                                    fontSize: 11,
                                }}
                            >
                                <span>{file.name}</span>
                                <button
                                    onClick={() => removeAttachment(file.path)}
                                    style={{
                                        background: 'none',
                                        border: 'none',
                                        color: 'var(--text-faint)',
                                        cursor: 'pointer',
                                        padding: 0,
                                        fontSize: 12,
                                        lineHeight: 1,
                                    }}
                                    title="Remove attachment"
                                >
                                    ×
                                </button>
                            </div>
                        ))}
                    </div>
                )}
                <div className="ai-input-box">
                    <button className="ai-input-action" title="Attach active file or choose a file" onClick={() => { void handleAttachFile() }}>
                        <Paperclip size={15} />
                    </button>
                    <input
                        type="text"
                        className="ai-input-field"
                        placeholder={serverStatus === 'online' ? 'Ask AI anything...' : 'AI Engine offline - type a message...'}
                        value={input}
                        onChange={e => setInput(e.target.value)}
                        onKeyDown={e => e.key === 'Enter' && !e.shiftKey && sendMessage()}
                    />
                    {isTyping ? (
                        <button className="ai-send-btn" onClick={() => setIsTyping(false)}>
                            <Square size={14} fill="currentColor" />
                        </button>
                    ) : (
                        <button className="ai-send-btn" onClick={sendMessage} disabled={!input.trim()}>
                            <ArrowUp size={16} />
                        </button>
                    )}
                </div>
                <div style={{
                    textAlign: 'center', fontSize: 10, color: 'var(--text-faint)', marginTop: 6
                }}>
                    Neural Studio AI · Local C++ brain through Python connector
                </div>
            </div>
        </div>
            
            {/* Claude-style embedded Web Browser Panel */}
            {showBrowser && (
                <div style={{ flex: 1, height: '100%', display: 'flex', flexDirection: 'column', background: 'var(--bg-lighter)' }}>
                    <div style={{ 
                        display: 'flex', justifyContent: 'space-between', alignItems: 'center', 
                        padding: '8px 16px', background: 'var(--bg-dark)', borderBottom: '1px solid var(--border)' 
                    }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8, fontSize: 12, fontWeight: 600, color: 'var(--text-secondary)' }}>
                            <Globe size={14} /> Neural Web Engine
                        </div>
                        <button 
                            onClick={() => setShowBrowser(false)}
                            style={{ background: 'none', border: 'none', color: 'var(--text-faint)', cursor: 'pointer', padding: 4 }}
                            title="Close Browser Split"
                        >
                            ✕
                        </button>
                    </div>
                    <div style={{ flex: 1, position: 'relative', overflow: 'hidden' }}>
                        <WebViewPanel url={browserUrl} />
                    </div>
                </div>
            )}
        </div>
    )
}
