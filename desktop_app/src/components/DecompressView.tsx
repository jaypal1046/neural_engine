import { useRef } from 'react';

export function DecompressView() {
    const hiddenFileInput = useRef<HTMLInputElement>(null);

    return (
        <div className="card">
            <h3>Configuration</h3>
            <hr className="divider" />
            <div className="card-content">
                <div className="field-row">
                    <label>Archive (.myzip)</label>
                    <input type="text" placeholder="Select a .myzip archive..." id="d_input_file" />

                    <input type="file" ref={hiddenFileInput} accept=".myzip" style={{ display: 'none' }} onChange={(e) => {
                        const file = e.target.files?.[0];
                        if (file) {
                            (document.getElementById('d_input_file') as HTMLInputElement).value = file.name;
                            (document.getElementById('d_output_file') as HTMLInputElement).value = file.name.replace('.myzip', '');
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
            <button className="primary-btn mt-large" onClick={async () => {
                const ap = (document.getElementById('d_input_file') as HTMLInputElement).value;
                const op = (document.getElementById('d_output_file') as HTMLInputElement).value;
                const out = document.querySelector('.log-area pre');
                if (!out) return;

                out.textContent += `\nCalling Python API to decompress: ${ap}...\n\n`;

                try {
                    const res = await fetch('http://127.0.0.1:8001/api/decompress_stream', {
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
            }}>Restore File →</button>
        </div>
    );
}
