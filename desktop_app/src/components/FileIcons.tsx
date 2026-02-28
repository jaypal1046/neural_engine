/**
 * SVG File Icons — VS Code-style file type icons
 * Renders inline SVG icons colored by file extension
 */

const ICON_COLORS: Record<string, string> = {
    ts: '#3178C6',
    tsx: '#3178C6',
    js: '#F7DF1E',
    jsx: '#61DAFB',
    css: '#264DE4',
    scss: '#CD6799',
    less: '#1D365D',
    html: '#E44D26',
    json: '#F7DF1E',
    py: '#3776AB',
    md: '#083FA1',
    txt: '#888',
    bat: '#C1F12E',
    sh: '#4EAA25',
    cpp: '#659AD2',
    h: '#A074C4',
    c: '#659AD2',
    rs: '#CE412B',
    go: '#00ADD8',
    java: '#ED8B00',
    yaml: '#CB171E',
    yml: '#CB171E',
    toml: '#9C4121',
    xml: '#E44D26',
    svg: '#FFB13B',
    png: '#4CAF50',
    jpg: '#4CAF50',
    jpeg: '#4CAF50',
    gif: '#4CAF50',
    ico: '#4CAF50',
    lock: '#6B7280',
    env: '#ECD53F',
    gitignore: '#F14E32',
    dockerfile: '#2496ED',
    dart: '#0175C2',
    swift: '#F05138',
    kt: '#7F52FF',
    rb: '#CC342D',
    php: '#777BB4',
    vue: '#42B883',
    svelte: '#FF3E00',
    sql: '#336791',
    graphql: '#E10098',
    wasm: '#654FF0',
}

const ICON_LETTERS: Record<string, string> = {
    ts: 'TS',
    tsx: 'TX',
    js: 'JS',
    jsx: 'JX',
    css: 'CS',
    scss: 'SC',
    html: '<>',
    json: '{}',
    py: 'Py',
    md: 'M↓',
    txt: 'Tx',
    sh: '$',
    bat: '>_',
    cpp: '++',
    c: 'C',
    h: 'H',
    rs: 'Rs',
    go: 'Go',
    java: 'Jv',
    yaml: 'YA',
    yml: 'YA',
    toml: 'TM',
    xml: '<>',
    svg: 'SV',
    lock: '🔒',
    env: '⚙',
    gitignore: 'GI',
    dockerfile: 'Dk',
    dart: 'Dt',
    swift: 'Sw',
    kt: 'Kt',
    rb: 'Rb',
    php: 'PH',
    vue: 'Vu',
    svelte: 'Sv',
    sql: 'SQ',
    graphql: 'GQ',
}

interface FileIconProps {
    filename: string
    size?: number
    className?: string
}

export function FileIcon({ filename, size = 16, className }: FileIconProps) {
    const ext = filename.split('.').pop()?.toLowerCase() || ''
    const color = ICON_COLORS[ext] || '#6B7280'
    const letter = ICON_LETTERS[ext] || ext.slice(0, 2).toUpperCase() || '?'

    // Special icons for known filenames
    const specialFiles: Record<string, { color: string; letter: string }> = {
        'package.json': { color: '#CB3837', letter: 'NP' },
        'tsconfig.json': { color: '#3178C6', letter: 'TS' },
        'vite.config.ts': { color: '#646CFF', letter: 'Vi' },
        '.eslintrc': { color: '#4B32C3', letter: 'ES' },
        '.prettierrc': { color: '#F7B93E', letter: 'Pr' },
        'README.md': { color: '#083FA1', letter: 'Rd' },
        'LICENSE': { color: '#D4AA00', letter: 'Li' },
        '.gitignore': { color: '#F14E32', letter: 'GI' },
        'Dockerfile': { color: '#2496ED', letter: 'Dk' },
    }

    const lowerName = filename.toLowerCase()
    const special = Object.entries(specialFiles).find(([k]) => lowerName === k.toLowerCase())
    const finalColor = special ? special[1].color : color
    const finalLetter = special ? special[1].letter : letter

    return (
        <svg
            width={size}
            height={size}
            viewBox="0 0 20 20"
            fill="none"
            className={`file-icon ${className || ''}`}
            style={{ flexShrink: 0 }}
        >
            {/* File shape */}
            <path
                d="M4 2.5C4 1.67 4.67 1 5.5 1H12L16 5V17.5C16 18.33 15.33 19 14.5 19H5.5C4.67 19 4 18.33 4 17.5V2.5Z"
                fill={finalColor}
                fillOpacity="0.12"
                stroke={finalColor}
                strokeWidth="0.8"
                strokeOpacity="0.5"
            />
            {/* Fold corner */}
            <path
                d="M12 1V5H16"
                stroke={finalColor}
                strokeWidth="0.8"
                strokeOpacity="0.4"
                fill={finalColor}
                fillOpacity="0.06"
            />
            {/* Extension text */}
            <text
                x="10"
                y="14"
                textAnchor="middle"
                fontSize={finalLetter.length > 2 ? '5.5' : '6.5'}
                fontWeight="700"
                fontFamily="'Inter', 'Segoe UI', Arial, sans-serif"
                fill={finalColor}
            >
                {finalLetter}
            </text>
        </svg>
    )
}

