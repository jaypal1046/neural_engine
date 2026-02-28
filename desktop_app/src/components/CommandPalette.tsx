import { useState, useEffect, useRef } from 'react'
import {
    Search, FolderOpen, Globe, BrainCircuit, Terminal, Settings,
    FileText, Zap
} from 'lucide-react'

interface Command {
    id: string
    label: string
    shortcut?: string
    action: () => void
}

interface Props {
    commands: Command[]
    onClose: () => void
}

const ICON_MAP: Record<string, any> = {
    'open-file': FolderOpen,
    'open-folder': FolderOpen,
    'new-browser': Globe,
    'ai-chat': BrainCircuit,
    'toggle-terminal': Terminal,
    'toggle-sidebar': FileText,
    'compress': Zap,
    'decompress': FileText,
    'settings': Settings,
}

export function CommandPalette({ commands, onClose }: Props) {
    const [query, setQuery] = useState('')
    const [activeIndex, setActiveIndex] = useState(0)
    const inputRef = useRef<HTMLInputElement>(null)

    const filtered = commands.filter(cmd =>
        cmd.label.toLowerCase().includes(query.toLowerCase())
    )

    useEffect(() => {
        inputRef.current?.focus()
    }, [])

    useEffect(() => {
        setActiveIndex(0)
    }, [query])

    const handleKeyDown = (e: React.KeyboardEvent) => {
        if (e.key === 'Escape') {
            onClose()
        } else if (e.key === 'ArrowDown') {
            e.preventDefault()
            setActiveIndex(prev => Math.min(prev + 1, filtered.length - 1))
        } else if (e.key === 'ArrowUp') {
            e.preventDefault()
            setActiveIndex(prev => Math.max(prev - 1, 0))
        } else if (e.key === 'Enter') {
            if (filtered[activeIndex]) {
                filtered[activeIndex].action()
                onClose()
            }
        }
    }

    return (
        <div className="command-palette-overlay" onClick={onClose}>
            <div className="command-palette" onClick={e => e.stopPropagation()}>
                <div className="command-palette-input">
                    <Search size={16} style={{ color: 'var(--text-muted)' }} />
                    <input
                        ref={inputRef}
                        type="text"
                        placeholder="Type a command..."
                        value={query}
                        onChange={e => setQuery(e.target.value)}
                        onKeyDown={handleKeyDown}
                    />
                </div>
                <div className="command-palette-list">
                    {filtered.map((cmd, i) => {
                        const Icon = ICON_MAP[cmd.id] || Zap
                        return (
                            <div
                                key={cmd.id}
                                className={`command-item ${i === activeIndex ? 'active' : ''}`}
                                onClick={() => { cmd.action(); onClose() }}
                                onMouseEnter={() => setActiveIndex(i)}
                            >
                                <span className="command-item-icon">
                                    <Icon size={15} />
                                </span>
                                <span className="command-item-label">{cmd.label}</span>
                                {cmd.shortcut && (
                                    <span className="command-item-shortcut">
                                        <kbd>{cmd.shortcut}</kbd>
                                    </span>
                                )}
                            </div>
                        )
                    })}
                    {filtered.length === 0 && (
                        <div style={{
                            padding: 16, textAlign: 'center', color: 'var(--text-faint)', fontSize: 13
                        }}>
                            No matching commands
                        </div>
                    )}
                </div>
            </div>
        </div>
    )
}
