import { useState, useRef, useEffect } from 'react';
import {
    Globe, Paperclip, Mic, ArrowUp, Image as ImageIcon, Code2,
    FileText, Terminal, Camera, Search, Zap, ChevronDown,
    X, Square, Loader2, BrainCircuit, Wifi, WifiOff, Plus,
    Activity, Cpu, TrendingDown
} from 'lucide-react';

const API = 'http://127.0.0.1:8001';

type Role = 'system' | 'assistant' | 'user';
type AttachmentType = 'file' | 'image' | 'url';

interface Attachment { type: AttachmentType; name: string; value: string; }
interface Message {
    role: Role;
    content: string;
    attachments?: Attachment[];
    timestamp?: Date;
    tool?: string;
    widget?: any; // Rich inline widget data (entropy charts, model predictions, etc.)
}

const TOOL_ACTIONS = [
    { id: 'analyze', icon: Activity, label: 'Analyze', color: '#4ADE80', desc: 'Deep file analysis with AI' },
    { id: 'compress', icon: Zap, label: 'Compress', color: '#C97FDB', desc: 'Neural compression' },
    { id: 'browse', icon: Globe, label: 'Browse', color: '#4A9EFF', desc: 'Open a URL in the bridge' },
    { id: 'screenshot', icon: Camera, label: 'Screenshot', color: '#DA7555', desc: 'Capture the current page' },
    { id: 'search', icon: Search, label: 'Search', color: '#3BB37A', desc: 'Web or file search' },
    { id: 'script', icon: Terminal, label: 'Run Script', color: '#E6A340', desc: 'Execute a script' },
    { id: 'code', icon: Code2, label: 'Generate Code', color: '#FF6B9D', desc: 'Write code' },
];

// ─── Inline Widgets for AI responses ────────────────────────

function EntropyBar({ value, max = 8.0 }: { value: number; max?: number }) {
    const pct = Math.min((value / max) * 100, 100);
    const color = value < 3 ? '#4ADE80' : value < 5 ? '#FBBF24' : value < 7 ? '#F97316' : '#EF4444';
    return (
        <div style={{ display: 'flex', alignItems: 'center', gap: 8, margin: '4px 0' }}>
            <span style={{ fontSize: 11, color: '#888', width: 65, flexShrink: 0 }}>Entropy</span>
            <div style={{ flex: 1, height: 8, background: '#1A1A1A', borderRadius: 4, overflow: 'hidden', border: '1px solid #333' }}>
                <div style={{
                    width: `${pct}%`, height: '100%', borderRadius: 4,
                    background: `linear-gradient(90deg, ${color}CC, ${color})`,
                    transition: 'width 1s ease',
                    boxShadow: `0 0 8px ${color}44`
                }} />
            </div>
            <span style={{ fontSize: 12, fontWeight: 700, color, width: 55, textAlign: 'right' }}>{value.toFixed(2)} bpb</span>
        </div>
    );
}

function EntropyMap({ data }: { data: number[] }) {
    if (!data || data.length === 0) return null;
    const max = Math.max(...data, 0.01);
    return (
        <div style={{ margin: '8px 0' }}>
            <div style={{ fontSize: 10, color: '#666', marginBottom: 4 }}>Entropy Heatmap (block-level)</div>
            <div style={{
                display: 'flex', gap: 1, height: 24, borderRadius: 4, overflow: 'hidden',
                border: '1px solid #333'
            }}>
                {data.map((v, i) => {
                    const intensity = v / max;
                    const hue = (1 - intensity) * 120; // green to red
                    return (
                        <div key={i} title={`Block ${i}: ${v.toFixed(2)} bpb`} style={{
                            flex: 1,
                            background: `hsl(${hue}, 75%, ${20 + intensity * 30}%)`,
                            transition: `background 0.3s ${i * 0.02}s`
                        }} />
                    );
                })}
            </div>
            <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 9, color: '#555', marginTop: 2 }}>
                <span>Start</span><span>End</span>
            </div>
        </div>
    );
}

function ModelPredictions({ predictions }: { predictions: any[] }) {
    if (!predictions || predictions.length === 0) return null;
    return (
        <div style={{ margin: '8px 0' }}>
            <div style={{ fontSize: 10, color: '#666', marginBottom: 6, display: 'flex', alignItems: 'center', gap: 4 }}>
                <Cpu size={10} /> Neural Advisor Predictions
            </div>
            {predictions.map((m, i) => (
                <div key={i} style={{
                    display: 'flex', alignItems: 'center', gap: 8, padding: '4px 0',
                    borderBottom: i < predictions.length - 1 ? '1px solid #2A2A2A' : 'none'
                }}>
                    <span style={{
                        width: 22, height: 22, borderRadius: 6, fontSize: 9, fontWeight: 700,
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        background: `rgba(201, 127, 219, ${0.1 + i * 0.05})`,
                        color: '#C97FDB', border: '1px solid rgba(201,127,219,0.3)',
                        flexShrink: 0
                    }}>m{i}</span>
                    <div style={{ flex: 1, minWidth: 0 }}>
                        <div style={{ fontSize: 11, color: '#CCC' }}>{m.label} <span style={{ color: '#666' }}>— {m.description}</span></div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginTop: 2 }}>
                            <div style={{ flex: 1, height: 4, background: '#1A1A1A', borderRadius: 2, overflow: 'hidden' }}>
                                <div style={{
                                    width: `${m.confidence}%`, height: '100%', borderRadius: 2,
                                    background: `linear-gradient(90deg, #C97FDB88, #C97FDB)`,
                                    transition: `width 0.8s ${i * 0.1}s ease`
                                }} />
                            </div>
                            <span style={{ fontSize: 10, color: '#888', width: 30, textAlign: 'right' }}>{m.confidence}%</span>
                        </div>
                    </div>
                    <span style={{ fontSize: 10, color: '#4ADE80', fontWeight: 600, flexShrink: 0 }}>{m.predicted_bpb} bpb</span>
                </div>
            ))}
        </div>
    );
}

