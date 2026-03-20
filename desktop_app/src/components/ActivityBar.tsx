import {
    FolderOpen, Search, GitBranch, Puzzle, BrainCircuit, Globe,
    Settings, Server, PlaySquare, TrendingUp, Database
} from 'lucide-react'

interface Props {
    activePanel: string
    onTogglePanel: (panel: string) => void
    onOpenAI: () => void
    onOpenBrowser: () => void
}

const TOP_ITEMS = [
    { id: 'explorer', icon: FolderOpen, tooltip: 'Explorer' },
    { id: 'search', icon: Search, tooltip: 'Search' },
    { id: 'git', icon: GitBranch, tooltip: 'Source Control' },
    { id: 'run', icon: PlaySquare, tooltip: 'Run and Debug' },
    { id: 'extensions', icon: Puzzle, tooltip: 'Extensions' },
    { id: 'mcp', icon: Server, tooltip: 'MCP Servers' },
    { id: 'ai-stats', icon: TrendingUp, tooltip: 'AI Training Stats' },
    { id: 'project-memory', icon: Database, tooltip: 'Project Memory' },
]

const BOTTOM_ITEMS = [
    { id: 'ai', icon: BrainCircuit, tooltip: 'AI Copilot', action: 'openAI' as const },
    { id: 'browser-action', icon: Globe, tooltip: 'Browser', action: 'openBrowser' as const },
    { id: 'settings-action', icon: Settings, tooltip: 'Settings' },
]

export function ActivityBar({ activePanel, onTogglePanel, onOpenAI, onOpenBrowser }: Props) {
    return (
        <div className="activity-bar">
            <div className="activity-bar-top">
                {TOP_ITEMS.map(item => (
                    <button
                        key={item.id}
                        className={`activity-btn tooltip-wrap ${activePanel === item.id ? 'active' : ''}`}
                        data-tooltip={item.tooltip}
                        onClick={() => onTogglePanel(item.id)}
                    >
                        <item.icon size={20} strokeWidth={1.6} />
                    </button>
                ))}
            </div>
            <div className="activity-bar-bottom">
                {BOTTOM_ITEMS.map(item => (
                    <button
                        key={item.id}
                        className="activity-btn tooltip-wrap"
                        data-tooltip={item.tooltip}
                        onClick={() => {
                            if (item.action === 'openAI') onOpenAI()
                            else if (item.action === 'openBrowser') onOpenBrowser()
                            else onTogglePanel(item.id)
                        }}
                    >
                        <item.icon size={20} strokeWidth={1.6} />
                    </button>
                ))}
            </div>
        </div>
    )
}
