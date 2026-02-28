import { useState, useEffect, useCallback } from 'react'
import { GitBranch, RefreshCw, Plus, Minus, Check, FileText, ChevronDown, ChevronRight } from 'lucide-react'

interface GitChange {
    status: string
    file: string
    staged: boolean
}

interface GitCommit {
    hash: string
    author: string
    message: string
    date: string
}

interface Props {
    projectRoot: string
    onFileClick?: (file: string) => void
}

export function GitPanel({ projectRoot, onFileClick }: Props) {
    const [branch, setBranch] = useState('loading...')
    const [changes, setChanges] = useState<GitChange[]>([])
    const [commits, setCommits] = useState<GitCommit[]>([])
    const [commitMsg, setCommitMsg] = useState('')
    const [loading, setLoading] = useState(false)
    const [committing, setCommitting] = useState(false)
    const [error, setError] = useState('')
    const [showChanges, setShowChanges] = useState(true)
    const [showCommits, setShowCommits] = useState(true)
    const [toast, setToast] = useState('')

    const showToast = (msg: string) => {
        setToast(msg)
        setTimeout(() => setToast(''), 3000)
    }

    const refresh = useCallback(async () => {
        if (!projectRoot) return
        setLoading(true)
        setError('')
        try {
            const gitApi = (window as any).gitApi
            if (!gitApi) { setError('Git API not available'); setLoading(false); return }

            const status = await gitApi.status(projectRoot)
            if (status?.error) {
                setError('Not a git repository')
                setBranch('—')
                setChanges([])
            } else {
                setBranch(status.branch || 'unknown')
                setChanges(status.changes || [])
            }

            const log = await gitApi.log(projectRoot, 15)
            setCommits(Array.isArray(log) ? log : [])
        } catch (err: any) {
            setError(err.message || 'Git error')
        }
        setLoading(false)
    }, [projectRoot])

    useEffect(() => { refresh() }, [refresh])

    const handleStage = async (file: string) => {
        const gitApi = (window as any).gitApi
        if (!gitApi) return
        await gitApi.stage(projectRoot, file)
        showToast(`Staged: ${file}`)
        refresh()
    }

    const handleUnstage = async (file: string) => {
        const gitApi = (window as any).gitApi
        if (!gitApi) return
        await gitApi.unstage(projectRoot, file)
        showToast(`Unstaged: ${file}`)
        refresh()
    }

    const handleStageAll = async () => {
        const gitApi = (window as any).gitApi
        if (!gitApi) return
        await gitApi.stageAll(projectRoot)
        showToast('All changes staged')
        refresh()
    }

    const handleCommit = async () => {
        if (!commitMsg.trim()) return
        setCommitting(true)
        try {
            const gitApi = (window as any).gitApi
            if (!gitApi) return
            const result = await gitApi.commit(projectRoot, commitMsg.trim())
            if (result.success) {
                setCommitMsg('')
                showToast('Committed successfully ✓')
                refresh()
            } else {
                showToast('Commit failed: ' + (result.error || ''))
            }
        } catch (err: any) {
            showToast('Error: ' + err.message)
        }
        setCommitting(false)
    }

    const getStatusColor = (status: string) => {
        if (status.includes('M')) return 'var(--yellow, #FBBF24)'
        if (status.includes('A') || status.includes('?')) return 'var(--green, #4ADE80)'
        if (status.includes('D')) return 'var(--red, #F87171)'
        if (status.includes('R')) return 'var(--purple, #C97FDB)'
        if (status.includes('U')) return 'var(--orange, #F97316)'
        return 'var(--text-muted)'
    }

    const getStatusLabel = (status: string) => {
        if (status.includes('M')) return 'M'
        if (status.includes('A')) return 'A'
        if (status.includes('D')) return 'D'
        if (status.includes('R')) return 'R'
        if (status.includes('?')) return 'U'
        return status.trim()
    }

    const stagedChanges = changes.filter(c => c.staged)
    const unstagedChanges = changes.filter(c => !c.staged)

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>Source Control</span>
                <div className="sidebar-header-actions">
                    <button className="sidebar-action-btn" onClick={refresh} title="Refresh">
                        <RefreshCw size={13} className={loading ? 'spin' : ''} />
                    </button>
                </div>
            </div>

            {/* Branch */}
            <div style={{
                padding: '4px 12px 8px', display: 'flex', alignItems: 'center', gap: 6,
                fontSize: 12, color: 'var(--text-muted)', flexShrink: 0
            }}>
                <GitBranch size={13} style={{ color: 'var(--accent)' }} />
                <span style={{ fontWeight: 600, color: 'var(--text)' }}>{branch}</span>
                {changes.length > 0 && (
                    <span style={{
                        fontSize: 10, padding: '1px 6px', borderRadius: 8,
                        background: 'var(--accent-soft)', color: 'var(--accent)', fontWeight: 600
                    }}>{changes.length}</span>
                )}
            </div>

            {/* Toast */}
            {toast && (
                <div style={{
                    margin: '0 8px 8px', padding: '6px 10px', borderRadius: 6,
                    background: 'rgba(74,222,128,0.1)', border: '1px solid rgba(74,222,128,0.2)',
                    color: '#4ADE80', fontSize: 11, fontWeight: 600
                }}>{toast}</div>
            )}

            {error && (
                <div style={{
                    margin: '0 8px 8px', padding: '6px 10px', borderRadius: 6,
                    background: 'rgba(248,113,113,0.1)', border: '1px solid rgba(248,113,113,0.2)',
                    color: '#F87171', fontSize: 11
                }}>{error}</div>
            )}

            {/* Commit message */}
            <div style={{ padding: '0 12px 8px', flexShrink: 0 }}>
                <div style={{
                    display: 'flex', gap: 4, alignItems: 'stretch',
                }}>
                    <input
                        type="text"
                        placeholder="Commit message..."
                        value={commitMsg}
                        onChange={e => setCommitMsg(e.target.value)}
                        onKeyDown={e => { if (e.key === 'Enter') handleCommit() }}
                        style={{
                            flex: 1, background: 'var(--bg-surface)', border: '1px solid var(--border)',
                            borderRadius: 6, padding: '6px 10px', color: 'var(--text)', fontSize: 12,
                            outline: 'none', fontFamily: 'inherit'
                        }}
                    />
                    <button
                        onClick={handleCommit}
                        disabled={!commitMsg.trim() || committing}
                        style={{
                            padding: '6px 12px', borderRadius: 6, border: 'none', fontSize: 11, fontWeight: 700,
                            background: commitMsg.trim() ? 'var(--accent)' : 'var(--bg-hover)',
                            color: commitMsg.trim() ? '#fff' : 'var(--text-faint)',
                            cursor: commitMsg.trim() ? 'pointer' : 'default', fontFamily: 'inherit',
                            transition: 'all 0.15s', flexShrink: 0
                        }}
                    >
                        {committing ? '...' : <Check size={14} />}
                    </button>
                </div>
                <button onClick={handleStageAll} style={{
                    width: '100%', marginTop: 4, padding: '5px', borderRadius: 5,
                    border: '1px solid var(--border)', background: 'transparent',
                    color: 'var(--text-muted)', fontSize: 11, cursor: 'pointer', fontFamily: 'inherit',
                    transition: 'all 0.12s'
                }}
                    onMouseEnter={e => { e.currentTarget.style.background = 'var(--bg-hover)'; e.currentTarget.style.color = 'var(--text)' }}
                    onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--text-muted)' }}
                >
                    <Plus size={11} style={{ marginRight: 4, verticalAlign: 'middle' }} /> Stage All Changes
                </button>
            </div>

            {/* Scrollable area */}
            <div style={{ flex: 1, overflowY: 'auto', overflowX: 'hidden' }}>
                {/* Staged changes */}
                {stagedChanges.length > 0 && (
                    <div>
                        <div onClick={() => setShowChanges(!showChanges)} style={{
                            display: 'flex', alignItems: 'center', gap: 4, padding: '6px 12px',
                            fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)',
                            textTransform: 'uppercase', cursor: 'pointer', letterSpacing: '0.5px'
                        }}>
                            {showChanges ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                            Staged ({stagedChanges.length})
                        </div>
                        {showChanges && stagedChanges.map(change => (
                            <div key={change.file + 'staged'} style={{
                                display: 'flex', alignItems: 'center', gap: 6, padding: '3px 12px 3px 24px',
                                fontSize: 12, cursor: 'pointer', transition: 'background 0.1s'
                            }}
                                onClick={() => onFileClick && onFileClick(change.file)}
                                onMouseEnter={e => (e.currentTarget.style.background = 'var(--bg-hover)')}
                                onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                            >
                                <span style={{
                                    fontSize: 10, fontWeight: 700, minWidth: 14, textAlign: 'center',
                                    color: getStatusColor(change.status)
                                }}>{getStatusLabel(change.status)}</span>
                                <FileText size={12} style={{ color: 'var(--text-faint)', flexShrink: 0 }} />
                                <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', color: 'var(--text-secondary)' }}>
                                    {change.file}
                                </span>
                                <button onClick={(e) => { e.stopPropagation(); handleUnstage(change.file) }}
                                    title="Unstage" style={{
                                        border: 'none', background: 'transparent', cursor: 'pointer',
                                        color: 'var(--text-faint)', padding: 2, display: 'flex', borderRadius: 3
                                    }}
                                    onMouseEnter={e => (e.currentTarget.style.color = 'var(--text)')}
                                    onMouseLeave={e => (e.currentTarget.style.color = 'var(--text-faint)')}
                                >
                                    <Minus size={13} />
                                </button>
                            </div>
                        ))}
                    </div>
                )}

                {/* Unstaged changes */}
                {unstagedChanges.length > 0 && (
                    <div>
                        <div style={{
                            display: 'flex', alignItems: 'center', gap: 4, padding: '6px 12px',
                            fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)',
                            textTransform: 'uppercase', letterSpacing: '0.5px'
                        }}>
                            Changes ({unstagedChanges.length})
                        </div>
                        {unstagedChanges.map(change => (
                            <div key={change.file + 'unstaged'} style={{
                                display: 'flex', alignItems: 'center', gap: 6, padding: '3px 12px 3px 24px',
                                fontSize: 12, cursor: 'pointer', transition: 'background 0.1s'
                            }}
                                onClick={() => onFileClick && onFileClick(change.file)}
                                onMouseEnter={e => (e.currentTarget.style.background = 'var(--bg-hover)')}
                                onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                            >
                                <span style={{
                                    fontSize: 10, fontWeight: 700, minWidth: 14, textAlign: 'center',
                                    color: getStatusColor(change.status)
                                }}>{getStatusLabel(change.status)}</span>
                                <FileText size={12} style={{ color: 'var(--text-faint)', flexShrink: 0 }} />
                                <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', color: 'var(--text-secondary)' }}>
                                    {change.file}
                                </span>
                                <button onClick={(e) => { e.stopPropagation(); handleStage(change.file) }}
                                    title="Stage" style={{
                                        border: 'none', background: 'transparent', cursor: 'pointer',
                                        color: 'var(--text-faint)', padding: 2, display: 'flex', borderRadius: 3
                                    }}
                                    onMouseEnter={e => (e.currentTarget.style.color = 'var(--text)')}
                                    onMouseLeave={e => (e.currentTarget.style.color = 'var(--text-faint)')}
                                >
                                    <Plus size={13} />
                                </button>
                            </div>
                        ))}
                    </div>
                )}

                {changes.length === 0 && !error && !loading && (
                    <div style={{ padding: 16, textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
                        <Check size={16} style={{ color: 'var(--green)', marginBottom: 4 }} /><br />
                        No pending changes
                    </div>
                )}

                {/* Recent commits */}
                {commits.length > 0 && (
                    <div style={{ marginTop: 8 }}>
                        <div onClick={() => setShowCommits(!showCommits)} style={{
                            display: 'flex', alignItems: 'center', gap: 4, padding: '6px 12px',
                            fontSize: 11, fontWeight: 700, color: 'var(--text-secondary)',
                            textTransform: 'uppercase', cursor: 'pointer', letterSpacing: '0.5px'
                        }}>
                            {showCommits ? <ChevronDown size={12} /> : <ChevronRight size={12} />}
                            Recent Commits ({commits.length})
                        </div>
                        {showCommits && commits.map((commit, i) => (
                            <div key={i} style={{
                                padding: '5px 12px 5px 24px', fontSize: 11,
                                borderBottom: '1px solid var(--border-subtle)',
                                cursor: 'pointer', transition: 'background 0.1s'
                            }}
                                onMouseEnter={e => (e.currentTarget.style.background = 'var(--bg-hover)')}
                                onMouseLeave={e => (e.currentTarget.style.background = 'transparent')}
                            >
                                <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
                                    <span style={{
                                        fontFamily: "'JetBrains Mono', monospace", fontSize: 10,
                                        color: 'var(--accent)', fontWeight: 600
                                    }}>{commit.hash}</span>
                                    <span style={{ flex: 1, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap', color: 'var(--text-secondary)' }}>
                                        {commit.message}
                                    </span>
                                </div>
                                <div style={{ fontSize: 10, color: 'var(--text-faint)', marginTop: 2 }}>
                                    {commit.author} · {commit.date}
                                </div>
                            </div>
                        ))}
                    </div>
                )}
            </div>
        </div>
    )
}
