import { ArrowDownToLine, ArrowUpFromLine, Search, Globe, Zap, Sparkles } from 'lucide-react'

interface Props {
    activeTab: string;
    setActiveTab: (tab: string) => void;
}

export function Sidebar({ activeTab, setActiveTab }: Props) {
    return (
        <nav className="sidebar">
            <div className="logo">
                <h1>Neural <span>Studio</span></h1>
                <p>Your personal compression & AI tool</p>
            </div>
            <hr className="divider" />
            <ul className="nav-links">
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
                <li className={activeTab === 'helper' ? 'active' : ''} onClick={() => setActiveTab('helper')}>
                    <Sparkles size={16} style={{ marginRight: '12px' }} /> AI Assistant
                </li>
            </ul>
            <div className="sidebar-bottom">
                <hr className="divider" />
                <p className="label">APPEARANCE</p>
                <select className="theme-select">
                    <option>Light</option>
                    <option>Dark</option>
                    <option>System</option>
                </select>
                <p className="version">v2.0 &middot; Neural Studio</p>
            </div>
        </nav>
    )
}
