import { useState, useEffect } from 'react'
import { apiClient } from '../lib/apiClient'
import type { ApiLogEntry } from '../lib/apiClient'
import { Terminal, ChevronRight, ChevronDown, Clock, Activity, Code, AlertCircle } from 'lucide-react'

export function DevToolsPanel() {
  const [logs, setLogs] = useState<ApiLogEntry[]>([])
  const [selectedLogId, setSelectedLogId] = useState<string | null>(null)
  const [hideHealth, setHideHealth] = useState(true)


  useEffect(() => {
    setLogs(apiClient.getLogs())
    const unsubscribe = apiClient.subscribe(() => {
      setLogs(apiClient.getLogs())
    })
    return unsubscribe
  }, [])

  const filteredLogs = logs.filter(l => !hideHealth || !l.url.includes('/api/health'))
  const selectedLog = logs.find(l => l.id === selectedLogId)


  return (
    <div style={{
      display: 'flex',
      flexDirection: 'column',
      height: '100%',
      background: 'var(--bg-dark)',
      color: 'var(--text)',
      fontFamily: 'var(--font-family)',
      userSelect: 'text',
    }}>
      <div style={{
        padding: '12px 16px',
        borderBottom: '1px solid var(--border)',
        display: 'flex',
        alignItems: 'center',
        gap: 8,
        background: 'rgba(0,0,0,0.2)',
      }}>
        <Terminal size={16} color="var(--accent)" />
        <span style={{ fontWeight: 600, fontSize: 13 }}>API Inspector</span>
        <div style={{ marginLeft: 'auto', display: 'flex', gap: 12, alignItems: 'center' }}>
          <label style={{ display: 'flex', alignItems: 'center', gap: 6, fontSize: 11, cursor: 'pointer', color: hideHealth ? 'var(--accent)' : 'var(--text-muted)' }}>
            <input 
              type="checkbox" 
              checked={hideHealth} 
              onChange={(e) => setHideHealth(e.target.checked)}
              style={{ cursor: 'pointer' }}
            />
            Hide Health
          </label>
          <div style={{ fontSize: 11, color: 'var(--text-muted)', display: 'flex', alignItems: 'center', gap: 4 }}>
            <Activity size={12} />
            {filteredLogs.length} calls
          </div>
        </div>

      </div>

      <div style={{ display: 'flex', flex: 1, overflow: 'hidden' }}>
        {/* Log List */}
        <div style={{
          width: '40%',
          borderRight: '1px solid var(--border)',
          overflowY: 'auto',
          display: 'flex',
          flexDirection: 'column',
        }}>
          {filteredLogs.length === 0 ? (
            <div style={{ padding: 40, textAlign: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
              {logs.length === 0 ? 'No API calls recorded yet.' : 'No calls matching filters.'}
            </div>
          ) : (
            filteredLogs.map((log) => (

              <button
                key={log.id}
                onClick={() => setSelectedLogId(log.id)}
                style={{
                  padding: '10px 12px',
                  border: 'none',
                  borderBottom: '1px solid var(--border)',
                  background: selectedLogId === log.id ? 'rgba(74,158,255,0.1)' : 'transparent',
                  textAlign: 'left',
                  cursor: 'pointer',
                  color: 'inherit',
                  display: 'flex',
                  flexDirection: 'column',
                  gap: 4,
                  width: '100%',
                  transition: 'background 0.15s',
                }}
              >
                <div style={{ display: 'flex', alignItems: 'center', gap: 6, width: '100%' }}>
                  <span style={{
                    fontSize: 10,
                    fontWeight: 800,
                    padding: '2px 6px',
                    borderRadius: 4,
                    background: log.method === 'POST' ? 'rgba(74,222,128,0.1)' : 'rgba(74,158,255,0.1)',
                    color: log.method === 'POST' ? '#4ADE80' : 'var(--accent)',
                  }}>
                    {log.method}
                  </span>
                  <span style={{
                    fontSize: 11,
                    fontWeight: 600,
                    color: log.error ? '#EF4444' : 'var(--text)',
                    overflow: 'hidden',
                    textOverflow: 'ellipsis',
                    whiteSpace: 'nowrap',
                    flex: 1
                  }}>
                    {log.url.replace(/http:\/\/127\.0\.0\.1:8001/, '')}
                  </span>
                </div>
                <div style={{ display: 'flex', justifyContent: 'space-between', fontSize: 10, color: 'var(--text-muted)' }}>
                  <span>{log.timestamp.toLocaleTimeString([], { hour12: false })}</span>
                  <span style={{ display: 'flex', alignItems: 'center', gap: 3 }}>
                    {log.responseStatus && (
                        <span style={{ color: log.responseStatus >= 400 ? '#EF4444' : '#4ADE80' }}>
                           {log.responseStatus}
                        </span>
                    )}
                    {log.duration && ` • ${log.duration}ms`}
                  </span>
                </div>
              </button>
            ))
          )}
        </div>

        {/* Details Panel */}
        <div style={{ flex: 1, overflowY: 'auto', background: 'rgba(0,0,0,0.1)' }}>
          {selectedLog ? (
            <div style={{ padding: 16, display: 'flex', flexDirection: 'column', gap: 20 }}>
              <section>
                <h4 style={{ fontSize: 11, textTransform: 'uppercase', color: 'var(--text-muted)', marginBottom: 8, display: 'flex', alignItems: 'center', gap: 6 }}>
                  <Code size={12} /> General
                </h4>
                <div style={{ background: '#000', padding: 12, borderRadius: 8, fontSize: 12, border: '1px solid var(--border)' }}>
                  <div style={{ marginBottom: 4 }}><span style={{ color: 'var(--text-muted)' }}>URL:</span> {selectedLog.url}</div>
                  <div style={{ marginBottom: 4 }}><span style={{ color: 'var(--text-muted)' }}>Method:</span> {selectedLog.method}</div>
                  <div style={{ marginBottom: 4 }}><span style={{ color: 'var(--text-muted)' }}>Status:</span> <span style={{ color: (selectedLog.responseStatus || 0) >= 400 ? '#EF4444' : '#4ADE80' }}>{selectedLog.responseStatus || 'N/A'}</span></div>
                  <div><span style={{ color: 'var(--text-muted)' }}>Duration:</span> {selectedLog.duration}ms</div>
                </div>
              </section>

              {selectedLog.error && (
                <section>
                  <h4 style={{ fontSize: 11, textTransform: 'uppercase', color: '#EF4444', marginBottom: 8, display: 'flex', alignItems: 'center', gap: 6 }}>
                    <AlertCircle size={12} /> Error
                  </h4>
                  <div style={{ background: 'rgba(239,68,68,0.1)', padding: 12, borderRadius: 8, fontSize: 12, border: '1px solid rgba(239,68,68,0.3)', color: '#EF4444' }}>
                    {selectedLog.error}
                  </div>
                </section>
              )}

              <section>
                <h4 style={{ fontSize: 11, textTransform: 'uppercase', color: 'var(--text-muted)', marginBottom: 8 }}>Request Payload</h4>
                <pre style={{
                  background: '#000',
                  padding: 12,
                  borderRadius: 8,
                  fontSize: 12,
                  border: '1px solid var(--border)',
                  overflowX: 'auto',
                  maxHeight: 300,
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word',
                  userSelect: 'text',
                }}>
                  {JSON.stringify(selectedLog.requestBody, null, 2) || '{}'}
                </pre>
              </section>

              <section>
                <h4 style={{ fontSize: 11, textTransform: 'uppercase', color: 'var(--text-muted)', marginBottom: 8 }}>Response Body</h4>
                <pre style={{
                  background: '#000',
                  padding: 12,
                  borderRadius: 8,
                  fontSize: 12,
                  border: '1px solid var(--border)',
                  overflowX: 'auto',
                  maxHeight: 500,
                  whiteSpace: 'pre-wrap',
                  wordBreak: 'break-word',
                  userSelect: 'text',
                }}>
                  {JSON.stringify(selectedLog.responseBody, null, 2) || (selectedLog.responseStatus ? '{}' : 'Waiting for response...')}
                </pre>
              </section>
            </div>
          ) : (
            <div style={{ height: '100%', display: 'flex', alignItems: 'center', justifyContent: 'center', color: 'var(--text-faint)', fontSize: 12 }}>
              Select an API call to view details
            </div>
          )}
        </div>
      </div>
    </div>
  )
}
