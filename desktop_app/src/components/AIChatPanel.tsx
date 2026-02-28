import { useState, useRef, useEffect } from 'react'
import {
    ArrowUp, Paperclip, Globe, BrainCircuit, Zap,
    Square, Copy, Check, Wand2
} from 'lucide-react'

const API = 'http://127.0.0.1:8001'

interface Message {
    role: 'user' | 'assistant' | 'system'
    content: string
    timestamp: Date
    tool?: string
}

const QUICK_ACTIONS = [
    { label: 'Analyze', color: '#4ADE80' },
    { label: 'Compress', color: '#C97FDB' },
    { label: 'Generate Code', color: '#FF6B9D' },
    { label: 'Explain', color: '#4A9EFF' },
    { label: 'Optimize', color: '#FBBF24' },
]

const CodeBlock = ({ code, language }: { code: string, language: string }) => {
    const [copied, setCopied] = useState(false)
    const [applied, setApplied] = useState(false)

    const copy = () => {
        navigator.clipboard.writeText(code)
        setCopied(true)
        setTimeout(() => setCopied(false), 2000)
    }

    const apply = () => {
        window.dispatchEvent(new CustomEvent('ai-apply-code', { detail: { code } }))
        setApplied(true)
        setTimeout(() => setApplied(false), 2000)
    }

    return (
        <div style={{ margin: '8px 0', borderRadius: 6, overflow: 'hidden', border: '1px solid var(--border)' }}>
            <div style={{ display: 'flex', justifyContent: 'space-between', padding: '4px 8px', background: 'var(--bg-dark)', borderBottom: '1px solid var(--border)', fontSize: 10, color: 'var(--text-muted)' }}>
                <span>{language || 'code'}</span>
                <div style={{ display: 'flex', gap: 8 }}>
                    <button onClick={copy} style={{ background: 'none', border: 'none', color: 'inherit', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: 4 }}>
                        {copied ? <Check size={12} /> : <Copy size={12} />} {copied ? 'Copied' : 'Copy'}
                    </button>
                    <button onClick={apply} style={{ background: 'none', border: 'none', color: 'var(--accent)', cursor: 'pointer', display: 'flex', alignItems: 'center', gap: 4 }} title="Apply to Active File">
                        {applied ? <Check size={12} /> : <Wand2 size={12} />} {applied ? 'Applied' : 'Apply'}
                    </button>
                </div>
            </div>
            <pre style={{ margin: 0, padding: 8, background: '#1A1D23', color: '#ABB2BF', fontSize: 12, overflowX: 'auto', whiteSpace: 'pre-wrap', wordBreak: 'break-word' }}>
                <code style={{ fontFamily: 'inherit' }}>{code}</code>
            </pre>
        </div>
    )
}

