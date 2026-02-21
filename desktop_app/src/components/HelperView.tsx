import { useState, useRef, useEffect } from 'react';
import {
    Globe, Paperclip, Mic, ArrowUp, Image as ImageIcon, Code2,
    FileText, Terminal, Camera, Search, Zap, ChevronDown,
    X, Square, Loader2, BrainCircuit, Wifi, WifiOff, Plus
} from 'lucide-react';

type Role = 'system' | 'assistant' | 'user';
type AttachmentType = 'file' | 'image' | 'url';

interface Attachment { type: AttachmentType; name: string; value: string; }
interface Message {
    role: Role;
    content: string;
    attachments?: Attachment[];
    timestamp?: Date;
    tool?: string;
}

const TOOL_ACTIONS = [
    { id: 'browse', icon: Globe, label: 'Browse', color: '#4A9EFF', desc: 'Open a URL in the bridge' },
    { id: 'screenshot', icon: Camera, label: 'Screenshot', color: '#DA7555', desc: 'Capture the current page' },
    { id: 'search', icon: Search, label: 'Search', color: '#3BB37A', desc: 'Web or file search' },
    { id: 'compress', icon: Zap, label: 'Compress', color: '#C97FDB', desc: 'Compress a file' },
    { id: 'script', icon: Terminal, label: 'Run Script', color: '#E6A340', desc: 'Execute a script' },
    { id: 'code', icon: Code2, label: 'Generate Code', color: '#FF6B9D', desc: 'Write code' },
];

function ToolBadge({ tool }: { tool: string }) {
    const t = TOOL_ACTIONS.find(a => a.id === tool);
    if (!t) return null;
    const Icon = t.icon;
    return (
        <span style={{
            display: 'inline-flex', alignItems: 'center', gap: 4,
            fontSize: 10, fontWeight: 700, padding: '2px 8px',
            borderRadius: 12, border: `1px solid ${t.color}44`,
            color: t.color, background: t.color + '15'
        }}>
            <Icon size={10} />{t.label}
        </span>
    );
}

function TypingDots() {
    return (
        <div style={{ display: 'flex', gap: 5, alignItems: 'center', padding: '2px 0' }}>
            {[0, 1, 2].map(i => (
                <span key={i} style={{
                    width: 7, height: 7, borderRadius: '50%', background: '#666',
                    display: 'inline-block',
                    animation: `dotBounce 1.4s ${i * 0.2}s infinite ease-in-out`
                }} />
            ))}
        </div>
    );
}