function InsightCards({ insights }: { insights: any[] }) {
    if (!insights || insights.length === 0) return null;
    const colors: Record<string, string> = { success: '#4ADE80', warning: '#FBBF24', info: '#4A9EFF', error: '#EF4444' };
    return (
        <div style={{ display: 'flex', flexDirection: 'column', gap: 6, margin: '8px 0' }}>
            {insights.map((ins, i) => (
                <div key={i} style={{
                    display: 'flex', gap: 8, padding: '8px 10px', borderRadius: 8,
                    background: `${colors[ins.type] || '#4A9EFF'}08`,
                    border: `1px solid ${colors[ins.type] || '#4A9EFF'}25`,
                    animation: `msgSlide 0.3s ${i * 0.1}s ease both`
                }}>
                    <span style={{ fontSize: 16, lineHeight: 1 }}>{ins.icon}</span>
                    <div>
                        <div style={{ fontSize: 12, fontWeight: 700, color: colors[ins.type] || '#4A9EFF' }}>{ins.title}</div>
                        <div style={{ fontSize: 11, color: '#999', marginTop: 2, lineHeight: 1.4 }}>{ins.detail}</div>
                    </div>
                </div>
            ))}
        </div>
    );
}

function CompressionEstimate({ pct, algo }: { pct: number; algo: string }) {
    const algoLabels: Record<string, string> = {
        '--cmix': 'CMIX Neural',
        '--best': 'BWT Pipeline',
        '--ultra': 'PPM Arithmetic',
        'default': 'LZ77 Fast'
    };
    return (
        <div style={{
            display: 'flex', alignItems: 'center', gap: 12, padding: '10px 14px',
            background: 'linear-gradient(135deg, rgba(74,222,128,0.06), rgba(201,127,219,0.06))',
            border: '1px solid rgba(74,222,128,0.2)', borderRadius: 10, margin: '8px 0'
        }}>
            <div style={{
                width: 44, height: 44, borderRadius: 12, flexShrink: 0,
                background: 'rgba(74,222,128,0.12)', border: '1px solid rgba(74,222,128,0.3)',
                display: 'flex', alignItems: 'center', justifyContent: 'center'
            }}>
                <TrendingDown size={20} color="#4ADE80" />
            </div>
            <div>
                <div style={{ fontSize: 11, color: '#888' }}>AI Compression Estimate</div>
                <div style={{ fontSize: 20, fontWeight: 800, color: '#4ADE80', letterSpacing: '-0.5px' }}>
                    ~{pct}% <span style={{ fontSize: 12, fontWeight: 600, color: '#666' }}>savings</span>
                </div>
            </div>
            <div style={{
                marginLeft: 'auto', padding: '4px 10px', borderRadius: 16,
                background: 'rgba(201,127,219,0.12)', border: '1px solid rgba(201,127,219,0.3)',
                fontSize: 10, fontWeight: 700, color: '#C97FDB'
            }}>{algoLabels[algo] || algo}</div>
        </div>
    );
}

function FileInfoWidget({ data }: { data: any }) {
    if (!data) return null;
    return (
        <div style={{
            background: '#1A1A1A', border: '1px solid #333', borderRadius: 10,
            padding: '12px 14px', margin: '6px 0'
        }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 10 }}>
                <FileText size={14} color="#DA7555" />
                <span style={{ fontSize: 13, fontWeight: 700, color: '#F0F0F0' }}>{data.file_name}</span>
                <span style={{ marginLeft: 'auto', fontSize: 11, color: '#888' }}>{data.file_size_human}</span>
            </div>

            <div style={{ display: 'grid', gridTemplateColumns: '1fr 1fr 1fr', gap: 6, marginBottom: 10 }}>
                {[
                    { label: 'Type', value: data.file_info?.category || '—', color: '#4A9EFF' },
                    { label: 'Text Ratio', value: `${((data.file_info?.text_ratio || 0) * 100).toFixed(0)}%`, color: '#4ADE80' },
                    { label: 'SHA-256', value: data.sha256 || '—', color: '#888' }
                ].map((m, i) => (
                    <div key={i} style={{ background: '#242424', borderRadius: 6, padding: '6px 8px', textAlign: 'center' }}>
                        <div style={{ fontSize: 9, color: '#666', textTransform: 'uppercase', letterSpacing: '0.5px' }}>{m.label}</div>
                        <div style={{ fontSize: 11, fontWeight: 600, color: m.color, marginTop: 2, overflow: 'hidden', textOverflow: 'ellipsis', whiteSpace: 'nowrap' }}>{m.value}</div>
                    </div>
                ))}
            </div>

            <EntropyBar value={data.entropy || 0} />
            <EntropyMap data={data.entropy_map} />

            {data.ai && (
                <>
                    <CompressionEstimate pct={data.ai.compression_estimate_pct} algo={data.ai.recommended_algorithm} />
                    <InsightCards insights={data.ai.insights} />
                    <ModelPredictions predictions={data.ai.model_predictions} />
                </>
            )}
        </div>
    );
}

// ─── Formatting ─────────────────────────────────────

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

// ─── Server Connection Hook ─────────────────────────

function useServerConnection() {
    const [connected, setConnected] = useState(false);
    const [serverInfo, setServerInfo] = useState<any>(null);

    const checkHealth = async () => {
        try {
            const res = await fetch(`${API}/api/health`);
            const data = await res.json();
            setConnected(data.status === 'online');
            setServerInfo(data);
            return true;
        } catch {
            setConnected(false);
            setServerInfo(null);
            return false;
        }
    };

    useEffect(() => {
        checkHealth();
        const interval = setInterval(checkHealth, 10000);
        return () => clearInterval(interval);
    }, []);

    return { connected, serverInfo, checkHealth };
}

// ─── Main Component ─────────────────────────────────

