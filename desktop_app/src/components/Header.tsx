import { useState, useEffect } from 'react';
import { Wifi, WifiOff, Cpu } from 'lucide-react';

const API = 'http://127.0.0.1:8001';

export function Header({ activeTab }: { activeTab: string }) {
    const [serverStatus, setServerStatus] = useState<'online' | 'offline' | 'checking'>('checking');
    const [serverInfo, setServerInfo] = useState<any>(null);

    useEffect(() => {
        const check = async () => {
            try {
                const res = await fetch(`${API}/api/health`);
                const data = await res.json();
                setServerStatus(data.status === 'online' ? 'online' : 'offline');
                setServerInfo(data);
            } catch {
                setServerStatus('offline');
            }
        };
        check();
        const interval = setInterval(check, 15000);
        return () => clearInterval(interval);
    }, []);

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
                        {activeTab === 'helper' && 'AI Compression Copilot'}
                    </h2>
                    {(activeTab === 'compress' || activeTab === 'decompress') && (
                        <div style={{ color: "var(--text-faint)", marginTop: 5, fontSize: 13 }}>
                            {activeTab === 'compress' && 'Shrink any file using 1,046 neural advisors and entropy-based algorithms.'}
                            {activeTab === 'decompress' && 'Restore a .myzip archive back to its original file.'}
                        </div>
                    )}
                    {activeTab === 'helper' && (
                        <div style={{ color: "var(--text-faint)", marginTop: 5, fontSize: 13 }}>
                            Compression-aware AI powered by the CMIX neural engine.
                        </div>
                    )}
                </div>

                {/* Right-side server status */}
                <div style={{
                    marginLeft: 'auto', alignSelf: 'center', display: 'flex',
                    alignItems: 'center', gap: 8
                }}>
                    <div style={{
                        display: 'flex', alignItems: 'center', gap: 6,
                        padding: '5px 12px', borderRadius: 20,
                        background: serverStatus === 'online' ? 'rgba(74,222,128,0.08)' : 'rgba(239,68,68,0.08)',
                        border: `1px solid ${serverStatus === 'online' ? 'rgba(74,222,128,0.25)' : 'rgba(239,68,68,0.25)'}`,
                        fontSize: 11, fontWeight: 600,
                        color: serverStatus === 'online' ? '#4ADE80' : '#EF4444',
                        transition: 'all 0.3s'
                    }}>
                        {serverStatus === 'online' ? <Wifi size={11} /> : <WifiOff size={11} />}
                        {serverStatus === 'online' ? 'API Connected' : 'API Offline'}
                    </div>
                    {serverStatus === 'online' && serverInfo && (
                        <div style={{
                            display: 'flex', alignItems: 'center', gap: 4,
                            padding: '5px 10px', borderRadius: 20,
                            background: 'rgba(201,127,219,0.08)', border: '1px solid rgba(201,127,219,0.2)',
                            fontSize: 10, fontWeight: 700, color: '#C97FDB'
                        }}>
                            <Cpu size={10} /> {serverInfo.models} models
                        </div>
                    )}
                </div>
            </div>

            {/* Tag row */}
            <div className="tag-row">
                {activeTab === 'compress' && (
                    <>
                        <span className="tag" style={{ backgroundColor: 'rgba(201,127,219,0.12)', color: '#C97FDB' }}>Neural Engine V10</span>
                        <span className="tag" style={{ backgroundColor: 'rgba(74,222,128,0.1)', color: '#4ADE80' }}>AI Analysis</span>
                    </>
                )}
                {activeTab === 'decompress' && <span className="tag" style={{ backgroundColor: "rgb(22,36,60)", color: "rgb(74,158,255)" }}>Mirror-Mode Restore</span>}
                {activeTab === 'browser' && <span className="tag">Copilot Bridge</span>}
                {activeTab === 'helper' && (
                    <>
                        <span className="tag" style={{ backgroundColor: 'rgba(201,127,219,0.12)', color: '#C97FDB' }}>Compression AI</span>
                        <span className="tag" style={{ backgroundColor: 'rgba(74,158,255,0.1)', color: '#4A9EFF' }}>1,046 Advisors</span>
                    </>
                )}
            </div>

            <hr className="header-divider" />
        </>
    );
}