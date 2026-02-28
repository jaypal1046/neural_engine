import { useState } from 'react'
import { Wifi, WifiOff, Plus, Trash2, Activity } from 'lucide-react'

interface MCPServer {
    id: string
    name: string
    url: string
    status: 'connected' | 'disconnected' | 'error'
    type: 'local' | 'remote'
    capabilities: string[]
    lastPing?: string
}

export function MCPPanel({ serverStatus }: { serverStatus: string }) {
    const [servers, setServers] = useState<MCPServer[]>([
        {
            id: 'neural-api', name: 'Neural Engine API', url: 'http://127.0.0.1:8001',
            status: serverStatus === 'online' ? 'connected' : 'disconnected',
            type: 'local',
            capabilities: ['compress', 'decompress', 'analyze', 'chat', 'train'],
            lastPing: '< 1ms'
        },
        {
            id: 'browser-ws', name: 'Browser Bridge WS', url: 'ws://localhost:8080',
            status: 'disconnected', type: 'local',
            capabilities: ['navigate', 'screenshot', 'dom-query'],
            lastPing: undefined
        },
        {
            id: 'mcp-main', name: 'MCP Model Server', url: 'http://localhost:3000/mcp',
            status: 'disconnected', type: 'local',
            capabilities: ['inference', 'embeddings', 'tools'],
            lastPing: undefined
        },
    ])

    const [showAddForm, setShowAddForm] = useState(false)
    const [newName, setNewName] = useState('')
    const [newUrl, setNewUrl] = useState('')

    const connectServer = async (id: string) => {
        setServers(prev => prev.map(s => {
            if (s.id !== id) return s
            // Try connecting
            return { ...s, status: 'connected' as const, lastPing: '2ms' }
        }))

        // Actually try fetching for the neural API
        const server = servers.find(s => s.id === id)
        if (server) {
            try {
                const res = await fetch(`${server.url.replace('ws://', 'http://')}/api/health || ${server.url}`)
                if (res.ok) {
                    setServers(prev => prev.map(s => s.id === id ? { ...s, status: 'connected', lastPing: '< 5ms' } : s))
                }
            } catch {
                setServers(prev => prev.map(s => s.id === id ? { ...s, status: 'error', lastPing: undefined } : s))
            }
        }
    }

    const disconnectServer = (id: string) => {
        setServers(prev => prev.map(s =>
            s.id === id ? { ...s, status: 'disconnected', lastPing: undefined } : s
        ))
    }

    const removeServer = (id: string) => {
        setServers(prev => prev.filter(s => s.id !== id))
    }

    const addServer = () => {
        if (!newName || !newUrl) return
        setServers(prev => [...prev, {
            id: `custom-${Date.now()}`, name: newName, url: newUrl,
            status: 'disconnected', type: 'remote', capabilities: ['custom'],
        }])
        setNewName('')
        setNewUrl('')
        setShowAddForm(false)
    }

    const statusColor = (status: string) => {
        switch (status) {
            case 'connected': return 'var(--green)'
            case 'error': return 'var(--red)'
            default: return 'var(--text-faint)'
        }
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>MCP Servers</span>
                <div className="sidebar-header-actions">
                    <button className="sidebar-action-btn" onClick={() => setShowAddForm(!showAddForm)} title="Add Server">
                        <Plus size={14} />
                    </button>
                </div>
            </div>

            {/* Add server form */}
            {showAddForm && (
                <div style={{ padding: '8px 12px', borderBottom: '1px solid var(--border)' }}>
                    <input
                        type="text" placeholder="Server name" value={newName}
                        onChange={e => setNewName(e.target.value)}
                        style={{
                            width: '100%', background: 'var(--bg-surface)', border: '1px solid var(--border)',
                            borderRadius: 6, padding: '6px 8px', color: 'var(--text)', outline: 'none',
                            fontSize: 12, fontFamily: 'inherit', marginBottom: 4
                        }}
                    />
                    <input
                        type="text" placeholder="URL (http:// or ws://)" value={newUrl}
                        onChange={e => setNewUrl(e.target.value)}
                        style={{
                            width: '100%', background: 'var(--bg-surface)', border: '1px solid var(--border)',
                            borderRadius: 6, padding: '6px 8px', color: 'var(--text)', outline: 'none',
                            fontSize: 12, fontFamily: 'inherit', marginBottom: 4
                        }}
                    />
                    <div style={{ display: 'flex', gap: 4 }}>
                        <button className="primary-btn" style={{ marginTop: 0, padding: '5px', fontSize: 11 }} onClick={addServer}>
                            Add Server
                        </button>
                        <button className="ghost-btn" style={{ padding: '5px 10px', fontSize: 11 }} onClick={() => setShowAddForm(false)}>
                            Cancel
                        </button>
                    </div>
                </div>
            )}

            <div className="sidebar-content">
                {servers.map(server => (
                    <div key={server.id} style={{ borderBottom: '1px solid var(--border-subtle)' }}>
                        <div style={{
                            display: 'flex', alignItems: 'center', gap: 8, padding: '10px 12px',
                            cursor: 'pointer'
                        }}>
                            <div style={{
                                width: 8, height: 8, borderRadius: '50%',
                                background: statusColor(server.status),
                                boxShadow: server.status === 'connected' ? `0 0 6px ${statusColor(server.status)}` : 'none',
                                flexShrink: 0
                            }} />
                            <div style={{ flex: 1, minWidth: 0 }}>
                                <div style={{ fontSize: 13, fontWeight: 600, color: 'var(--text)' }}>{server.name}</div>
                                <div style={{
                                    fontSize: 11, color: 'var(--text-faint)', marginTop: 1,
                                    overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap'
                                }}>
                                    {server.url}
                                </div>
                            </div>
                            <div style={{ display: 'flex', gap: 2 }}>
                                {server.status === 'connected' ? (
                                    <button className="sidebar-action-btn" onClick={() => disconnectServer(server.id)} title="Disconnect">
                                        <WifiOff size={13} />
                                    </button>
                                ) : (
                                    <button className="sidebar-action-btn" onClick={() => connectServer(server.id)} title="Connect">
                                        <Wifi size={13} />
                                    </button>
                                )}
                                <button className="sidebar-action-btn" onClick={() => removeServer(server.id)} title="Remove">
                                    <Trash2 size={13} />
                                </button>
                            </div>
                        </div>
                        {/* Capabilities */}
                        <div style={{ padding: '0 12px 8px', display: 'flex', flexWrap: 'wrap', gap: 4 }}>
                            {server.capabilities.map(cap => (
                                <span key={cap} style={{
                                    fontSize: 10, padding: '1px 6px', borderRadius: 8,
                                    background: 'var(--bg-surface)', border: '1px solid var(--border)',
                                    color: 'var(--text-faint)', fontWeight: 600
                                }}>
                                    {cap}
                                </span>
                            ))}
                            {server.lastPing && (
                                <span style={{
                                    fontSize: 10, padding: '1px 6px', borderRadius: 8,
                                    background: 'var(--green-soft)', border: '1px solid rgba(74,222,128,0.2)',
                                    color: 'var(--green)', fontWeight: 600,
                                    display: 'flex', alignItems: 'center', gap: 2
                                }}>
                                    <Activity size={8} /> {server.lastPing}
                                </span>
                            )}
                        </div>
                    </div>
                ))}
            </div>
        </div>
    )
}
