import { useState, useEffect, useCallback } from 'react'
import { Play, Plus, Trash2, Settings, Save, ChevronDown, RefreshCw } from 'lucide-react'

export interface RunConfig {
    name: string
    type: 'node' | 'python' | 'custom' | 'npm' | 'dart' | 'go' | 'cpp' | 'java' | 'shell'
    command: string
    args?: string
    cwd?: string
    env?: Record<string, string>
    preLaunchTask?: string
}

const DEFAULT_CONFIGS: RunConfig[] = [
    { name: 'Node.js: Current File', type: 'node', command: 'node "${file}"' },
    { name: 'Python: Current File', type: 'python', command: 'python "${file}"' },
    { name: 'TypeScript: Current File', type: 'node', command: 'npx ts-node "${file}"' },
    { name: 'npm: start', type: 'npm', command: 'npm start' },
    { name: 'npm: dev', type: 'npm', command: 'npm run dev' },
    { name: 'npm: build', type: 'npm', command: 'npm run build' },
    { name: 'npm: test', type: 'npm', command: 'npm test' },
]

interface Props {
    projectRoot: string
    activeFilePath?: string
}

const STORAGE_KEY = 'neural-run-configs'

function loadConfigs(): RunConfig[] {
    try {
        const saved = localStorage.getItem(STORAGE_KEY)
        if (saved) return JSON.parse(saved)
    } catch { /* ignore */ }
    return [...DEFAULT_CONFIGS]
}

function saveConfigs(configs: RunConfig[]) {
    localStorage.setItem(STORAGE_KEY, JSON.stringify(configs))
}

