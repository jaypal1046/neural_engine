import { useState, useEffect, useCallback } from 'react'
import { X, CheckCircle, AlertTriangle, AlertCircle, Info } from 'lucide-react'

export type NotificationType = 'success' | 'error' | 'warning' | 'info'

export interface Notification {
    id: string
    type: NotificationType
    title: string
    message?: string
    duration?: number // ms, 0 = manual dismiss
    timestamp: Date
}

interface Props {
    notifications: Notification[]
    onDismiss: (id: string) => void
}

const ICONS = {
    success: { Icon: CheckCircle, color: '#4ADE80' },
    error: { Icon: AlertCircle, color: '#EF4444' },
    warning: { Icon: AlertTriangle, color: '#FBBF24' },
    info: { Icon: Info, color: '#4A9EFF' },
}

export function NotificationManager({ notifications, onDismiss }: Props) {
    return (
        <div className="notification-container">
            {notifications.map((notif) => (
                <NotificationCard key={notif.id} notification={notif} onDismiss={onDismiss} />
            ))}
        </div>
    )
}

function NotificationCard({ notification, onDismiss }: { notification: Notification, onDismiss: (id: string) => void }) {
    const [exiting, setExiting] = useState(false)
    const { Icon, color } = ICONS[notification.type]

    const dismiss = useCallback(() => {
        setExiting(true)
        setTimeout(() => onDismiss(notification.id), 300)
    }, [notification.id, onDismiss])

    useEffect(() => {
        if (notification.duration && notification.duration > 0) {
            const timeout = setTimeout(dismiss, notification.duration)
            return () => clearTimeout(timeout)
        }
    }, [notification.duration, dismiss])

    return (
        <div className={`notification-card ${notification.type} ${exiting ? 'exit' : 'enter'}`}>
            <div className="notification-icon" style={{ color }}>
                <Icon size={16} />
            </div>
            <div className="notification-body">
                <div className="notification-title">{notification.title}</div>
                {notification.message && (
                    <div className="notification-message">{notification.message}</div>
                )}
            </div>
            <button className="notification-close" onClick={dismiss}>
                <X size={14} />
            </button>
            {notification.duration && notification.duration > 0 && (
                <div
                    className="notification-progress"
                    style={{
                        background: color,
                        animationDuration: `${notification.duration}ms`
                    }}
                />
            )}
        </div>
    )
}

// ── Hook for managing notifications ──
let _notifyCounter = 0

export function useNotifications() {
    const [notifications, setNotifications] = useState<Notification[]>([])

    const notify = useCallback((type: NotificationType, title: string, message?: string, duration = 4000) => {
        const id = `notif-${++_notifyCounter}-${Date.now()}`
        setNotifications(prev => [...prev, { id, type, title, message, duration, timestamp: new Date() }])
    }, [])

    const dismiss = useCallback((id: string) => {
        setNotifications(prev => prev.filter(n => n.id !== id))
    }, [])

    return { notifications, notify, dismiss }
}
