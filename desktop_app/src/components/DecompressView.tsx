import { useRef, useState } from 'react';
import { BrainCircuit, ChevronRight, Cpu } from 'lucide-react';

const API = 'http://127.0.0.1:8001';

export function DecompressView() {
    const hiddenFileInput = useRef<HTMLInputElement>(null);
    const [isDecompressing, setIsDecompressing] = useState(false);

    const startDecompression = async () => {
        const ap = (document.getElementById('d_input_file') as HTMLInputElement).value;
        const op = (document.getElementById('d_output_file') as HTMLInputElement).value;
        const out = document.querySelector('.log-area pre');
        if (!out || !ap.trim()) return;

        setIsDecompressing(true);
        out.textContent = `Neural Mirror-Mode Decompression starting...\nArchive: ${ap}\nOutput: ${op}\n\nRebuilding 1,046 advisor neural network from scratch...\n\n`;

        try {
            const res = await fetch(`${API}/api/decompress_stream`, {
                method: 'POST', headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ archive_path: ap, output_path: op })
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
        setIsDecompressing(false);
    };

    return (
        <div className="card">
            <h3>Configuration</h3>
            <hr className="divider" />
            <div className="card-content">
                {/* Info banner */}
                <div style={{
                    display: 'flex', alignItems: 'center', gap: 8, padding: '8px 12px',
                    background: 'rgba(74,158,255,0.06)', border: '1px solid rgba(74,158,255,0.15)',
                    borderRadius: 8, marginBottom: 12, marginTop: 8
                }}>
                    <BrainCircuit size={14} color="#4A9EFF" />
                    <span style={{ fontSize: 11, color: '#888', lineHeight: 1.5 }}>
                        <strong style={{ color: '#4A9EFF' }}>Mirror-Mode:</strong> The decompressor creates a blank neural network and replays every bit.
                        It learns at the same speed as the compressor — they stay in perfect sync.
                    </span>
                </div>

                <div className="field-row">
                    <label>Archive (.myzip)</label>
                    <input type="text" placeholder="Select a .myzip archive..." id="d_input_file" />

                    <input type="file" ref={hiddenFileInput} accept=".myzip" style={{ display: 'none' }} onChange={(e) => {
                        const file = e.target.files?.[0];
                        if (file) {
                            (document.getElementById('d_input_file') as HTMLInputElement).value = file.name;
                            (document.getElementById('d_output_file') as HTMLInputElement).value = file.name.replace('.myzip', '');
                            setTimeout(() => alert("Notice: Because you are running Neural Studio inside a standard Web Browser instead of the Desktop App context, JS sandbox security prevents grabbing the absolute file path (e.g. C:/User/Data/...)\n\nWe have pasted the file's name into the box, but if it is not in the same folder as the server, you must manually type/paste the full C:/... path to decompress it!"), 100);
                        }
                    }} />

                    <button className="ghost-btn" onClick={async () => {
                        try {
                            if (window.ipcRenderer) {
                                const file = await window.ipcRenderer.selectFile();
                                if (file) {
                                    (document.getElementById('d_input_file') as HTMLInputElement).value = file;
                                    (document.getElementById('d_output_file') as HTMLInputElement).value = file.replace('.myzip', '');
                                }
                            } else {
                                hiddenFileInput.current?.click();
                            }
                        } catch (e) {
                            hiddenFileInput.current?.click();
                        }
                    }}>Browse</button>
                </div>
                <div className="field-row">
                    <label>Output File</label>
                    <input type="text" placeholder="Where to save the recovered file..." id="d_output_file" />
                    <button className="ghost-btn" onClick={async () => {
                        try {
                            if (window.ipcRenderer) {
                                const file = await window.ipcRenderer.selectSaveFile();
                                if (file) {
                                    (document.getElementById('d_output_file') as HTMLInputElement).value = file;
                                }
                            } else {
                                alert("Save dialogs are only supported in the specific Neural Studio Desktop app environment. Please paste your target directory manually.");
                            }
                        } catch (e) {
                            alert("Save dialogs are only supported in the Desktop application environment.");
                        }
                    }}>Browse</button>
                </div>
            </div>
            <button className="primary-btn mt-large" onClick={startDecompression}
                disabled={isDecompressing}
                style={{
                    background: isDecompressing ? '#555' : 'var(--accent)',
                    display: 'flex', alignItems: 'center', justifyContent: 'center', gap: 8
                }}>
                {isDecompressing ? (
                    <>Decompressing... <Cpu size={14} style={{ animation: 'spin 1s linear infinite' }} /></>
                ) : (
                    <>Restore File <ChevronRight size={16} /></>
                )}
            </button>
        </div>
    );
}