// Folder icon
export function FolderIcon({ open = false, size = 16 }: { open?: boolean; size?: number }) {
    return (
        <svg width={size} height={size} viewBox="0 0 20 20" fill="none" style={{ flexShrink: 0 }}>
            {open ? (
                <>
                    <path d="M2 5.5C2 4.67 2.67 4 3.5 4H8L10 6H16.5C17.33 6 18 6.67 18 7.5V8H5L2 16V5.5Z"
                        fill="#E8A838" fillOpacity="0.2" stroke="#E8A838" strokeWidth="0.8" strokeOpacity="0.6" />
                    <path d="M3 16L5 8H18L16 16H3Z"
                        fill="#E8A838" fillOpacity="0.25" stroke="#E8A838" strokeWidth="0.8" strokeOpacity="0.5" />
                </>
            ) : (
                <path d="M2 5.5C2 4.67 2.67 4 3.5 4H8L10 6H16.5C17.33 6 18 6.67 18 7.5V15.5C18 16.33 17.33 17 16.5 17H3.5C2.67 17 2 16.33 2 15.5V5.5Z"
                    fill="#E8A838" fillOpacity="0.2" stroke="#E8A838" strokeWidth="0.8" strokeOpacity="0.6" />
            )}
        </svg>
    )
}

// Tab icon resolver — returns SVG icon for tab types
export function TabIcon({ type, filename, size = 14 }: { type: string; filename?: string; size?: number }) {
    switch (type) {
        case 'file':
            return <FileIcon filename={filename || 'file'} size={size} />
        case 'welcome':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><path d="M10 3L18 9V17H13V12H7V17H2V9L10 3Z" fill="#4ADE80" fillOpacity="0.2" stroke="#4ADE80" strokeWidth="1" /></svg>
        case 'webview':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="7" fill="#4A9EFF" fillOpacity="0.15" stroke="#4A9EFF" strokeWidth="1" /><path d="M3 10H17M10 3C12 6 12 14 10 17M10 3C8 6 8 14 10 17" stroke="#4A9EFF" strokeWidth="0.8" /></svg>
        case 'ai-chat':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="7" fill="#C97FDB" fillOpacity="0.15" stroke="#C97FDB" strokeWidth="1" /><path d="M7 8.5C7 8.5 8 7 10 7C12 7 13 8.5 13 8.5" stroke="#C97FDB" strokeWidth="1" /><circle cx="7.5" cy="10.5" r="0.8" fill="#C97FDB" /><circle cx="12.5" cy="10.5" r="0.8" fill="#C97FDB" /><path d="M7 13C8 14 12 14 13 13" stroke="#C97FDB" strokeWidth="0.8" /></svg>
        case 'settings':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><circle cx="10" cy="10" r="3" fill="#6B7280" fillOpacity="0.2" stroke="#6B7280" strokeWidth="1" /><path d="M10 2V4M10 16V18M2 10H4M16 10H18M4.2 4.2L5.6 5.6M14.4 14.4L15.8 15.8M15.8 4.2L14.4 5.6M5.6 14.4L4.2 15.8" stroke="#6B7280" strokeWidth="1" /></svg>
        case 'compress':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><rect x="3" y="3" width="14" height="14" rx="2" fill="#C97FDB" fillOpacity="0.15" stroke="#C97FDB" strokeWidth="0.8" /><path d="M7 7H13M7 10H13M7 13H10" stroke="#C97FDB" strokeWidth="0.8" /></svg>
        case 'decompress':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><rect x="3" y="3" width="14" height="14" rx="2" fill="#4ADE80" fillOpacity="0.15" stroke="#4ADE80" strokeWidth="0.8" /><path d="M10 6V14M6 10H14" stroke="#4ADE80" strokeWidth="1" /></svg>
        case 'search':
            return <svg width={size} height={size} viewBox="0 0 20 20" fill="none"><circle cx="9" cy="9" r="5" stroke="#4A9EFF" strokeWidth="1" fill="#4A9EFF" fillOpacity="0.1" /><path d="M13 13L17 17" stroke="#4A9EFF" strokeWidth="1.2" /></svg>
        default:
            return <FileIcon filename="file" size={size} />
    }
}

// Get file icon as emoji fallback (for places that need strings)
export function getFileIconColor(filename: string): string {
    const ext = filename.split('.').pop()?.toLowerCase() || ''
    return ICON_COLORS[ext] || '#6B7280'
}