export function AIChatPanel({ serverStatus }: { serverStatus: string }) {
    const [messages, setMessages] = useState<Message[]>([
        {
            role: 'system',
            content: 'Neural Studio AI Copilot — connected to neural engine',
            timestamp: new Date(),
        },
        {
            role: 'assistant',
            content: `Welcome to Neural Studio AI Copilot! 🧠\n\nI'm connected to your local Neural Engine API and ready to help with:\n\n• **File compression & analysis** — powered by 1,046 neural advisors\n• **Code generation** — write Python, C++, JavaScript, and more\n• **Architecture analysis** — understand your codebase\n• **Web research** — search and summarize anything\n\nHow can I help you today?`,
            timestamp: new Date(),
        },
    ])
    const [input, setInput] = useState('')
    const [isTyping, setIsTyping] = useState(false)
    const [webEnabled, setWebEnabled] = useState(false)
    const historyRef = useRef<HTMLDivElement>(null)

    useEffect(() => {
        if (historyRef.current) {
            historyRef.current.scrollTop = historyRef.current.scrollHeight
        }
    }, [messages, isTyping])

    useEffect(() => {
        const handleQuery = (e: Event) => {
            const customEvent = e as CustomEvent<{ text: string }>
            if (customEvent.detail?.text) {
                setInput(prev => prev ? `${prev}\n\n${customEvent.detail.text}` : customEvent.detail.text)
            }
        }
        window.addEventListener('ai-chat-query', handleQuery)
        return () => window.removeEventListener('ai-chat-query', handleQuery)
    }, [])

    const sendMessage = async () => {
        const text = input.trim()
        if (!text || isTyping) return

        const userMsg: Message = { role: 'user', content: text, timestamp: new Date() }
        setMessages(prev => [...prev, userMsg])
        setInput('')
        setIsTyping(true)

        try {
            const systemPrompt = "You are Neural Studio Copilot running inside the user's IDE. " +
                "You can trigger actual IDE actions by using EXACTLY these formats in your text (on a new line): " +
                "\n[ACTION: OPEN_BROWSER] https://example.com" +
                "\n[ACTION: SEARCH] query" +
                "\n[ACTION: RUN_COMMAND] command";

            // Try the actual API
            const res = await fetch(`${API}/api/chat`, {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({
                    message: `${systemPrompt}\n\nUser request:\n${text}`,
                    history: messages.filter(m => m.role !== 'system').map(m => ({
                        role: m.role,
                        content: m.content,
                    })),
                    web_search: webEnabled,
                }),
            })

            if (res.ok) {
                const data = await res.json()
                let replyContent = data.response || data.reply || data.content || JSON.stringify(data)
                let toolUsed = data.tool

                // Intercept Agentic Actions
                const browseMatch = replyContent.match(/\[ACTION:\s*OPEN_BROWSER\]\s*([^\n]+)/i)
                if (browseMatch) {
                    window.dispatchEvent(new CustomEvent('ai-open-browser', { detail: { url: browseMatch[1].trim() } }))
                    replyContent = replyContent.replace(browseMatch[0], '')
                    toolUsed = `Opened Browser: ${browseMatch[1].trim()}`
                }
                const searchMatch = replyContent.match(/\[ACTION:\s*SEARCH\]\s*([^\n]+)/i)
                if (searchMatch) {
                    window.dispatchEvent(new CustomEvent('ai-open-browser', { detail: { url: 'https://google.com/search?q=' + encodeURIComponent(searchMatch[1].trim()) } }))
                    replyContent = replyContent.replace(searchMatch[0], '')
                    toolUsed = `Searched Web: ${searchMatch[1].trim()}`
                }
                const runMatch = replyContent.match(/\[ACTION:\s*RUN_COMMAND\]\s*([^\n]+)/i)
                if (runMatch) {
                    window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd: runMatch[1].trim() } }))
                    replyContent = replyContent.replace(runMatch[0], '')
                    toolUsed = `Ran Terminal Command: ${runMatch[1].trim()}`
                }

                setMessages(prev => [...prev, {
                    role: 'assistant',
                    content: replyContent.trim(),
                    timestamp: new Date(),
                    tool: toolUsed,
                }])
            } else {
                // Fallback response
                setMessages(prev => [...prev, {
                    role: 'assistant',
                    content: `I received your message: "${text}"\n\nThe Neural Engine API responded with status ${res.status}. Make sure the server is running on port 8001.`,
                    timestamp: new Date(),
                }])
            }
        } catch (err) {
            setMessages(prev => [...prev, {
                role: 'assistant',
                content: `⚠️ Could not reach Neural Engine API at ${API}\n\nPlease ensure the Python server is running. You can restart it from the status bar or terminal.\n\nIn the meantime, I can still help with general questions!`,
                timestamp: new Date(),
            }])
        }

        setIsTyping(false)
    }

    const handleQuickAction = (action: string) => {
        setInput(`/${action.toLowerCase()} `)
    }

    const formatTime = (d: Date) => d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' })

    const formatContent = (text: string) => {
        const blocks = text.split(/(```[\s\S]*?```)/g)
        return blocks.map((block, idx) => {
            if (block.startsWith('```') && block.endsWith('```')) {
                const lines = block.slice(3, -3).split('\n')
                const lang = lines[0].trim()
                const code = lines.slice(1).join('\n')
                return <CodeBlock key={idx} language={lang} code={code} />
            }

            // Handle bold outside codeblocks
            const parts = block.split(/(\*\*[^*]+\*\*)/g)
            return parts.map((part, i) => {
                if (part.startsWith('**') && part.endsWith('**')) {
                    return <strong key={`${idx}-${i}`} style={{ color: 'var(--accent)' }}>{part.slice(2, -2)}</strong>
                }
                // Handle inline code
                const codeParts = part.split(/(`[^`]+`)/g)
                return codeParts.map((cp, j) => {
                    if (cp.startsWith('`') && cp.endsWith('`')) {
                        return <code key={`${idx}-${i}-${j}`} className="inline-code">{cp.slice(1, -1)}</code>
                    }
                    return <span key={`${idx}-${i}-${j}`} style={{ whiteSpace: 'pre-wrap' }}>{cp}</span>
                })
            })
        })
    }

    return (
        <div className="ai-chat-panel">
            {/* Header */}
            <div className="ai-chat-header">
                <div className="ai-chat-header-left">
                    <div className="ai-chat-avatar">
                        <BrainCircuit size={16} />
                    </div>
                    <div>
                        <div className="ai-chat-name">AI Copilot</div>
                        <div className="ai-chat-status">
                            <div className="ai-status-dot" style={{
                                background: serverStatus === 'online' ? 'var(--green)' : 'var(--red)',
                                boxShadow: `0 0 6px ${serverStatus === 'online' ? 'var(--green)' : 'var(--red)'}`,
                            }} />
                            {serverStatus === 'online' ? 'Connected to Neural Engine' : 'Engine offline'}
                        </div>
                    </div>
                </div>
                <button
                    onClick={() => setWebEnabled(!webEnabled)}
                    style={{
                        display: 'flex', alignItems: 'center', gap: 5, padding: '4px 10px',
                        borderRadius: 16, border: '1px solid', cursor: 'pointer',
                        fontSize: 11, fontWeight: 600, fontFamily: 'inherit', transition: 'all 0.15s',
                        background: webEnabled ? 'rgba(74,158,255,0.08)' : 'transparent',
                        color: webEnabled ? 'var(--accent)' : 'var(--text-faint)',
                        borderColor: webEnabled ? 'rgba(74,158,255,0.3)' : 'var(--border)',
                    }}
                >
                    <Globe size={12} /> Web
                </button>
            </div>

            {/* Chat history */}
            <div className="ai-chat-history" ref={historyRef}>
                {messages.map((msg, i) => {
                    if (msg.role === 'system') {
                        return (
                            <div key={i} style={{
                                textAlign: 'center', fontSize: 11, color: 'var(--text-faint)',
                                background: 'var(--bg-dark)', border: '1px solid var(--border)',
                                borderRadius: 16, padding: '4px 12px', margin: '0 auto', width: 'fit-content'
                            }}>
                                {msg.content}
                            </div>
                        )
                    }

                    return (
                        <div key={i} className={`ai-msg-row ${msg.role}`}>
                            <div className={`ai-msg-avatar-sm ${msg.role}`}>
                                {msg.role === 'assistant' ? '🧠' : '👤'}
                            </div>
                            <div className="ai-msg-content">
                                {msg.tool && (
                                    <div style={{
                                        fontSize: 10, fontWeight: 700, padding: '1px 6px', borderRadius: 8,
                                        border: '1px solid rgba(201,127,219,0.3)', background: 'rgba(201,127,219,0.08)',
                                        color: '#C97FDB', display: 'inline-block', marginBottom: 4
                                    }}>
                                        🔧 {msg.tool}
                                    </div>
                                )}
                                <div className={`ai-bubble ${msg.role}`}>
                                    {formatContent(msg.content)}
                                </div>
                                <div className="ai-msg-time" style={{
                                    textAlign: msg.role === 'user' ? 'right' : 'left'
                                }}>
                                    {formatTime(msg.timestamp)}
                                </div>
                            </div>
                        </div>
                    )
                })}

                {isTyping && (
                    <div className="ai-msg-row">
                        <div className="ai-msg-avatar-sm assistant">🧠</div>
                        <div className="ai-typing">
                            <div className="dot" /><div className="dot" /><div className="dot" />
                        </div>
                    </div>
                )}
            </div>

            {/* Quick actions */}
            <div style={{
                display: 'flex', gap: 4, padding: '6px 16px', overflowX: 'auto',
                flexShrink: 0, scrollbarWidth: 'none'
            }}>
                {QUICK_ACTIONS.map(action => (
                    <button key={action.label} onClick={() => handleQuickAction(action.label)} style={{
                        display: 'flex', alignItems: 'center', gap: 4, padding: '4px 10px',
                        borderRadius: 16, border: '1px solid var(--border)', background: 'var(--bg-dark)',
                        color: 'var(--text-muted)', fontSize: 11, fontWeight: 600, cursor: 'pointer',
                        whiteSpace: 'nowrap', fontFamily: 'inherit', transition: 'all 0.12s'
                    }}
                        onMouseEnter={e => {
                            e.currentTarget.style.borderColor = action.color
                            e.currentTarget.style.color = action.color
                        }}
                        onMouseLeave={e => {
                            e.currentTarget.style.borderColor = 'var(--border)'
                            e.currentTarget.style.color = 'var(--text-muted)'
                        }}
                    >
                        <Zap size={10} /> {action.label}
                    </button>
                ))}
            </div>

            {/* Input */}
            <div className="ai-input-area">
                <div className="ai-input-box">
                    <button className="ai-input-action" title="Attach file">
                        <Paperclip size={15} />
                    </button>
                    <input
                        type="text"
                        className="ai-input-field"
                        placeholder={serverStatus === 'online' ? 'Ask AI anything...' : 'AI Engine offline — type a message...'}
                        value={input}
                        onChange={e => setInput(e.target.value)}
                        onKeyDown={e => e.key === 'Enter' && !e.shiftKey && sendMessage()}
                    />
                    {isTyping ? (
                        <button className="ai-send-btn" onClick={() => setIsTyping(false)}>
                            <Square size={14} fill="currentColor" />
                        </button>
                    ) : (
                        <button className="ai-send-btn" onClick={sendMessage} disabled={!input.trim()}>
                            <ArrowUp size={16} />
                        </button>
                    )}
                </div>
                <div style={{
                    textAlign: 'center', fontSize: 10, color: 'var(--text-faint)', marginTop: 6
                }}>
                    Neural Studio AI · Powered by 1,046 compression advisors
                </div>
            </div>
        </div>
    )
}
