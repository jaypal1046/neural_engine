import { ArrowDownToLine, ArrowUpFromLine, Search, Globe, Zap, BrainCircuit } from 'lucide-react'

interface Props {
    activeTab: string;
    setActiveTab: (tab: string) => void;
}

export function Sidebar({ activeTab, setActiveTab }: Props) {
    return (
        <nav className="sidebar">
            <div className="logo">
                <h1>Neural <span>Studio</span></h1>
                <p style={{ display: 'flex', alignItems: 'center', gap: 4 }}>
                    V10 · Compression AI Engine
                    <span style={{
                        fontSize: 8, fontWeight: 700, padding: '1px 5px', borderRadius: 6,
                        background: 'rgba(201,127,219,0.15)', color: '#C97FDB',
                        border: '1px solid rgba(201,127,219,0.3)', marginLeft: 2
                    }}>AI</span>
                </p>
            </div>
            <hr className="divider" />
            <ul className="nav-links">
                <li className={activeTab === 'helper' ? 'active' : ''} onClick={() => setActiveTab('helper')}>
                    <BrainCircuit size={16} style={{ marginRight: '12px' }} /> AI Copilot
                </li>
                <li className={activeTab === 'compress' ? 'active' : ''} onClick={() => setActiveTab('compress')}>
                    <ArrowDownToLine size={16} style={{ marginRight: '12px' }} /> Compress
                </li>
                <li className={activeTab === 'decompress' ? 'active' : ''} onClick={() => setActiveTab('decompress')}>
                    <ArrowUpFromLine size={16} style={{ marginRight: '12px' }} /> Decompress
                </li>
                <li className={activeTab === 'search' ? 'active' : ''} onClick={() => setActiveTab('search')}>
                    <Search size={16} style={{ marginRight: '12px' }} /> Fast Search
                </li>
                <li className={activeTab === 'browser' ? 'active' : ''} onClick={() => setActiveTab('browser')}>
                    <Globe size={16} style={{ marginRight: '12px' }} /> Web Bridge
                </li>
                <li className={activeTab === 'scripts' ? 'active' : ''} onClick={() => setActiveTab('scripts')}>
                    <Zap size={16} style={{ marginRight: '12px' }} /> Scripts
                </li>
            </ul>
            <div className="sidebar-bottom">
                <hr className="divider" />
                <p className="version">v10.0 &middot; Neural Studio &middot; 1,046 Advisors</p>
            </div>
        </nav>
    )
}
