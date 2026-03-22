import { useState, useEffect, useCallback } from 'react'
import {
    ChevronRight, File, Folder, FolderOpen, RefreshCw,
    FilePlus, FolderPlus, Trash2, Edit3
} from 'lucide-react'
import { createDir, createFile, deletePath, readDir, renamePath, selectDirectory } from '../lib/desktopBridge'

interface FileNode {
    name: string
    path: string
    type: 'file' | 'directory'
    children?: FileNode[]
    size?: number
    extension?: string
}

interface Props {
    projectRoot: string
    onFileOpen: (filePath: string, fileName: string) => void
}

// File icon color mapping
function getIconColor(ext: string): string {
    const map: Record<string, string> = {
        '.ts': '#3178C6', '.tsx': '#3178C6', '.js': '#F7DF1E', '.jsx': '#61DAFB',
        '.css': '#264DE4', '.html': '#E44D26', '.json': '#F7DF1E', '.py': '#3776AB',
        '.md': '#083FA1', '.txt': '#888', '.bat': '#C1F12E', '.sh': '#4EAA25',
        '.cpp': '#659AD2', '.h': '#A074C4', '.c': '#659AD2', '.rs': '#CE412B',
        '.go': '#00ADD8', '.java': '#ED8B00', '.yaml': '#CB171E', '.yml': '#CB171E',
        '.toml': '#9C4121', '.xml': '#E44D26', '.svg': '#FFB13B',
    }
    return map[ext] || 'var(--text-muted)'
}