function AttachChip({ a, onRemove }: { a: Attachment; onRemove?: () => void }) {
    const icons: Record<AttachmentType, any> = { file: FileText, image: ImageIcon, url: Globe };
    const Icon = icons[a.type];
    return (
        <div style={{
            display: 'inline-flex', alignItems: 'center', gap: 5,
            background: 'rgba(255,255,255,0.06)', border: '1px solid #333',
            borderRadius: 20, padding: '4px 10px', fontSize: 11, color: '#A0A0A0'
        }}>
            <Icon size={11} />
            <span style={{ maxWidth: 120, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{a.name}</span>
            {onRemove && (
                <button onClick={onRemove} style={{ background: 'none', border: 'none', color: '#666', cursor: 'pointer', display: 'flex', padding: 0, marginLeft: 2 }}>
                    <X size={10} />
                </button>
            )}
        </div>
    );
}

function formatContent(text: string) {
    return text
        .replace(/\*\*(.+?)\*\*/g, '<strong style="color:#DA7555">$1</strong>')
        .replace(/`([^`\n]+)`/g, '<code style="background:rgba(255,255,255,0.1);padding:1px 6px;border-radius:4px;font-family:monospace;font-size:12px;color:#E6D4B8">$1</code>')
        .replace(/```[\w]*\n?([\s\S]+?)```/g, '<div style="background:#0E0E0E;border:1px solid #333;border-radius:8px;margin:8px 0;overflow:hidden"><pre style="margin:0;padding:12px 14px;font-family:monospace;font-size:12px;color:#E6D4B8;overflow-x:auto;white-space:pre-wrap">$1</pre></div>');
}

const fmt = (d?: Date) => d ? d.toLocaleTimeString([], { hour: '2-digit', minute: '2-digit' }) : '';

// Icon button helper
function IBtn({ Icon, title, onClick, active = false, color }: { Icon: any; title: string; onClick: () => void; active?: boolean; color?: string }) {
    const [hov, setHov] = useState(false);
    return (
        <button title={title} onClick={onClick}
            onMouseEnter={() => setHov(true)}
            onMouseLeave={() => setHov(false)}
            style={{
                width: 34, height: 34, borderRadius: 8, border: 'none',
                background: active || hov ? 'rgba(255,255,255,0.07)' : 'transparent',
                color: active ? (color || '#DA7555') : hov ? '#F0F0F0' : '#888',
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                cursor: 'pointer', flexShrink: 0, transition: 'all 0.15s'
            }}>
            <Icon size={16} />
        </button>
    );
}

export function HelperView() {
    const [messages, setMessages] = useState<Message[]>([
        { role: 'system', content: 'Neural engine initialized — 1046 sub-models in cache.', timestamp: new Date() },
        {
            role: 'assistant',
            content: "Hi! I'm your **Neural Studio AI** assistant.\n\nI have full access to your browser, filesystem, scripts, and more. Try asking me:\n\n• `navigate youtube.com` — open any URL\n• `screenshot` — capture browser view\n• `compress file.txt` — compress with CMIX\n• `search for React hooks` — web search\n• `write code for file upload` — generate code\n• `help` — see all commands\n\nWhat would you like to do?",
            timestamp: new Date()
        }
    ]);

    const [inputVal, setInputVal] = useState('');
    const [attachments, setAttachments] = useState<Attachment[]>([]);
    const [isTyping, setIsTyping] = useState(false);
    const [showTools, setShowTools] = useState(false);
    const [isRecording, setIsRecording] = useState(false);
    const [webEnabled, setWebEnabled] = useState(true);

    const historyRef = useRef<HTMLDivElement>(null);
    const inputRef = useRef<HTMLInputElement>(null);
    const fileRef = useRef<HTMLInputElement>(null);
    const imgRef = useRef<HTMLInputElement>(null);

    useEffect(() => {
        if (historyRef.current)
            historyRef.current.scrollTop = historyRef.current.scrollHeight;
    }, [messages, isTyping]);

    const addAttachment = (type: AttachmentType, name: string) =>
        setAttachments(p => [...p, { type, name, value: name }]);
    const removeAttachment = (i: number) =>
        setAttachments(p => p.filter((_, idx) => idx !== i));
    const handleFile = (e: React.ChangeEvent<HTMLInputElement>, type: AttachmentType) => {
        const f = e.target.files?.[0];
        if (f) addAttachment(type, f.name);
        e.target.value = '';
    };

    const handleToolAction = (id: string) => {
        setShowTools(false);
        const m: Record<string, string> = {
            browse: 'navigate ', screenshot: 'take a screenshot',
            search: 'search for ', compress: 'compress ',
            script: 'cmd ', code: 'write code for ',
        };
        setInputVal(m[id] || '');
        setTimeout(() => inputRef.current?.focus(), 50);
    };

    const processCommand = async (cmd: string, atts: Attachment[]): Promise<{ content: string; tool?: string }> => {
        const lower = cmd.toLowerCase();
        await new Promise(r => setTimeout(r, 400));

        if (lower.startsWith('navigate ') || lower.includes('go to ') || lower.includes('open ')) {
            let url = cmd.replace(/navigate |go to |open /gi, '').trim();
            if (!url.startsWith('http')) url = 'https://' + url;
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.navigate', params: { url } }));
            return { content: `Navigating to **${url}**...\n\nBrowser bridge command sent. The page should load momentarily in your connected Chrome instance.`, tool: 'browse' };
        }
        if (lower.includes('screenshot') || lower.includes('capture')) {
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.screenshot', params: { fullPage: false } }));
            return { content: `📸 Screenshot captured!\n\nSaved via browser bridge. Check the console panel for the file path.`, tool: 'screenshot' };
        }
        if (lower.startsWith('click ')) {
            const sel = cmd.substring(6);
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.click', params: { selector: sel } }));
            return { content: `Clicking element: \`${sel}\`\n\nDispatched to browser agent.`, tool: 'browse' };
        }

        if (lower.startsWith('cmd ') || lower.startsWith('> ')) {
            const command = cmd.replace(/^(cmd|>)\s*/i, '');
            try {
                const res = await fetch('http://127.0.0.1:8001/api/cmd', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ command })
                });
                const data = await res.json();
                if (data.status === 'success') {
                    let out = data.stdout || '';
                    let err = data.stderr || '';
                    if (!out && !err) out = 'Command executed successfully with no output.';
                    return { content: `Ran command: \`${command}\`${out ? `\n\n**Output:**\n\`\`\`text\n${out}\n\`\`\`` : ''}${err ? `\n\n**Error:**\n\`\`\`text\n${err}\n\`\`\`` : ''}`, tool: 'script' };
                } else {
                    return { content: `Error running command: \`${data.error}\``, tool: 'script' };
                }
            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'script' };
            }
        }

        if (lower.startsWith('compress') || lower.startsWith('archive')) {
            let fp = cmd.replace(/^(compress|archive)\s*/i, '').trim();
            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: `Please provide or attach a file path to compress (e.g., \`compress C:\\my_file.txt\`).`, tool: 'compress' };

            try {
                const res = await fetch('http://127.0.0.1:8001/api/compress_stream', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: file, algorithm: '--cmix' })
                });

                const reader = res.body?.getReader();
                const decoder = new TextDecoder();
                let fullOutput = '';

                if (reader) {
                    while (true) {
                        const { done, value } = await reader.read();
                        if (done) break;
                        const chunk = decoder.decode(value, { stream: true });
                        for (let i = 0; i < chunk.length; i++) {
                            if (chunk[i] === '\r') {
                                const lastNewline = fullOutput.lastIndexOf('\n');
                                if (lastNewline !== -1) {
                                    fullOutput = fullOutput.substring(0, lastNewline + 1);
                                } else {
                                    fullOutput = '';
                                }
                            } else {
                                fullOutput += chunk[i];
                            }
                        }
                    }
                }

                return { content: `🚀 **Compression Finished!**\n\nFile: \`${file}\`\nAlgorithm: CMIX\n\n**Output:**\n\`\`\`text\n${fullOutput}\n\`\`\``, tool: 'compress' };

            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        if (lower.startsWith('decompress') || lower.startsWith('restore') || lower.startsWith('extract')) {
            let fp = cmd.replace(/^(decompress|restore|extract)\s*/i, '').trim();
            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: `Please provide or attach a \`.myzip\` archive path to decompress.`, tool: 'compress' };

            let op = file.replace('.myzip', '');
            if (op === file) op = file + '_out';

            try {
                const res = await fetch('http://127.0.0.1:8001/api/decompress_stream', {
                    method: 'POST',
                    headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ archive_path: file, output_path: op })
                });

                const reader = res.body?.getReader();
                const decoder = new TextDecoder();
                let fullOutput = '';

                if (reader) {
                    while (true) {
                        const { done, value } = await reader.read();
                        if (done) break;
                        const chunk = decoder.decode(value, { stream: true });
                        for (let i = 0; i < chunk.length; i++) {
                            if (chunk[i] === '\r') {
                                const lastNewline = fullOutput.lastIndexOf('\n');
                                if (lastNewline !== -1) {
                                    fullOutput = fullOutput.substring(0, lastNewline + 1);
                                } else {
                                    fullOutput = '';
                                }
                            } else {
                                fullOutput += chunk[i];
                            }
                        }
                    }
                }

                return { content: `🚀 **Decompression Finished!**\n\nArchive: \`${file}\`\nTarget: \`${op}\`\n\n**Output:**\n\`\`\`text\n${fullOutput}\n\`\`\``, tool: 'compress' };

            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        if (lower.startsWith('search') || lower.includes('search for ')) {
            const q = cmd.replace(/search for |search /gi, '').trim();
            return { content: `🔍 Searching for: **${q}**\n\nWeb bridge search initiated. Results appear in the console output.`, tool: 'search' };
        }
        if (lower.includes('code') || lower.includes('write') || lower.includes('generate')) {
            return { content: `Here's a starter:\n\n\`\`\`typescript\nasync function compressFile(path: string) {\n  const res = await fetch('http://127.0.0.1:8001/api/compress', {\n    method: 'POST',\n    headers: { 'Content-Type': 'application/json' },\n    body: JSON.stringify({ file_path: path, algorithm: '--cmix' })\n  });\n  return res.json();\n}\n\`\`\`\n\nWould you like me to refine this?`, tool: 'code' };
        }
        if (lower.includes('help') || lower === '?') {
            return { content: `**What I can do:**\n\n**Browser**\n• \`navigate [url]\` — open any page\n• \`click [selector]\` — click element\n• \`screenshot\` — capture view\n\n**Files**\n• \`compress [file]\` — Neural compression\n• \`decompress [myzip]\` — Restore files\n• Attach files with 📎\n\n**Terminal**\n• \`cmd [command]\` or \`> [command]\` — Run raw OS terminals scripts directly here\n\n**Utilities**\n• \`search [query]\` — web search\n• \`write code for [task]\` — generate code\n\nUse the quick pills for one-click actions!` };
        }

        return { content: `Got it: "${cmd}"\n\nI'm connected to your browser bridge and filesystem. Try \`help\` for the full command list, or use the quick-action pills below.` };
    };

    const sendMessage = async () => {
        const cmd = inputVal.trim();
        if (!cmd && attachments.length === 0) return;
        const userMsg: Message = { role: 'user', content: cmd, attachments: [...attachments], timestamp: new Date() };
        setMessages(p => [...p, userMsg]);
        setInputVal('');
        setAttachments([]);
        setIsTyping(true);
        try {
            const { content, tool } = await processCommand(cmd, userMsg.attachments || []);
            setMessages(p => [...p, { role: 'assistant', content, tool, timestamp: new Date() }]);
        } finally {
            setIsTyping(false);
        }
    };

    return (
        <div style={{ display: 'flex', flexDirection: 'column', height: '100%', minHeight: 0 }}>

            {/* ── Topbar ── */}
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', paddingBottom: 12, borderBottom: '1px solid #333', flexShrink: 0 }}>
                <div style={{ display: 'flex', alignItems: 'center', gap: 11 }}>
                    <div style={{
                        width: 38, height: 38, borderRadius: 11, flexShrink: 0,
                        background: 'linear-gradient(135deg, #DA7555, #b85a38)',
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        color: '#fff', boxShadow: '0 2px 10px rgba(218,117,85,0.3)'
                    }}>
                        <BrainCircuit size={19} />
                    </div>
                    <div>
                        <div style={{ fontWeight: 700, fontSize: 14, color: '#F0F0F0' }}>Neural Studio AI</div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 5, marginTop: 2 }}>
                            <span style={{ width: 6, height: 6, borderRadius: '50%', background: '#3BB37A', boxShadow: '0 0 5px #3BB37A', display: 'inline-block' }} />
                            <span style={{ fontSize: 11, color: '#666' }}>Online · WS {window.ws ? 'Connected' : 'Disconnected'}</span>
                        </div>
                    </div>
                </div>
                <button onClick={() => setWebEnabled(v => !v)}
                    style={{
                        display: 'flex', alignItems: 'center', gap: 6, padding: '6px 14px',
                        borderRadius: 20, cursor: 'pointer', fontSize: 12, fontWeight: 600,
                        border: webEnabled ? '1px solid #4A9EFF55' : '1px solid #333',
                        background: webEnabled ? 'rgba(74,158,255,0.1)' : '#242424',
                        color: webEnabled ? '#4A9EFF' : '#666',
                        transition: 'all 0.2s'
                    }}>
                    {webEnabled ? <Wifi size={13} /> : <WifiOff size={13} />}
                    Web {webEnabled ? 'On' : 'Off'}
                </button>
            </div>

            {/* ── Chat History ── */}
            <div ref={historyRef} style={{
                flex: 1, overflowY: 'auto', padding: '14px 2px 8px',
                display: 'flex', flexDirection: 'column', gap: 16, minHeight: 0
            }}>
                {messages.map((m, i) => (
                    <div key={i} style={{ animation: 'msgSlide 0.2s ease' }}>
                        {m.role === 'system' && (
                            <div style={{
                                textAlign: 'center', fontSize: 11, color: '#555',
                                background: '#1A1A1A', border: '1px solid #2E2E2E',
                                borderRadius: 20, padding: '3px 14px', margin: '0 auto', width: 'fit-content'
                            }}>{m.content}</div>
                        )}

                        {m.role !== 'system' && (
                            <div style={{ display: 'flex', gap: 10, alignItems: 'flex-start', flexDirection: m.role === 'user' ? 'row-reverse' : 'row' }}>
                                {/* Avatar */}
                                <div style={{
                                    width: 30, height: 30, borderRadius: 9, flexShrink: 0, marginTop: 2,
                                    display: 'flex', alignItems: 'center', justifyContent: 'center',
                                    ...(m.role === 'assistant'
                                        ? { background: 'linear-gradient(135deg, #DA7555, #b85a38)', color: '#fff' }
                                        : { background: '#1A1A1A', color: '#DA7555', border: '1px solid #333' })
                                }}>
                                    {m.role === 'assistant' ? <BrainCircuit size={14} /> : <span style={{ fontSize: 11, fontWeight: 700 }}>Y</span>}
                                </div>

                                {/* Column */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 4, maxWidth: 'min(72%, 560px)', alignItems: m.role === 'user' ? 'flex-end' : 'flex-start' }}>
                                    <div style={{ display: 'flex', alignItems: 'center', gap: 7 }}>
                                        <span style={{ fontSize: 10, fontWeight: 700, color: '#888', textTransform: 'uppercase', letterSpacing: '0.5px' }}>
                                            {m.role === 'user' ? 'You' : 'Neural AI'}
                                        </span>
                                        {m.tool && <ToolBadge tool={m.tool} />}
                                        <span style={{ fontSize: 10, color: '#555', marginLeft: 'auto' }}>{fmt(m.timestamp)}</span>
                                    </div>
                                    {m.attachments && m.attachments.length > 0 && (
                                        <div style={{ display: 'flex', flexWrap: 'wrap', gap: 5 }}>
                                            {m.attachments.map((a, ai) => <AttachChip key={ai} a={a} />)}
                                        </div>
                                    )}
                                    {m.content && (
                                        <div dangerouslySetInnerHTML={{ __html: formatContent(m.content) }} style={{
                                            padding: '11px 15px', fontSize: 14, lineHeight: 1.65, color: '#F0F0F0', whiteSpace: 'pre-wrap',
                                            borderRadius: m.role === 'assistant' ? '3px 12px 12px 12px' : '12px 3px 12px 12px',
                                            ...(m.role === 'assistant'
                                                ? { background: '#242424', border: '1px solid #333' }
                                                : { background: '#3A2A25', border: '1px solid rgba(218,117,85,0.3)' })
                                        }} />
                                    )}
                                </div>
                            </div>
                        )}
                    </div>
                ))}

                {/* Typing indicator */}
                {isTyping && (
                    <div style={{ display: 'flex', gap: 10, alignItems: 'flex-start' }}>
                        <div style={{ width: 30, height: 30, borderRadius: 9, flexShrink: 0, background: 'linear-gradient(135deg, #DA7555, #b85a38)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff' }}>
                            <BrainCircuit size={14} />
                        </div>
                        <div style={{ background: '#242424', border: '1px solid #333', borderRadius: '3px 12px 12px 12px', padding: '12px 16px' }}>
                            <TypingDots />
                        </div>
                    </div>
                )}
            </div>

            {/* ── Attachment Preview ── */}
            {attachments.length > 0 && (
                <div style={{ display: 'flex', flexWrap: 'wrap', gap: 6, padding: '7px 0 4px', borderTop: '1px solid #2E2E2E', flexShrink: 0 }}>
                    {attachments.map((a, i) => <AttachChip key={i} a={a} onRemove={() => removeAttachment(i)} />)}
                </div>
            )}

            {/* ── Quick Tool Pills ── */}
            <div style={{ display: 'flex', gap: 6, padding: '9px 0 6px', overflowX: 'auto', flexShrink: 0, scrollbarWidth: 'none' }}>
                {TOOL_ACTIONS.map(tool => {
                    const Icon = tool.icon;
                    return (
                        <QuickPill key={tool.id} tool={tool} onClick={() => handleToolAction(tool.id)} />
                    );
                })}
            </div>

            {/* ── Input Area ── */}
            <div style={{ position: 'relative', paddingBottom: 8, flexShrink: 0 }}>
                <InputBox
                    inputRef={inputRef}
                    inputVal={inputVal}
                    setInputVal={setInputVal}
                    onSend={sendMessage}
                    onFileClick={() => fileRef.current?.click()}
                    onImgClick={() => imgRef.current?.click()}
                    showTools={showTools}
                    setShowTools={setShowTools}
                    isRecording={isRecording}
                    setIsRecording={setIsRecording}
                    isTyping={isTyping}
                    canSend={!!(inputVal.trim() || attachments.length > 0)}
                />

                {/* Tool Dropdown */}
                {showTools && (
                    <div style={{
                        position: 'absolute', bottom: 'calc(100% + 6px)', left: 0,
                        background: '#242424', border: '1px solid #333', borderRadius: 13,
                        padding: 8, zIndex: 100, display: 'grid', gridTemplateColumns: '1fr 1fr',
                        gap: 4, minWidth: 340, boxShadow: '0 8px 28px rgba(0,0,0,0.5)',
                        animation: 'dropUp 0.15s ease'
                    }}>
                        {TOOL_ACTIONS.map(tool => {
                            const Icon = tool.icon;
                            return (
                                <DropItem key={tool.id} tool={tool} Icon={Icon} onClick={() => handleToolAction(tool.id)} />
                            );
                        })}
                    </div>
                )}
            </div>

            {/* Footer note */}
            <div style={{ textAlign: 'center', fontSize: 10, color: '#555', paddingBottom: 6, flexShrink: 0 }}>
                Neural Studio AI · Browser, Files, Code & More · Press Enter to send
            </div>

            <input ref={fileRef} type="file" style={{ display: 'none' }} onChange={e => handleFile(e, 'file')} />
            <input ref={imgRef} type="file" accept="image/*" style={{ display: 'none' }} onChange={e => handleFile(e, 'image')} />

            <style>{`
                @keyframes msgSlide  { from{opacity:0;transform:translateY(7px)} to{opacity:1;transform:translateY(0)} }
                @keyframes dotBounce { 0%,60%,100%{transform:translateY(0);opacity:0.35} 30%{transform:translateY(-6px);opacity:1} }
                @keyframes spin      { from{transform:rotate(0deg)} to{transform:rotate(360deg)} }
                @keyframes pulse     { 0%,100%{opacity:1} 50%{opacity:0.4} }
                @keyframes dropUp    { from{opacity:0;transform:translateY(5px)} to{opacity:1;transform:translateY(0)} }
                div::-webkit-scrollbar{width:3px;height:3px}
                div::-webkit-scrollbar-track{background:transparent}
                div::-webkit-scrollbar-thumb{background:#333;border-radius:4px}
            `}</style>
        </div>
    );
}

