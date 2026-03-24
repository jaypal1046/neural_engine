export function BrowserView() {
    return (
        <div className="card">
            <h3>Connect Pipeline</h3>
            <hr className="divider" />
            <div className="card-content">
                <div className="field-row">
                    <label>Target URL</label>
                    <input type="text" placeholder="https://google.com" id="target_url" />
                    <button className="ghost-btn" onClick={() => {
                        const url = (document.getElementById('target_url') as HTMLInputElement).value;
                        if ((window as any).ws) {
                            (window as any).ws.send(JSON.stringify({
                                id: `cmd_${Date.now()}`, type: "command", from: "vscode",
                                command: "browserAgent.navigate", params: { url }
                            }));
                        } else {
                            alert("WS API Not Connected!");
                        }
                    }}>Launch Bridge</button>
                </div>
            </div>
            
            <div className="log-area mt-medium" style={{ background: '#1a1a1a', padding: '10px', borderRadius: '4px', minHeight: '100px', maxHeight: '200px', overflowY: 'auto' }}>
                <pre style={{ margin: 0, fontSize: '12px', color: '#00ff00', fontFamily: 'monospace' }}></pre>
            </div>

            <button className="primary-btn mt-medium" onClick={() => {
                const out = document.querySelector('.log-area pre');
                if (out) out.textContent += `\nConnecting to localhost:8080...`;
                
                (window as any).ws = new WebSocket('ws://localhost:8080');
                (window as any).ws.onopen = () => {
                    if (out) out.textContent += `\n[WS] Connected! Ready to command Chrome.`;
                    (window as any).ws.send(JSON.stringify({
                        id: `conn_${Date.now()}`, type: "command", from: "vscode",
                        command: "browserAgent.connect", params: {}
                    }))
                };
                (window as any).ws.onmessage = (m: any) => {
                    if (out) out.textContent += `\n[Bridge]: ${m.data}`;
                    if (out) out.parentElement?.scrollTo(0, out.parentElement.scrollHeight);
                };
                (window as any).ws.onerror = () => {
                   if (out) out.textContent += `\n[WS] Error: Connection failed. Is browser_bridge.py running?`;
                };
            }}>Connect WS API →</button>
        </div>
    );
}
