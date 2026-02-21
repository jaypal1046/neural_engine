export function ScriptsView() {
    return (
        <div className="card">
            <h3>Script Runner</h3>
            <hr className="divider" />
            <div className="card-content">
                <div style={{ marginTop: "16px", marginBottom: "20px" }}>
                    <p style={{ color: "var(--text-sub)", fontSize: "14px", lineHeight: "1.5" }}>
                        Execute automated batch scripts, Neural workflows, or Python extensions.
                    </p>
                </div>
                <div className="field-row">
                    <label>Script Path</label>
                    <input type="text" placeholder="Select a script file (.bat, .py, .sh)..." id="script_file" />
                    <button className="ghost-btn" onClick={async () => {
                        const file = await window.ipcRenderer.selectFile();
                        if (file) {
                            (document.getElementById('script_file') as HTMLInputElement).value = file;
                        }
                    }}>Browse</button>
                </div>
            </div>
            <button className="primary-btn mt-large" onClick={() => {
                const out = document.querySelector('.log-area pre');
                if (out) out.textContent += `\nExecuting script...`;
            }}>Run Script</button>

            <div className="log-area" style={{ marginTop: "24px" }}>
                <pre style={{ color: "var(--text-faint)" }}>Ready.</pre>
            </div>
        </div>
    );
}
