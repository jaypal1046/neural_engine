import { useState, useEffect } from 'react'
import { TrendingUp, Brain, Zap, Target, Award, Activity, RefreshCw } from 'lucide-react'

const API = 'http://127.0.0.1:8001'

interface TrainingStats {
    ai_score: number
    knowledge_items: number
    total_words: number
    weak_responses: number
    corrections_made: number
    training_runs: number
    last_trained: string | null
    auto_learning_enabled: boolean
    rlhf_iterations: number
    advanced_reasoning_used: number
}

export function AIStatsPanel() {
    const [stats, setStats] = useState<TrainingStats | null>(null)
    const [loading, setLoading] = useState(true)
    const [autoRefresh, setAutoRefresh] = useState(true)

    const fetchStats = async () => {
        try {
            const res = await fetch(`${API}/api/brain/stats`)
            if (res.ok) {
                const data = await res.json()

                // Override auto_learning_enabled with actual localStorage value
                // (backend doesn't have access to browser localStorage)
                const settingsJson = localStorage.getItem('neural-studio-settings')
                if (settingsJson) {
                    try {
                        const settings = JSON.parse(settingsJson)
                        data.auto_learning_enabled = settings['auto-learning'] !== false
                    } catch {
                        // Invalid JSON, use backend default
                    }
                }

                setStats(data)
            }
        } catch (err) {
            console.error('Failed to fetch stats:', err)
        } finally {
            setLoading(false)
        }
    }

    useEffect(() => {
        fetchStats()
        if (autoRefresh) {
            const interval = setInterval(fetchStats, 5000) // Refresh every 5s
            return () => clearInterval(interval)
        }
    }, [autoRefresh])

    if (loading) {
        return (
            <div style={{ display: 'flex', justifyContent: 'center', alignItems: 'center', height: '100%' }}>
                <div style={{ textAlign: 'center', color: 'var(--text-muted)' }}>
                    <RefreshCw size={32} style={{ animation: 'spin 1s linear infinite' }} />
                    <div style={{ marginTop: 12, fontSize: 13 }}>Loading training stats...</div>
                </div>
            </div>
        )
    }

    if (!stats) {
        return (
            <div style={{ padding: 24, textAlign: 'center', color: 'var(--text-muted)' }}>
                <Brain size={48} style={{ opacity: 0.3, margin: '0 auto 16px' }} />
                <div>No training stats available</div>
                <button
                    onClick={fetchStats}
                    style={{
                        marginTop: 16, padding: '8px 16px', borderRadius: 6,
                        background: 'var(--accent)', border: 'none', color: '#fff',
                        cursor: 'pointer', fontFamily: 'inherit', fontSize: 13
                    }}
                >
                    Retry
                </button>
            </div>
        )
    }

    // Safety: Ensure ai_score exists and is a number
    const aiScore = stats.ai_score !== undefined && stats.ai_score !== null ? stats.ai_score : 0
    const scoreColor = aiScore >= 90 ? '#4ADE80' : aiScore >= 75 ? '#FBBF24' : '#EF4444'
    const scoreGrade = aiScore >= 90 ? 'A+' : aiScore >= 85 ? 'A' : aiScore >= 75 ? 'B' : aiScore >= 65 ? 'C' : 'D'

    return (
        <div style={{ height: '100%', overflow: 'auto', background: 'var(--bg-surface)', padding: 24 }}>
            {/* Header */}
            <div style={{ display: 'flex', alignItems: 'center', justifyContent: 'space-between', marginBottom: 24 }}>
                <div>
                    <h1 style={{ fontSize: 24, fontWeight: 700, margin: 0, display: 'flex', alignItems: 'center', gap: 12 }}>
                        <Brain size={28} style={{ color: 'var(--accent)' }} />
                        AI Training Dashboard
                    </h1>
                    <div style={{ fontSize: 13, color: 'var(--text-muted)', marginTop: 4 }}>
                        Real-time performance metrics and self-learning progress
                    </div>
                </div>
                <button
                    onClick={() => setAutoRefresh(!autoRefresh)}
                    style={{
                        display: 'flex', alignItems: 'center', gap: 6, padding: '6px 12px',
                        borderRadius: 6, border: '1px solid var(--border)',
                        background: autoRefresh ? 'var(--accent-soft)' : 'var(--bg-dark)',
                        color: autoRefresh ? 'var(--accent)' : 'var(--text-muted)',
                        cursor: 'pointer', fontSize: 12, fontFamily: 'inherit'
                    }}
                >
                    <RefreshCw size={14} style={{ animation: autoRefresh ? 'spin 2s linear infinite' : 'none' }} />
                    Auto-refresh {autoRefresh ? 'ON' : 'OFF'}
                </button>
            </div>

            {/* AI Score - Big card */}
            <div style={{
                background: 'linear-gradient(135deg, var(--accent-soft) 0%, var(--bg-card) 100%)',
                borderRadius: 12, padding: 32, marginBottom: 24,
                border: '1px solid var(--border)', position: 'relative', overflow: 'hidden'
            }}>
                <div style={{
                    position: 'absolute', top: 0, right: 0, fontSize: 180, fontWeight: 900,
                    color: 'rgba(255,255,255,0.03)', lineHeight: 1, pointerEvents: 'none', userSelect: 'none'
                }}>
                    {scoreGrade}
                </div>
                <div style={{ position: 'relative', zIndex: 1 }}>
                    <div style={{ fontSize: 13, fontWeight: 600, color: 'var(--text-secondary)', textTransform: 'uppercase', letterSpacing: 1 }}>
                        Overall AI Score
                    </div>
                    <div style={{ display: 'flex', alignItems: 'baseline', gap: 12, marginTop: 12 }}>
                        <div style={{ fontSize: 72, fontWeight: 900, color: scoreColor, lineHeight: 1 }}>
                            {aiScore}%
                        </div>
                        <div style={{ fontSize: 48, fontWeight: 700, color: 'var(--text-muted)' }}>
                            {scoreGrade}
                        </div>
                    </div>
                    <div style={{ marginTop: 16, display: 'flex', gap: 12 }}>
                        <div style={{
                            padding: '6px 12px', borderRadius: 8, background: 'var(--bg-dark)',
                            fontSize: 12, color: 'var(--text-muted)'
                        }}>
                            Target: <span style={{ color: 'var(--text)', fontWeight: 600 }}>95%+</span>
                        </div>
                        <div style={{
                            padding: '6px 12px', borderRadius: 8, background: stats.auto_learning_enabled ? 'rgba(74,222,128,0.15)' : 'var(--bg-dark)',
                            fontSize: 12, color: stats.auto_learning_enabled ? '#4ADE80' : 'var(--text-muted)',
                            border: '1px solid',
                            borderColor: stats.auto_learning_enabled ? 'rgba(74,222,128,0.3)' : 'transparent'
                        }}>
                            {stats.auto_learning_enabled ? '✓ Auto-Learning Active' : 'Auto-Learning Disabled'}
                        </div>
                    </div>
                </div>
            </div>

            {/* Stats grid */}
            <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: 16, marginBottom: 24 }}>
                <StatCard
                    icon={<TrendingUp size={20} />}
                    label="Knowledge Items"
                    value={(stats.knowledge_items || 0).toLocaleString()}
                    subtitle={`${(stats.total_words || 0).toLocaleString()} words total`}
                    color="#4A9EFF"
                />
                <StatCard
                    icon={<Target size={20} />}
                    label="Weak Responses"
                    value={(stats.weak_responses || 0).toLocaleString()}
                    subtitle="Auto-detected & flagged"
                    color="#FBBF24"
                />
                <StatCard
                    icon={<Zap size={20} />}
                    label="Corrections Made"
                    value={(stats.corrections_made || 0).toLocaleString()}
                    subtitle="Learned from errors"
                    color="#C97FDB"
                />
                <StatCard
                    icon={<Award size={20} />}
                    label="Training Runs"
                    value={(stats.training_runs || 0).toLocaleString()}
                    subtitle={stats.last_trained || 'Never trained'}
                    color="#4ADE80"
                />
                <StatCard
                    icon={<Activity size={20} />}
                    label="RLHF Iterations"
                    value={(stats.rlhf_iterations || 0).toLocaleString()}
                    subtitle="Alignment training cycles"
                    color="#FF6B9D"
                />
                <StatCard
                    icon={<Brain size={20} />}
                    label="Advanced Reasoning"
                    value={(stats.advanced_reasoning_used || 0).toLocaleString()}
                    subtitle="ToT + Debate + Reflection"
                    color="#4ADE80"
                />
            </div>

            {/* Training Actions */}
            <div style={{
                background: 'var(--bg-card)', borderRadius: 12, padding: 24,
                border: '1px solid var(--border)'
            }}>
                <div style={{ fontSize: 16, fontWeight: 700, marginBottom: 16, display: 'flex', alignItems: 'center', gap: 8 }}>
                    <Zap size={18} style={{ color: 'var(--accent)' }} />
                    Training Actions
                </div>
                <div style={{ display: 'grid', gridTemplateColumns: 'repeat(auto-fit, minmax(200px, 1fr))', gap: 12 }}>
                    <ActionButton label="Run Auto-Learning" description="Start self-learning daemon" color="#4A9EFF" />
                    <ActionButton label="RLHF Training" description="SFT + Reward Model + PPO" color="#C97FDB" />
                    <ActionButton label="Self-Assess" description="Test AI on 20 questions" color="#FBBF24" />
                    <ActionButton label="View Logs" description="See training history" color="#4ADE80" />
                </div>
            </div>

            {/* Progress indicator */}
            <div style={{ marginTop: 24, fontSize: 11, textAlign: 'center', color: 'var(--text-faint)' }}>
                Last updated: {new Date().toLocaleTimeString()} · Auto-refresh every 5s
            </div>
        </div>
    )
}