export function FileExplorer({ projectRoot, onFileOpen }: Props) {
    const [tree, setTree] = useState<FileNode[]>([])
    const [expanded, setExpanded] = useState<Set<string>>(new Set())
    const [selected, setSelected] = useState<string>('')
    const [loading, setLoading] = useState(false)
    const [contextMenu, setContextMenu] = useState<{ x: number; y: number; node?: FileNode } | null>(null)
    const [promptState, setPromptState] = useState<{ type: 'file' | 'folder', targetDir: string } | null>(null)
    const [promptValue, setPromptValue] = useState('')

    const pickDirectory = useCallback(async () => selectDirectory(), [])

    const loadTree = useCallback(async () => {
        if (!projectRoot) return
        console.log(`[FileExplorer] Loading tree for: ${projectRoot}`);
        setLoading(true)
        try {
            const result = await readDir(projectRoot)
            console.log(`[FileExplorer] Tree loaded. Entries: ${result?.length || 0}`);
            setTree(result || [])
        } catch (e) {
            console.error('[FileExplorer] Failed to load file tree:', e)
        }
        setLoading(false)
    }, [projectRoot])

    useEffect(() => {
        loadTree()
    }, [loadTree])

    // File watcher - auto-refresh tree on filesystem changes
    useEffect(() => {
        if (!projectRoot || !(window as any).fileWatcher?.watch) return

        let refreshTimer: ReturnType<typeof setTimeout> | null = null;

        (window as any).fileWatcher.watch(projectRoot)
        const cleanup = (window as any).fileWatcher.onChange((_event: string, _filename: string) => {
            // Debounce refresh to avoid too many reloads
            if (refreshTimer) clearTimeout(refreshTimer)
            refreshTimer = setTimeout(() => {
                loadTree()
            }, 500)
        })

        return () => {
            if (refreshTimer) clearTimeout(refreshTimer)
            cleanup?.()
                ; (window as any).fileWatcher?.unwatch?.()
        }
    }, [projectRoot, loadTree])

    const toggleExpand = (path: string) => {
        setExpanded(prev => {
            const next = new Set(prev)
            if (next.has(path)) next.delete(path)
            else next.add(path)
            return next
        })
    }

    const handleClick = (node: FileNode) => {
        setSelected(node.path)
        if (node.type === 'directory') {
            toggleExpand(node.path)
        } else {
            onFileOpen(node.path, node.name)
        }
    }

    const handleContextMenu = (e: React.MouseEvent, node: FileNode) => {
        e.preventDefault()
        setContextMenu({ x: e.clientX, y: e.clientY, node })
    }

    useEffect(() => {
        if (contextMenu) {
            const close = () => setContextMenu(null)
            window.addEventListener('click', close)
            return () => window.removeEventListener('click', close)
        }
    }, [contextMenu])

    const handleDelete = async (path: string) => {
        await deletePath(path)
        loadTree()
        setContextMenu(null)
    }

    const processNewItem = async (type: 'file' | 'folder', name: string, targetDir: string) => {
        if (!name || !name.trim()) return
        const trimmedName = name.trim()
        const sep = targetDir.includes('\\') ? '\\' : '/'
        const cleanDir = targetDir.endsWith(sep) ? targetDir.slice(0, -1) : targetDir
        const targetPath = `${cleanDir}${sep}${trimmedName}`
        
        console.log('[FileExplorer] Creating:', { type, targetPath })
        
        try {
            let res: any
            if (type === 'file') {
                res = await createFile(targetPath)
            } else {
                res = await createDir(targetPath)
            }
            console.log('[FileExplorer] Result:', res)
            if (res?.error) {
                alert(`Error: ${res.error}`)
            }
        } catch (err: any) {
            alert(`Failed: ${err?.message || String(err)}`)
        }
        loadTree()
    }





    const handleNewFile = async () => {
        // If no workspace is open, ask user to select one first (VS Code style)
        let targetDir = projectRoot
        if (!targetDir || targetDir.trim() === '') {
            if (!window.ipcRenderer?.selectDirectory && !window.ipcRenderer?.invoke) {
                alert('Workspace operations are only supported in the Electron desktop app.')
                setContextMenu(null)
                return
            }
            const picked = await pickDirectory()
            if (!picked) { setContextMenu(null); return }
            targetDir = picked
            window.dispatchEvent(new CustomEvent('set-project-root', { detail: { root: picked } }))
        } else {
            const targetNode = contextMenu?.node || (selected ? { path: selected, type: tree.find(n => n.path === selected)?.type || 'directory' } : null)
            if (targetNode) {
                const lastSlash = Math.max(targetNode.path.lastIndexOf('\\'), targetNode.path.lastIndexOf('/'))
                targetDir = targetNode.type === 'file' && lastSlash !== -1
                    ? targetNode.path.substring(0, lastSlash)
                    : targetNode.path
            }
        }

        setPromptState({ type: 'file', targetDir })
        setPromptValue('')
        setContextMenu(null)
    }

    const handleNewFolder = async () => {
        // If no workspace is open, ask user to select one first (VS Code style)
        let targetDir = projectRoot
        if (!targetDir || targetDir.trim() === '') {
            if (!window.ipcRenderer?.selectDirectory && !window.ipcRenderer?.invoke) {
                alert('Workspace operations are only supported in the Electron desktop app.')
                setContextMenu(null)
                return
            }
            const picked = await pickDirectory()
            if (!picked) { setContextMenu(null); return }
            targetDir = picked
            window.dispatchEvent(new CustomEvent('set-project-root', { detail: { root: picked } }))
        } else {
            const targetNode = contextMenu?.node || (selected ? { path: selected, type: tree.find(n => n.path === selected)?.type || 'directory' } : null)
            if (targetNode) {
                const lastSlash = Math.max(targetNode.path.lastIndexOf('\\'), targetNode.path.lastIndexOf('/'))
                targetDir = targetNode.type === 'file' && lastSlash !== -1
                    ? targetNode.path.substring(0, lastSlash)
                    : targetNode.path
            }
        }

        setPromptState({ type: 'folder', targetDir })
        setPromptValue('')
        setContextMenu(null)
    }

    // Listen for global "New File" event from the top menu / keyboard shortcut
    useEffect(() => {
        const handler = () => {
            handleNewFile()
        }
        window.addEventListener('explorer-new-file', handler)
        return () => window.removeEventListener('explorer-new-file', handler)
    }, [projectRoot, handleNewFile])

    const handleReveal = () => {
        const targetPath = contextMenu?.node?.path || selected || projectRoot
        if (targetPath && window.shell?.openPath) {
            window.shell.openPath(targetPath)
        }
        setContextMenu(null)
    }

    const handleOpenTerminal = () => {
        const targetPath = contextMenu?.node?.path || selected || projectRoot
        let dir = targetPath
        if (contextMenu?.node?.type === 'file' || (!contextMenu?.node && tree.find(n => n.path === selected)?.type === 'file')) {
            dir = targetPath.substring(0, targetPath.lastIndexOf('\\'))
        }
        // In this app architecture, terminals are global. The user might want us to launch a terminal or just open the terminal panel.
        // We'll emit an event to open the panel with this cwd, but let's just use window.appApi or trigger a custom event.
        const event = new CustomEvent('open-terminal-cwd', { detail: dir })
        window.dispatchEvent(event)
        setContextMenu(null)
    }

    const handleCopyPath = () => {
        if (contextMenu?.node?.path) {
            navigator.clipboard.writeText(contextMenu.node.path)
        }
        setContextMenu(null)
    }

    const handleCopyRelativePath = () => {
        if (contextMenu?.node?.path && projectRoot) {
            let relative = contextMenu.node.path.replace(projectRoot, '')
            if (relative.startsWith('\\') || relative.startsWith('/')) {
                relative = relative.substring(1)
            }
            navigator.clipboard.writeText(relative)
        }
        setContextMenu(null)
    }

    const renderNode = (node: FileNode, depth: number = 0) => {
        const isExpanded = expanded.has(node.path)
        const isSelected = selected === node.path

        return (
            <div key={node.path}>
                <div
                    className={`tree-item ${isSelected ? 'selected' : ''}`}
                    style={{ '--tree-depth': depth } as React.CSSProperties}
                    onClick={() => handleClick(node)}
                    onContextMenu={(e) => handleContextMenu(e, node)}
                >
                    {node.type === 'directory' ? (
                        <>
                            <span className="tree-icon">
                                <ChevronRight size={14} className={`tree-chevron ${isExpanded ? 'open' : ''}`} />
                            </span>
                            <span className="tree-icon" style={{ color: '#E8A838' }}>
                                {isExpanded ? <FolderOpen size={15} /> : <Folder size={15} />}
                            </span>
                        </>
                    ) : (
                        <>
                            <span className="tree-icon" style={{ width: 14 }} />
                            <span className="tree-icon" style={{ color: getIconColor(node.extension || '') }}>
                                <File size={14} />
                            </span>
                        </>
                    )}
                    <span className="tree-label">{node.name}</span>
                </div>
                {node.type === 'directory' && isExpanded && node.children?.map(child => renderNode(child, depth + 1))}
            </div>
        )
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>Explorer</span>
                <div className="sidebar-header-actions">
                    <button className="sidebar-action-btn" onClick={handleNewFile} title="New File">
                        <FilePlus size={14} />
                    </button>
                    <button className="sidebar-action-btn" onClick={handleNewFolder} title="New Folder">
                        <FolderPlus size={14} />
                    </button>
                    <button className="sidebar-action-btn" onClick={loadTree} title="Refresh">
                        <RefreshCw size={14} className={loading ? 'spin' : ''} />
                    </button>
                </div>
            </div>
            <div className="sidebar-content" onContextMenu={(e) => {
                // Only trigger if clicking exactly on this container or file-tree background, not on a node
                if ((e.target as HTMLElement).closest('.tree-item')) return;
                e.preventDefault();
                setContextMenu({ x: e.clientX, y: e.clientY, node: undefined });
            }}>
                <div className="explorer-tree" style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
                    {loading ? (
                        <div style={{ padding: '16px', textAlign: 'center', color: 'var(--text-faint)', fontSize: '12px' }}>
                            Loading...
                        </div>
                    ) : projectRoot && tree.length > 0 ? (
                        tree.map(node => renderNode(node))
                    ) : (
                        <div style={{
                            padding: '30px 20px',
                            textAlign: 'center',
                            display: 'flex',
                            flexDirection: 'column',
                            gap: '16px',
                            alignItems: 'center',
                            color: 'var(--text-secondary)'
                        }}>
                            {!projectRoot ? (
                                <>
                                    <p style={{ fontSize: '13px', margin: 0, opacity: 0.8, lineHeight: 1.5 }}>
                                        You have not yet opened a folder.
                                    </p>
                                    <button 
                                        onClick={() => {
                                            console.log('[FileExplorer] Open Folder button clicked');
                                            window.dispatchEvent(new CustomEvent('open-project-folder'));
                                        }}
                                        style={{
                                            background: 'var(--accent)',
                                            color: 'white',
                                            border: 'none',
                                            padding: '10px 20px',
                                            borderRadius: '6px',
                                            cursor: 'pointer',
                                            fontSize: '13px',
                                            fontWeight: 600,
                                            width: '100%',
                                            maxWidth: '160px',
                                            transition: 'all 0.2s ease',
                                            boxShadow: '0 4px 12px rgba(var(--accent-rgb), 0.3)'
                                        }}
                                        onMouseOver={(e) => e.currentTarget.style.filter = 'brightness(1.1)'}
                                        onMouseOut={(e) => e.currentTarget.style.filter = 'none'}
                                    >
                                        Open Folder
                                    </button>
                                </>
                            ) : (
                                <p style={{ fontSize: '13px', opacity: 0.5 }}>This folder is empty.</p>
                            )}
                            {!projectRoot && (
                                <div style={{ fontSize: '11px', opacity: 0.5, marginTop: '10px', display: 'flex', flexDirection: 'column', gap: '4px' }}>
                                    <span>Or create a new file via File menu</span>
                                    <span style={{ fontSize: '10px' }}>(Shortcut: Ctrl+Shift+O to open folder)</span>
                                </div>
                            )}
                        </div>
                    )}
                </div>
            </div>

            {/* Context Menu */}
            {contextMenu && (
                <div className="context-menu" style={{ left: contextMenu.x, top: contextMenu.y }}>
                    <button className="context-menu-item" onClick={handleNewFile}>
                        <FilePlus size={13} /> New File
                    </button>
                    <button className="context-menu-item" onClick={handleNewFolder}>
                        <FolderPlus size={13} /> New Folder
                    </button>
                    {contextMenu.node && (
                        <>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={handleReveal}>
                                Reveal in File Explorer
                            </button>
                            <button className="context-menu-item" onClick={handleOpenTerminal}>
                                Open in Integrated Terminal
                            </button>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={handleCopyPath}>
                                Copy Path
                            </button>
                            <button className="context-menu-item" onClick={handleCopyRelativePath}>
                                Copy Relative Path
                            </button>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={() => {
                                setPromptState({ type: 'rename', targetDir: contextMenu.node!.path } as any)
                                setPromptValue(contextMenu.node!.name)
                                setContextMenu(null)
                            }}>
                                <Edit3 size={13} /> Rename
                            </button>
                            <button className="context-menu-item danger" onClick={() => handleDelete(contextMenu.node!.path)}>
                                <Trash2 size={13} /> Delete
                            </button>
                        </>
                    )}
                </div>
            )}

            {/* Custom Prompt Dialog */}
            {promptState && (
                <div style={{
                    position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
                    background: 'rgba(0,0,0,0.6)', zIndex: 1000,
                    display: 'flex', alignItems: 'center', justifyContent: 'center'
                }}>
                    <div style={{
                        background: 'var(--bg-card)', padding: '20px', borderRadius: '10px',
                        border: '1px solid var(--border)', width: '420px', maxWidth: '90%',
                        display: 'flex', flexDirection: 'column', gap: '12px',
                        boxShadow: '0 20px 60px rgba(0,0,0,0.5)'
                    }}>
                        {/* Title */}
                        <div style={{ fontSize: 14, fontWeight: 700, color: 'var(--text)' }}>
                            {(promptState.type as any) === 'rename' ? '✏️ Rename' : promptState.type === 'file' ? '📄 New File' : '📁 New Folder'}
                        </div>

                        {/* Location row (only for new file/folder, not rename) */}
                        {(promptState.type as any) !== 'rename' && (
                            <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                                <label style={{ fontSize: 11, color: 'var(--text-faint)', fontWeight: 600, letterSpacing: '0.05em' }}>LOCATION</label>
                                <div style={{ display: 'flex', gap: '6px', alignItems: 'center' }}>
                                    <div style={{
                                        flex: 1, background: 'var(--bg-inset)', border: '1px solid var(--border)',
                                        borderRadius: '4px', padding: '5px 8px', fontSize: 11,
                                        color: 'var(--text-secondary)', overflow: 'hidden', textOverflow: 'ellipsis',
                                        whiteSpace: 'nowrap', fontFamily: 'monospace'
                                    }}>
                                        {promptState.targetDir || '(no folder opened)'}
                                    </div>
                                    <button
                                        onClick={async () => {
                                            const dir = await pickDirectory()
                                            if (dir) {
                                                setPromptState(prev => prev ? { ...prev, targetDir: dir } : prev)
                                            }
                                        }}
                                        style={{
                                            background: 'var(--bg-inset)', border: '1px solid var(--border)',
                                            color: 'var(--text)', padding: '5px 10px', borderRadius: '4px',
                                            cursor: 'pointer', fontSize: 11, whiteSpace: 'nowrap'
                                        }}
                                    >Browse...</button>
                                </div>
                            </div>
                        )}

                        {/* Name input */}
                        <div style={{ display: 'flex', flexDirection: 'column', gap: '4px' }}>
                            <label style={{ fontSize: 11, color: 'var(--text-faint)', fontWeight: 600, letterSpacing: '0.05em' }}>
                                {(promptState.type as any) === 'rename' ? 'NEW NAME' : 'NAME'}
                            </label>
                            <input
                                autoFocus
                                value={promptValue}
                                onChange={(e) => setPromptValue(e.target.value)}
                                placeholder={(promptState.type === 'file' ? 'filename.ext' : promptState.type === 'folder' ? 'folder-name' : '')}
                                style={{
                                    background: 'var(--bg-inset)', border: '1px solid var(--accent)',
                                    color: 'var(--text)', padding: '7px 10px', borderRadius: '4px',
                                    outline: 'none', fontSize: 13, fontFamily: 'monospace'
                                }}
                                onKeyDown={(e) => {
                                    if (e.key === 'Enter') {
                                        if ((promptState.type as any) === 'rename') {
                                            if (promptValue) {
                                                const lastSlash = Math.max(promptState.targetDir.lastIndexOf('\\'), promptState.targetDir.lastIndexOf('/'))
                                                const dir = lastSlash !== -1 ? promptState.targetDir.substring(0, lastSlash) : promptState.targetDir
                                                const sep = dir.includes('\\') ? '\\' : '/'
                                                renamePath(promptState.targetDir, `${dir}${sep}${promptValue}`).then((res: any) => {
                                                    if (res?.error) alert(`Failed to rename: ${res.error}`)
                                                    loadTree()
                                                })
                                            }
                                        } else {
                                            processNewItem(promptState.type, promptValue, promptState.targetDir)
                                        }
                                        setPromptState(null)
                                    } else if (e.key === 'Escape') {
                                        setPromptState(null)
                                    }
                                }}
                            />
                        </div>

                        {/* Buttons */}
                        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end', marginTop: '4px' }}>
                            <button
                                onClick={() => setPromptState(null)}
                                style={{
                                    background: 'transparent', border: '1px solid var(--border)', color: 'var(--text-secondary)',
                                    padding: '6px 16px', borderRadius: '5px', cursor: 'pointer', fontSize: 12
                                }}
                            >Cancel</button>
                            <button
                                onClick={() => {
                                    if ((promptState.type as any) === 'rename') {
                                        if (promptValue) {
                                            const lastSlash = Math.max(promptState.targetDir.lastIndexOf('\\'), promptState.targetDir.lastIndexOf('/'))
                                            const dir = lastSlash !== -1 ? promptState.targetDir.substring(0, lastSlash) : promptState.targetDir
                                            const sep = dir.includes('\\') ? '\\' : '/'
                                            renamePath(promptState.targetDir, `${dir}${sep}${promptValue}`).then((res: any) => {
                                                if (res?.error) alert(`Failed to rename: ${res.error}`)
                                                loadTree()
                                            })
                                        }
                                    } else {
                                        processNewItem(promptState.type, promptValue, promptState.targetDir)
                                    }
                                    setPromptState(null)
                                }}
                                style={{
                                    background: 'var(--accent)', border: 'none', color: 'white',
                                    padding: '6px 16px', borderRadius: '5px', cursor: 'pointer', fontSize: 12, fontWeight: 600
                                }}
                            >
                                {(promptState.type as any) === 'rename' ? 'Rename' : 'Create'}
                            </button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    )
}
