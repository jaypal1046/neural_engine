import { useState, useRef, useEffect, useCallback } from 'react'
import { Terminal, Plus, X, Trash2, AlertCircle, AlertTriangle, Info, CheckCircle, FileText, Columns, Maximize2 } from 'lucide-react'

interface TerminalInstance {
    id: string
    title: string
    output: string[]
    alive: boolean
}

export interface ProblemItem {
    type: 'error' | 'warning' | 'info'
    message: string
    file: string
    line?: number
    column?: number
    source?: string
}

interface OutputEntry {
    timestamp: string
    text: string
    type: 'info' | 'error' | 'server'
}

interface Props {
    activeTab: string
    onTabChange: (tab: string) => void
    onClose: () => void
    projectRoot: string
    problems?: ProblemItem[]
    onProblemClick?: (file: string, line?: number) => void
}

export function TerminalPanel({ activeTab, onTabChange, onClose, projectRoot, problems = [], onProblemClick }: Props) {
    const [terminals, setTerminals] = useState<TerminalInstance[]>([])
    const [activeTerminalId, setActiveTerminalId] = useState<string>('')
    const [inputValues, setInputValues] = useState<Record<string, string>>({})
    const [outputLogs, setOutputLogs] = useState<OutputEntry[]>([])

    // Terminal split state
    const [splitTerminals, setSplitTerminals] = useState<string[]>([]) // IDs in split view
    const [isSplit, setIsSplit] = useState(false)

    const outputRef = useRef<HTMLDivElement>(null)
    const outputPanelRef = useRef<HTMLDivElement>(null)
    const inputRefs = useRef<Record<string, HTMLInputElement | null>>({})

    // Create first terminal on mount
    useEffect(() => {
        if (terminals.length === 0) {
            createTerminal()
        }
    }, [])

    // Listen for server logs
    useEffect(() => {
        const handleServerLog = (e: Event) => {
            const ce = e as CustomEvent<{ text: string; type?: string }>
            if (ce.detail?.text) {
                setOutputLogs(prev => [...prev.slice(-500), {
                    timestamp: new Date().toLocaleTimeString(),
                    text: ce.detail.text,
                    type: (ce.detail.type as any) || 'server'
                }])
            }
        }
        window.addEventListener('server-log', handleServerLog)
        return () => window.removeEventListener('server-log', handleServerLog)
    }, [])

    // Auto-scroll output panel
    useEffect(() => {
        if (outputPanelRef.current && activeTab === 'output') {
            outputPanelRef.current.scrollTop = outputPanelRef.current.scrollHeight
        }
    }, [outputLogs, activeTab])

    const createTerminal = useCallback(async (customCwd?: string) => {
        const cwd = typeof customCwd === 'string' ? customCwd : projectRoot
        const id = `term-${Date.now()}`
        const newTerm: TerminalInstance = {
            id,
            title: `Terminal ${terminals.length + 1}`,
            output: [`Neural Studio Terminal — ${new Date().toLocaleString()}\n`],
            alive: true,
        }

        setTerminals(prev => [...prev, newTerm])
        setActiveTerminalId(id)
        setInputValues(prev => ({ ...prev, [id]: '' }))

        // Spawn actual terminal process
        if (window.terminal?.spawn) {
            try {
                await window.terminal.spawn(id, cwd)

                window.terminal.onData(id, (data: string) => {
                    setTerminals(prev => prev.map(t =>
                        t.id === id ? { ...t, output: [...t.output, data] } : t
                    ))
                })

                window.terminal.onExit(id, (_code: number) => {
                    setTerminals(prev => prev.map(t =>
                        t.id === id ? { ...t, alive: false, output: [...t.output, '\n[Process exited]\n'] } : t
                    ))
                })
            } catch (e) {
                console.error('Failed to spawn terminal:', e)
                setTerminals(prev => prev.map(t =>
                    t.id === id ? { ...t, output: [...t.output, 'Failed to start terminal process.\n'] } : t
                ))
            }
        }
    }, [terminals.length, projectRoot])

    const closeTerminal = useCallback((id: string) => {
        if (window.terminal?.kill) {
            window.terminal.kill(id)
        }
        // Remove from split view if present
        setSplitTerminals(prev => prev.filter(sid => sid !== id))
        setTerminals(prev => {
            const next = prev.filter(t => t.id !== id)
            if (activeTerminalId === id && next.length > 0) {
                setActiveTerminalId(next[next.length - 1].id)
            }
            return next
        })
    }, [activeTerminalId])

    const clearTerminal = useCallback(() => {
        if (activeTab === 'output') {
            setOutputLogs([])
        } else {
            setTerminals(prev => prev.map(t =>
                t.id === activeTerminalId ? { ...t, output: [] } : t
            ))
        }
    }, [activeTerminalId, activeTab])

    const handleInput = useCallback((termId: string, e: React.KeyboardEvent) => {
        if (e.key === 'Enter') {
            e.preventDefault()
            const cmd = inputValues[termId] || ''
            setInputValues(prev => ({ ...prev, [termId]: '' }))

            if (window.terminal?.write) {
                window.terminal.write(termId, cmd + '\n')
            }

            setTerminals(prev => prev.map(t =>
                t.id === termId ? { ...t, output: [...t.output, `> ${cmd}\n`] } : t
            ))
        }
    }, [inputValues])

    // Auto-scroll active terminal
    useEffect(() => {
        if (outputRef.current) {
            outputRef.current.scrollTop = outputRef.current.scrollHeight
        }
    }, [terminals, activeTerminalId])

    // Focus input on terminal switch
    useEffect(() => {
        inputRefs.current[activeTerminalId]?.focus()
    }, [activeTerminalId])

    // Listen for custom commands to run
    useEffect(() => {
        const handleRunCommand = (e: Event) => {
            const customEvent = e as CustomEvent<{ cmd: string }>
            const cmd = customEvent.detail?.cmd
            if (cmd && activeTerminalId && window.terminal?.write) {
                window.terminal.write(activeTerminalId, cmd + '\n')
                setTerminals(prev => prev.map(t =>
                    t.id === activeTerminalId ? { ...t, output: [...t.output, `> ${cmd}\n`] } : t
                ))
            }
        }
        window.addEventListener('run-terminal-command', handleRunCommand)
        
        const handleOpenTerminalCwd = (e: Event) => {
            const customEvent = e as CustomEvent<string>
            const cwd = customEvent.detail
            if (cwd) {
                createTerminal(cwd)
            }
        }
        window.addEventListener('open-terminal-cwd', handleOpenTerminalCwd)

        return () => {
            window.removeEventListener('run-terminal-command', handleRunCommand)
            window.removeEventListener('open-terminal-cwd', handleOpenTerminalCwd)
        }
    }, [activeTerminalId, createTerminal])

    // Split terminal: show current + create new side-by-side
    const handleSplitTerminal = useCallback(async () => {
        if (isSplit) {
            // Unsplit — go back to single view
            setIsSplit(false)
            setSplitTerminals([])
            return
        }
        // Create a new terminal for the right split
        const id = `term-${Date.now()}`
        const newTerm: TerminalInstance = {
            id,
            title: `Terminal ${terminals.length + 1}`,
            output: [`Neural Studio Terminal — ${new Date().toLocaleString()}\n`],
            alive: true,
        }
        setTerminals(prev => [...prev, newTerm])
        setInputValues(prev => ({ ...prev, [id]: '' }))

        // Spawn
        if (window.terminal?.spawn) {
            try {
                await window.terminal.spawn(id, projectRoot)
                window.terminal.onData(id, (data: string) => {
                    setTerminals(prev => prev.map(t =>
                        t.id === id ? { ...t, output: [...t.output, data] } : t
                    ))
                })
                window.terminal.onExit(id, (_code: number) => {
                    setTerminals(prev => prev.map(t =>
                        t.id === id ? { ...t, alive: false, output: [...t.output, '\n[Process exited]\n'] } : t
                    ))
                })
            } catch (e) {
                console.error('Failed to spawn terminal:', e)
            }
        }

        setSplitTerminals([activeTerminalId, id])
        setIsSplit(true)
    }, [activeTerminalId, isSplit, terminals.length, projectRoot])

    const activeTerm = terminals.find(t => t.id === activeTerminalId)
    const errorCount = problems.filter(p => p.type === 'error').length
    const warningCount = problems.filter(p => p.type === 'warning').length

    // Render a single terminal pane
    const renderTerminalPane = (term: TerminalInstance, isActive: boolean) => (
        <div
            className={`terminal-split-pane ${isActive ? 'active' : ''}`}
            key={term.id}
            onClick={() => {
                setActiveTerminalId(term.id)
                inputRefs.current[term.id]?.focus()
            }}
        >
            {/* Pane header */}
            <div className="terminal-pane-header">
                <Terminal size={11} />
                <span className="terminal-pane-title">{term.title}</span>
                {!term.alive && <span className="terminal-pane-dead">exited</span>}
                {isSplit && (
                    <button
                        className="terminal-pane-maximize"
                        onClick={(e) => {
                            e.stopPropagation()
                            setIsSplit(false)
                            setSplitTerminals([])
                            setActiveTerminalId(term.id)
                        }}
                        title="Maximize"
                    >
                        <Maximize2 size={11} />
                    </button>
                )}
            </div>
            {/* Pane output */}
            <div className="terminal-output terminal-split-output" ref={isActive ? outputRef : undefined}>
                {term.output.map((line, i) => (
                    <span key={i}>{line}</span>
                ))}
            </div>
            {term.alive && (
                <div className="terminal-input-line">
                    <span className="terminal-prompt">❯</span>
                    <input
                        ref={(el) => { inputRefs.current[term.id] = el }}
                        className="terminal-input"
                        value={inputValues[term.id] || ''}
                        onChange={e => setInputValues(prev => ({ ...prev, [term.id]: e.target.value }))}
                        onKeyDown={(e) => handleInput(term.id, e)}
                        placeholder="Type a command..."
                        spellCheck={false}
                        autoFocus={isActive}
                    />
                </div>
            )}
        </div>
    )

    return (
        <>
            {/* Panel tabs bar */}
            <div className="panel-tabs-bar">
                <button
                    className={`panel-tab ${activeTab === 'terminal' ? 'active' : ''}`}
                    onClick={() => onTabChange('terminal')}
                >
                    <Terminal size={12} /> Terminal
                </button>
                <button
                    className={`panel-tab ${activeTab === 'output' ? 'active' : ''}`}
                    onClick={() => onTabChange('output')}
                >
                    <FileText size={12} /> Output
                    {outputLogs.length > 0 && (
                        <span className="panel-tab-badge">{outputLogs.length}</span>
                    )}
                </button>
                <button
                    className={`panel-tab ${activeTab === 'problems' ? 'active' : ''}`}
                    onClick={() => onTabChange('problems')}
                >
                    <AlertCircle size={12} /> Problems
                    {(errorCount + warningCount) > 0 && (
                        <span className={`panel-tab-badge ${errorCount > 0 ? 'error' : 'warning'}`}>
                            {errorCount + warningCount}
                        </span>
                    )}
                </button>

                <div className="panel-actions">
                    {/* Terminal instance tabs */}
                    {activeTab === 'terminal' && !isSplit && terminals.map(term => (
                        <button
                            key={term.id}
                            className={`panel-tab terminal-instance-tab ${term.id === activeTerminalId ? 'active' : ''}`}
                            onClick={() => setActiveTerminalId(term.id)}
                        >
                            <Terminal size={10} />
                            {term.title}
                            <span
                                className="terminal-tab-close"
                                onClick={(e) => { e.stopPropagation(); closeTerminal(term.id) }}
                            >×</span>
                        </button>
                    ))}

                    {activeTab === 'terminal' && (
                        <button
                            className={`panel-action-btn ${isSplit ? 'active' : ''}`}
                            onClick={handleSplitTerminal}
                            title={isSplit ? 'Unsplit Terminal' : 'Split Terminal Right'}
                        >
                            <Columns size={14} />
                        </button>
                    )}
                    <button className="panel-action-btn" onClick={() => createTerminal()} title="New Terminal">
                        <Plus size={14} />
                    </button>
                    <button className="panel-action-btn" onClick={clearTerminal} title="Clear">
                        <Trash2 size={14} />
                    </button>
                    <button className="panel-action-btn" onClick={onClose} title="Close Panel">
                        <X size={14} />
                    </button>
                </div>
            </div>

            {/* Panel content */}
            <div className="panel-content">
                {activeTab === 'terminal' && (
                    <>
                        {isSplit && splitTerminals.length >= 2 ? (
                            // ── Split View ──
                            <div className="terminal-split-container">
                                {splitTerminals.map(sid => {
                                    const term = terminals.find(t => t.id === sid)
                                    if (!term) return null
                                    return renderTerminalPane(term, sid === activeTerminalId)
                                })}
                            </div>
                        ) : (
                            // ── Single View ──
                            activeTerm && (
                                <div className="terminal-container" onClick={() => inputRefs.current[activeTerminalId]?.focus()}>
                                    <div className="terminal-output" ref={outputRef}>
                                        {activeTerm.output.map((line, i) => (
                                            <span key={i}>{line}</span>
                                        ))}
                                    </div>
                                    {activeTerm.alive && (
                                        <div className="terminal-input-line">
                                            <span className="terminal-prompt">❯</span>
                                            <input
                                                ref={(el) => { inputRefs.current[activeTerminalId] = el }}
                                                className="terminal-input"
                                                value={inputValues[activeTerminalId] || ''}
                                                onChange={e => setInputValues(prev => ({ ...prev, [activeTerminalId]: e.target.value }))}
                                                onKeyDown={(e) => handleInput(activeTerminalId, e)}
                                                placeholder="Type a command..."
                                                spellCheck={false}
                                                autoFocus
                                            />
                                        </div>
                                    )}
                                </div>
                            )
                        )}
                    </>
                )}

                {activeTab === 'output' && (
                    <div className="terminal-container">
                        <div className="terminal-output output-panel" ref={outputPanelRef}>
                            {outputLogs.length === 0 ? (
                                <div className="output-empty">
                                    <FileText size={20} style={{ color: 'var(--text-faint)', opacity: 0.5 }} />
                                    <span>No output yet. Server logs and task output will appear here.</span>
                                </div>
                            ) : (
                                outputLogs.map((entry, i) => (
                                    <div key={i} className={`output-line ${entry.type}`}>
                                        <span className="output-timestamp">[{entry.timestamp}]</span>
                                        <span className="output-text">{entry.text}</span>
                                    </div>
                                ))
                            )}
                        </div>
                    </div>
                )}

                {activeTab === 'problems' && (
                    <div className="terminal-container">
                        <div className="terminal-output problems-panel">
                            {problems.length === 0 ? (
                                <div className="problems-empty">
                                    <CheckCircle size={20} style={{ color: 'var(--green)' }} />
                                    <span>No problems detected in workspace.</span>
                                </div>
                            ) : (
                                <>
                                    <div className="problems-summary">
                                        {errorCount > 0 && (
                                            <span className="problems-count error">
                                                <AlertCircle size={12} /> {errorCount} error{errorCount !== 1 ? 's' : ''}
                                            </span>
                                        )}
                                        {warningCount > 0 && (
                                            <span className="problems-count warning">
                                                <AlertTriangle size={12} /> {warningCount} warning{warningCount !== 1 ? 's' : ''}
                                            </span>
                                        )}
                                    </div>
                                    {problems.map((problem, i) => (
                                        <button
                                            key={i}
                                            className={`problem-item ${problem.type}`}
                                            onClick={() => onProblemClick?.(problem.file, problem.line)}
                                        >
                                            <span className="problem-icon">
                                                {problem.type === 'error' && <AlertCircle size={13} />}
                                                {problem.type === 'warning' && <AlertTriangle size={13} />}
                                                {problem.type === 'info' && <Info size={13} />}
                                            </span>
                                            <span className="problem-message">{problem.message}</span>
                                            <span className="problem-source">{problem.source || ''}</span>
                                            <span className="problem-location">
                                                {problem.file.split(/[/\\]/).pop()}
                                                {problem.line && `:${problem.line}`}
                                                {problem.column && `:${problem.column}`}
                                            </span>
                                        </button>
                                    ))}
                                </>
                            )}
                        </div>
                    </div>
                )}
            </div>
        </>
    )
}
