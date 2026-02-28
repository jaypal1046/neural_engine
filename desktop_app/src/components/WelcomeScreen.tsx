import {
    Globe, BrainCircuit, FolderOpen, Zap, Terminal,
    Puzzle, GitBranch, Server
} from 'lucide-react'

interface Props {
    openFile: (filePath: string, fileName: string) => void
    openWebView: () => void
    openAIChat: () => void
    openCompress: () => void
}

export function WelcomeScreen({ openFile, openWebView, openAIChat, openCompress }: Props) {
    return (
        <div className="welcome-screen">
            {/* Logo */}
            <div className="welcome-logo">
                <svg width="36" height="36" viewBox="0 0 24 24" fill="none" stroke="white" strokeWidth="1.5">
                    <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5" />
                </svg>
            </div>

            <div className="welcome-title">
                Neural <span>Studio IDE</span>
            </div>
            <div className="welcome-subtitle">
                A powerful AI-powered IDE with built-in browser, neural compression engine,
                integrated terminal, Git support, MCP servers, and more.
            </div>

            {/* Action cards */}
            <div className="welcome-actions">
                <div className="welcome-action" onClick={openAIChat}>
                    <div className="welcome-action-icon" style={{ background: 'rgba(74,158,255,0.1)', color: '#4A9EFF' }}>
                        <BrainCircuit size={20} />
                    </div>
                    <span className="welcome-action-label">AI Copilot</span>
                    <span className="welcome-action-desc">Chat with AI</span>
                </div>

                <div className="welcome-action" onClick={openWebView}>
                    <div className="welcome-action-icon" style={{ background: 'rgba(34,211,238,0.1)', color: '#22D3EE' }}>
                        <Globe size={20} />
                    </div>
                    <span className="welcome-action-label">Browser</span>
                    <span className="welcome-action-desc">Built-in WebView</span>
                </div>

                <div className="welcome-action" onClick={openCompress}>
                    <div className="welcome-action-icon" style={{ background: 'rgba(201,127,219,0.1)', color: '#C97FDB' }}>
                        <Zap size={20} />
                    </div>
                    <span className="welcome-action-label">Compress</span>
                    <span className="welcome-action-desc">Neural compression</span>
                </div>

                <div className="welcome-action" onClick={() => {
                    if (window.ipcRenderer?.selectFile) {
                        window.ipcRenderer.selectFile().then((f: string) => {
                            if (f) openFile(f, f.split(/[\\/]/).pop()!)
                        })
                    }
                }}>
                    <div className="welcome-action-icon" style={{ background: 'rgba(251,191,36,0.1)', color: '#FBBF24' }}>
                        <FolderOpen size={20} />
                    </div>
                    <span className="welcome-action-label">Open File</span>
                    <span className="welcome-action-desc">Browse & edit</span>
                </div>
            </div>

            {/* Keyboard shortcuts */}
            <div className="welcome-shortcuts">
                <div className="welcome-shortcut">
                    <kbd>Ctrl+Shift+P</kbd>
                    <span>Command Palette</span>
                </div>
                <div className="welcome-shortcut">
                    <kbd>Ctrl+`</kbd>
                    <span>Toggle Terminal</span>
                </div>
                <div className="welcome-shortcut">
                    <kbd>Ctrl+B</kbd>
                    <span>Toggle Sidebar</span>
                </div>
                <div className="welcome-shortcut">
                    <kbd>Ctrl+W</kbd>
                    <span>Close Tab</span>
                </div>
            </div>

            {/* Feature badges */}
            <div style={{
                display: 'flex', flexWrap: 'wrap', gap: 6, justifyContent: 'center', marginTop: 8
            }}>
                {[
                    { icon: Globe, label: 'WebView', color: '#22D3EE' },
                    { icon: Terminal, label: 'Terminal', color: '#4ADE80' },
                    { icon: GitBranch, label: 'Git', color: '#F97316' },
                    { icon: Puzzle, label: 'Extensions', color: '#C97FDB' },
                    { icon: Server, label: 'MCP', color: '#DA7555' },
                    { icon: BrainCircuit, label: 'AI', color: '#4A9EFF' },
                ].map(feat => (
                    <span key={feat.label} style={{
                        display: 'flex', alignItems: 'center', gap: 4,
                        fontSize: 10, fontWeight: 600, padding: '3px 8px',
                        borderRadius: 12, background: `color-mix(in srgb, ${feat.color} 8%, transparent)`,
                        border: `1px solid color-mix(in srgb, ${feat.color} 20%, transparent)`,
                        color: feat.color
                    }}>
                        <feat.icon size={10} /> {feat.label}
                    </span>
                ))}
            </div>
        </div>
    )
}
