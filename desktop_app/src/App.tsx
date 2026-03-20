import { useState, useEffect, useRef, useCallback } from 'react'
import './App.css'

// Components
import { ActivityBar } from './components/ActivityBar'
import { FileExplorer } from './components/FileExplorer'
import { ExtensionsPanel } from './components/ExtensionsPanel'
import { GitPanel } from './components/GitPanel'
import { MCPPanel } from './components/MCPPanel'
import { EditorTabs } from './components/EditorTabs'
import { WebViewPanel } from './components/WebViewPanel'
import { AIChatPanel } from './components/AIChatPanel'
import { TerminalPanel } from './components/TerminalPanel'
import { StatusBar } from './components/StatusBar'
import { CommandPalette } from './components/CommandPalette'
import { WelcomeScreen } from './components/WelcomeScreen'
import { SettingsPanel } from './components/SettingsPanel'
import { SearchPanel } from './components/SearchPanel'
import { MonacoEditor } from './components/MonacoEditor'
import { GitDiffEditor } from './components/GitDiffEditor'
import { RunPanel } from './components/RunPanel'
import { QuickOpen } from './components/QuickOpen'
import { NotificationManager, useNotifications } from './components/NotificationManager'
import { AIStatsPanel } from './components/AIStatsPanel'
import { DocumentViewer } from './components/DocumentViewer'
import { ProjectMemoryPanel } from './components/ProjectMemoryPanel'

declare global {
  interface Window {
    ws: any;
    ipcRenderer: any;
    fs: any;
    terminal: any;
    shell: any;
    appApi: any;
    searchApi: any;
    gitApi: any;
  }
}

export interface TabItem {
  id: string;
  label: string;
  type: 'welcome' | 'webview' | 'file' | 'compress' | 'decompress' | 'search' | 'scripts' | 'ai-chat' | 'settings' | 'git-diff';
  filePath?: string;
  url?: string;
  icon?: string;
  modified?: boolean;
}

