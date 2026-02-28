import { useState, useRef } from 'react'
import { X, Play, SplitSquareHorizontal } from 'lucide-react'
import { TabIcon } from './FileIcons'
import type { TabItem } from '../App'

interface Props {
    tabs: TabItem[]
    activeTabId: string
    onSelectTab: (id: string) => void
    onCloseTab: (id: string) => void
    onRunActive?: () => void
    canRunActive?: boolean
    onReorderTabs?: (tabs: TabItem[]) => void
    onSplitEditor?: () => void
}

export function EditorTabs({ tabs, activeTabId, onSelectTab, onCloseTab, onRunActive, canRunActive, onReorderTabs, onSplitEditor }: Props) {
    const [dragIndex, setDragIndex] = useState<number | null>(null)
    const [dragOverIndex, setDragOverIndex] = useState<number | null>(null)
    const dragRef = useRef<HTMLDivElement>(null)

    const handleDragStart = (e: React.DragEvent, index: number) => {
        setDragIndex(index)
        e.dataTransfer.effectAllowed = 'move'
        // Make drag ghost semi-transparent
        const el = e.currentTarget as HTMLElement
        el.style.opacity = '0.5'
    }

    const handleDragEnd = (e: React.DragEvent) => {
        const el = e.currentTarget as HTMLElement
        el.style.opacity = '1'

        if (dragIndex !== null && dragOverIndex !== null && dragIndex !== dragOverIndex && onReorderTabs) {
            const reordered = [...tabs]
            const [moved] = reordered.splice(dragIndex, 1)
            reordered.splice(dragOverIndex, 0, moved)
            onReorderTabs(reordered)
        }

        setDragIndex(null)
        setDragOverIndex(null)
    }

    const handleDragOver = (e: React.DragEvent, index: number) => {
        e.preventDefault()
        e.dataTransfer.dropEffect = 'move'
        setDragOverIndex(index)
    }

    const handleDragLeave = () => {
        setDragOverIndex(null)
    }

    return (
        <div className="editor-tabs-bar" style={{ display: 'flex', justifyContent: 'space-between', alignItems: 'center' }} ref={dragRef}>
            <div style={{ display: 'flex', flex: 1, overflowX: 'auto', overflowY: 'hidden' }}>
                {tabs.map((tab, index) => (
                    <div
                        key={tab.id}
                        className={`editor-tab ${tab.id === activeTabId ? 'active' : ''} ${dragOverIndex === index ? 'drag-over' : ''}`}
                        onClick={() => onSelectTab(tab.id)}
                        draggable
                        onDragStart={(e) => handleDragStart(e, index)}
                        onDragEnd={handleDragEnd}
                        onDragOver={(e) => handleDragOver(e, index)}
                        onDragLeave={handleDragLeave}
                        onMouseDown={(e) => {
                            // Middle click to close
                            if (e.button === 1) {
                                e.preventDefault()
                                onCloseTab(tab.id)
                            }
                        }}
                    >
                        <span className="tab-icon">
                            <TabIcon type={tab.type} filename={tab.label} size={14} />
                        </span>
                        <span className="tab-label">{tab.label}</span>
                        {tab.modified && (
                            <span className="tab-dot" style={{ background: 'var(--accent)' }} />
                        )}
                        <button
                            className="tab-close"
                            onClick={(e) => { e.stopPropagation(); onCloseTab(tab.id) }}
                        >
                            <X size={12} />
                        </button>
                    </div>
                ))}
            </div>

            {/* Editor Actions */}
            <div style={{ padding: '0 8px', display: 'flex', alignItems: 'center', gap: 4 }}>
                {onSplitEditor && (
                    <button
                        className="sidebar-action-btn"
                        onClick={onSplitEditor}
                        title="Split Editor Right"
                        style={{ color: 'var(--text-muted)' }}
                    >
                        <SplitSquareHorizontal size={14} />
                    </button>
                )}
                {canRunActive && (
                    <button
                        className="sidebar-action-btn"
                        onClick={onRunActive}
                        title="Run This File"
                        style={{ color: 'var(--green)' }}
                    >
                        <Play size={14} fill="currentColor" />
                    </button>
                )}
            </div>
        </div>
    )
}
