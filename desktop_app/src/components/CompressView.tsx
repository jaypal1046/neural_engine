import { useState, useRef, useEffect } from 'react';
import { Activity, Zap, ChevronRight, Cpu } from 'lucide-react';

const API = 'http://127.0.0.1:8001';

export function CompressView() {
    const [algorithm, setAlgorithm] = useState('--cmix');
    const [isCompressing, setIsCompressing] = useState(false);
    const [analysis, setAnalysis] = useState<any>(null);
    const [isAnalyzing, setIsAnalyzing] = useState(false);
    const hiddenFileInput = useRef<HTMLInputElement>(null);

    // Auto-analyze when file is selected
    const analyzeFile = async (filePath: string) => {
        if (!filePath.trim()) return;
        setIsAnalyzing(true);
        setAnalysis(null);
        try {
            const res = await fetch(`${API}/api/analyze`, {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ file_path: filePath })
            });
            const data = await res.json();
            if (!data.error) {
                setAnalysis(data);
                // Auto-select recommended algorithm
                if (data.ai?.recommended_algorithm) {
                    setAlgorithm(data.ai.recommended_algorithm);
                }
            }
        } catch { /* server not available */ }
        setIsAnalyzing(false);
    };

    const startCompression = async () => {
        const fp = (document.getElementById('input_file') as HTMLInputElement).value;
        const out = document.querySelector('.log-area pre');
        if (!out || !fp.trim()) return;

        setIsCompressing(true);
        out.textContent = `Neural Engine V10 starting...\nFile: ${fp}\nAlgorithm: ${algorithm}\n\n`;

        try {
            const res = await fetch(`${API}/api/compress_stream`, {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ file_path: fp, algorithm: algorithm })
            });

            const reader = res.body?.getReader();
            const decoder = new TextDecoder();

            if (reader) {
                let currentText = out.textContent || '';
                while (true) {
                    const { done, value } = await reader.read();
                    if (done) break;
                    const chunk = decoder.decode(value, { stream: true });

                    for (let i = 0; i < chunk.length; i++) {
                        const c = chunk[i];
                        if (c === '\r') {
                            const lastNewline = currentText.lastIndexOf('\n');
                            if (lastNewline !== -1) {
                                currentText = currentText.substring(0, lastNewline + 1);
                            } else {
                                currentText = '';
                            }
                        } else {
                            currentText += c;
                        }
                    }

                    out.textContent = currentText;
                    if (out.parentElement) out.parentElement.scrollTop = out.parentElement.scrollHeight;
                }
            }
        } catch (e) {
            out.textContent += `\nError: ${e}`;
        }
        setIsCompressing(false);
    };

    return (
        <div className="card">
            <h3>Configuration</h3>
            <hr className="divider" />
            <div className="card-content">
                <div className="field-row">
                    <label>Input File</label>
                    <input type="text" placeholder="Click Browse or paste a file path..." id="input_file"
                        onBlur={(e) => analyzeFile(e.target.value)} />

                    <input type="file" ref={hiddenFileInput} style={{ display: 'none' }} onChange={(e) => {
                        const file = e.target.files?.[0];
                        if (file) {
                            (document.getElementById('input_file') as HTMLInputElement).value = file.name;
                            (document.getElementById('output_file') as HTMLInputElement).value = file.name + '.aiz';
                            analyzeFile(file.name);
                            setTimeout(() => alert("Notice: Because you are running Neural Studio inside a standard Web Browser instead of the Desktop App, Javascript sandbox security prevents grabbing the absolute file path (e.g. C:/User/Data/...) \n\nWe have pasted the file's name into the box, but if it is not in the same folder as the server, you must manually type/paste the full C:/... path to compress it!"), 100);
                        }
                    }} />

                    <button className="ghost-btn" onClick={async () => {
                        try {
                            if (window.ipcRenderer) {
                                const file = await window.ipcRenderer.selectFile();
                                if (file) {
                                    (document.getElementById('input_file') as HTMLInputElement).value = file;
                                    (document.getElementById('output_file') as HTMLInputElement).value = file + '.aiz';
                                    analyzeFile(file);
                                }
                            } else {
                                hiddenFileInput.current?.click();
                            }
                        } catch (e) {
                            hiddenFileInput.current?.click();
                        }
                    }}>Browse</button>

                    {/* Inline Analyze button */}
                    <button className="ghost-btn" style={{
                        borderColor: '#C97FDB44', color: '#C97FDB',
                        display: 'flex', alignItems: 'center', gap: 4
                    }} onClick={() => {
                        const fp = (document.getElementById('input_file') as HTMLInputElement).value;
                        analyzeFile(fp);
                    }}>
                        <Activity size={13} />
                        {isAnalyzing ? 'Scanning...' : 'AI Scan'}
                    </button>
                </div>
                <div className="field-row">
                    <label>Save Archive As</label>
                    <input type="text" placeholder="Destination .aiz path..." id="output_file" />
                    <button className="ghost-btn" onClick={async () => {
                        try {
                            if (window.ipcRenderer) {
                                const file = await window.ipcRenderer.selectSaveFile();
                                if (file) {
                                    (document.getElementById('output_file') as HTMLInputElement).value = file;
                                }
                            } else {
                                alert("Save dialogs are only supported in the specific Neural Studio Desktop app environment. Please paste your target directory manually.");
                            }
                        } catch (e) {
                            alert("Save dialogs are only supported in the Desktop application environment.");
                        }
                    }}>Browse</button>
                </div>
                <div className="field-row" style={{ marginTop: "24px" }}>
                    <label>Algorithm</label>
                    <select
                        value={algorithm}
                        onChange={(e) => setAlgorithm(e.target.value)}
                        style={{ backgroundColor: "var(--surface)", border: "1px solid var(--border)", color: "var(--text)", padding: "10px", borderRadius: "8px", outline: "none", cursor: "pointer" }}
                    >
                        <option value="--cmix">CMIX — Neural (1,046 Advisors)</option>
                        <option value="--best">BWT — Best Balanced</option>
                        <option value="--ultra">PPM — Ultra Max</option>
                    </select>
                    {analysis?.ai?.recommended_algorithm && (
                        <span style={{
                            marginLeft: 8, fontSize: 10, fontWeight: 700, padding: '3px 8px',
                            borderRadius: 12, background: 'rgba(74,222,128,0.1)',
                            border: '1px solid rgba(74,222,128,0.3)', color: '#4ADE80',
                            display: 'flex', alignItems: 'center', gap: 3, whiteSpace: 'nowrap'
                        }}>
                            <Zap size={9} /> AI Recommended
                        </span>
                    )}
                </div>
                <div style={{ marginTop: "16px", display: "flex", gap: "12px", flexWrap: 'wrap' }}>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "#C97FDB" }}>CMIX</span> Best ratio</span>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>BEST</span> Balanced</span>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>ULTRA</span> Max compress</span>
                </div>

                {/* ─── AI Analysis Panel (appears after file scan) ─── */}
                {analysis && (
                    <div style={{
                        marginTop: 20, padding: '14px 16px',
                        background: '#1A1A1A', border: '1px solid #333', borderRadius: 10,
                        animation: 'fadeSlide 0.3s ease'
                    }}>
                        <div style={{ display: 'flex', alignItems: 'center', gap: 6, marginBottom: 10 }}>
                            <Cpu size={13} color="#C97FDB" />
                            <span style={{ fontSize: 12, fontWeight: 700, color: '#C97FDB' }}>Neural Analysis</span>
                            <span style={{ marginLeft: 'auto', fontSize: 11, color: '#888' }}>{analysis.file_size_human}</span>
                        </div>

                        {/* Entropy bar */}
                        <div style={{ display: 'flex', alignItems: 'center', gap: 8, margin: '6px 0' }}>
                            <span style={{ fontSize: 11, color: '#888', width: 55 }}>Entropy</span>
                            <div style={{ flex: 1, height: 6, background: '#242424', borderRadius: 3, overflow: 'hidden' }}>
                                <div style={{
                                    width: `${(analysis.entropy / 8) * 100}%`, height: '100%', borderRadius: 3,
                                    background: analysis.entropy < 3 ? '#4ADE80' : analysis.entropy < 5 ? '#FBBF24' : '#EF4444',
                                    transition: 'width 0.8s ease'
                                }} />
                            </div>
                            <span style={{
                                fontSize: 11, fontWeight: 700, width: 55, textAlign: 'right',
                                color: analysis.entropy < 3 ? '#4ADE80' : analysis.entropy < 5 ? '#FBBF24' : '#EF4444'
                            }}>{analysis.entropy?.toFixed(2)} bpb</span>
                        </div>

                        {/* Compression estimate */}
                        {analysis.ai && (
                            <div style={{
                                display: 'flex', alignItems: 'center', gap: 10, marginTop: 10,
                                padding: '8px 12px', borderRadius: 8,
                                background: 'rgba(74,222,128,0.05)', border: '1px solid rgba(74,222,128,0.15)'
                            }}>
                                <span style={{ fontSize: 20, fontWeight: 800, color: '#4ADE80' }}>
                                    ~{analysis.ai.compression_estimate_pct}%
                                </span>
                                <span style={{ fontSize: 11, color: '#888' }}>estimated savings</span>
                            </div>
                        )}

                        {/* AI Insights */}
                        {analysis.ai?.insights?.map((ins: any, i: number) => (
                            <div key={i} style={{
                                display: 'flex', gap: 6, padding: '6px 0', marginTop: 4,
                                borderTop: i > 0 ? '1px solid #2A2A2A' : 'none',
                                fontSize: 11, color: '#999'
                            }}>
                                <span>{ins.icon}</span>
                                <div>
                                    <span style={{ fontWeight: 700, color: '#CCC' }}>{ins.title}:</span> {ins.detail}
                                </div>
                            </div>
                        ))}
                    </div>
                )}

                {isAnalyzing && (
                    <div style={{
                        marginTop: 16, textAlign: 'center', padding: '20px',
                        color: '#C97FDB', fontSize: 12, fontWeight: 600,
                        background: '#1A1A1A', borderRadius: 10, border: '1px solid #333'
                    }}>
                        <div style={{ animation: 'spin 1.5s linear infinite', display: 'inline-block', marginBottom: 6 }}>
                            <Cpu size={18} />
                        </div>
                        <div>1,046 Neural Advisors scanning file...</div>
                    </div>
                )}
            </div>
            <button className="primary-btn mt-large" onClick={startCompression}
                disabled={isCompressing}
                style={{
                    background: isCompressing ? '#555' : 'var(--accent)',
                    display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 8
                }}>
                {isCompressing ? (
                    <>Compressing... <Cpu size={14} style={{ animation: 'spin 1s linear infinite' }} /></>
                ) : (
                    <>Start Compression <ChevronRight size={16} /></>
                )}
            </button>

            <style>{`
                @keyframes fadeSlide { from { opacity: 0; transform: translateY(8px); } to { opacity: 1; transform: translateY(0); } }
                @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
            `}</style>
        </div>
    );
}
