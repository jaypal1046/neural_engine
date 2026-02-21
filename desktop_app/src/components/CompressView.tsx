import { useState, useRef } from 'react';

export function CompressView() {
    const [algorithm, setAlgorithm] = useState('--cmix');
    const hiddenFileInput = useRef<HTMLInputElement>(null);

    return (
        <div className="card">
            <h3>Configuration</h3>
            <hr className="divider" />
            <div className="card-content">
                <div className="field-row">
                    <label>Input File</label>
                    <input type="text" placeholder="Click Browse or paste a file path..." id="input_file" />

                    {/* Hidden file input for native web fallback */}
                    <input type="file" ref={hiddenFileInput} style={{ display: 'none' }} onChange={(e) => {
                        const file = e.target.files?.[0];
                        if (file) {
                            (document.getElementById('input_file') as HTMLInputElement).value = file.name;
                            (document.getElementById('output_file') as HTMLInputElement).value = file.name + '.myzip';
                            setTimeout(() => alert("Notice: Because you are running Neural Studio inside a standard Web Browser instead of the Desktop App, Javascript sandbox security prevents grabbing the absolute file path (e.g. C:/User/Data/...) \n\nWe have pasted the file's name into the box, but if it is not in the same folder as the server, you must manually type/paste the full C:/... path to compress it!"), 100);
                        }
                    }} />

                    <button className="ghost-btn" onClick={async () => {
                        try {
                            if (window.ipcRenderer) {
                                const file = await window.ipcRenderer.selectFile();
                                if (file) {
                                    (document.getElementById('input_file') as HTMLInputElement).value = file;
                                    (document.getElementById('output_file') as HTMLInputElement).value = file + '.myzip';
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
                    <label>Save Archive As</label>
                    <input type="text" placeholder="Destination .myzip path..." id="output_file" />
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
                        <option value="--cmix">CMIX — Neural Genetic</option>
                        <option value="--best">LZMA — Best Balanced</option>
                        <option value="--ultra">LZMA — Ultra Max</option>
                    </select>
                </div>
                <div style={{ marginTop: "16px", display: "flex", gap: "12px" }}>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>CMIX</span> Best ratio</span>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>DEFAULT</span> Fastest</span>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>BEST</span> Balanced</span>
                    <span style={{ fontSize: "11px", fontWeight: "bold", background: "var(--surface)", padding: "4px 12px", borderRadius: "16px", border: "1px solid var(--border)" }}><span style={{ color: "var(--accent)" }}>ULTRA</span> Max compress</span>
                </div>
            </div>
            <button className="primary-btn mt-large" onClick={async () => {
                const fp = (document.getElementById('input_file') as HTMLInputElement).value;
                const out = document.querySelector('.log-area pre');
                if (!out) return;

                out.textContent += `\nCalling Python API to compress: ${fp} using ${algorithm}...\n\n`;

                try {
                    const res = await fetch('http://127.0.0.1:8001/api/compress_stream', {
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
            }}>Start Compression →</button>
        </div>
    );
}
