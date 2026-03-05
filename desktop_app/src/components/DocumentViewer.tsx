import { useState, useEffect } from 'react'
import ReactMarkdown from 'react-markdown'
import remarkGfm from 'remark-gfm'
import mammoth from 'mammoth'

interface Props {
    filePath: string
    projectRoot?: string
    onEdit?: () => void
}

export function DocumentViewer({ filePath, projectRoot, onEdit }: Props) {
    // projectRoot might be needed later, using it here to suppress TS6133
    void projectRoot;
    const ext = filePath.split('.').pop()?.toLowerCase() || ''
    const [content, setContent] = useState<string | null>(null)
    const [loading, setLoading] = useState(true)

    useEffect(() => {
        const loadDocs = async () => {
            setLoading(true)
            try {
                if (ext === 'md') {
                    const res = await window.fs?.readFile(filePath)
                    setContent(typeof res === 'string' ? res : '// Could not load markdown')
                } else if (ext === 'docx') {
                    if (window.fs?.readFileBase64) {
                        const base64 = await window.fs.readFileBase64(filePath)
                        if (base64 && !base64.error) {
                            const buffer = Uint8Array.from(atob(base64), c => c.charCodeAt(0))
                            const result = await mammoth.convertToHtml({ arrayBuffer: buffer.buffer })
                            setContent(result.value)
                        } else {
                            setContent('<p>Error loading DOCX file.</p>')
                        }
                    } else {
                        setContent('<p>fs.readFileBase64 not found</p>')
                    }
                }
            } catch (err) {
                console.error('Failed to load document', err)
                setContent('Error loading document')
            }
            setLoading(false)
        }

        if (ext === 'md' || ext === 'docx') {
            loadDocs()
        } else {
            setLoading(false)
        }
    }, [filePath, ext])

    if (loading) {
        return (
            <div style={{ flex: 1, display: 'flex', flexDirection: 'column', alignItems: 'center', justifyContent: 'center', gap: 8, background: '#1A1D23', color: '#6B7280' }}>
                <div className="spin">
                    <svg width="20" height="20" viewBox="0 0 24 24" fill="none" stroke="#4A9EFF" strokeWidth="2">
                        <path d="M21 12a9 9 0 11-6.219-8.56" />
                    </svg>
                </div>
                <span style={{ fontSize: 12 }}>Loading document...</span>
            </div>
        )
    }

    if (ext === 'pdf') {
        const normalizedPath = filePath.replace(/\\/g, '/')
        return (
            <div style={{ flex: 1, height: '100%', background: '#fff' }}>
                <webview src={`file:///${normalizedPath}`} style={{ width: '100%', height: '100%' }} plugins />
            </div>
        )
    }

    if (ext === 'md') {
        return (
            <div style={{ flex: 1, height: '100%', padding: '2rem', overflowY: 'auto', background: '#1A1D23', color: '#ABB2BF', fontFamily: 'system-ui', position: 'relative' }}>
                {onEdit && (
                    <button
                        onClick={onEdit}
                        style={{ position: 'absolute', top: 16, right: 16, padding: '6px 12px', background: '#4A9EFF', color: '#fff', border: 'none', borderRadius: 4, cursor: 'pointer', fontSize: 12, fontWeight: 600 }}
                    >
                        Edit Markdown
                    </button>
                )}
                <div className="markdown-body" style={{ maxWidth: 800, margin: '0 auto', fontSize: 14, lineHeight: 1.6 }}>
                    <ReactMarkdown remarkPlugins={[remarkGfm]}>
                        {content || ''}
                    </ReactMarkdown>
                </div>
            </div>
        )
    }

    if (ext === 'docx') {
        return (
            <div style={{ flex: 1, height: '100%', padding: '2rem', overflowY: 'auto', background: '#fff', color: '#000', fontFamily: 'serif' }}>
                <div dangerouslySetInnerHTML={{ __html: content || 'Could not load DOCX' }} style={{ maxWidth: 800, margin: '0 auto', fontSize: 16, lineHeight: 1.6 }} />
            </div>
        )
    }

    return (
        <div style={{ flex: 1, display: 'flex', alignItems: 'center', justifyContent: 'center', background: '#1A1D23', color: '#6B7280' }}>
            Unsupported file type: {ext}
        </div>
    )
}