function App() {
  // ── Activity bar state ──
  const [activePanel, setActivePanel] = useState<string>('explorer')
  const [sidebarOpen, setSidebarOpen] = useState(true)
  const [sidebarWidth, setSidebarWidth] = useState(260)

  // ── Editor tabs ──
  const [tabs, setTabs] = useState<TabItem[]>([
    { id: 'welcome', label: 'Welcome', type: 'welcome' }
  ])
  const [activeTabId, setActiveTabId] = useState('welcome')

  // ── Bottom panel ──
  const [bottomPanelOpen, setBottomPanelOpen] = useState(true)
  const [bottomPanelHeight, setBottomPanelHeight] = useState(260)
  const [activeBottomTab, setActiveBottomTab] = useState('terminal')

  // ── Command palette ──
  const [commandPaletteOpen, setCommandPaletteOpen] = useState(false)

  // ── Zen Mode ──
  const [zenMode, setZenMode] = useState(false)

  // ── Quick Open ──
  const [quickOpenOpen, setQuickOpenOpen] = useState(false)
  const [quickOpenMode, setQuickOpenMode] = useState<'file' | 'line' | 'symbol'>('file')
  const [quickOpenQuery, setQuickOpenQuery] = useState('')

  // ── Menu dropdowns ──
  const [openMenu, setOpenMenu] = useState<string | null>(null)

  // ── Notifications ──
  const { notifications, notify, dismiss } = useNotifications()

  // ── Server status ──
  const [serverStatus, setServerStatus] = useState<'online' | 'offline' | 'checking'>('checking')

  // ── Project root ──
  const [projectRoot, setProjectRoot] = useState('')

  // Load project root
  useEffect(() => {
    if (window.fs?.getProjectRoot) {
      window.fs.getProjectRoot().then((root: string) => setProjectRoot(root))
    }
  }, [])

  useEffect(() => {
    if (projectRoot && window.appApi?.setWorkspaceRoot) {
      window.appApi.setWorkspaceRoot(projectRoot).catch(() => {
        // Ignore workspace sync failures
      })
    }
  }, [projectRoot])

  // ── Workspace Persistence — Restore on mount ──
  useEffect(() => {
    try {
      const saved = localStorage.getItem('neural-workspace')
      if (saved) {
        const ws = JSON.parse(saved)
        if (ws.tabs?.length > 0) setTabs(ws.tabs)
        if (ws.activeTabId) setActiveTabId(ws.activeTabId)
        if (ws.sidebarOpen !== undefined) setSidebarOpen(ws.sidebarOpen)
        if (ws.sidebarWidth) setSidebarWidth(ws.sidebarWidth)
        if (ws.activePanel) setActivePanel(ws.activePanel)
        if (ws.bottomPanelOpen !== undefined) setBottomPanelOpen(ws.bottomPanelOpen)
        if (ws.bottomPanelHeight) setBottomPanelHeight(ws.bottomPanelHeight)
        if (ws.activeBottomTab) setActiveBottomTab(ws.activeBottomTab)
      }
    } catch {
      // Ignore parse errors
    }
  }, [])

  // ── Workspace Persistence — Save on state change ──
  useEffect(() => {
    const timeout = setTimeout(() => {
      try {
        localStorage.setItem('neural-workspace', JSON.stringify({
          tabs,
          activeTabId,
          sidebarOpen,
          sidebarWidth,
          activePanel,
          bottomPanelOpen,
          bottomPanelHeight,
          activeBottomTab,
        }))
      } catch {
        // Ignore write errors
      }
    }, 500) // Debounce to avoid excessive writes
    return () => clearTimeout(timeout)
  }, [tabs, activeTabId, sidebarOpen, sidebarWidth, activePanel, bottomPanelOpen, bottomPanelHeight, activeBottomTab])

  // Apply global settings
  useEffect(() => {
    const applySettings = () => {
      try {
        const saved = localStorage.getItem('neural-studio-settings')
        if (saved) {
          const s = JSON.parse(saved)
          const root = document.documentElement
          if (s['font-family']) root.style.setProperty('--font-family', `'${s['font-family']}', monospace`)
          if (s['term-font-size']) root.style.setProperty('--term-font-size', `${s['term-font-size']}px`)

          // Cursor style
          if (s['cursor-style']) {
            root.style.setProperty('--term-cursor', s['cursor-style'] === 'underline' ? 'underline' : 'solid')
          }
        }
      } catch { }
    }

    applySettings() // Initial load
    window.addEventListener('settings-changed', applySettings)
    return () => window.removeEventListener('settings-changed', applySettings)
  }, [])

  // Health check
  useEffect(() => {
    const check = async () => {
      try {
        const res = await fetch('http://127.0.0.1:8001/api/health')
        const data = await res.json()
        setServerStatus(data.status === 'online' ? 'online' : 'offline')
      } catch {
        setServerStatus('offline')
      }
    }
    check()
    const interval = setInterval(check, 5000)
    return () => clearInterval(interval)
  }, [])

  // Click outside to close menu
  useEffect(() => {
    if (openMenu) {
      const close = () => setOpenMenu(null)
      window.addEventListener('click', close)
      return () => window.removeEventListener('click', close)
    }
  }, [openMenu])

  // Keyboard shortcuts
  useEffect(() => {
    const handler = (e: KeyboardEvent) => {
      if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'P') {
        e.preventDefault()
        setCommandPaletteOpen(prev => !prev)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === '`') {
        e.preventDefault()
        setBottomPanelOpen(prev => !prev)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'b') {
        e.preventDefault()
        setSidebarOpen(prev => !prev)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'w') {
        e.preventDefault()
        closeTab(activeTabId)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === ',') {
        e.preventDefault()
        openTab({ id: 'settings', label: '⚙ Settings', type: 'settings' })
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'o' && !e.shiftKey) {
        e.preventDefault()
        handleOpenFile()
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'n') {
        e.preventDefault()
        handleNewFile()
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'p' && !e.shiftKey) {
        e.preventDefault()
        setQuickOpenMode('file')
        setQuickOpenQuery('')
        setQuickOpenOpen(prev => !prev)
      }
      if ((e.ctrlKey || e.metaKey) && e.key === 'g') {
        e.preventDefault()
        setQuickOpenMode('file')
        setQuickOpenQuery(':')
        setQuickOpenOpen(true)
      }
      // Zen Mode toggle: Ctrl+Shift+Z
      if ((e.ctrlKey || e.metaKey) && e.shiftKey && e.key === 'Z') {
        e.preventDefault()
        setZenMode(prev => !prev)
      }
    }
    window.addEventListener('keydown', handler)
    return () => window.removeEventListener('keydown', handler)
  }, [activeTabId])

  // ── Quick Open event listener ──
  useEffect(() => {
    const handleOpenQuickOpen = (e: Event) => {
      const ce = e as CustomEvent<{ mode?: string, initialQuery?: string }>
      setQuickOpenMode((ce.detail?.mode as any) || 'file')
      setQuickOpenQuery(ce.detail?.initialQuery || '')
      setQuickOpenOpen(true)
    }
    window.addEventListener('open-quick-open', handleOpenQuickOpen)
    return () => window.removeEventListener('open-quick-open', handleOpenQuickOpen)
  }, [])

  // ── Notify on save events ──
  useEffect(() => {
    const handleSaveNotify = () => {
      notify('success', 'File saved', undefined, 2000)
    }
    window.addEventListener('file-saved', handleSaveNotify)
    return () => window.removeEventListener('file-saved', handleSaveNotify)
  }, [notify])

  // ── Tab management ──
  const openTab = useCallback((tab: TabItem) => {
    setTabs(prev => {
      const existing = prev.find(t => t.id === tab.id)
      if (existing) {
        setActiveTabId(tab.id)
        return prev
      }
      setActiveTabId(tab.id)
      return [...prev, tab]
    })
  }, [])

  const closeTab = useCallback((tabId: string) => {
    setTabs(prev => {
      const idx = prev.findIndex(t => t.id === tabId)
      if (idx === -1) return prev
      const newTabs = prev.filter(t => t.id !== tabId)
      if (activeTabId === tabId && newTabs.length > 0) {
        const newIdx = Math.min(idx, newTabs.length - 1)
        setActiveTabId(newTabs[newIdx].id)
      }
      if (newTabs.length === 0) {
        const welcomeTab: TabItem = { id: 'welcome', label: 'Welcome', type: 'welcome' }
        setActiveTabId('welcome')
        return [welcomeTab]
      }
      return newTabs
    })
  }, [activeTabId])

  const openFile = useCallback((filePath: string, fileName: string) => {
    const id = `file:${filePath}`
    openTab({ id, label: fileName, type: 'file', filePath })
  }, [openTab])

  useEffect(() => {
    const handleAIOpenFile = (e: Event) => {
      const ce = e as CustomEvent<{ path?: string, line?: number }>
      const rawPath = ce.detail?.path
      if (!rawPath) return

      const isAbsolute = /^[A-Za-z]:[\\/]/.test(rawPath) || rawPath.startsWith('\\\\')
      const resolvedPath = isAbsolute
        ? rawPath
        : projectRoot
          ? `${projectRoot}${projectRoot.endsWith('\\') || projectRoot.endsWith('/') ? '' : '\\'}${rawPath.replace(/\//g, '\\')}`
          : rawPath
      const fileName = resolvedPath.split(/[\\/]/).pop() || resolvedPath
      openFile(resolvedPath, fileName)

      const line = ce.detail?.line
      if (typeof line === 'number' && line > 0) {
        window.setTimeout(() => {
          window.dispatchEvent(new CustomEvent('editor-go-to-line', { detail: { line } }))
        }, 120)
      }
    }

    window.addEventListener('ai-open-file', handleAIOpenFile)
    return () => window.removeEventListener('ai-open-file', handleAIOpenFile)
  }, [openFile, projectRoot])

  useEffect(() => {
    const handleOpenBottomPanel = (e: Event) => {
      const ce = e as CustomEvent<{ tab?: string }>
      setBottomPanelOpen(true)
      if (ce.detail?.tab) {
        setActiveBottomTab(ce.detail.tab)
      }
    }

    window.addEventListener('open-bottom-panel', handleOpenBottomPanel)
    return () => window.removeEventListener('open-bottom-panel', handleOpenBottomPanel)
  }, [])

  const openWebView = useCallback((url?: string) => {
    const id = 'webview-main'
    openTab({ id, label: '🌐 Browser', type: 'webview', url: url || '' })
  }, [openTab])

  const openAIChat = useCallback(() => {
    openTab({ id: 'ai-chat', label: '🧠 AI Copilot', type: 'ai-chat' })
  }, [openTab])

  const openProjectMemory = useCallback(() => {
    setActivePanel('project-memory')
    setSidebarOpen(true)
  }, [])

  useEffect(() => {
    const handleOpenAIEv = () => openAIChat()
    const handleOpenBrowserEv = (e: Event) => {
      const ce = e as CustomEvent<{ url?: string }>
      openWebView(ce.detail?.url)
    }
    window.addEventListener('open-ai-chat', handleOpenAIEv)
    window.addEventListener('ai-open-browser', handleOpenBrowserEv)
    return () => {
      window.removeEventListener('open-ai-chat', handleOpenAIEv)
      window.removeEventListener('ai-open-browser', handleOpenBrowserEv)
    }
  }, [openAIChat, openWebView])

  // ── File operations ──
  const handleOpenFile = async () => {
    if (window.ipcRenderer?.selectFile) {
      const f = await window.ipcRenderer.selectFile()
      if (f) openFile(f, f.split(/[\\/]/).pop()!)
    }
  }

  const handleOpenFolder = async () => {
    if (window.ipcRenderer?.selectDirectory) {
      const dir = await window.ipcRenderer.selectDirectory()
      if (dir) {
        setProjectRoot(dir)
        setActivePanel('explorer')
        setSidebarOpen(true)
      }
    }
  }

  const handleNewFile = async () => {
    const name = prompt('New file name:')
    if (!name || !projectRoot) return
    if (window.fs?.createFile) {
      const path = `${projectRoot}\\${name}`
      await window.fs.createFile(path)
      openFile(path, name)
    }
  }

  const handleRunTask = () => {
    setBottomPanelOpen(true)
    setActiveBottomTab('terminal')
    // Send run cmd
    const cmd = prompt('Run command:', 'npm run dev')
    if (cmd) {
      setTimeout(() => {
        window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd } }))
      }, 300)
    }
  }

  const handleRunActiveFile = () => {
    const activeTab = tabs.find(t => t.id === activeTabId)
    if (!activeTab || activeTab.type !== 'file' || !activeTab.filePath || !projectRoot) return

    setBottomPanelOpen(true)
    setActiveBottomTab('terminal')

    let cmd = ''
    const ext = activeTab.filePath.split('.').pop()?.toLowerCase()

    // Create a relative path
    const relativePath = activeTab.filePath.startsWith(projectRoot)
      ? activeTab.filePath.slice(projectRoot.length + 1)
      : activeTab.filePath

    const targetFile = `"${relativePath}"`

    switch (ext) {
      case 'ts':
      case 'tsx':
        cmd = `ts-node ${targetFile}`
        break
      case 'js':
      case 'jsx':
        cmd = `node ${targetFile}`
        break
      case 'py':
        cmd = `python ${targetFile}`
        break
      case 'dart':
        cmd = `dart run ${targetFile}`
        break
      case 'go':
        cmd = `go run ${targetFile}`
        break
      case 'sh':
        cmd = `bash ${targetFile}`
        break
      case 'java':
        cmd = `java ${targetFile}`
        break
      case 'cpp':
        cmd = `g++ ${targetFile} -o app.exe && .\\app.exe`
        break
      default:
        cmd = `echo Cannot automatically run .${ext} files`
    }

    setTimeout(() => {
      window.dispatchEvent(new CustomEvent('run-terminal-command', { detail: { cmd } }))
    }, 300)
  }

  // ── Toggle sidebar panel ──
  const togglePanel = useCallback((panel: string) => {
    if (panel === 'settings-action') {
      openTab({ id: 'settings', label: '⚙ Settings', type: 'settings' })
      return
    }
    if (activePanel === panel && sidebarOpen) {
      setSidebarOpen(false)
    } else {
      setActivePanel(panel)
      setSidebarOpen(true)
    }
  }, [activePanel, sidebarOpen, openTab])

  // ── Panel resize ──
  const resizeRef = useRef<{ startY: number; startHeight: number } | null>(null)
  const sidebarResizeRef = useRef<{ startX: number; startWidth: number } | null>(null)

  const onResizeStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault()
    resizeRef.current = { startY: e.clientY, startHeight: bottomPanelHeight }
    const onMove = (ev: MouseEvent) => {
      if (!resizeRef.current) return
      const delta = resizeRef.current.startY - ev.clientY
      const newHeight = Math.max(100, Math.min(600, resizeRef.current.startHeight + delta))
      setBottomPanelHeight(newHeight)
    }
    const onUp = () => {
      resizeRef.current = null
      window.removeEventListener('mousemove', onMove)
      window.removeEventListener('mouseup', onUp)
    }
    window.addEventListener('mousemove', onMove)
    window.addEventListener('mouseup', onUp)
  }, [bottomPanelHeight])

  const onSidebarResizeStart = useCallback((e: React.MouseEvent) => {
    e.preventDefault()
    sidebarResizeRef.current = { startX: e.clientX, startWidth: sidebarWidth }
    const onMove = (ev: MouseEvent) => {
      if (!sidebarResizeRef.current) return
      const delta = ev.clientX - sidebarResizeRef.current.startX
      const newWidth = Math.max(150, Math.min(800, sidebarResizeRef.current.startWidth + delta))
      setSidebarWidth(newWidth)
    }
    const onUp = () => {
      sidebarResizeRef.current = null
      window.removeEventListener('mousemove', onMove)
      window.removeEventListener('mouseup', onUp)
    }
    window.addEventListener('mousemove', onMove)
    window.addEventListener('mouseup', onUp)
  }, [sidebarWidth])

  // ── Command palette commands ──
  const commands = [
    { id: 'open-file', label: 'Open File', shortcut: 'Ctrl+O', action: handleOpenFile },
    { id: 'open-folder', label: 'Open Folder', shortcut: 'Ctrl+Shift+O', action: handleOpenFolder },
    { id: 'new-file', label: 'New File', shortcut: 'Ctrl+N', action: handleNewFile },
    { id: 'new-browser', label: 'New Browser Tab', shortcut: '', action: () => openWebView() },
    { id: 'ai-chat', label: 'Open AI Copilot', shortcut: '', action: () => openAIChat() },
    { id: 'project-memory', label: 'Open Project Memory', shortcut: '', action: () => openProjectMemory() },
    { id: 'toggle-terminal', label: 'Toggle Terminal', shortcut: 'Ctrl+`', action: () => setBottomPanelOpen(p => !p) },
    { id: 'toggle-sidebar', label: 'Toggle Sidebar', shortcut: 'Ctrl+B', action: () => setSidebarOpen(p => !p) },
    { id: 'compress', label: 'Open Compress', shortcut: '', action: () => openTab({ id: 'compress', label: 'Compress', type: 'compress' }) },
    { id: 'decompress', label: 'Open Decompress', shortcut: '', action: () => openTab({ id: 'decompress', label: 'Decompress', type: 'decompress' }) },
    { id: 'settings', label: 'Settings', shortcut: 'Ctrl+,', action: () => openTab({ id: 'settings', label: '⚙ Settings', type: 'settings' }) },
    { id: 'run-file', label: 'Run Active File', shortcut: 'F5', action: handleRunActiveFile },
    { id: 'run-task', label: 'Run Task...', shortcut: '', action: handleRunTask },
    { id: 'run-build', label: 'Run Build', shortcut: 'Ctrl+Shift+B', action: () => { setBottomPanelOpen(true); setActiveBottomTab('terminal') } },
    { id: 'quick-open', label: 'Go to File...', shortcut: 'Ctrl+P', action: () => { setQuickOpenMode('file'); setQuickOpenQuery(''); setQuickOpenOpen(true) } },
    { id: 'go-to-line', label: 'Go to Line...', shortcut: 'Ctrl+G', action: () => { setQuickOpenMode('file'); setQuickOpenQuery(':'); setQuickOpenOpen(true) } },
    { id: 'toggle-zen-mode', label: 'Toggle Zen Mode', shortcut: 'Ctrl+Shift+Z', action: () => setZenMode(p => !p) },
  ]

  // ── Menu definitions ──
  const menus: Record<string, { label: string; shortcut?: string; action: () => void; divider?: boolean }[]> = {
    File: [
      { label: 'New File', shortcut: 'Ctrl+N', action: handleNewFile },
      { label: 'Open File...', shortcut: 'Ctrl+O', action: handleOpenFile },
      { label: 'Open Folder...', shortcut: 'Ctrl+Shift+O', action: handleOpenFolder },
      { label: '', action: () => { }, divider: true },
      { label: 'Save', shortcut: 'Ctrl+S', action: () => { } },
      { label: 'Save All', shortcut: 'Ctrl+Shift+S', action: () => { } },
      { label: '', action: () => { }, divider: true },
      { label: 'Preferences: Settings', shortcut: 'Ctrl+,', action: () => openTab({ id: 'settings', label: '⚙ Settings', type: 'settings' }) },
      { label: 'Close Tab', shortcut: 'Ctrl+W', action: () => closeTab(activeTabId) },
    ],
    Edit: [
      { label: 'Undo', shortcut: 'Ctrl+Z', action: () => document.execCommand('undo') },
      { label: 'Redo', shortcut: 'Ctrl+Y', action: () => document.execCommand('redo') },
      { label: '', action: () => { }, divider: true },
      { label: 'Cut', shortcut: 'Ctrl+X', action: () => document.execCommand('cut') },
      { label: 'Copy', shortcut: 'Ctrl+C', action: () => document.execCommand('copy') },
      { label: 'Paste', shortcut: 'Ctrl+V', action: () => document.execCommand('paste') },
      { label: '', action: () => { }, divider: true },
      { label: 'Find', shortcut: 'Ctrl+F', action: () => { } },
      { label: 'Replace', shortcut: 'Ctrl+H', action: () => { } },
    ],
    View: [
      { label: 'Command Palette', shortcut: 'Ctrl+Shift+P', action: () => setCommandPaletteOpen(true) },
      { label: '', action: () => { }, divider: true },
      { label: 'Explorer', shortcut: 'Ctrl+Shift+E', action: () => togglePanel('explorer') },
      { label: 'Search', shortcut: 'Ctrl+Shift+F', action: () => togglePanel('search') },
      { label: 'Source Control', shortcut: 'Ctrl+Shift+G', action: () => togglePanel('git') },
      { label: 'Extensions', shortcut: 'Ctrl+Shift+X', action: () => togglePanel('extensions') },
      { label: 'Project Memory', shortcut: '', action: () => openProjectMemory() },
      { label: '', action: () => { }, divider: true },
      { label: 'Toggle Sidebar', shortcut: 'Ctrl+B', action: () => setSidebarOpen(p => !p) },
      { label: 'Toggle Terminal', shortcut: 'Ctrl+`', action: () => setBottomPanelOpen(p => !p) },
      { label: 'Toggle Zen Mode', shortcut: 'Ctrl+Shift+Z', action: () => setZenMode(p => !p) },
    ],
    Run: [
      { label: 'Run Task...', shortcut: '', action: handleRunTask },
      { label: 'Run Build Task', shortcut: 'Ctrl+Shift+B', action: () => { setBottomPanelOpen(true); setActiveBottomTab('terminal') } },
      { label: '', action: () => { }, divider: true },
      { label: 'New Terminal', shortcut: '', action: () => { setBottomPanelOpen(true); setActiveBottomTab('terminal') } },
    ],
    Terminal: [
      { label: 'New Terminal', shortcut: 'Ctrl+Shift+`', action: () => { setBottomPanelOpen(true); setActiveBottomTab('terminal') } },
      { label: 'Split Terminal', shortcut: '', action: () => { setBottomPanelOpen(true); setActiveBottomTab('terminal') } },
      { label: '', action: () => { }, divider: true },
      { label: 'Run Task...', shortcut: '', action: handleRunTask },
      { label: 'Run Build Task', shortcut: 'Ctrl+Shift+B', action: () => { setBottomPanelOpen(true) } },
    ],
    Help: [
      { label: 'Welcome', shortcut: '', action: () => openTab({ id: 'welcome', label: 'Welcome', type: 'welcome' }) },
      { label: 'Documentation', shortcut: '', action: () => openWebView('https://github.com') },
      { label: '', action: () => { }, divider: true },
      { label: 'About', shortcut: '', action: () => alert('Neural Studio IDE v1.0\nPowered by 1,046 Neural Advisors') },
    ],
  }

  // ── Get active tab ──
  const activeTab = tabs.find(t => t.id === activeTabId)
  const activeFilePath = activeTab?.type === 'file' ? activeTab.filePath : undefined

  // ── Render active editor content ──
  const renderEditorContent = () => {
    if (!activeTab) return <WelcomeScreen openFile={openFile} openWebView={openWebView} openAIChat={openAIChat}
      openCompress={() => openTab({ id: 'compress', label: 'Compress', type: 'compress' })} />

    switch (activeTab.type) {
      case 'welcome':
        return <WelcomeScreen openFile={openFile} openWebView={openWebView} openAIChat={openAIChat}
          openCompress={() => openTab({ id: 'compress', label: 'Compress', type: 'compress' })} />
      case 'file': {
        const ext = activeTab.filePath?.split('.').pop()?.toLowerCase() || ''
        if (ext === 'pdf' || ext === 'docx' || (ext === 'md' && !activeTab.label.includes('(edit)'))) {
          return <DocumentViewer
            filePath={activeTab.filePath!}
            projectRoot={projectRoot}
            onEdit={ext === 'md' ? () => {
              openTab({ ...activeTab, id: `edit:${activeTab.id}`, label: `${activeTab.label} (edit)` })
            } : undefined}
          />
        }
        return <MonacoEditor filePath={activeTab.filePath!} projectRoot={projectRoot}
          onModified={(mod) => setTabs(prev => prev.map(t => t.id === activeTab.id ? { ...t, modified: mod } : t))} />
      }
      case 'git-diff':
        return <GitDiffEditor projectRoot={projectRoot} filePath={activeTab.filePath!} absolutePath={`${projectRoot}\\${activeTab.filePath}`} onClose={() => closeTab(activeTabId)} />
      case 'webview':
        return <WebViewPanel url={activeTab.url} />
      case 'ai-chat':
        return <AIChatPanel serverStatus={serverStatus} />
      case 'settings':
        return <SettingsPanel />
      default:
        return <WelcomeScreen openFile={openFile} openWebView={openWebView} openAIChat={openAIChat}
          openCompress={() => openTab({ id: 'compress', label: 'Compress', type: 'compress' })} />
    }
  }

  // ── Render sidebar content ──
  const renderSidebarContent = () => {
    switch (activePanel) {
      case 'explorer':
        return <FileExplorer projectRoot={projectRoot} onFileOpen={openFile} />
      case 'search':
        return <SearchPanel projectRoot={projectRoot} onFileOpen={openFile} />
      case 'git':
        return <GitPanel projectRoot={projectRoot} onFileClick={(file: string) => openTab({ id: `diff:${file}`, label: `Diff: ${file.split(/[\\/]/).pop()}`, type: 'git-diff', filePath: file, icon: '🔄' })} />
      case 'run':
        return <RunPanel projectRoot={projectRoot} activeFilePath={activeFilePath} />
      case 'extensions':
        return <ExtensionsPanel />
      case 'mcp':
        return <MCPPanel serverStatus={serverStatus} />
      case 'ai-stats':
        return <AIStatsPanel />
      case 'project-memory':
        return <ProjectMemoryPanel projectRoot={projectRoot} onFileOpen={openFile} />
      case 'ai':
        return (
          <div style={{ display: 'flex', flexDirection: 'column', height: '100%' }}>
            <div className="sidebar-header"><span>AI Assistant</span></div>
            <div style={{ padding: '12px', textAlign: 'center' }}>
              <button className="primary-btn" style={{ marginTop: 0 }} onClick={openAIChat}>
                Open AI Chat in Editor
              </button>
            </div>
          </div>
        )
      default:
        return <FileExplorer projectRoot={projectRoot} onFileOpen={openFile} />
    }
  }

  return (
    <div className={`ide-root no-select ${zenMode ? 'zen-mode' : ''}`}>
      {/* ── Titlebar ── */}
      <div className="ide-titlebar">
        <div className="titlebar-logo">
          <div className="titlebar-logo-icon">
            <svg width="12" height="12" viewBox="0 0 24 24" fill="none" stroke="white" strokeWidth="2.5">
              <path d="M12 2L2 7l10 5 10-5-10-5zM2 17l10 5 10-5M2 12l10 5 10-5" />
            </svg>
          </div>
          Neural Studio
        </div>
        <div className="titlebar-menus">
          {Object.keys(menus).map(menuName => (
            <div key={menuName} className="menu-wrapper" style={{ position: 'relative' }}>
              <button
                className={`titlebar-menu-item ${openMenu === menuName ? 'active' : ''}`}
                onClick={(e) => { e.stopPropagation(); setOpenMenu(openMenu === menuName ? null : menuName) }}
                onMouseEnter={() => { if (openMenu && openMenu !== menuName) setOpenMenu(menuName) }}
              >
                {menuName}
              </button>
              {openMenu === menuName && (
                <div className="dropdown-menu" onClick={e => e.stopPropagation()}>
                  {menus[menuName].map((item, i) => (
                    item.divider ? (
                      <div key={i} className="dropdown-divider" />
                    ) : (
                      <button key={i} className="dropdown-item" onClick={() => { item.action(); setOpenMenu(null) }}>
                        <span>{item.label}</span>
                        {item.shortcut && <kbd>{item.shortcut}</kbd>}
                      </button>
                    )
                  ))}
                </div>
              )}
            </div>
          ))}
        </div>
        <div className="titlebar-center">
          <div className="titlebar-search" onClick={() => setCommandPaletteOpen(true)}>
            <svg width="13" height="13" viewBox="0 0 24 24" fill="none" stroke="currentColor" strokeWidth="2">
              <circle cx="11" cy="11" r="8" /><path d="M21 21l-4.35-4.35" />
            </svg>
            <span>Neural Studio IDE</span>
            <kbd>Ctrl+Shift+P</kbd>
          </div>
        </div>
      </div>

      {/* ── Main body ── */}
      <div className="ide-body">
        {/* Activity Bar */}
        <ActivityBar
          activePanel={activePanel}
          onTogglePanel={togglePanel}
          onOpenAI={openAIChat}
          onOpenBrowser={openWebView}
        />

        {/* Sidebar */}
        {sidebarOpen && (
          <div style={{ display: 'flex', height: '100%', flexShrink: 0 }}>
            <div className="ide-sidebar" style={{ width: sidebarWidth }}>
              {renderSidebarContent()}
            </div>
            <div className="sidebar-resize-handle" onMouseDown={onSidebarResizeStart} />
          </div>
        )}

        {/* Main editor area */}
        <div className="ide-main">
          {/* Editor tabs */}
          <EditorTabs
            tabs={tabs}
            activeTabId={activeTabId}
            onSelectTab={setActiveTabId}
            onCloseTab={closeTab}
            canRunActive={tabs.find(t => t.id === activeTabId)?.type === 'file'}
            onRunActive={handleRunActiveFile}
            onReorderTabs={setTabs}
            onSplitEditor={() => {
              // Open current file in a new split tab
              const activeTab = tabs.find(t => t.id === activeTabId)
              if (activeTab?.type === 'file' && activeTab.filePath) {
                const splitId = `split:${activeTab.filePath}:${Date.now()}`
                openTab({ ...activeTab, id: splitId, label: `${activeTab.label} (split)` })
              }
            }}
          />

          {/* Editor content */}
          <div className="editor-content">
            {renderEditorContent()}
          </div>

          {/* Bottom panel */}
          {bottomPanelOpen && (
            <div className="bottom-panel" style={{ height: bottomPanelHeight }}>
              <div className="panel-resize-handle" onMouseDown={onResizeStart} />
              <TerminalPanel
                activeTab={activeBottomTab}
                onTabChange={setActiveBottomTab}
                onClose={() => setBottomPanelOpen(false)}
                projectRoot={projectRoot}
              />
            </div>
          )}
        </div>
      </div>

      {/* Status Bar */}
      <StatusBar
        serverStatus={serverStatus}
        projectRoot={projectRoot}
        activeFilePath={activeFilePath}
        zenMode={zenMode}
        onToggleZen={() => setZenMode(p => !p)}
        onToggleTerminal={() => setBottomPanelOpen(p => !p)}
        onToggleProblems={() => { setBottomPanelOpen(true); setActiveBottomTab('problems') }}
      />

      {/* Command Palette */}
      {commandPaletteOpen && (
        <CommandPalette
          commands={commands}
          onClose={() => setCommandPaletteOpen(false)}
        />
      )}

      {/* Quick Open (Ctrl+P) */}
      {quickOpenOpen && (
        <QuickOpen
          projectRoot={projectRoot}
          onOpen={openFile}
          onClose={() => setQuickOpenOpen(false)}
          mode={quickOpenMode}
          initialQuery={quickOpenQuery}
        />
      )}

      {/* Notifications */}
      <NotificationManager notifications={notifications} onDismiss={dismiss} />
    </div>
  )
}

// Old FileEditor removed — now using MonacoEditor component

// SVG file icons are now in src/components/FileIcons.tsx (used by EditorTabs)

export default App