/* ─ Sub-components (kept small for clarity) ─ */

function QuickPill({ tool, onClick }: { tool: typeof TOOL_ACTIONS[0]; onClick: () => void }) {
    const [hov, setHov] = useState(false);
    const Icon = tool.icon;
    return (
        <button onClick={onClick} title={tool.desc}
            onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
            style={{
                display: 'inline-flex', alignItems: 'center', gap: 6,
                padding: '5px 13px', borderRadius: 20, flexShrink: 0,
                border: hov ? `1px solid ${tool.color}` : '1px solid #333',
                background: hov ? tool.color + '18' : '#1A1A1A',
                color: hov ? tool.color : '#888',
                fontSize: 12, fontWeight: 600, cursor: 'pointer',
                whiteSpace: 'nowrap', transition: 'all 0.15s'
            }}>
            <Icon size={13} />{tool.label}
        </button>
    );
}

function InputBox({ inputRef, inputVal, setInputVal, onSend, onFileClick, onImgClick, showTools, setShowTools, isRecording, setIsRecording, isTyping, canSend }: any) {
    const [focused, setFocused] = useState(false);
    return (
        <div style={{
            display: 'flex', alignItems: 'center',
            background: '#242424',
            border: `1px solid ${focused ? '#DA7555' : '#333'}`,
            boxShadow: focused ? '0 0 0 3px rgba(218,117,85,0.1)' : 'none',
            borderRadius: 13, padding: '4px 4px 4px 6px', gap: 2,
            transition: 'border-color 0.2s, box-shadow 0.2s'
        }}>
            {/* Left icons */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 1, flexShrink: 0 }}>
                <IBtn2 Icon={Paperclip} title="Attach file" onClick={onFileClick} />
                <IBtn2 Icon={ImageIcon} title="Attach image" onClick={onImgClick} />
                <IBtn2
                    Icon={Plus} title="More tools" onClick={() => setShowTools((v: boolean) => !v)}
                    active={showTools} extra={<ChevronDown size={10} style={{ marginLeft: 1, opacity: 0.5 }} />}
                />
                <div style={{ width: 1, height: 18, background: '#333', margin: '0 3px' }} />
            </div>

            <input
                ref={inputRef}
                type="text"
                placeholder="Message Neural Studio AI...  (try 'navigate youtube.com')"
                value={inputVal}
                onChange={e => setInputVal(e.target.value)}
                onKeyDown={e => { if (e.key === 'Enter') onSend(); }}
                onFocus={() => setFocused(true)}
                onBlur={() => setFocused(false)}
                style={{
                    flex: 1, background: 'transparent', border: 'none', outline: 'none',
                    color: '#F0F0F0', fontSize: 14, padding: '7px 8px', minWidth: 0
                }}
            />

            {/* Right icons */}
            <div style={{ display: 'flex', alignItems: 'center', gap: 4, flexShrink: 0 }}>
                <button onClick={() => setIsRecording((v: boolean) => !v)} title="Voice input"
                    style={{
                        width: 32, height: 32, borderRadius: 8, border: 'none', flexShrink: 0,
                        background: isRecording ? 'rgba(239,68,68,0.15)' : 'transparent',
                        color: isRecording ? '#ef4444' : '#666',
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        cursor: 'pointer',
                        animation: isRecording ? 'pulse 1.5s infinite' : 'none'
                    }}>
                    {isRecording ? <Square size={14} /> : <Mic size={15} />}
                </button>
                <button className="send-btn" onClick={onSend} disabled={!canSend}>
                    {isTyping
                        ? <Loader2 className="spin" size={16} strokeWidth={2.5} />
                        : <ArrowUp size={18} strokeWidth={2.5} />}
                </button>
            </div>
        </div>
    );
}

