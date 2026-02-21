export function Header({ activeTab }: { activeTab: string }) {
    return (
        <>
            <div className="header">
                <div>
                    <h2>
                        {activeTab === 'compress' && 'Compress a File'}
                        {activeTab === 'decompress' && 'Decompress Archive'}
                        {activeTab === 'search' && 'Accelerated Search'}
                        {activeTab === 'browser' && 'Web Automator'}
                        {activeTab === 'scripts' && 'Script Environment'}
                        {activeTab === 'helper' && 'Command Panel'}
                    </h2>
                    {(activeTab === 'compress' || activeTab === 'decompress') && (
                        <div style={{ color: "var(--text-faint)", marginTop: 5, fontSize: 13 }}>
                            {activeTab === 'compress' && 'Shrink any file using neural and entropy-based algorithms.'}
                            {activeTab === 'decompress' && 'Restore a .myzip archive back to its original file.'}
                        </div>
                    )}
                </div>

                {/* Right-side badge */}
                {activeTab === 'helper' && (
                    <div className="toggle" style={{ marginLeft: "auto", alignSelf: 'center' }}>
                        Web Bridge: <span className="toggle-badge">active</span>
                    </div>
                )}
            </div>

            {/* Tag row — only for tabs that need it */}
            <div className="tag-row">
                {activeTab === 'compress' && <span className="tag">Neural Engine</span>}
                {activeTab === 'decompress' && <span className="tag" style={{ backgroundColor: "rgb(22,36,60)", color: "rgb(74,158,255)" }}>Restore</span>}
                {activeTab === 'browser' && <span className="tag">Copilot Bridge</span>}
            </div>

            <hr className="header-divider" />
        </>
    );
}