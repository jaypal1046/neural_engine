import { useState, useRef, useCallback } from 'react'
import {
    ArrowLeft, ArrowRight, RotateCw, Home, Lock, ExternalLink,
    Star, Shield, Globe, Bug, Wand2
} from 'lucide-react'

interface Props {
    url?: string
}

export function WebViewPanel({ url: initialUrl }: Props) {
    const [currentUrl, setCurrentUrl] = useState(initialUrl || '')
    const [inputUrl, setInputUrl] = useState(initialUrl || '')
    const [isLoading, setIsLoading] = useState(false)
    const [canGoBack] = useState(false)
    const [canGoForward] = useState(false)
    const [isSecure, setIsSecure] = useState(false)
    const [bookmarks] = useState([
        { title: 'Google', url: 'https://www.google.com' },
        { title: 'GitHub', url: 'https://github.com' },
        { title: 'Stack Overflow', url: 'https://stackoverflow.com' },
        { title: 'MDN Web Docs', url: 'https://developer.mozilla.org' },
        { title: 'npm', url: 'https://www.npmjs.com' },
    ])

    const webviewRef = useRef<any>(null)

    const navigate = useCallback((url: string) => {
        let normalizedUrl = url.trim()
        if (!normalizedUrl) return

        // Auto-add protocol
        if (!normalizedUrl.startsWith('http://') && !normalizedUrl.startsWith('https://') && !normalizedUrl.startsWith('file://')) {
            if (normalizedUrl.includes('.') && !normalizedUrl.includes(' ')) {
                normalizedUrl = 'https://' + normalizedUrl
            } else {
                normalizedUrl = `https://www.google.com/search?q=${encodeURIComponent(normalizedUrl)}`
            }
        }

        setCurrentUrl(normalizedUrl)
        setInputUrl(normalizedUrl)
        setIsSecure(normalizedUrl.startsWith('https://'))
        setIsLoading(true)

        // Simulate load completion
        setTimeout(() => setIsLoading(false), 1500)
    }, [])

    const handleKeyDown = (e: React.KeyboardEvent) => {
        if (e.key === 'Enter') {
            navigate(inputUrl)
        }
    }

    const goHome = () => {
        navigate('https://www.google.com')
    }

    const refresh = () => {
        if (currentUrl) {
            setIsLoading(true)
            // Re-trigger webview reload
            if (webviewRef.current) {
                webviewRef.current.reload()
            }
            setTimeout(() => setIsLoading(false), 1000)
        }
    }

    const openExternal = () => {
        if (currentUrl && window.shell?.openExternal) {
            window.shell.openExternal(currentUrl)
        }
    }

    const openDevTools = () => {
        if (webviewRef.current) {
            webviewRef.current.openDevTools()
        }
    }

    const askAIAboutPage = async () => {
        if (webviewRef.current) {
            try {
                // Execute JS in the webview to extract the page text
                const text = await webviewRef.current.executeJavaScript(`
                    (function() {
                        return document.body ? document.body.innerText.substring(0, 3000) : "No content";
                    })();
                `)
                const title = await webviewRef.current.executeJavaScript(`document.title`) || 'Webpage'
                const query = `Can you analyze this webpage for me? Title: ${title}\nURL: ${currentUrl}\n\nContent Snapshot:\n${text}`
                window.dispatchEvent(new CustomEvent('ai-chat-query', { detail: { text: query } }))
                window.dispatchEvent(new CustomEvent('open-ai-chat'))
            } catch (err) {
                console.error("Failed to read webpage contents", err)
            }
        }
    }

    // If no URL yet, show placeholder
    if (!currentUrl) {
        return (
            <div className="webview-container">
                <div className="webview-toolbar">
                    <button className="webview-nav-btn" disabled><ArrowLeft size={16} /></button>
                    <button className="webview-nav-btn" disabled><ArrowRight size={16} /></button>
                    <button className="webview-nav-btn" onClick={refresh} disabled><RotateCw size={16} /></button>
                    <button className="webview-nav-btn" onClick={goHome}><Home size={16} /></button>
                    <div className="webview-url-bar">
                        <Globe size={13} style={{ color: 'var(--text-faint)' }} />
                        <input
                            type="text"
                            placeholder="Enter URL or search..."
                            value={inputUrl}
                            onChange={e => setInputUrl(e.target.value)}
                            onKeyDown={handleKeyDown}
                        />
                    </div>
                    <button className="webview-nav-btn" disabled><ExternalLink size={16} /></button>
                </div>

                {/* Bookmarks bar */}
                <div style={{
                    display: 'flex', gap: 4, padding: '4px 10px',
                    borderBottom: '1px solid var(--border-subtle)', flexShrink: 0, overflowX: 'auto'
                }}>
                    {bookmarks.map(bm => (
                        <button key={bm.url} onClick={() => navigate(bm.url)} style={{
                            display: 'flex', alignItems: 'center', gap: 4, padding: '3px 8px',
                            borderRadius: 4, border: 'none', background: 'transparent', color: 'var(--text-muted)',
                            fontSize: 11, cursor: 'pointer', whiteSpace: 'nowrap', fontFamily: 'inherit',
                            transition: 'all 0.12s'
                        }}
                            onMouseEnter={e => { e.currentTarget.style.background = 'var(--bg-hover)'; e.currentTarget.style.color = 'var(--text)' }}
                            onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--text-muted)' }}
                        >
                            <Star size={10} /> {bm.title}
                        </button>
                    ))}
                </div>

                <div className="webview-placeholder">
                    <div className="webview-placeholder-icon">
                        <Globe size={28} />
                    </div>
                    <div style={{ fontSize: 16, fontWeight: 600, color: 'var(--text-secondary)' }}>
                        Built-in Browser
                    </div>
                    <div style={{ fontSize: 12, maxWidth: 300, textAlign: 'center', lineHeight: 1.6 }}>
                        Browse the web directly inside Neural Studio. Enter a URL above or click a bookmark to get started.
                    </div>
                    <div style={{ display: 'flex', gap: 8, marginTop: 8 }}>
                        {bookmarks.slice(0, 3).map(bm => (
                            <button key={bm.url} onClick={() => navigate(bm.url)} style={{
                                padding: '8px 16px', borderRadius: 8, border: '1px solid var(--border)',
                                background: 'var(--bg-card)', color: 'var(--text-secondary)',
                                cursor: 'pointer', fontSize: 12, fontWeight: 600, fontFamily: 'inherit',
                                transition: 'all 0.15s'
                            }}
                                onMouseEnter={e => { e.currentTarget.style.borderColor = 'var(--accent)'; e.currentTarget.style.color = 'var(--text)' }}
                                onMouseLeave={e => { e.currentTarget.style.borderColor = 'var(--border)'; e.currentTarget.style.color = 'var(--text-secondary)' }}
                            >
                                {bm.title}
                            </button>
                        ))}
                    </div>
                </div>
            </div>
        )
    }

    return (
        <div className="webview-container">
            {/* Toolbar */}
            <div className="webview-toolbar">
                <button className="webview-nav-btn" disabled={!canGoBack}
                    onClick={() => webviewRef.current?.goBack()}>
                    <ArrowLeft size={16} />
                </button>
                <button className="webview-nav-btn" disabled={!canGoForward}
                    onClick={() => webviewRef.current?.goForward()}>
                    <ArrowRight size={16} />
                </button>
                <button className="webview-nav-btn" onClick={refresh}>
                    <RotateCw size={16} className={isLoading ? 'spin' : ''} />
                </button>
                <button className="webview-nav-btn" onClick={goHome}>
                    <Home size={16} />
                </button>

                <div className="webview-url-bar">
                    {isSecure ? (
                        <Lock size={12} className="lock-icon" />
                    ) : (
                        <Shield size={12} style={{ color: 'var(--text-faint)' }} />
                    )}
                    <input
                        type="text"
                        value={inputUrl}
                        onChange={e => setInputUrl(e.target.value)}
                        onKeyDown={handleKeyDown}
                        onFocus={e => e.target.select()}
                    />
                </div>

                <button className="webview-nav-btn" onClick={openDevTools} title="Inspect / DevTools">
                    <Bug size={16} />
                </button>
                <button className="webview-nav-btn" onClick={askAIAboutPage} title="Ask AI about this page" style={{ color: 'var(--accent)' }}>
                    <Wand2 size={16} />
                </button>
                <button className="webview-nav-btn" onClick={openExternal} title="Open in external browser">
                    <ExternalLink size={16} />
                </button>
            </div>

            {/* Bookmarks bar */}
            <div style={{
                display: 'flex', gap: 4, padding: '4px 10px',
                borderBottom: '1px solid var(--border-subtle)', flexShrink: 0, overflowX: 'auto'
            }}>
                {bookmarks.map(bm => (
                    <button key={bm.url} onClick={() => navigate(bm.url)} style={{
                        display: 'flex', alignItems: 'center', gap: 4, padding: '3px 8px',
                        borderRadius: 4, border: 'none', background: 'transparent', color: 'var(--text-muted)',
                        fontSize: 11, cursor: 'pointer', whiteSpace: 'nowrap', fontFamily: 'inherit',
                        transition: 'all 0.12s'
                    }}
                        onMouseEnter={e => { e.currentTarget.style.background = 'var(--bg-hover)'; e.currentTarget.style.color = 'var(--text)' }}
                        onMouseLeave={e => { e.currentTarget.style.background = 'transparent'; e.currentTarget.style.color = 'var(--text-muted)' }}
                    >
                        <Star size={10} /> {bm.title}
                    </button>
                ))}
            </div>

            {/* Loading bar */}
            {isLoading && (
                <div style={{
                    height: 2, background: 'var(--bg-card)', overflow: 'hidden', flexShrink: 0
                }}>
                    <div style={{
                        height: '100%', background: 'var(--accent)', width: '60%',
                        animation: 'loadBar 1.5s ease-in-out infinite',
                    }} />
                </div>
            )}

            {/* Webview */}
            <webview
                ref={webviewRef}
                src={currentUrl}
                className="webview-frame"
                // @ts-ignore - webview attributes
                allowpopups="true"
                style={{ flex: 1 }}
            />
        </div>
    )
}
