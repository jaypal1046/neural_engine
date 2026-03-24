import { useState, useEffect } from 'react'
import { 
  ShieldCheck, 
  Cpu, 
  Download, 
  ArrowRight, 
  CheckCircle2, 
  AlertTriangle,
  Loader2,
  Terminal,
  BrainCircuit
} from 'lucide-react'
import { apiClient } from '../lib/apiClient'

const API_BASE = 'http://127.0.0.1:8001'

interface SetupWizardProps {
  onComplete: () => void
}

export function SetupWizard({ onComplete }: SetupWizardProps) {
  const [step, setStep] = useState(1)
  const [ollamaStatus, setOllamaStatus] = useState<{
    ollama_running: boolean
    models: string[]
    default_model: string
    error?: string
  } | null>(null)
  const [checking, setChecking] = useState(false)
  const [pulling, setPulling] = useState(false)
  const [pullProgress, setPullProgress] = useState<any>(null)
  const [installingOllama, setInstallingOllama] = useState(false)
  const [installProgress, setInstallProgress] = useState<string>('')

  const checkStatus = async () => {
    setChecking(true)
    try {
      const res = await apiClient.fetch(`${API_BASE}/api/setup/status`)
      if (res.ok) {
        setOllamaStatus(await res.json())
      }
    } catch (e) {
      setOllamaStatus({ ollama_running: false, models: [], default_model: '' })
    } finally {
      setChecking(false)
    }
  }

  useEffect(() => {
    if (step === 2) {
      checkStatus()
    }
  }, [step])

  const pullModel = async (model: string) => {
    setPulling(true)
    try {
      const res = await apiClient.fetch(`${API_BASE}/api/setup/pull-model`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ model })
      })

      const reader = res.body?.getReader()
      const decoder = new TextDecoder()
      let sseBuffer = ''

      if (reader) {
        while (true) {
          const { done, value } = await reader.read()
          if (done) break
          sseBuffer += decoder.decode(value, { stream: true })

          let newlineIndex
          while ((newlineIndex = sseBuffer.indexOf('\n\n')) >= 0) {
            const line = sseBuffer.slice(0, newlineIndex).trim()
            sseBuffer = sseBuffer.slice(newlineIndex + 2)
            if (line.startsWith('data: ')) {
              try {
                const data = JSON.parse(line.slice(6))
                setPullProgress(data)
                if (data.status === 'success') {
                    await checkStatus()
                }
              } catch (e) {}
            }
          }
        }
      }
      await checkStatus()
    } catch (e) {
      console.error('Pull failed', e)
    } finally {
      setPulling(false)
      setPullProgress(null)
    }
  }

  const installOllama = async () => {
    setInstallingOllama(true)
    setInstallProgress('Initializing installation...')
    try {
      const res = await apiClient.fetch(`${API_BASE}/api/setup/install-ollama`, {
        method: 'POST'
      })

      const reader = res.body?.getReader()
      const decoder = new TextDecoder()
      let sseBuffer = ''

      if (reader) {
        while (true) {
          const { done, value } = await reader.read()
          if (done) break
          sseBuffer += decoder.decode(value, { stream: true })

          let newlineIndex
          while ((newlineIndex = sseBuffer.indexOf('\n\n')) >= 0) {
            const line = sseBuffer.slice(0, newlineIndex).trim()
            sseBuffer = sseBuffer.slice(newlineIndex + 2)
            if (line.startsWith('data: ')) {
              try {
                const data = JSON.parse(line.slice(6))
                setInstallProgress(data.message || data.status)
                if (data.status === 'success') {
                  setTimeout(() => checkStatus(), 2000)
                }
              } catch (e) {}
            }
          }
        }
      }
    } catch (e) {
      setInstallProgress('Installation error. Please try manual install.')
    } finally {
      setTimeout(() => {
        setInstallingOllama(false)
        checkStatus()
      }, 3000)
    }
  }

  const isModelInstalled = (model: string) => {
    return ollamaStatus?.models.some(m => m.includes(model))
  }

  return (
    <div style={{
      position: 'fixed',
      inset: 0,
      background: 'rgba(5, 5, 10, 0.95)',
      backdropFilter: 'blur(20px)',
      display: 'flex',
      alignItems: 'center',
      justifyContent: 'center',
      zIndex: 9999,
      fontFamily: 'var(--font-sans)',
      color: 'var(--text)'
    }}>
      <div style={{
        width: '100%',
        maxWidth: 560,
        background: 'rgba(23, 23, 33, 0.7)',
        border: '1px solid rgba(255, 255, 255, 0.1)',
        borderRadius: 24,
        padding: 40,
        boxShadow: '0 25px 50px -12px rgba(0, 0, 0, 0.5)',
        position: 'relative',
        overflow: 'hidden'
      }}>
        {/* Progress Bar */}
        <div style={{
          position: 'absolute',
          top: 0,
          left: 0,
          right: 0,
          height: 4,
          background: 'rgba(255,255,255,0.05)'
        }}>
          <div style={{
            height: '100%',
            width: `${(step / 3) * 100}%`,
            background: 'linear-gradient(90deg, #4F46E5, #9333EA)',
            transition: 'width 0.5s cubic-bezier(0.4, 0, 0.2, 1)'
          }} />
        </div>

        {step === 1 && (
          <div style={{ animation: 'fadeIn 0.5s ease-out' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 24 }}>
              <div style={{ 
                width: 48, height: 48, borderRadius: 12, 
                background: 'linear-gradient(135deg, #4F46E5, #9333EA)',
                display: 'flex', alignItems: 'center', justifyContent: 'center'
              }}>
                <ShieldCheck size={28} color="#fff" />
              </div>
              <h1 style={{ fontSize: 24, fontWeight: 800, margin: 0 }}>Privacy & Terms</h1>
            </div>
            
            <p style={{ color: 'var(--text-muted)', lineHeight: 1.6, marginBottom: 24 }}>
              Welcome to Neural Studio. To provide local AI capabilities, we connect to your local Ollama instance. Your data stays on your device.
            </p>

            <div style={{ 
              background: 'rgba(0,0,0,0.2)', 
              borderRadius: 12, 
              padding: 16, 
              fontSize: 13, 
              color: 'var(--text-muted)',
              maxHeight: 200,
              overflowY: 'auto',
              border: '1px solid var(--border)',
              marginBottom: 32
            }}>
              <p>• All AI processing is performed locally via your device's GPU/CPU.</p>
              <p>• Code context is indexed locally and never uploaded to external servers.</p>
              <p>• You maintain full control over your models and data.</p>
              <p>• By continuing, you agree to our local-first data processing policy.</p>
            </div>

            <button 
              onClick={() => setStep(2)}
              className="setup-button-primary"
              style={{
                width: '100%',
                padding: '14px',
                borderRadius: 12,
                background: 'var(--accent)',
                color: '#fff',
                border: 'none',
                fontWeight: 600,
                fontSize: 15,
                display: 'flex',
                alignItems: 'center',
                justifyContent: 'center',
                gap: 8,
                cursor: 'pointer',
                transition: 'all 0.2s'
              }}
            >
              I Accept & Continue <ArrowRight size={18} />
            </button>
          </div>
        )}

        {step === 2 && (
          <div style={{ animation: 'fadeIn 0.5s ease-out' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 24 }}>
              <div style={{ 
                width: 48, height: 48, borderRadius: 12, 
                background: ollamaStatus?.ollama_running ? 'var(--green-soft)' : 'var(--amber-soft)',
                display: 'flex', alignItems: 'center', justifyContent: 'center'
              }}>
                <Cpu size={28} color={ollamaStatus?.ollama_running ? 'var(--green)' : 'var(--amber)'} />
              </div>
              <h1 style={{ fontSize: 24, fontWeight: 800, margin: 0 }}>Ollama Environment</h1>
            </div>

            <div style={{ 
              padding: 24, 
              background: 'rgba(255,255,255,0.03)', 
              borderRadius: 16, 
              border: '1px solid var(--border)',
              marginBottom: 32,
              textAlign: 'center'
            }}>
              {checking ? (
                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 12 }}>
                  <Loader2 className="animate-spin" size={32} color="var(--accent)" />
                  <span style={{ fontSize: 14, color: 'var(--text-muted)' }}>Detecting local environment...</span>
                </div>
              ) : ollamaStatus?.ollama_running ? (
                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 12 }}>
                  <CheckCircle2 size={40} color="var(--green)" />
                  <span style={{ fontSize: 16, fontWeight: 600 }}>Ollama is running!</span>
                  <span style={{ fontSize: 13, color: 'var(--text-muted)' }}>Found {ollamaStatus.models.length} local models.</span>
                </div>
              ) : (
                <div style={{ display: 'flex', flexDirection: 'column', alignItems: 'center', gap: 12 }}>
                  <AlertTriangle size={40} color="var(--amber)" />
                  <span style={{ fontSize: 16, fontWeight: 600 }}>Ollama not detected</span>
                  <p style={{ fontSize: 13, color: 'var(--text-muted)', margin: 0 }}>
                    Please install Ollama from <a href="https://ollama.com" target="_blank" style={{ color: 'var(--accent)' }}>ollama.com</a> and ensure it is running.
                  </p>
                  
                  <div style={{ display: 'flex', gap: 10, marginTop: 12 }}>
                    <button 
                      onClick={checkStatus}
                      style={{
                        padding: '10px 20px',
                        borderRadius: 10,
                        background: 'rgba(255,255,255,0.05)',
                        border: '1px solid var(--border)',
                        color: 'var(--text)',
                        fontSize: 13,
                        fontWeight: 600,
                        cursor: 'pointer'
                      }}
                    >
                      Retry Detection
                    </button>
                    
                    <button 
                      onClick={installOllama}
                      disabled={installingOllama}
                      style={{
                        padding: '10px 20px',
                        borderRadius: 10,
                        background: 'var(--accent)',
                        color: '#fff',
                        border: 'none',
                        fontSize: 13,
                        fontWeight: 600,
                        cursor: installingOllama ? 'wait' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: 8
                      }}
                    >
                      {installingOllama ? <Loader2 className="animate-spin" size={16} /> : <Download size={16} />}
                      Install Ollama (Auto)
                    </button>
                  </div>

                  {installingOllama && (
                    <div style={{ marginTop: 16, width: '100%', animation: 'fadeIn 0.3s' }}>
                        <div style={{ fontSize: 11, color: 'var(--accent)', marginBottom: 6, fontWeight: 500 }}>
                            {installProgress}
                        </div>
                        <div style={{ height: 4, background: 'rgba(255,255,255,0.05)', borderRadius: 2, overflow: 'hidden' }}>
                            <div style={{ 
                                height: '100%', 
                                width: '100%',
                                background: 'linear-gradient(90deg, transparent, var(--accent), transparent)',
                                animation: 'shimmer 1.5s infinite linear',
                                backgroundSize: '200% 100%'
                            }} />
                        </div>
                    </div>
                  )}

                  {ollamaStatus?.error && !installingOllama && (
                    <div style={{ 
                        marginTop: 12, 
                        fontSize: 11, 
                        color: 'rgba(239, 68, 68, 0.9)', 
                        background: 'rgba(239, 68, 68, 0.1)', 
                        padding: '8px 12px', 
                        borderRadius: 8, 
                        fontFamily: 'monospace',
                        border: '1px solid rgba(239, 68, 68, 0.2)',
                        textAlign: 'left',
                        maxWidth: '100%',
                        overflowX: 'auto'
                    }}>
                        {ollamaStatus.error}
                    </div>
                  )}
                </div>
              )}
            </div>

            <div style={{ display: 'flex', gap: 12 }}>
              <button 
                onClick={() => setStep(1)}
                style={{
                  flex: 1,
                  padding: '14px',
                  borderRadius: 12,
                  background: 'rgba(0,0,0,0.2)',
                  color: 'var(--text-muted)',
                  border: '1px solid var(--border)',
                  fontWeight: 600,
                  cursor: 'pointer'
                }}
              >
                Back
              </button>
              <button 
                disabled={!ollamaStatus?.ollama_running}
                onClick={() => setStep(3)}
                style={{
                  flex: 2,
                  padding: '14px',
                  borderRadius: 12,
                  background: ollamaStatus?.ollama_running ? 'var(--accent)' : 'rgba(255,255,255,0.05)',
                  color: ollamaStatus?.ollama_running ? '#fff' : 'rgba(255,255,255,0.2)',
                  border: 'none',
                  fontWeight: 600,
                  cursor: ollamaStatus?.ollama_running ? 'pointer' : 'not-allowed',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: 8
                }}
              >
                Configure Models <ArrowRight size={18} />
              </button>
            </div>
          </div>
        )}

        {step === 3 && (
          <div style={{ animation: 'fadeIn 0.5s ease-out' }}>
            <div style={{ display: 'flex', alignItems: 'center', gap: 12, marginBottom: 24 }}>
              <div style={{ 
                width: 48, height: 48, borderRadius: 12, 
                background: 'var(--accent-soft)',
                display: 'flex', alignItems: 'center', justifyContent: 'center'
              }}>
                <BrainCircuit size={28} color="var(--accent)" />
              </div>
              <h1 style={{ fontSize: 24, fontWeight: 800, margin: 0 }}>Model Setup</h1>
            </div>

            <p style={{ color: 'var(--text-muted)', fontSize: 13, marginBottom: 20 }}>
              Choose a base model to start. We recommend **Qwen 2.5 Coder** for the best performance.
            </p>

            <div style={{ display: 'flex', flexDirection: 'column', gap: 12, marginBottom: 32 }}>
              {[
                { name: 'qwen2.5-coder:7b', label: 'Qwen 2.5 Coder (7B)', desc: 'Optimized for RAG & Coding' },
                { name: 'llama3:latest', label: 'Llama 3', desc: 'General Purpose Intelligence' }
              ].map(model => (
                <div 
                  key={model.name}
                  style={{
                    padding: 16,
                    background: 'rgba(255,255,255,0.03)',
                    border: '1px solid var(--border)',
                    borderRadius: 12,
                    display: 'flex',
                    alignItems: 'center',
                    justifyContent: 'space-between'
                  }}
                >
                  <div>
                    <div style={{ fontSize: 14, fontWeight: 700 }}>{model.label}</div>
                    <div style={{ fontSize: 12, color: 'var(--text-muted)' }}>{model.desc}</div>
                  </div>
                  {isModelInstalled(model.name) ? (
                    <div style={{ color: 'var(--green)', display: 'flex', alignItems: 'center', gap: 4, fontSize: 12, fontWeight: 600 }}>
                      <CheckCircle2 size={16} /> Ready
                    </div>
                  ) : (
                    <button 
                      disabled={pulling}
                      onClick={() => pullModel(model.name)}
                      style={{
                        padding: '6px 12px',
                        borderRadius: 6,
                        background: 'var(--accent)',
                        color: '#fff',
                        border: 'none',
                        fontSize: 12,
                        fontWeight: 600,
                        cursor: pulling ? 'wait' : 'pointer',
                        display: 'flex',
                        alignItems: 'center',
                        gap: 6
                      }}
                    >
                      {pulling ? <Loader2 className="animate-spin" size={14} /> : <Download size={14} />}
                      Download
                    </button>
                  )}
                </div>
              ))}
            </div>

            {pulling && pullProgress && (
              <div style={{ marginBottom: 24, padding: 16, background: 'rgba(0,0,0,0.2)', borderRadius: 12, border: '1px solid var(--border)' }}>
                <div style={{ display: 'flex', justifyContent: 'space-between', marginBottom: 8, fontSize: 12 }}>
                  <span style={{ color: 'var(--text-muted)' }}>{pullProgress.status}</span>
                  {pullProgress.completed && pullProgress.total && (
                    <span style={{ color: 'var(--accent)' }}>
                      {Math.round((pullProgress.completed / pullProgress.total) * 100)}%
                    </span>
                  )}
                </div>
                <div style={{ height: 4, background: 'rgba(255,255,255,0.05)', borderRadius: 2, overflow: 'hidden' }}>
                  <div style={{ 
                    height: '100%', 
                    width: `${pullProgress.completed && pullProgress.total ? (pullProgress.completed / pullProgress.total) * 100 : 0}%`,
                    background: 'var(--accent)',
                    transition: 'width 0.3s ease'
                  }} />
                </div>
              </div>
            )}

            <div style={{ display: 'flex', gap: 12 }}>
              <button 
                onClick={() => setStep(2)}
                style={{
                  flex: 1,
                  padding: '14px',
                  borderRadius: 12,
                  background: 'rgba(0,0,0,0.2)',
                  color: 'var(--text-muted)',
                  border: '1px solid var(--border)',
                  fontWeight: 600,
                  cursor: 'pointer'
                }}
              >
                Back
              </button>
              <button 
                onClick={onComplete}
                style={{
                  flex: 2,
                  padding: '14px',
                  borderRadius: 12,
                  background: 'var(--accent)',
                  color: '#fff',
                  border: 'none',
                  fontWeight: 600,
                  cursor: 'pointer',
                  display: 'flex',
                  alignItems: 'center',
                  justifyContent: 'center',
                  gap: 8
                }}
              >
                Start Using Neural Studio <ArrowRight size={18} />
              </button>
            </div>
          </div>
        )}
      </div>

      <style>{`
        @keyframes fadeIn {
          from { opacity: 0; transform: translateY(10px); }
          to { opacity: 1; transform: translateY(0); }
        }
        .animate-spin { animation: spin 1s linear infinite; }
        @keyframes spin { from { transform: rotate(0deg); } to { transform: rotate(360deg); } }
        @keyframes shimmer {
            0% { background-position: 200% 0; }
            100% { background-position: -200% 0; }
        }
      `}</style>
    </div>
  )
}
