import { app, BrowserWindow, dialog, ipcMain } from 'electron'
import { fileURLToPath } from 'node:url'
import path from 'node:path'
import { spawn, ChildProcess } from 'node:child_process'
import { existsSync } from 'node:fs'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

process.env.APP_ROOT = path.join(__dirname, '..')

export const VITE_DEV_SERVER_URL = process.env['VITE_DEV_SERVER_URL']
export const MAIN_DIST = path.join(process.env.APP_ROOT, 'dist-electron')
export const RENDERER_DIST = path.join(process.env.APP_ROOT, 'dist')

process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL ? path.join(process.env.APP_ROOT, 'public') : RENDERER_DIST

let win: BrowserWindow | null
let serverProcess: ChildProcess | null = null

// =============================================================================
// Auto-start Python server
// =============================================================================

function findPython(): string {
    // Try common python paths on Windows
    const candidates = ['python', 'python3', 'py']
    return candidates[0] // 'python' is most common on Windows
}

function startServer() {
    // Server is at project_root/server/main.py
    // project_root = APP_ROOT/../  (since APP_ROOT = desktop_app/)
    const projectRoot = path.join(process.env.APP_ROOT!, '..')
    const serverScript = path.join(projectRoot, 'server', 'main.py')

    if (!existsSync(serverScript)) {
        console.error(`[Neural Studio] Server script not found: ${serverScript}`)
        return
    }

    console.log(`[Neural Studio] Starting Python server: ${serverScript}`)

    const pythonCmd = findPython()

    serverProcess = spawn(pythonCmd, [serverScript], {
        cwd: path.join(projectRoot, 'server'),
        stdio: ['pipe', 'pipe', 'pipe'],
        env: { ...process.env },
        shell: true,
    })

    serverProcess.stdout?.on('data', (data: Buffer) => {
        const msg = data.toString().trim()
        if (msg) console.log(`[Server] ${msg}`)
        // Forward server logs to renderer
        if (win) {
            win.webContents.send('server-log', msg)
        }
    })

    serverProcess.stderr?.on('data', (data: Buffer) => {
        const msg = data.toString().trim()
        if (msg) console.log(`[Server:ERR] ${msg}`)
    })

    serverProcess.on('exit', (code) => {
        console.log(`[Neural Studio] Server process exited with code ${code}`)
        serverProcess = null
    })

    serverProcess.on('error', (err) => {
        console.error(`[Neural Studio] Failed to start server:`, err.message)
        serverProcess = null
    })
}

function stopServer() {
    if (serverProcess) {
        console.log('[Neural Studio] Stopping Python server...')
        // On Windows, we need to kill the process tree
        if (process.platform === 'win32' && serverProcess.pid) {
            spawn('taskkill', ['/pid', String(serverProcess.pid), '/f', '/t'], { shell: true })
        } else {
            serverProcess.kill('SIGTERM')
        }
        serverProcess = null
    }
}

// =============================================================================
// Window
// =============================================================================

function createWindow() {
    win = new BrowserWindow({
        width: 1220,
        height: 780,
        minWidth: 900,
        minHeight: 600,
        icon: path.join(process.env.VITE_PUBLIC || '', 'electron-vite.svg'),
        webPreferences: {
            preload: path.join(__dirname, 'preload.mjs'),
        },
        autoHideMenuBar: true,
        title: 'Neural Studio V10',
    })

    win.webContents.on('did-finish-load', () => {
        win?.webContents.send('main-process-message', (new Date).toLocaleString())
    })

    if (VITE_DEV_SERVER_URL) {
        win.loadURL(VITE_DEV_SERVER_URL)
    } else {
        win.loadFile(path.join(RENDERER_DIST, 'index.html'))
    }
}

// =============================================================================
// App lifecycle
// =============================================================================

app.on('window-all-closed', () => {
    if (process.platform !== 'darwin') {
        stopServer()
        app.quit()
        win = null
    }
})

app.on('before-quit', () => {
    stopServer()
})

app.on('activate', () => {
    if (BrowserWindow.getAllWindows().length === 0) {
        createWindow()
    }
})

app.whenReady().then(() => {
    // IPC handlers for file dialogs
    ipcMain.handle('dialog:openFile', async () => {
        const { canceled, filePaths } = await dialog.showOpenDialog({})
        if (!canceled) return filePaths[0]
        return null
    })
    ipcMain.handle('dialog:saveFile', async () => {
        const { canceled, filePath } = await dialog.showSaveDialog({})
        if (!canceled) return filePath
        return null
    })

    // IPC handler: check if server is running
    ipcMain.handle('server:status', () => {
        return { running: serverProcess !== null, pid: serverProcess?.pid }
    })

    // IPC handler: restart server
    ipcMain.handle('server:restart', () => {
        stopServer()
        setTimeout(startServer, 500)
        return { status: 'restarting' }
    })

    // Start the Python server automatically
    startServer()

    // Create the window
    createWindow()
})
