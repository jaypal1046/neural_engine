import { useState, useEffect, useCallback } from 'react'
import {
    ChevronRight, File, Folder, FolderOpen, RefreshCw,
    FilePlus, FolderPlus, Trash2, Edit3
} from 'lucide-react'

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

    const loadTree = useCallback(async () => {
        if (!window.fs?.readDir || !projectRoot) return
        setLoading(true)
        try {
            const result = await window.fs.readDir(projectRoot)
            setTree(result || [])
        } catch (e) {
            console.error('Failed to load file tree:', e)
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
        if (window.fs?.delete) {
            await window.fs.delete(path)
            loadTree()
        }
        setContextMenu(null)
    }

    const handleNewFile = async () => {
        const name = prompt('File name:')
        if (!name) return
        const dir = selected || projectRoot
        const targetDir = selected && tree.find(n => n.path === selected && n.type === 'file')
            ? selected.substring(0, selected.lastIndexOf('\\'))
            : dir
        if (window.fs?.createFile) {
            await window.fs.createFile(`${targetDir}\\${name}`)
            loadTree()
        }
    }

    const handleNewFolder = async () => {
        const name = prompt('Folder name:')
        if (!name) return
        const dir = selected || projectRoot
        const targetDir = selected && tree.find(n => n.path === selected && n.type === 'file')
            ? selected.substring(0, selected.lastIndexOf('\\'))
            : dir
        if (window.fs?.createDir) {
            await window.fs.createDir(`${targetDir}\\${name}`)
            loadTree()
        }
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
            <div className="sidebar-content">
                <div className="file-tree">
                    {tree.length === 0 && !loading && (
                        <div style={{ padding: '16px', textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
                            No folder opened
                        </div>
                    )}
                    {tree.map(node => renderNode(node))}
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
                            <button className="context-menu-item" onClick={() => {
                                const newName = prompt('New name:', contextMenu.node!.name)
                                if (newName && window.fs?.rename) {
                                    const dir = contextMenu.node!.path.substring(0, contextMenu.node!.path.lastIndexOf('\\'))
                                    window.fs.rename(contextMenu.node!.path, `${dir}\\${newName}`).then(() => loadTree())
                                }
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
        </div>
    )
}