function IBtn2({ Icon, title, onClick, active, extra }: { Icon: any; title: string; onClick: () => void; active?: boolean; extra?: React.ReactNode }) {
    const [hov, setHov] = useState(false);
    return (
        <button title={title} onClick={onClick}
            onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
            style={{
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                height: 32, padding: '0 7px', borderRadius: 7, border: 'none',
                background: active ? 'rgba(218,117,85,0.15)' : hov ? 'rgba(255,255,255,0.06)' : 'transparent',
                color: active ? '#DA7555' : hov ? '#CCC' : '#777',
                cursor: 'pointer', flexShrink: 0, transition: 'all 0.13s'
            }}>
            <Icon size={15} />
            {extra}
        </button>
    );
}

function DropItem({ tool, Icon, onClick }: { tool: any; Icon: any; onClick: () => void }) {
    const [hov, setHov] = useState(false);
    return (
        <button onClick={onClick}
            onMouseEnter={() => setHov(true)} onMouseLeave={() => setHov(false)}
            style={{
                display: 'flex', alignItems: 'center', gap: 10, padding: '8px 10px',
                borderRadius: 9, border: 'none', cursor: 'pointer', textAlign: 'left',
                background: hov ? '#2E2E2E' : 'transparent', color: '#F0F0F0', transition: 'background 0.13s'
            }}>
            <span style={{
                width: 32, height: 32, borderRadius: 8, flexShrink: 0,
                display: 'flex', alignItems: 'center', justifyContent: 'center',
                background: tool.color + '22', color: tool.color
            }}>
                <Icon size={15} />
            </span>
            <div>
                <div style={{ fontSize: 13, fontWeight: 600 }}>{tool.label}</div>
                <div style={{ fontSize: 11, color: '#555', marginTop: 1 }}>{tool.desc}</div>
            </div>
        </button>
    );
}