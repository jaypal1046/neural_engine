import { useEffect, useState } from 'react'
import { Database, RefreshCw, FolderOpen, FileText, TerminalSquare, Clock3 } from 'lucide-react'

interface MemoryItem {
    path?: string
    timestamp?: string
    kind?: string
}

interface CommandItem {
    command?: string
    timestamp?: string
}

interface EventItem {
    type?: string
    timestamp?: string
    filePath?: string
    folderPath?: string
    command?: string
    cwd?: string
}

interface WorkspaceMemoryState {
    workspaceRoot?: string
    workspaceId?: string
    lastActivityAt?: string
    eventCounts?: Record<string, number>
    recentFiles?: MemoryItem[]
    recentCommands?: CommandItem[]
}

interface WorkspaceMemorySnapshot {
    workspaceRoot?: string
    workspaceId?: string
    storagePath?: string
    state?: WorkspaceMemoryState
    events?: EventItem[]
}

interface Props {
    projectRoot: string
    onFileOpen: (filePath: string, fileName: string) => void
}

function formatTime(value?: string) {
    if (!value) return 'No activity yet'
    try {
        return new Date(value).toLocaleString()
    } catch {
        return value
    }
}

export function ProjectMemoryPanel({ projectRoot, onFileOpen }: Props) {
    const [memory, setMemory] = useState<WorkspaceMemorySnapshot | null>(null)
    const [loading, setLoading] = useState(true)
    const [refreshing, setRefreshing] = useState(false)

    const loadMemory = async (showSpinner: boolean = false) => {
        if (!window.appApi?.getWorkspaceMemory || !projectRoot) {
            setLoading(false)
            return
        }

        if (showSpinner) setRefreshing(true)
        try {
            const snapshot = await window.appApi.getWorkspaceMemory(projectRoot)
            setMemory(snapshot)
        } catch (error) {
            console.error('Failed to load workspace memory:', error)
        } finally {
            setLoading(false)
            setRefreshing(false)
        }
    }

    useEffect(() => {
        setLoading(true)
        loadMemory()
        const interval = window.setInterval(() => {
            loadMemory()
        }, 4000)
        return () => window.clearInterval(interval)
    }, [projectRoot])

    const state = memory?.state
    const recentFiles = state?.recentFiles || []
    const recentCommands = state?.recentCommands || []
    const events = memory?.events || []
    const eventEntries = Object.entries(state?.eventCounts || {})

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            <div className="sidebar-header">
                <span>Project Memory</span>
                <div className="sidebar-header-actions">
                    <button
                        className="sidebar-action-btn"
                        onClick={() => loadMemory(true)}
                        title="Refresh project memory"
                    >
                        <RefreshCw size={13} style={{ animation: refreshing ? 'spin 1s linear infinite' : 'none' }} />
                    </button>
                </div>
            </div>

            <div className="sidebar-content" style={{ padding: 12, display: 'flex', flexDirection: 'column', gap: 12 }}>
                {loading ? (
                    <div style={{ color: 'var(--text-muted)', fontSize: 12 }}>Loading project memory...</div>
                ) : (
                    <>
                        <div style={{
                            background: 'var(--bg-card)',
                            border: '1px solid var(--border)',
                            borderRadius: 10,
                            padding: 12,
                        }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 10 }}>
                                <Database size={16} style={{ color: 'var(--accent)' }} />
                                <div style={{ fontSize: 12, fontWeight: 700 }}>Separate Local Memory Store</div>
                            </div>
                            <div style={{ fontSize: 11, color: 'var(--text-secondary)', lineHeight: 1.6 }}>
                                <div>Workspace: {memory?.workspaceRoot || projectRoot}</div>
                                <div>Storage: {memory?.storagePath || 'Not available yet'}</div>
                                <div>Last activity: {formatTime(state?.lastActivityAt)}</div>
                            </div>
                        </div>

                        <div style={{
                            background: 'var(--bg-card)',
                            border: '1px solid var(--border)',
                            borderRadius: 10,
                            padding: 12,
                        }}>
                            <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', marginBottom: 10, textTransform: 'uppercase', letterSpacing: 0.5 }}>
                                Event Counts
                            </div>
                            {eventEntries.length === 0 ? (
                                <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>No tracked activity yet.</div>
                            ) : (
                                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(2, minmax(0, 1fr))', gap: 8 }}>
                                    {eventEntries.map(([eventType, count]) => (
                                        <div key={eventType} style={{
                                            background: 'var(--bg-darkest)',
                                            border: '1px solid var(--border-subtle)',
                                            borderRadius: 8,
                                            padding: '8px 10px',
                                        }}>
                                            <div style={{ fontSize: 10, color: 'var(--text-faint)', textTransform: 'uppercase' }}>{eventType.replace(/_/g, ' ')}</div>
                                            <div style={{ fontSize: 18, fontWeight: 700, color: 'var(--text)' }}>{String(count)}</div>
                                        </div>
                                    ))}
                                </div>
                            )}
                        </div>

                        <div style={{
                            background: 'var(--bg-card)',
                            border: '1px solid var(--border)',
                            borderRadius: 10,
                            padding: 12,
                        }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 10 }}>
                                <FileText size={14} style={{ color: 'var(--accent)' }} />
                                <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', textTransform: 'uppercase', letterSpacing: 0.5 }}>
                                    Recent Files
                                </div>
                            </div>
                            {recentFiles.length === 0 ? (
                                <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>Open files or folders to build project memory.</div>
                            ) : (
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                                    {recentFiles.slice(0, 8).map((item, index) => {
                                        const filePath = item.path || ''
                                        const fileName = filePath.split(/[\\/]/).pop() || filePath
                                        return (
                                            <button
                                                key={`${filePath}-${index}`}
                                                onClick={() => filePath && onFileOpen(filePath, fileName)}
                                                style={{
                                                    width: '100%',
                                                    textAlign: 'left',
                                                    background: 'var(--bg-darkest)',
                                                    border: '1px solid var(--border-subtle)',
                                                    borderRadius: 8,
                                                    padding: 10,
                                                    color: 'var(--text)',
                                                    cursor: filePath ? 'pointer' : 'default',
                                                    fontFamily: 'inherit',
                                                }}
                                            >
                                                <div style={{ fontSize: 12, fontWeight: 600 }}>{fileName}</div>
                                                <div style={{ fontSize: 10, color: 'var(--text-muted)', marginTop: 4, wordBreak: 'break-all' }}>{filePath}</div>
                                                <div style={{ fontSize: 10, color: 'var(--text-faint)', marginTop: 4 }}>
                                                    {item.kind || 'file_accessed'} at {formatTime(item.timestamp)}
                                                </div>
                                            </button>
                                        )
                                    })}
                                </div>
                            )}
                        </div>

                        <div style={{
                            background: 'var(--bg-card)',
                            border: '1px solid var(--border)',
                            borderRadius: 10,
                            padding: 12,
                        }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 10 }}>
                                <TerminalSquare size={14} style={{ color: 'var(--green)' }} />
                                <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', textTransform: 'uppercase', letterSpacing: 0.5 }}>
                                    Recent Commands
                                </div>
                            </div>
                            {recentCommands.length === 0 ? (
                                <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>Run commands in the integrated terminal to record them here.</div>
                            ) : (
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                                    {recentCommands.slice(0, 8).map((item, index) => (
                                        <div
                                            key={`${item.command}-${index}`}
                                            style={{
                                                background: 'var(--bg-darkest)',
                                                border: '1px solid var(--border-subtle)',
                                                borderRadius: 8,
                                                padding: 10,
                                            }}
                                        >
                                            <div style={{ fontSize: 11, color: 'var(--green)', fontFamily: 'JetBrains Mono, monospace', wordBreak: 'break-word' }}>
                                                {item.command || 'Unknown command'}
                                            </div>
                                            <div style={{ fontSize: 10, color: 'var(--text-faint)', marginTop: 4 }}>
                                                {formatTime(item.timestamp)}
                                            </div>
                                        </div>
                                    ))}
                                </div>
                            )}
                        </div>

                        <div style={{
                            background: 'var(--bg-card)',
                            border: '1px solid var(--border)',
                            borderRadius: 10,
                            padding: 12,
                        }}>
                            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 10 }}>
                                <Clock3 size={14} style={{ color: 'var(--yellow)' }} />
                                <div style={{ fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)', textTransform: 'uppercase', letterSpacing: 0.5 }}>
                                    Recent Activity
                                </div>
                            </div>
                            {events.length === 0 ? (
                                <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>No events recorded yet.</div>
                            ) : (
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 8 }}>
                                    {events.slice().reverse().slice(0, 12).map((event, index) => {
                                        const detail = event.filePath || event.folderPath || event.command || event.cwd || ''
                                        const canOpenFile = Boolean(event.filePath)
                                        const filePath = event.filePath || ''
                                        const fileName = filePath.split(/[\\/]/).pop() || filePath
                                        return (
                                            <div
                                                key={`${event.timestamp}-${index}`}
                                                style={{
                                                    background: 'var(--bg-darkest)',
                                                    border: '1px solid var(--border-subtle)',
                                                    borderRadius: 8,
                                                    padding: 10,
                                                }}
                                            >
                                                <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 4 }}>
                                                    {event.type === 'command_executed' ? <TerminalSquare size={12} style={{ color: 'var(--green)' }} /> : null}
                                                    {event.type === 'file_accessed' ? <FileText size={12} style={{ color: 'var(--accent)' }} /> : null}
                                                    {event.type === 'folder_accessed' ? <FolderOpen size={12} style={{ color: 'var(--yellow)' }} /> : null}
                                                    {!['command_executed', 'file_accessed', 'folder_accessed'].includes(event.type || '') ? <Database size={12} style={{ color: 'var(--purple)' }} /> : null}
                                                    <div style={{ fontSize: 11, fontWeight: 600, color: 'var(--text)' }}>
                                                        {(event.type || 'event').replace(/_/g, ' ')}
                                                    </div>
                                                </div>
                                                <div style={{ fontSize: 10, color: 'var(--text-muted)', wordBreak: 'break-all' }}>{detail || 'No extra details'}</div>
                                                <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginTop: 6, gap: 8 }}>
                                                    <div style={{ fontSize: 10, color: 'var(--text-faint)' }}>{formatTime(event.timestamp)}</div>
                                                    {canOpenFile && (
                                                        <button
                                                            onClick={() => onFileOpen(filePath, fileName)}
                                                            style={{
                                                                background: 'var(--accent-soft)',
                                                                border: '1px solid rgba(74, 158, 255, 0.2)',
                                                                color: 'var(--accent)',
                                                                borderRadius: 6,
                                                                padding: '4px 8px',
                                                                fontSize: 10,
                                                                cursor: 'pointer',
                                                                fontFamily: 'inherit',
                                                            }}
                                                        >
                                                            Open file
                                                        </button>
                                                    )}
                                                </div>
                                            </div>
                                        )
                                    })}
                                </div>
                            )}
                        </div>
                    </>
                )}
            </div>
        </div>
    )
}
