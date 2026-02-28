import { GitBranch, Bell, Cpu, Zap, AlertCircle, AlertTriangle, CheckCircle } from 'lucide-react'

interface Props {
    serverStatus: string
    projectRoot: string
    activeFilePath?: string
    cursorLine?: number
    cursorCol?: number
    encoding?: string
    eol?: string
    problems?: { errors: number; warnings: number }
    zenMode?: boolean
    onToggleZen?: () => void
    onToggleTerminal?: () => void
    onToggleProblems?: () => void
}

function getLanguageName(filePath?: string): string {
    if (!filePath) return ''
    const ext = filePath.split('.').pop()?.toLowerCase() || ''
    const langs: Record<string, string> = {
        ts: 'TypeScript',
        tsx: 'TypeScript React',
        js: 'JavaScript',
        jsx: 'JavaScript React',
        css: 'CSS',
        scss: 'SCSS',
        html: 'HTML',
        json: 'JSON',
        py: 'Python',
        md: 'Markdown',
        txt: 'Plain Text',
        sh: 'Shell Script',
        bat: 'Batch',
        cpp: 'C++',
        c: 'C',
        h: 'C Header',
        rs: 'Rust',
        go: 'Go',
        java: 'Java',
        dart: 'Dart',
        yaml: 'YAML',
        yml: 'YAML',
        toml: 'TOML',
        xml: 'XML',
        svg: 'SVG',
        sql: 'SQL',
        graphql: 'GraphQL',
        vue: 'Vue',
        svelte: 'Svelte',
        php: 'PHP',
        rb: 'Ruby',
        swift: 'Swift',
        kt: 'Kotlin',
    }
    return langs[ext] || ext.toUpperCase() || ''
}

export function StatusBar({ serverStatus, activeFilePath, cursorLine, cursorCol, encoding = 'UTF-8', eol = 'LF', problems, zenMode, onToggleZen, onToggleTerminal, onToggleProblems }: Props) {
    const language = getLanguageName(activeFilePath)
    const hasErrors = (problems?.errors || 0) > 0
    const hasWarnings = (problems?.warnings || 0) > 0

    return (
        <div className="status-bar">
            {/* Left side */}
            <div className="status-item clickable" title="Source Control">
                <GitBranch size={12} />
                <span>main</span>
            </div>

            {/* Problems indicator */}
            <div className="status-item clickable" onClick={onToggleProblems} title="Problems">
                {hasErrors ? (
                    <>
                        <AlertCircle size={11} style={{ color: 'var(--red)' }} />
                        <span style={{ color: 'var(--red)' }}>{problems!.errors}</span>
                    </>
                ) : (
                    <CheckCircle size={11} style={{ color: 'var(--green)', opacity: 0.7 }} />
                )}
                {hasWarnings && (
                    <>
                        <AlertTriangle size={11} style={{ color: 'var(--yellow)', marginLeft: 4 }} />
                        <span style={{ color: 'var(--yellow)' }}>{problems!.warnings}</span>
                    </>
                )}
            </div>

            <div className="status-item">
                {serverStatus === 'online' ? (
                    <>
                        <div className="status-dot-online" />
                        <span>API Connected</span>
                    </>
                ) : (
                    <>
                        <div className="status-dot-offline" />
                        <span>API Offline</span>
                    </>
                )}
            </div>

            <div className="status-item">
                <Zap size={11} />
                <span>Neural Engine V10</span>
            </div>

            <div className="status-separator" />

            {/* Right side */}
            {cursorLine && cursorCol && (
                <div className="status-item clickable" title="Go to Line">
                    <span>Ln {cursorLine}, Col {cursorCol}</span>
                </div>
            )}

            <div className="status-item">
                <span>{encoding}</span>
            </div>

            <div className="status-item">
                <span>{eol}</span>
            </div>

            {language && (
                <div className="status-item clickable" title={`Language: ${language}`}>
                    <span>{language}</span>
                </div>
            )}

            {zenMode !== undefined && (
                <div className="status-item clickable" onClick={onToggleZen} title="Toggle Zen Mode (Ctrl+Shift+Z)">
                    <span>{zenMode ? '🧘 Zen' : ''}</span>
                </div>
            )}

            <div className="status-item clickable" onClick={onToggleTerminal} title="Toggle Terminal">
                <Cpu size={11} />
            </div>

            <div className="status-item">
                <Bell size={11} />
            </div>
        </div>
    )
}
