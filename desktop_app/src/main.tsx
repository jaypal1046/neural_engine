import { StrictMode, Component } from 'react'
import type { ErrorInfo } from 'react'
import { createRoot } from 'react-dom/client'
import './index.css'
import App from './App.tsx'

// Error boundary to catch any render failures
class ErrorBoundary extends Component<{ children: React.ReactNode }, { error: Error | null }> {
  constructor(props: { children: React.ReactNode }) {
    super(props)
    this.state = { error: null }
  }

  static getDerivedStateFromError(error: Error) {
    return { error }
  }

  componentDidCatch(error: Error, info: ErrorInfo) {
    console.error('[Neural Studio] Render Error:', error, info)
  }

  render() {
    if (this.state.error) {
      return (
        <div style={{
          padding: 40, color: '#F87171', background: '#0D0D0D',
          fontFamily: "'JetBrains Mono', monospace", height: '100vh'
        }}>
          <h2 style={{ color: '#fff' }}>Neural Studio — Render Error</h2>
          <pre style={{ fontSize: 13, whiteSpace: 'pre-wrap', marginTop: 12, color: '#F87171' }}>
            {this.state.error.message}
          </pre>
          <pre style={{ fontSize: 11, color: '#6B7280', marginTop: 8, whiteSpace: 'pre-wrap' }}>
            {this.state.error.stack}
          </pre>
          <button onClick={() => window.location.reload()}
            style={{
              marginTop: 20, padding: '8px 24px', background: '#4A9EFF',
              color: '#fff', border: 'none', borderRadius: 6, cursor: 'pointer', fontSize: 13
            }}>
            Reload
          </button>
        </div>
      )
    }
    return this.props.children
  }
}

createRoot(document.getElementById('root')!).render(
  <StrictMode>
    <ErrorBoundary>
      <App />
    </ErrorBoundary>
  </StrictMode>,
)