function StatCard({ icon, label, value, subtitle, color }: {
    icon: React.ReactNode
    label: string
    value: string
    subtitle: string
    color: string
}) {
    return (
        <div style={{
            background: 'var(--bg-card)', borderRadius: 10, padding: 16,
            border: '1px solid var(--border)', transition: 'all 0.2s',
            cursor: 'default'
        }}
            onMouseEnter={e => {
                e.currentTarget.style.borderColor = color
                e.currentTarget.style.transform = 'translateY(-2px)'
            }}
            onMouseLeave={e => {
                e.currentTarget.style.borderColor = 'var(--border)'
                e.currentTarget.style.transform = 'translateY(0)'
            }}
        >
            <div style={{ display: 'flex', alignItems: 'center', gap: 8, marginBottom: 12, color }}>
                {icon}
                <div style={{ fontSize: 11, fontWeight: 600, textTransform: 'uppercase', letterSpacing: 0.5 }}>
                    {label}
                </div>
            </div>
            <div style={{ fontSize: 28, fontWeight: 700, color: 'var(--text)', lineHeight: 1 }}>
                {value}
            </div>
            <div style={{ fontSize: 11, color: 'var(--text-muted)', marginTop: 6 }}>
                {subtitle}
            </div>
        </div>
    )
}

function ActionButton({ label, description, color }: {
    label: string
    description: string
    color: string
}) {
    return (
        <button style={{
            background: 'var(--bg-dark)', borderRadius: 8, padding: '12px 16px',
            border: '1px solid var(--border)', cursor: 'pointer', textAlign: 'left',
            fontFamily: 'inherit', transition: 'all 0.15s'
        }}
            onMouseEnter={e => {
                e.currentTarget.style.borderColor = color
                e.currentTarget.style.background = `${color}10`
            }}
            onMouseLeave={e => {
                e.currentTarget.style.borderColor = 'var(--border)'
                e.currentTarget.style.background = 'var(--bg-dark)'
            }}
        >
            <div style={{ fontSize: 13, fontWeight: 600, color: 'var(--text)', marginBottom: 4 }}>
                {label}
            </div>
            <div style={{ fontSize: 11, color: 'var(--text-muted)' }}>
                {description}
            </div>
        </button>
    )
}
