export function SearchView() {
    return (
        <div className="card">
            <h3>Fast Search Engine</h3>
            <hr className="divider" />
            <div className="card-content">
                <div style={{ marginTop: "16px" }}>
                    <p style={{ color: "var(--text-sub)", fontSize: "14px", lineHeight: "1.5" }}>
                        Neural Studio's ultra-fast embedded search allows you to index local directories
                        and perform full-text regex searches across massive codebases in milliseconds.
                    </p>
                </div>
                <div className="field-row">
                    <input type="text" placeholder="Search query (regex supported)..." style={{ fontSize: "15px", padding: "14px" }} />
                    <button className="primary-btn" style={{ width: "auto", marginTop: 0, marginLeft: "12px", padding: "14px 24px" }}>Search</button>
                </div>
                <div className="log-area" style={{ marginTop: "24px", minHeight: "300px" }}>
                    <pre style={{ color: "var(--text-faint)" }}>No active search. Indexed 0 files.</pre>
                </div>
            </div>
        </div>
    );
}
