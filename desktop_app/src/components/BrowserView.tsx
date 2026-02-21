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
                        fetch("http://127.0.0.1:8001/api/status").then(r => r.json()).then(console.log);
                        if (window.ws) {
                            window.ws.send(JSON.stringify({
                                id: `cmd_${Date.now()}`, type: "command", from: "vscode",
                                command: "browserAgent.navigate", params: { url }
                            }));
                        }
                    }}>Launch Bridge</button>
                </div>
            </div>
            <button className="primary-btn mt-large" onClick={() => {
                const out = document.querySelector('.log-area pre');
                if (out) out.textContent += `\nConnecting Javascript WS to localhost:8080...`;
                window.ws = new WebSocket('ws://localhost:8080');
                window.ws.onopen = () => {
                    if (out) out.textContent += `\n[WS] Connected! Ready to command Chrome.`;
                    window.ws.send(JSON.stringify({
                        id: `conn_${Date.now()}`, type: "command", from: "vscode",
                        command: "browserAgent.connect", params: {}
                    }))
                };
                window.ws.onmessage = (m: any) => {
                    if (out) out.textContent += `\n[Chrome]: ${m.data.substring(0, 100)}...`;
                };
            }}>Connect WS API →</button>
        </div>
    );
}