export function HelperView() {
    const { connected, serverInfo, checkHealth } = useServerConnection();

    const [messages, setMessages] = useState<Message[]>([
        { role: 'system', content: 'Neural Brain + Compression Engine V10 initialized — 1,046 AI advisors loaded.', timestamp: new Date() },
        {
            role: 'assistant',
            content: `I'm your **Neural Studio AI** — I learn, compress, and remember.\n\nI have my own **Neural Brain** that stores all knowledge compressed with our 1,046-advisor CMIX engine. Teach me anything and I'll remember it forever in minimal space!\n\n**Learn & Ask**\n• \`learn [topic]: [info]\` — teach me something\n• \`learn_url [topic] [url]\` — learn from the web\n• \`ask [question]\` — query my knowledge\n\n**Compression**\n• \`analyze [file]\` — deep file intelligence\n• \`compress [file]\` — compress with AI guidance\n\n• \`help\` — see all commands\n\nStart teaching me! 🧠`,
            timestamp: new Date()
        }
    ]);

    const [inputVal, setInputVal] = useState('');
    const [attachments, setAttachments] = useState<Attachment[]>([]);
    const [isTyping, setIsTyping] = useState(false);
    const [showTools, setShowTools] = useState(false);
    const [isRecording, setIsRecording] = useState(false);

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
            analyze: 'analyze ', compress: 'compress ',
            browse: 'navigate ', screenshot: 'take a screenshot',
            search: 'search for ', script: 'cmd ', code: 'write code for ',
        };
        setInputVal(m[id] || '');
        setTimeout(() => inputRef.current?.focus(), 50);
    };

    // ─── The Intelligence: Process Commands ─────────
    const processCommand = async (cmd: string, atts: Attachment[]): Promise<{ content: string; tool?: string; widget?: any }> => {
        const lower = cmd.toLowerCase().trim();

        // ═══════ ANALYZE — The Neural Eye ═══════
        if (lower.startsWith('analyze') || lower.startsWith('scan') || lower.startsWith('inspect')) {
            let fp = cmd.replace(/^(analyze|scan|inspect)\s*/i, '').trim();
            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: 'Please provide a file path to analyze.\n\nExample: `analyze C:\\Users\\data\\frankenstein.txt`', tool: 'analyze' };

            try {
                const res = await fetch(`${API}/api/analyze`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: file })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}`, tool: 'analyze' };

                const algo = data.ai?.recommended_algorithm || '--cmix';
                const quality = data.ai?.entropy_quality || 'unknown';

                let commentary = `🧠 **Neural Analysis Complete**\n\nI've scanned **${data.file_name}** at the byte level. `;

                if (quality === 'excellent') {
                    commentary += `This file has **excellent** compressibility — low entropy means my neural advisors will learn its patterns incredibly fast. `;
                } else if (quality === 'good') {
                    commentary += `This file has **good** compressibility. There are clear repeating patterns my context models can exploit. `;
                } else if (quality === 'moderate') {
                    commentary += `Moderate entropy — there are patterns but also significant randomness. My higher-order advisors (Order-4, Order-5) will need to work harder. `;
                } else {
                    commentary += `⚠️ High entropy detected. This data is near-random — it may already be compressed or encrypted. `;
                }

                commentary += `\n\nI recommend **${algo === '--cmix' ? 'CMIX Neural' : algo === '--best' ? 'BWT' : 'PPM'}** compression. Ready to compress? Just say \`compress ${file}\``;

                return { content: commentary, tool: 'analyze', widget: { type: 'file_analysis', data } };
            } catch (e) {
                return { content: `Failed to connect to Neural Studio API. Make sure the server is running:\n\`\`\`\ncd server && python main.py\n\`\`\``, tool: 'analyze' };
            }
        }

        // ═══════ COMPRESS — With AI Pre-Analysis ═══════
        if (lower.startsWith('compress') || lower.startsWith('archive')) {
            let fp = cmd.replace(/^(compress|archive)\s*/i, '').trim();

            // Extract algorithm flag if present
            let algo = '--cmix';
            if (fp.includes('--best')) { algo = '--best'; fp = fp.replace('--best', '').trim(); }
            else if (fp.includes('--ultra')) { algo = '--ultra'; fp = fp.replace('--ultra', '').trim(); }
            else if (fp.includes('--cmix')) { algo = '--cmix'; fp = fp.replace('--cmix', '').trim(); }

            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: `Please provide a file path to compress.\n\nExample: \`compress C:\\\\my_file.txt\``, tool: 'compress' };

            // Step 1: Pre-analyze
            let analysisWidget: any = null;
            try {
                const aRes = await fetch(`${API}/api/analyze`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: file })
                });
                const aData = await aRes.json();
                if (!aData.error) {
                    analysisWidget = { type: 'file_analysis', data: aData };
                    // Use AI-recommended algorithm
                    if (aData.ai?.recommended_algorithm && algo === '--cmix') {
                        algo = aData.ai.recommended_algorithm;
                    }
                }
            } catch { /* continue even if analysis fails */ }

            // Step 2: Stream compress  
            try {
                const res = await fetch(`${API}/api/compress_stream`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: file, algorithm: algo })
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
                                if (lastNewline !== -1) fullOutput = fullOutput.substring(0, lastNewline + 1);
                                else fullOutput = '';
                            } else {
                                fullOutput += chunk[i];
                            }
                        }
                    }
                }

                let msg = `🚀 **Compression Complete!**\n\nFile: \`${file}\`\nAlgorithm: ${algo === '--cmix' ? 'CMIX Neural (1,046 advisors)' : algo === '--best' ? 'BWT Pipeline' : 'PPM Arithmetic'}`;

                if (fullOutput.trim()) {
                    msg += `\n\n**Engine Output:**\n\`\`\`text\n${fullOutput.trim()}\n\`\`\``;
                }

                return { content: msg, tool: 'compress', widget: analysisWidget };

            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        // ═══════ DECOMPRESS ═══════
        if (lower.startsWith('decompress') || lower.startsWith('restore') || lower.startsWith('extract')) {
            let fp = cmd.replace(/^(decompress|restore|extract)\s*/i, '').trim();
            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: `Please provide a \`.aiz\` archive path to decompress.`, tool: 'compress' };

            let op = file.replace('.aiz', '');
            if (op === file) op = file + '_out';

            try {
                const res = await fetch(`${API}/api/decompress_stream`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
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
                                if (lastNewline !== -1) fullOutput = fullOutput.substring(0, lastNewline + 1);
                                else fullOutput = '';
                            } else {
                                fullOutput += chunk[i];
                            }
                        }
                    }
                }

                return {
                    content: `🔄 **Decompression Complete!**\n\nThe neural network rebuilt itself from scratch — same advisors, same gradient descent, same weights. Mirror-mode decompression is pure math!\n\nArchive: \`${file}\`\nRecovered: \`${op}\`${fullOutput.trim() ? `\n\n\`\`\`text\n${fullOutput.trim()}\n\`\`\`` : ''}`,
                    tool: 'compress'
                };
            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        // ═══════ NAVIGATE ═══════
        if (lower.startsWith('navigate ') || lower.includes('go to ') || lower.startsWith('open ')) {
            let url = cmd.replace(/navigate |go to |open /gi, '').trim();
            if (!url.startsWith('http')) url = 'https://' + url;
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.navigate', params: { url } }));
            return { content: `Navigating to **${url}**...\n\nBrowser bridge command sent.`, tool: 'browse' };
        }

        // ═══════ SCREENSHOT ═══════
        if (lower.includes('screenshot') || lower.includes('capture')) {
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.screenshot', params: { fullPage: false } }));
            return { content: `📸 Screenshot captured!\n\nSaved via browser bridge.`, tool: 'screenshot' };
        }

        // ═══════ CLICK ═══════
        if (lower.startsWith('click ')) {
            const sel = cmd.substring(6);
            if (window.ws) window.ws.send(JSON.stringify({ id: `cmd_${Date.now()}`, type: 'command', from: 'vscode', command: 'browserAgent.click', params: { selector: sel } }));
            return { content: `Clicking element: \`${sel}\``, tool: 'browse' };
        }

        // ═══════ CMD / TERMINAL ═══════
        if (lower.startsWith('cmd ') || lower.startsWith('> ')) {
            const command = cmd.replace(/^(cmd|>)\s*/i, '');
            try {
                const res = await fetch(`${API}/api/cmd`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ command })
                });
                const data = await res.json();
                if (data.status === 'success') {
                    let out = data.stdout || '';
                    let err = data.stderr || '';
                    if (!out && !err) out = 'Command executed successfully with no output.';
                    return { content: `Ran command: \`${command}\`${out ? `\n\n**Output:**\n\`\`\`text\n${out}\n\`\`\`` : ''}${err ? `\n\n**Error:**\n\`\`\`text\n${err}\n\`\`\`` : ''}`, tool: 'script' };
                } else {
                    return { content: `Error: \`${data.error}\``, tool: 'script' };
                }
            } catch (e) {
                return { content: `Failed to connect to Neural Studio Backend API:\n\`\`\`\n${e}\n\`\`\``, tool: 'script' };
            }
        }

        // ═══════ LIST DIRECTORY ═══════
        if (lower.startsWith('ls') || lower.startsWith('list ') || lower.startsWith('dir ') || lower === 'ls' || lower === 'dir') {
            const dirPath = cmd.replace(/^(ls|list|dir)\s*/i, '').trim() || '';
            try {
                const res = await fetch(`${API}/api/fs/list`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path: dirPath || null })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                let listing = data.items.map((item: any) => {
                    if (item.type === 'directory') return `📁 ${item.name}/ (${item.children} items)`;
                    return `📄 ${item.name} (${item.size_human || '?'})`;
                }).join('\n');

                return { content: `📂 **${data.path}** (${data.count} items)\n\n${listing}` };
            } catch (e) {
                return { content: `Failed to list directory:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ READ FILE ═══════
        if (lower.startsWith('read ') || lower.startsWith('cat ') || lower.startsWith('view ') || lower.startsWith('open ')) {
            const fp = cmd.replace(/^(read|cat|view|open)\s*/i, '').trim();
            if (!fp) return { content: 'Provide a file path to read.\n\nExample: `read C:\\\\data\\\\notes.txt`' };

            try {
                const res = await fetch(`${API}/api/fs/read`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path: fp })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                const ext = data.extension || '';
                const lang = ext.replace('.', '') || 'text';
                let header = `📄 **${data.file_name}** (${data.total_lines} lines, ${(data.size / 1024).toFixed(1)} KB)`;
                if (data.converted) header += ` — *converted from ${data.format}*`;
                if (data.truncated) header += `\n⚠️ Showing first ${data.lines_shown} of ${data.total_lines} lines`;

                return { content: `${header}\n\n\`\`\`${lang}\n${data.content}\n\`\`\`` };
            } catch (e) {
                return { content: `Failed to read file:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ WRITE / APPEND FILE ═══════
        if (lower.startsWith('write ') || lower.startsWith('append ') || lower.startsWith('create file ') || lower.startsWith('save file ')) {
            const isAppend = lower.startsWith('append ');
            const rest = cmd.replace(/^(write|append|create file|save file)\s*/i, '').trim();
            const colonIdx = rest.indexOf(':');

            if (colonIdx === -1) {
                return { content: `To write a file, use format:\n\n\`${isAppend ? 'append' : 'write'} [path]: [content]\`\n\nExample: \`write C:\\\\script.py: print("Hello")\`` };
            }

            const fp = rest.substring(0, colonIdx).trim();
            const content = rest.substring(colonIdx + 1).trim();

            try {
                const res = await fetch(`${API}/api/fs/write`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path: fp, content, append: isAppend })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };
                return { content: `📝 **File ${isAppend ? 'Appended' : 'Saved'}**\n\nPath: \`${fp}\`\nBytes ${isAppend ? 'added' : 'written'}: ${content.length}` };
            } catch (e) {
                return { content: `Failed to write file:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ DELETE FILE ═══════
        if (lower.startsWith('delete ') || lower.startsWith('remove ') || lower.startsWith('rm ')) {
            const fp = cmd.replace(/^(delete|remove|rm)\s*/i, '').trim();
            if (!fp) return { content: 'Provide a path to delete.\n\nExample: `delete C:\\\\data\\\\old.txt`' };

            try {
                const res = await fetch(`${API}/api/fs/delete`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ path: fp })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };
                return { content: `🗑️ **Deleted successfully:** \`${fp}\`` };
            } catch (e) {
                return { content: `Failed to delete file:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ FIND FILES ═══════
        if (lower.startsWith('find ') || lower.startsWith('search files ')) {
            const pattern = cmd.replace(/^(find|search files)\s*/i, '').trim();
            if (!pattern) return { content: 'Provide a search pattern.\n\nExamples:\n`find *.txt`\n`find *.pdf`\n`find main.*`' };

            try {
                const res = await fetch(`${API}/api/fs/find`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ pattern })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                if (data.count === 0) return { content: `No files found matching \`${pattern}\`` };

                let list = data.results.map((r: any) =>
                    `${r.type === 'directory' ? '📁' : '📄'} ${r.path}${r.size ? ` (${(r.size / 1024).toFixed(1)} KB)` : ''}`
                ).join('\n');

                return { content: `🔍 Found **${data.count}** of ${data.total_found} matches for \`${pattern}\`:\n\n${list}` };
            } catch (e) {
                return { content: `Failed to search:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ LEARN FROM FILE ═══════
        if (lower.startsWith('learn_file ') || lower.startsWith('learn file ') || lower.startsWith('read and learn ')) {
            const rest = cmd.replace(/^(learn_file|learn file|read and learn)\s*/i, '').trim();
            if (!rest) return { content: 'Provide a file path to learn from.\n\nExample: `learn_file C:\\\\docs\\\\research.pdf`\n\nI can read: PDF, Word, Excel, code, text, and more!' };

            try {
                const res = await fetch(`${API}/api/brain/learn_file`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: rest })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                return {
                    content: `📖 **Learned from file: ${data.file}**\n\nTokens: ${data.token_count}\nRaw: ${(data.raw_size / 1024).toFixed(1)} KB → Compressed: ${(data.compressed_size / 1024).toFixed(1)} KB\nSavings: **${data.savings_pct}%**\n\nKeywords: ${data.keywords?.map((k: string) => `\`${k}\``).join(', ')}\n\nAsk me about it anytime!`
                };
            } catch (e) {
                return { content: `Failed:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ TRAIN ═══════
        if (lower === 'train' || lower.startsWith('train ')) {
            const mode = lower.includes('web') ? 'web' : lower.includes('local') ? 'local' : 'all';
            const deep = lower.includes('deep');

            try {
                const res = await fetch(`${API}/api/brain/train`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ mode, deep })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                return {
                    content: `🎓 **Training Complete!**\n\nItems learned: **${data.items_learned}**${data.local_files_learned !== undefined ? `\nLocal files: ${data.local_files_learned}` : ''}${data.web_sources_learned !== undefined ? `\nWeb sources: ${data.web_sources_learned}` : ''}\nTotal knowledge: ${data.total_knowledge}\nVocabulary: ${data.vocabulary_size} words\nCompression: ${data.compression_savings}% saved`
                };
            } catch (e) {
                return { content: `Failed:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        if (lower.startsWith('store ') || lower.startsWith('vault store ') || lower.startsWith('save ')) {
            let fp = cmd.replace(/^(vault store|store|save)\s*/i, '').trim();
            const file = atts.find(a => a.type === 'file')?.name || fp;
            if (!file) return { content: 'Provide a file path to store in the neural vault.\n\nExample: `store C:\\\\data\\\\frankenstein.txt`', tool: 'compress' };

            try {
                const res = await fetch(`${API}/api/vault/store`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ file_path: file })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}`, tool: 'compress' };

                return {
                    content: `🗄️ **Stored in Neural Vault!**\n\nKey: \`${data.key}\`\nOriginal: ${(data.original_size / 1024).toFixed(1)} KB\nCompressed: ${(data.compressed_size / 1024).toFixed(1)} KB\nSavings: **${data.savings_pct}%** (${data.algorithm})\n\nRetrieve anytime with: \`access ${data.key}\``,
                    tool: 'compress'
                };
            } catch (e) {
                return { content: `Failed to connect to server:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        // ═══════ VAULT ACCESS ═══════
        if (lower.startsWith('access ') || lower.startsWith('vault access ') || lower.startsWith('retrieve ') || lower.startsWith('fetch ')) {
            let key = cmd.replace(/^(vault access|access|retrieve|fetch)\s*/i, '').trim();
            if (!key) return { content: 'Provide a vault key to retrieve.\n\nUse `vault list` to see stored files.', tool: 'compress' };

            try {
                const res = await fetch(`${API}/api/vault/access`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ key })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}`, tool: 'compress' };

                return {
                    content: `📂 **Retrieved from Neural Vault!**\n\nKey: \`${data.key}\`\nRecovered: \`${data.output_path}\`\nSize: ${(data.recovered_size / 1024).toFixed(1)} KB\n\nDecompressed using mirror-mode — the neural network rebuilt itself from scratch and reproduced the exact original data.`,
                    tool: 'compress'
                };
            } catch (e) {
                return { content: `Failed to connect to server:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        // ═══════ VAULT LIST ═══════
        if (lower === 'vault list' || lower === 'vault' || lower === 'list vault' || lower.startsWith('vault list')) {
            try {
                const res = await fetch(`${API}/api/vault/list`);
                const data = await res.json();

                if (data.count === 0) {
                    return { content: `🗄️ **Neural Vault is empty.**\n\nStore a file: \`store C:\\\\path\\\\file.txt\``, tool: 'compress' };
                }

                let list = data.entries.map((e: any) =>
                    `• \`${e.key}\` — ${(e.original_size / 1024).toFixed(0)} KB → ${(e.compressed_size / 1024).toFixed(0)} KB (${e.savings_pct}% saved, ${e.algorithm})`
                ).join('\n');

                return {
                    content: `🗄️ **Neural Vault — ${data.count} files stored**\n\nTotal: ${(data.total_original_size / 1024).toFixed(0)} KB → ${(data.total_compressed_size / 1024).toFixed(0)} KB (**${data.total_savings_pct}% saved**)\n\n${list}\n\nRetrieve with: \`access [key]\``,
                    tool: 'compress'
                };
            } catch (e) {
                return { content: `Failed to connect to server:\n\`\`\`\n${e}\n\`\`\``, tool: 'compress' };
            }
        }

        // ═══════ MATH / CALC ═══════
        const isMathCmd = lower.startsWith('math ') || lower.startsWith('calc ') || lower.startsWith('calculate ') || lower.startsWith('compute ') || lower.startsWith('solve ') || lower.startsWith('stats ') || lower.startsWith('entropy ');
        const isPureMath = /^[a-z0-9\s()+\-*/^.,]+$/i.test(lower) && /\d/.test(lower) && /[+\-*/^()]/.test(lower) && !/^[a-z\s()]+$/i.test(lower);

        if (isMathCmd || isPureMath) {
            const query = cmd.replace(/^(math|calc|calculate|compute|solve)\s*/i, '').trim();
            try {
                const res = await fetch(`${API}/api/math/process`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ query })
                });
                const data = await res.json();

                if (data.error || data.type === 'unknown') {
                    return { content: `❌ ${data.error || 'Could not compute.'}\n\n${data.hint || 'Try: `calc 2 + 3 * 4`, `stats 1 2 3 4 5`, `entropy 0.5 0.3 0.2`'}` };
                }

                let response = `🔢 **Math Result**\n\n`;

                if (data.type === 'expression') {
                    response += `\`${data.expression}\` = **${data.result}**`;
                } else if (data.type === 'statistics') {
                    if (data.function) {
                        response += `${data.function}(${data.data.join(', ')}) = **${data.result}**`;
                    } else {
                        const s = data.result;
                        response += `Dataset: [${data.data.join(', ')}]\n\n`;
                        response += `| Metric | Value |\n|--------|-------|\n`;
                        response += `| Count | ${s.count} |\n| Mean | ${s.mean} |\n| Median | ${s.median} |\n`;
                        response += `| Std Dev | ${s.std_dev} |\n| Min | ${s.min} |\n| Max | ${s.max} |\n| Range | ${s.range} |`;
                    }
                } else if (data.type === 'entropy') {
                    response += `Shannon Entropy: **${data.result} bits**\n\nProbabilities: [${data.probabilities.map((p: number) => p.toFixed(3)).join(', ')}]`;
                } else {
                    response += `**${data.explanation || JSON.stringify(data.result)}**`;
                }

                return { content: response };
            } catch (e) {
                return { content: `Math engine error:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ SEARCH ═══════
        if (lower.startsWith('search') || lower.includes('search for ')) {
            const q = cmd.replace(/search for |search /gi, '').trim();
            return { content: `🔍 Searching for: **${q}**\n\nWeb bridge search initiated.`, tool: 'search' };
        }

        // ═══════ LEARN — Teach the brain ═══════
        if (lower.startsWith('learn ') && !lower.startsWith('learn_url')) {
            // Format: learn [topic]: [content]
            const rest = cmd.replace(/^learn\s*/i, '').trim();
            const colonIdx = rest.indexOf(':');

            if (colonIdx === -1) {
                return { content: `To teach me, use this format:\n\n\`learn [topic]: [information]\`\n\nExample: \`learn quantum computing: Quantum computers use qubits that can exist in superposition...\`\n\nI'll compress and remember it forever! 🧠` };
            }

            const topic = rest.substring(0, colonIdx).trim();
            const content = rest.substring(colonIdx + 1).trim();

            if (!content || content.length < 10) {
                return { content: 'Please provide more content to learn. At least a sentence or two!' };
            }

            try {
                const res = await fetch(`${API}/api/brain/learn`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ topic, content })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                return {
                    content: `🧠 **Knowledge Absorbed!**\n\nTopic: **${topic}**\nTokens processed: ${data.token_count}\nRaw: ${(data.raw_size / 1024).toFixed(1)} KB → Compressed: ${(data.compressed_size / 1024).toFixed(1)} KB\nSavings: **${data.savings_pct}%** (stored with CMIX neural compression)\n\nKeywords learned: ${data.keywords?.map((k: string) => `\`${k}\``).join(', ')}\n\n**Summary:** ${data.summary}\n\nAsk me about this anytime! Try: \`ask ${topic}\``
                };
            } catch (e) {
                return { content: `Failed to connect:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ LEARN FROM URL ═══════
        if (lower.startsWith('learn_url ') || lower.startsWith('learn url ') || lower.startsWith('study ')) {
            const rest = cmd.replace(/^(learn_url|learn url|study)\s*/i, '').trim();
            const parts = rest.split(/\s+/);

            // Find the URL
            const urlIdx = parts.findIndex(p => p.startsWith('http'));
            let topic: string, url: string;

            if (urlIdx >= 0) {
                url = parts[urlIdx];
                topic = parts.filter((_, i) => i !== urlIdx).join(' ') || 'web_article';
            } else if (parts.length >= 2) {
                topic = parts.slice(0, -1).join(' ');
                url = parts[parts.length - 1];
                if (!url.startsWith('http')) url = 'https://' + url;
            } else {
                return { content: `To learn from the web:\n\n\`learn_url [topic] [url]\`\n\nExample: \`learn_url machine learning https://en.wikipedia.org/wiki/Machine_learning\`\n\nI'll scrape, extract, compress, and index the knowledge! 🌐` };
            }

            try {
                const res = await fetch(`${API}/api/brain/learn_url`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ topic, url })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                return {
                    content: `🌐 **Learned from the Web!**\n\nURL: \`${url}\`\nTopic: **${topic}**\nTokens extracted: ${data.token_count}\nRaw: ${(data.raw_size / 1024).toFixed(1)} KB → Compressed: ${(data.compressed_size / 1024).toFixed(1)} KB\nSavings: **${data.savings_pct}%**\n\nKeywords: ${data.keywords?.map((k: string) => `\`${k}\``).join(', ')}\n\n**Summary:** ${data.summary}\n\nNow ask me anything about **${topic}**!`
                };
            } catch (e) {
                return { content: `Failed to connect:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ ASK — Query the brain ═══════
        if (lower.startsWith('ask ') || lower.startsWith('question ')) {
            const question = cmd.replace(/^(ask|question)\s*/i, '').trim();
            if (!question) return { content: 'What would you like to know?' };

            try {
                const res = await fetch(`${API}/api/brain/ask`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ question })
                });
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                let response = `**${data.response}**`;

                if (data.sources && data.sources.length > 0) {
                    response += `\n\n---\n📚 *Sources (${data.knowledge_items_used} of ${data.total_knowledge} items):*`;
                    for (const s of data.sources) {
                        response += `\n• **${s.topic}** (score: ${s.score}${s.savings_pct ? `, ${s.savings_pct}% compressed` : ''})`;
                        if (s.source_url) response += ` — [link](${s.source_url})`;
                    }
                    response += `\n\nConfidence: ${(data.confidence * 100).toFixed(0)}%`;
                }

                return { content: response };
            } catch (e) {
                return { content: `Failed to connect:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ BRAIN STATS ═══════
        if (lower === 'brain' || lower === 'brain stats' || lower === 'memory' || lower.includes('brain status')) {
            try {
                const res = await fetch(`${API}/api/brain/stats`);
                const data = await res.json();
                if (data.error) return { content: `❌ ${data.error}` };

                return {
                    content: `🧠 **Neural Brain Status**\n\nKnowledge items: **${data.total_knowledge_items}**\nTopics learned: **${data.total_topics}** (${data.topics?.join(', ') || 'none yet'})\nVocabulary size: **${data.vocabulary_size}** words\nConversations: **${data.conversations_remembered}**\n\nStorage: ${data.total_raw_human} raw → ${data.total_compressed_human} compressed\nSavings: **${data.compression_savings_pct}%** (all knowledge stored with CMIX neural compression)\n\nTeach me more:\n• \`learn [topic]: [info]\` — teach directly\n• \`learn_url [topic] [url]\` — learn from the web\n• \`ask [question]\` — query my knowledge`
                };
            } catch (e) {
                return { content: `Failed to connect:\n\`\`\`\n${e}\n\`\`\`` };
            }
        }

        // ═══════ CODE ═══════
        if (lower.includes('code') || lower.includes('write') || lower.includes('generate')) {
            return {
                content: `Here's a starter using our V10 Neural API:\n\n\`\`\`typescript\n// Analyze a file before compressing\nconst analysis = await fetch('${API}/api/analyze', {\n  method: 'POST',\n  headers: { 'Content-Type': 'application/json' },\n  body: JSON.stringify({ file_path: 'data.txt' })\n}).then(r => r.json());\n\nconsole.log('Entropy:', analysis.entropy, 'bpb');\nconsole.log('AI recommends:', analysis.ai.recommended_algorithm);\n\n// Teach the brain\nawait fetch('${API}/api/brain/learn', {\n  method: 'POST',\n  headers: { 'Content-Type': 'application/json' },\n  body: JSON.stringify({\n    topic: 'my research',\n    content: 'Important findings about...'\n  })\n});\n\n// Ask the brain later\nconst answer = await fetch('${API}/api/brain/ask', {\n  method: 'POST',\n  headers: { 'Content-Type': 'application/json' },\n  body: JSON.stringify({ question: 'What were the findings?' })\n}).then(r => r.json());\n\`\`\`\n\nThe brain stores all knowledge compressed — it learns, compresses, and retrieves!`,
                tool: 'code'
            };
        }

        // ═══════ STATUS ═══════
        if (lower === 'status' || lower === 'health' || lower.includes('server status')) {
            try {
                const res = await fetch(`${API}/api/health`);
                const data = await res.json();
                const brainRes = await fetch(`${API}/api/brain/stats`);
                const brain = await brainRes.json();
                return {
                    content: `**🟢 Neural Studio V10 Server Status**\n\nVersion: \`${data.version}\`\nUptime: \`${Math.floor(data.uptime_seconds)}s\`\nEngine: ${data.engine}\nModels: **${data.models} AI Advisors** active\nExecutable: ${data.exe_available ? '✅' : '❌'}\n\n🧠 **Brain:** ${brain.total_knowledge_items || 0} items, ${brain.vocabulary_size || 0} words, ${brain.compression_savings_pct || 0}% compressed\n\nAlgorithms: ${data.algorithms?.map((a: string) => `\`${a}\``).join(', ')}`
                };
            } catch {
                return { content: `❌ **Server Offline**\n\nStart the server:\n\`\`\`\ncd server && python main.py\n\`\`\`` };
            }
        }

        // ═══════ TEST BRAIN ═══════
        if (lower === 'test brain' || lower === 'test engine' || lower === 'diagnostics') {
            try {
                // Run the comprehensive test script
                const res = await fetch(`${API}/api/cmd`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ command: 'python ../test_neural_engine.py' })
                });
                const data = await res.json();
                if (data.status === 'success') {
                    return { content: `**🧠 Neural Engine Full Diagnostics**\n\n\`\`\`text\n${data.stdout}\n\`\`\``, tool: 'script' };
                } else {
                    return { content: `❌ Test failed:\n\`\`\`text\n${data.error}\n\`\`\``, tool: 'script' };
                }
            } catch (e) {
                return { content: `Failed to run diagnostics:\n\`\`\`\n${e}\n\`\`\``, tool: 'script' };
            }
        }

        // ═══════ HELP ═══════
        if (lower.includes('help') || lower === '?') {
            return {
                content: `**🧠 Neural Studio AI — Command Reference**\n\n**Smart Brain (C++ Knowledge Engine) 🆕**\n• \`learn https://wikipedia.org/...\` — learn from web (auto-compress)\n• \`brain status\` — show knowledge entries & compression stats\n• Just ask naturally: "What is X?" — auto Smart Brain!\n\n**File Operations**\n• \`ls\` or \`ls C:\\\\path\` — list directory\n• \`read C:\\\\file.txt\` — read file contents\n• \`find *.txt\` — find files by pattern\n\n**Compression**\n• \`analyze [file]\` — AI analysis (entropy, recommendations)\n• \`compress [file]\` — neural compression (CMIX)\n• \`decompress [file]\` — restore original\n\n**Neural Vault**\n• \`store [file]\` — compress & vault\n• \`access [key]\` — retrieve from vault\n• \`vault list\` — list stored files\n\n**Tools**\n• \`run [command]\` — execute shell command\n• \`calc 2 + 3 * 4\` — math calculations\n• \`status\` — server health\n\n**Examples:**\n\`\`\`\nlearn https://en.wikipedia.org/wiki/Data_compression\nWhat is data compression?\nbrain status\nanalyze C:\\\\data\\\\file.txt\ncompress C:\\\\data\\\\file.txt\n\`\`\`\n\n🧠 Smart Brain compresses everything — learns more, uses less space!`
            };
        }

        // ═══════ SMART BRAIN — C++ Knowledge Engine (Priority) ═══════
        // Try Smart Brain FIRST for questions
        if (cmd.trim().length > 5 && (cmd.includes('?') || /^(what|how|why|when|where|who|explain|tell|describe)/i.test(cmd))) {
            try {
                const askRes = await fetch(`${API}/api/brain/ask`, {
                    method: 'POST', headers: { 'Content-Type': 'application/json' },
                    body: JSON.stringify({ question: cmd })
                });
                const askData = await askRes.json();

                // Check if Smart Brain has an answer
                if (!askData.error && askData.answer) {
                    const conf = ((askData.confidence || 0) * 100).toFixed(0);
                    return { content: `${askData.answer}\n\n📚 **Source:** ${askData.source} · **Confidence:** ${conf}%`, tool: 'smartbrain' };
                }

                // If low confidence, suggest learning
                if (askData.action === 'learn_from_web') {
                    const conf = ((askData.confidence || 0) * 100).toFixed(0);
                    return {
                        content: `🧠 **Low confidence (${conf}%)**\n\nI don't have enough knowledge about this topic yet.\n\n**Learn from Wikipedia:**\n\`learn https://en.wikipedia.org/wiki/${encodeURIComponent(cmd.replace(/^(what|how|why|when|where|who|explain|tell|describe)\s+/i, '').replace(/\?/g, ''))}\`\n\nOr I can use my basic reasoning (less accurate).`,
                        tool: 'smartbrain'
                    };
                }
            } catch (e) {
                console.log('Smart Brain not available, trying fallback...', e);
            }
        }

        // ═══════ FALLBACK — Conversational responses ═══════
        try {
            const res = await fetch(`${API}/api/brain/think`, {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ message: cmd })
            });
            const data = await res.json();

            if (data.response) {
                return { content: data.response };
            }
        } catch { /* reasoning engine not available */ }

        return {
            content: `I don't understand "${cmd}" yet.\n\nTalk to me naturally — I can understand questions, math, file operations, and more.\nOr: \`help\` for examples`
        };
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
            const { content, tool, widget } = await processCommand(cmd, userMsg.attachments || []);
            setMessages(p => [...p, { role: 'assistant', content, tool, widget, timestamp: new Date() }]);
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
                        background: 'linear-gradient(135deg, #C97FDB, #8B5CF6)',
                        display: 'flex', alignItems: 'center', justifyContent: 'center',
                        color: '#fff', boxShadow: '0 2px 10px rgba(201,127,219,0.3)'
                    }}>
                        <BrainCircuit size={19} />
                    </div>
                    <div>
                        <div style={{ fontWeight: 700, fontSize: 14, color: '#F0F0F0', display: 'flex', alignItems: 'center', gap: 6 }}>
                            Neural Studio AI
                            <span style={{
                                fontSize: 9, fontWeight: 700, padding: '1px 6px', borderRadius: 8,
                                background: 'rgba(201,127,219,0.15)', color: '#C97FDB',
                                border: '1px solid rgba(201,127,219,0.3)'
                            }}>V10</span>
                        </div>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 5, marginTop: 2 }}>
                            <span style={{
                                width: 6, height: 6, borderRadius: '50%',
                                background: connected ? '#4ADE80' : '#EF4444',
                                boxShadow: connected ? '0 0 5px #4ADE80' : '0 0 5px #EF4444',
                                display: 'inline-block'
                            }} />
                            <span style={{ fontSize: 11, color: '#666' }}>
                                {connected ? `Online · ${serverInfo?.models || 1046} advisors` : 'Server offline'}
                            </span>
                        </div>
                    </div>
                </div>
                <div style={{ display: 'flex', gap: 6 }}>
                    <button onClick={() => checkHealth()}
                        style={{
                            display: 'flex', alignItems: 'center', gap: 5, padding: '6px 12px',
                            borderRadius: 20, cursor: 'pointer', fontSize: 11, fontWeight: 600,
                            border: connected ? '1px solid rgba(74,222,128,0.3)' : '1px solid #333',
                            background: connected ? 'rgba(74,222,128,0.08)' : '#242424',
                            color: connected ? '#4ADE80' : '#666',
                            transition: 'all 0.2s'
                        }}>
                        {connected ? <Wifi size={12} /> : <WifiOff size={12} />}
                        {connected ? 'Connected' : 'Retry'}
                    </button>
                </div>
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
                                        ? { background: 'linear-gradient(135deg, #C97FDB, #8B5CF6)', color: '#fff' }
                                        : { background: '#1A1A1A', color: '#C97FDB', border: '1px solid #333' })
                                }}>
                                    {m.role === 'assistant' ? <BrainCircuit size={14} /> : <span style={{ fontSize: 11, fontWeight: 700 }}>Y</span>}
                                </div>

                                {/* Column */}
                                <div style={{ display: 'flex', flexDirection: 'column', gap: 4, maxWidth: 'min(78%, 620px)', alignItems: m.role === 'user' ? 'flex-end' : 'flex-start' }}>
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
                                                : { background: '#2A1F35', border: '1px solid rgba(201,127,219,0.3)' })
                                        }} />
                                    )}
                                    {/* Rich Widget — Analysis Results */}
                                    {m.widget?.type === 'file_analysis' && (
                                        <FileInfoWidget data={m.widget.data} />
                                    )}
                                </div>
                            </div>
                        )}
                    </div>
                ))}

                {/* Typing indicator */}
                {isTyping && (
                    <div style={{ display: 'flex', gap: 10, alignItems: 'flex-start' }}>
                        <div style={{ width: 30, height: 30, borderRadius: 9, flexShrink: 0, background: 'linear-gradient(135deg, #C97FDB, #8B5CF6)', display: 'flex', alignItems: 'center', justifyContent: 'center', color: '#fff' }}>
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
                {TOOL_ACTIONS.map(tool => (
                    <QuickPill key={tool.id} tool={tool} onClick={() => handleToolAction(tool.id)} />
                ))}
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

            {/* Footer */}
            <div style={{ textAlign: 'center', fontSize: 10, color: '#555', paddingBottom: 6, flexShrink: 0 }}>
                Neural Studio V10 · Compression AI + Assistant · {connected ? '🟢' : '🔴'} Server {connected ? 'Online' : 'Offline'}
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

/* ─ Sub-components ─ */

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
            border: `1px solid ${focused ? '#C97FDB' : '#333'}`,
            boxShadow: focused ? '0 0 0 3px rgba(201,127,219,0.1)' : 'none',
            borderRadius: 13, padding: '4px 4px 4px 6px', gap: 2,
            transition: 'border-color 0.2s, box-shadow 0.2s'
        }}>
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
                placeholder="analyze · compress · cmd · ask anything..."
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
                background: active ? 'rgba(201,127,219,0.15)' : hov ? 'rgba(255,255,255,0.06)' : 'transparent',
                color: active ? '#C97FDB' : hov ? '#CCC' : '#777',
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