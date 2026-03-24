import { useEffect, useRef, useState } from 'react';
import mermaid from 'mermaid';
import { Copy, AlertCircle, Check } from 'lucide-react';

mermaid.initialize({
  startOnLoad: true,
  theme: 'dark',
  securityLevel: 'loose',
  fontFamily: 'var(--font-family)',
});

interface MermaidProps {
  chart: string;
}

export function Mermaid({ chart }: MermaidProps) {
  const ref = useRef<HTMLDivElement>(null);
  const [error, setError] = useState<string | null>(null);
  const [copied, setCopied] = useState(false);

  useEffect(() => {
    if (ref.current && chart) {
      setError(null);
      ref.current.removeAttribute('data-processed');
      ref.current.innerHTML = chart;
      
      try {
        mermaid.contentLoaded();
      } catch (err) {
        console.error('Mermaid render error:', err);
        setError('Syntax error in diagram. Click the button below to copy the raw text.');
      }
    }
  }, [chart]);

  const copyToClipboard = () => {
    navigator.clipboard.writeText(chart);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  return (
    <div style={{ position: 'relative', margin: '12px 0', borderRadius: 8, overflow: 'hidden', border: '1px solid var(--border)', background: 'rgba(0,0,0,0.2)' }}>
      <div style={{ 
        display: 'flex', 
        justifyContent: 'space-between', 
        alignItems: 'center',
        padding: '6px 12px', 
        background: 'rgba(0,0,0,0.4)', 
        borderBottom: '1px solid var(--border)', 
        fontSize: 11,
        fontWeight: 600,
        color: 'var(--text-muted)' 
      }}>
        <div style={{ display: 'flex', alignItems: 'center', gap: 6 }}>
          <div style={{ width: 8, height: 8, borderRadius: '50%', background: '#C97FDB', opacity: 0.6 }} />
          <span style={{ textTransform: 'uppercase', letterSpacing: '0.05em' }}>Mermaid Diagram</span>
        </div>
        <button 
          onClick={copyToClipboard}
          style={{ 
            background: 'none', 
            border: 'none', 
            color: copied ? 'var(--green)' : 'inherit', 
            cursor: 'pointer',
            display: 'flex',
            alignItems: 'center',
            gap: 5,
            transition: 'all 0.2s',
            padding: '2px 4px'
          }}
          title="Copy diagram source"
        >
          {copied ? <Check size={13} /> : <Copy size={13} />}
          <span>{copied ? 'Copied' : 'Copy Source'}</span>
        </button>
      </div>
      
      <div 
        className="mermaid-container"
        style={{ 
          padding: '1.5rem', 
          overflowX: 'auto',
          display: 'flex',
          flexDirection: 'column',
          alignItems: 'center',
          minHeight: error ? 'auto' : '120px'
        }}
      >
        {error && (
          <div style={{ 
            color: '#EF4444', 
            fontSize: 12, 
            display: 'flex', 
            alignItems: 'center', 
            gap: 8,
            paddingBottom: 16,
            width: '100%',
            fontWeight: 500
          }}>
            <AlertCircle size={14} />
            {error}
          </div>
        )}
        <div className="mermaid" ref={ref} style={{ width: '100%', display: error ? 'none' : 'block' }}>
          {chart}
        </div>
        {error && (
          <pre style={{ 
            fontSize: 11, 
            background: '#000', 
            padding: 12, 
            borderRadius: 6, 
            width: '100%', 
            whiteSpace: 'pre-wrap', 
            color: '#6B7280',
            border: '1px solid rgba(255,255,255,0.05)',
            fontFamily: 'var(--font-code, "JetBrains Mono", monospace)'
          }}>
            {chart}
          </pre>
        )}
      </div>
    </div>
  );
}
