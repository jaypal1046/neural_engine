export function ConsoleOutput() {
    return (
        <div className="card mt-large">
            <div style={{ display: 'flex', justifyContent: 'space-between' }}>
                <h3>Output</h3>
                <span style={{ fontSize: "12px", color: "var(--text-faint)" }}>Idle</span>
            </div>
            <div style={{ height: "1px", background: "var(--border)", margin: "16px 0", width: "100%", position: "relative" }}>
                <div style={{ position: "absolute", left: 0, top: "-2px", height: "5px", width: "3px", background: "var(--accent)" }}></div>
                <span style={{ position: "absolute", right: 0, top: "-8px", fontSize: "11px", color: "var(--text-faint)", background: "var(--card)", paddingLeft: "8px" }}>0%</span>
            </div>
            <div className="log-area">
                <pre>Awaiting commands...</pre>
            </div>
        </div>
    );
}
