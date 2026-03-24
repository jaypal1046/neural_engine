import { useState, useEffect, useCallback, useRef } from 'react'
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
    const [promptState, setPromptState] = useState<{ type: 'file' | 'folder' | 'rename', targetDir: string } | null>(null)
    const [promptValue, setPromptValue] = useState('')
    const lastLoadedRootRef = useRef<string | null>(null)
    const isFetchingRef = useRef(false)

    const pickDirectory = useCallback(async () => selectDirectory(), [])

    const loadTree = useCallback(async () => {
        if (!projectRoot || isFetchingRef.current) return
        
        // Skip if already loaded for this exact root path (and we have data)
        if (lastLoadedRootRef.current === projectRoot && tree.length > 0) return

        console.log(`[FileExplorer] Loading tree for: ${projectRoot}`);
        isFetchingRef.current = true
        setLoading(true)
        try {
            const result = await readDir(projectRoot)
            console.log(`[FileExplorer] Tree loaded. Entries: ${result?.length || 0}`);
            setTree(result || [])
            lastLoadedRootRef.current = projectRoot
        } catch (e) {
            console.error('[FileExplorer] Failed to load file tree:', e)
        } finally {
            setLoading(false)
            isFetchingRef.current = false
        }
    }, [projectRoot, tree.length])

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
                // Clear cache on external change to force re-fetch
                lastLoadedRootRef.current = null 
                loadTree()
            }, 1000) // Increase debounce to 1s to be safe
        })

        return () => {
            if (refreshTimer) clearTimeout(refreshTimer)
            cleanup?.()
            try { (window as any).fileWatcher?.unwatch?.() } catch {}
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
        if (!confirm(`Are you sure you want to delete ${path}?`)) return
        await deletePath(path)
        lastLoadedRootRef.current = null
        loadTree()
        setContextMenu(null)
    }

    const processNewItem = async (type: 'file' | 'folder', name: string, targetDir: string) => {
        if (!name || !name.trim()) return
        const trimmedName = name.trim()
        const sep = targetDir.includes('\\') ? '\\' : '/'
        const cleanDir = targetDir.endsWith(sep) ? targetDir.slice(0, -1) : targetDir
        const targetPath = `${cleanDir}${sep}${trimmedName}`
        
        try {
            let res: any
            if (type === 'file') res = await createFile(targetPath)
            else res = await createDir(targetPath)
            
            if (res?.error) alert(`Error: ${res.error}`)
            else {
                lastLoadedRootRef.current = null
                loadTree()
            }
        } catch (err: any) {
            alert(`Failed: ${err?.message || String(err)}`)
        }
    }

    const handleNewFile = async () => {
        let targetDir = projectRoot
        if (!targetDir || targetDir.trim() === '') {
            const picked = await pickDirectory()
            if (!picked) return
            targetDir = picked
            window.dispatchEvent(new CustomEvent('set-project-root', { detail: { root: picked } }))
        } else {
            const targetNode = contextMenu?.node || (selected ? tree.find(n => n.path === selected) : null)
            if (targetNode) {
                targetDir = targetNode.type === 'file' 
                    ? targetNode.path.substring(0, Math.max(targetNode.path.lastIndexOf('\\'), targetNode.path.lastIndexOf('/')))
                    : targetNode.path
            }
        }
        setPromptState({ type: 'file', targetDir })
        setPromptValue('')
        setContextMenu(null)
    }

    const handleNewFolder = async () => {
        let targetDir = projectRoot
        if (!targetDir || targetDir.trim() === '') {
            const picked = await pickDirectory()
            if (!picked) return
            targetDir = picked
            window.dispatchEvent(new CustomEvent('set-project-root', { detail: { root: picked } }))
        } else {
            const targetNode = contextMenu?.node || (selected ? tree.find(n => n.path === selected) : null)
            if (targetNode) {
                targetDir = targetNode.type === 'file' 
                    ? targetNode.path.substring(0, Math.max(targetNode.path.lastIndexOf('\\'), targetNode.path.lastIndexOf('/')))
                    : targetNode.path
            }
        }
        setPromptState({ type: 'folder', targetDir })
        setPromptValue('')
        setContextMenu(null)
    }

    const handleReveal = () => {
        const targetPath = contextMenu?.node?.path || selected || projectRoot
        if (targetPath && window.shell?.openPath) window.shell.openPath(targetPath)
        setContextMenu(null)
    }

    const handleOpenTerminal = () => {
        const targetPath = contextMenu?.node?.path || selected || projectRoot
        let dir = targetPath
        if (contextMenu?.node?.type === 'file' || (!contextMenu?.node && tree.find(n => n.path === selected)?.type === 'file')) {
            dir = targetPath.substring(0, Math.max(targetPath.lastIndexOf('\\'), targetPath.lastIndexOf('/')))
        }
        window.dispatchEvent(new CustomEvent('open-terminal-cwd', { detail: dir }))
        setContextMenu(null)
    }

    const handleCopyPath = () => {
        if (contextMenu?.node?.path) navigator.clipboard.writeText(contextMenu.node.path)
        setContextMenu(null)
    }

    const handleCopyRelativePath = () => {
        if (contextMenu?.node?.path && projectRoot) {
            let relative = contextMenu.node.path.replace(projectRoot, '')
            if (relative.startsWith('\\') || relative.startsWith('/')) relative = relative.substring(1)
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
                    <button className="sidebar-action-btn" onClick={() => { lastLoadedRootRef.current = null; loadTree(); }} title="Refresh">
                        <RefreshCw size={14} className={loading ? 'spin' : ''} />
                    </button>
                </div>
            </div>
            <div className="sidebar-content" onContextMenu={(e) => {
                if ((e.target as HTMLElement).closest('.tree-item')) return;
                e.preventDefault();
                setContextMenu({ x: e.clientX, y: e.clientY, node: undefined });
            }}>
                <div className="explorer-tree" style={{ flex: 1, overflowY: 'auto', padding: '4px 0' }}>
                    {loading && tree.length === 0 ? (
                        <div style={{ padding: '16px', textAlign: 'center', color: 'var(--text-faint)', fontSize: '12px' }}>
                            Loading...
                        </div>
                    ) : projectRoot && tree.length > 0 ? (
                        tree.map(node => renderNode(node))
                    ) : (
                        <div style={{
                            padding: '30px 20px', textAlign: 'center', display: 'flex',
                            flexDirection: 'column', gap: '16px', alignItems: 'center', color: 'var(--text-secondary)'
                        }}>
                            {!projectRoot ? (
                                <>
                                    <p style={{ fontSize: '13px', margin: 0, opacity: 0.8, lineHeight: 1.5 }}>You have not yet opened a folder.</p>
                                    <button onClick={() => window.dispatchEvent(new CustomEvent('open-project-folder'))} className="primary-btn">
                                        Open Folder
                                    </button>
                                </>
                            ) : (
                                <p style={{ fontSize: '13px', opacity: 0.5 }}>This folder is empty.</p>
                            )}
                        </div>
                    )}
                </div>
            </div>

            {contextMenu && (
                <div className="context-menu" style={{ left: contextMenu.x, top: contextMenu.y }}>
                    <button className="context-menu-item" onClick={handleNewFile}><FilePlus size={13} /> New File</button>
                    <button className="context-menu-item" onClick={handleNewFolder}><FolderPlus size={13} /> New Folder</button>
                    {contextMenu.node && (
                        <>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={handleReveal}>Reveal in File Explorer</button>
                            <button className="context-menu-item" onClick={handleOpenTerminal}>Open in Integrated Terminal</button>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={handleCopyPath}>Copy Path</button>
                            <button className="context-menu-item" onClick={handleCopyRelativePath}>Copy Relative Path</button>
                            <div className="context-menu-divider" />
                            <button className="context-menu-item" onClick={() => {
                                setPromptState({ type: 'rename', targetDir: contextMenu.node!.path })
                                setPromptValue(contextMenu.node!.name)
                                setContextMenu(null)
                            }}><Edit3 size={13} /> Rename</button>
                            <button className="context-menu-item danger" onClick={() => handleDelete(contextMenu.node!.path)}><Trash2 size={13} /> Delete</button>
                        </>
                    )}
                </div>
            )}

            {promptState && (
                <div className="prompt-overlay" style={{
                    position: 'fixed', top: 0, left: 0, right: 0, bottom: 0,
                    background: 'rgba(0,0,0,0.6)', zIndex: 1000,
                    display: 'flex', alignItems: 'center', justifyContent: 'center'
                }}>
                    <div className="prompt-card" style={{
                        background: 'var(--bg-card)', padding: '20px', borderRadius: '10px',
                        border: '1px solid var(--border)', width: '420px', maxWidth: '90%',
                        display: 'flex', flexDirection: 'column', gap: '12px'
                    }}>
                        <div style={{ fontSize: 14, fontWeight: 700 }}>
                            {promptState.type === 'rename' ? '✏️ Rename' : promptState.type === 'file' ? '📄 New File' : '📁 New Folder'}
                        </div>
                        <input
                            autoFocus
                            value={promptValue}
                            onChange={(e) => setPromptValue(e.target.value)}
                            onKeyDown={(e) => {
                                if (e.key === 'Enter') {
                                    if (promptState.type === 'rename') {
                                        const lastSlash = Math.max(promptState.targetDir.lastIndexOf('\\'), promptState.targetDir.lastIndexOf('/'))
                                        const dir = lastSlash !== -1 ? promptState.targetDir.substring(0, lastSlash) : promptState.targetDir
                                        const sep = dir.includes('\\') ? '\\' : '/'
                                        renamePath(promptState.targetDir, `${dir}${sep}${promptValue}`).then(() => {
                                            lastLoadedRootRef.current = null
                                            loadTree()
                                        })
                                    } else {
                                        processNewItem(promptState.type as any, promptValue, promptState.targetDir)
                                    }
                                    setPromptState(null)
                                } else if (e.key === 'Escape') setPromptState(null)
                            }}
                            className="prompt-input"
                            style={{ width: '100%', padding: '8px', background: 'var(--bg-inset)', border: '1px solid var(--accent)', color: 'var(--text)', borderRadius: '4px' }}
                        />
                        <div style={{ display: 'flex', gap: '8px', justifyContent: 'flex-end' }}>
                            <button onClick={() => setPromptState(null)} className="secondary-btn">Cancel</button>
                            <button onClick={() => { /* same as enter key */ }} className="primary-btn">Confirm</button>
                        </div>
                    </div>
                </div>
            )}
        </div>
    )
}