export function RunPanel({ projectRoot, activeFilePath }: Props) {
    const [configs, setConfigs] = useState<RunConfig[]>(loadConfigs)
    const [selectedConfig, setSelectedConfig] = useState<number>(0)
    const [scripts, setScripts] = useState<Record<string, string>>({})
    const [loading, setLoading] = useState(true)
    const [showEditor, setShowEditor] = useState(false)
    const [editConfig, setEditConfig] = useState<RunConfig | null>(null)
    const [editIndex, setEditIndex] = useState<number>(-1)
    const [runningConfig, setRunningConfig] = useState<string | null>(null)

    // Load package.json scripts
    useEffect(() => {
        const load = async () => {
            if (!projectRoot || !window.fs?.readFile) {
                setLoading(false)
                return
            }
            try {
                const content = await window.fs.readFile(`${projectRoot}\\package.json`)
                if (content && typeof content === 'string') {
                    const pkg = JSON.parse(content)
                    if (pkg.scripts) setScripts(pkg.scripts)
                }
            } catch {
                // Not a Node.js project
            }
            setLoading(false)
        }
        load()
    }, [projectRoot])

    // Save configs when they change
    useEffect(() => {
        saveConfigs(configs)
    }, [configs])

    // Resolve variables in command
    const resolveCommand = useCallback((cmd: string): string => {
        const fileName = activeFilePath?.split(/[/\\]/).pop() || ''
        const fileDir = activeFilePath ? activeFilePath.substring(0, activeFilePath.lastIndexOf(/[/\\]/.test(activeFilePath) ? (activeFilePath.includes('/') ? '/' : '\\') : '\\')) : projectRoot
        const relPath = activeFilePath?.startsWith(projectRoot)
            ? activeFilePath.slice(projectRoot.length + 1)
            : activeFilePath || ''

        return cmd
            .replace(/\$\{file\}/g, `"${relPath}"`)
            .replace(/\$\{fileBasename\}/g, fileName)
            .replace(/\$\{fileDir\}/g, fileDir)
            .replace(/\$\{workspaceFolder\}/g, projectRoot)
    }, [activeFilePath, projectRoot])

    const runConfig = useCallback((config: RunConfig) => {
        const cmd = resolveCommand(config.command + (config.args ? ' ' + config.args : ''))
        setRunningConfig(config.name)

        // Open terminal panel
        window.dispatchEvent(new CustomEvent('open-bottom-panel', { detail: { tab: 'terminal' } }))

        setTimeout(() => {
            window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd } }))
            // Clear running state after a delay
            setTimeout(() => setRunningConfig(null), 2000)
        }, 300)
    }, [resolveCommand])

    const runSelected = useCallback(() => {
        if (configs[selectedConfig]) {
            runConfig(configs[selectedConfig])
        }
    }, [configs, selectedConfig, runConfig])

    const addConfig = () => {
        setEditConfig({
            name: 'New Configuration',
            type: 'custom',
            command: '',
        })
        setEditIndex(-1)
        setShowEditor(true)
    }

    const editExisting = (index: number) => {
        setEditConfig({ ...configs[index] })
        setEditIndex(index)
        setShowEditor(true)
    }

    const saveEditConfig = () => {
        if (!editConfig || !editConfig.name.trim() || !editConfig.command.trim()) return
        if (editIndex >= 0) {
            setConfigs(prev => prev.map((c, i) => i === editIndex ? editConfig : c))
        } else {
            setConfigs(prev => [...prev, editConfig])
        }
        setShowEditor(false)
        setEditConfig(null)
    }

    const deleteConfig = (index: number) => {
        setConfigs(prev => prev.filter((_, i) => i !== index))
        if (selectedConfig >= index && selectedConfig > 0) {
            setSelectedConfig(selectedConfig - 1)
        }
    }

    const resetDefaults = () => {
        setConfigs([...DEFAULT_CONFIGS])
        setSelectedConfig(0)
    }

    const typeColors: Record<string, string> = {
        node: '#4ADE80',
        python: '#3776AB',
        npm: '#CB3837',
        custom: '#FBBF24',
        dart: '#0175C2',
        go: '#00ADD8',
        cpp: '#659AD2',
        java: '#ED8B00',
        shell: '#4EAA25',
    }

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', background: 'var(--bg-surface)', overflow: 'hidden' }}>
            <div className="sidebar-header">
                <span>Run & Debug</span>
                <div className="sidebar-header-actions">
                    <button className="sidebar-action-btn" onClick={addConfig} title="Add Configuration">
                        <Plus size={14} />
                    </button>
                    <button className="sidebar-action-btn" onClick={resetDefaults} title="Reset to Defaults">
                        <RefreshCw size={14} />
                    </button>
                    <button className="sidebar-action-btn" onClick={() => setShowEditor(!showEditor)} title="Edit Configurations">
                        <Settings size={14} />
                    </button>
                </div>
            </div>

            <div className="sidebar-content" style={{ padding: '8px 12px' }}>
                {/* Run bar */}
                <div className="run-config-bar">
                    <button
                        className="run-config-play"
                        onClick={runSelected}
                        disabled={!configs[selectedConfig]}
                        title="Run Selected Configuration (F5)"
                    >
                        <Play size={14} fill="currentColor" />
                    </button>
                    <div className="run-config-select-wrap">
                        <select
                            className="run-config-select"
                            value={selectedConfig}
                            onChange={(e) => setSelectedConfig(Number(e.target.value))}
                        >
                            {configs.map((c, i) => (
                                <option key={i} value={i}>{c.name}</option>
                            ))}
                        </select>
                        <ChevronDown size={12} className="run-config-chevron" />
                    </div>
                </div>

                {/* Config editor */}
                {showEditor && editConfig && (
                    <div className="run-config-editor">
                        <div className="run-config-field">
                            <label>Name</label>
                            <input
                                value={editConfig.name}
                                onChange={e => setEditConfig({ ...editConfig, name: e.target.value })}
                                placeholder="Configuration name"
                            />
                        </div>
                        <div className="run-config-field">
                            <label>Type</label>
                            <select
                                value={editConfig.type}
                                onChange={e => setEditConfig({ ...editConfig, type: e.target.value as RunConfig['type'] })}
                            >
                                <option value="node">Node.js</option>
                                <option value="python">Python</option>
                                <option value="npm">npm</option>
                                <option value="dart">Dart</option>
                                <option value="go">Go</option>
                                <option value="cpp">C++</option>
                                <option value="java">Java</option>
                                <option value="shell">Shell</option>
                                <option value="custom">Custom</option>
                            </select>
                        </div>
                        <div className="run-config-field">
                            <label>Command</label>
                            <input
                                value={editConfig.command}
                                onChange={e => setEditConfig({ ...editConfig, command: e.target.value })}
                                placeholder='e.g. node "${file}" or npm run dev'
                            />
                        </div>
                        <div className="run-config-field">
                            <label>Arguments (optional)</label>
                            <input
                                value={editConfig.args || ''}
                                onChange={e => setEditConfig({ ...editConfig, args: e.target.value })}
                                placeholder="--verbose --port 3000"
                            />
                        </div>
                        <div className="run-config-hint">
                            Variables: <code>{'${file}'}</code> <code>{'${fileBasename}'}</code> <code>{'${workspaceFolder}'}</code>
                        </div>
                        <div className="run-config-editor-actions">
                            <button className="run-config-save" onClick={saveEditConfig}>
                                <Save size={12} /> Save
                            </button>
                            <button className="run-config-cancel" onClick={() => { setShowEditor(false); setEditConfig(null) }}>
                                Cancel
                            </button>
                        </div>
                    </div>
                )}

                {/* Configurations list */}
                <div style={{ fontSize: 11, fontWeight: 600, color: 'var(--text-secondary)', marginTop: 12, marginBottom: 8, textTransform: 'uppercase' }}>
                    Configurations ({configs.length})
                </div>

                <div style={{ display: 'flex', flexDirection: 'column', gap: 4 }}>
                    {configs.map((config, i) => (
                        <div
                            key={i}
                            className={`run-config-item ${selectedConfig === i ? 'selected' : ''} ${runningConfig === config.name ? 'running' : ''}`}
                            onClick={() => setSelectedConfig(i)}
                            onDoubleClick={() => runConfig(config)}
                        >
                            <div className="run-config-dot" style={{ background: typeColors[config.type] || '#888' }} />
                            <div className="run-config-item-info">
                                <span className="run-config-item-name">{config.name}</span>
                                <span className="run-config-item-cmd">{config.command}</span>
                            </div>
                            <div className="run-config-item-actions">
                                <button onClick={(e) => { e.stopPropagation(); runConfig(config) }} title="Run">
                                    <Play size={11} fill="currentColor" />
                                </button>
                                <button onClick={(e) => { e.stopPropagation(); editExisting(i) }} title="Edit">
                                    <Settings size={11} />
                                </button>
                                <button onClick={(e) => { e.stopPropagation(); deleteConfig(i) }} title="Delete">
                                    <Trash2 size={11} />
                                </button>
                            </div>
                        </div>
                    ))}
                </div>

                {/* Package.json scripts */}
                {Object.keys(scripts).length > 0 && (
                    <>
                        <div style={{ fontSize: 11, fontWeight: 600, color: 'var(--text-secondary)', marginTop: 16, marginBottom: 8, textTransform: 'uppercase' }}>
                            npm Scripts ({Object.keys(scripts).length})
                        </div>
                        {Object.entries(scripts).map(([name, cmd]) => (
                            <div
                                key={name}
                                className="run-config-item"
                                onClick={() => {
                                    window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd: `npm run ${name}` } }))
                                }}
                            >
                                <div className="run-config-dot" style={{ background: '#CB3837' }} />
                                <div className="run-config-item-info">
                                    <span className="run-config-item-name">{name}</span>
                                    <span className="run-config-item-cmd">{cmd as string}</span>
                                </div>
                                <div className="run-config-item-actions">
                                    <button onClick={(e) => {
                                        e.stopPropagation()
                                        window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd: `npm run ${name}` } }))
                                    }}>
                                        <Play size={11} fill="currentColor" />
                                    </button>
                                </div>
                            </div>
                        ))}
                    </>
                )}

                {loading && (
                    <div style={{ color: 'var(--text-faint)', fontSize: 12, marginTop: 12 }}>Loading tasks...</div>
                )}
            </div>
        </div>
    )
}
