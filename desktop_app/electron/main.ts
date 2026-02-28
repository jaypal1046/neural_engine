import { app, BrowserWindow, dialog, ipcMain, shell } from 'electron'
import { fileURLToPath } from 'node:url'
import path from 'node:path'
import { spawn, ChildProcess, exec } from 'node:child_process'
import { existsSync, readFileSync, writeFileSync, readdirSync, statSync, mkdirSync, unlinkSync, renameSync, rmdirSync, watch, FSWatcher } from 'node:fs'
import { platform, homedir } from 'node:os'

const __dirname = path.dirname(fileURLToPath(import.meta.url))

process.env.APP_ROOT = path.join(__dirname, '..')

export const VITE_DEV_SERVER_URL = process.env['VITE_DEV_SERVER_URL']
export const MAIN_DIST = path.join(process.env.APP_ROOT, 'dist-electron')
export const RENDERER_DIST = path.join(process.env.APP_ROOT, 'dist')

process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL ? path.join(process.env.APP_ROOT, 'public') : RENDERER_DIST

let win: BrowserWindow | null
let serverProcess: ChildProcess | null = null
let fileWatcher: FSWatcher | null = null
const terminalProcesses: Map<string, ChildProcess> = new Map()

// =============================================================================
// Auto-start Python server
// =============================================================================

function findPython(): string {
    const candidates = ['python', 'python3', 'py']
    return candidates[0]
}

function startServer() {
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
    })

    serverProcess.stdout?.on('data', (data: Buffer) => {
        const msg = data.toString().trim()
        if (msg) console.log(`[Server] ${msg}`)
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
        if (process.platform === 'win32' && serverProcess.pid) {
            spawn('taskkill', ['/pid', String(serverProcess.pid), '/f', '/t'], { shell: true })
        } else {
            serverProcess.kill('SIGTERM')
        }
        serverProcess = null
    }
}

// =============================================================================
// File System IPC Handlers
// =============================================================================

const IGNORED_DIRS = new Set(['.git', 'node_modules', '__pycache__', 'dist-electron', 'dist', '.next', '.vscode', 'build', 'out', '.svn', 'coverage', '.angular'])
const BINARY_EXTENSIONS = new Set(['.jpg', '.jpeg', '.png', '.gif', '.ico', '.webp', '.mp3', '.mp4', '.avi', '.mov', '.pdf', '.zip', '.rar', '.7z', '.tar', '.gz', '.exe', '.dll', '.so', '.woff', '.woff2', '.ttf', '.eot', '.otf'])

function getFileTree(dirPath: string, depth: number = 0, maxDepth: number = 3): any[] {
    if (depth > maxDepth) return []
    try {
        const entries = readdirSync(dirPath, { withFileTypes: true })
        const results: any[] = []

        const sorted = entries.sort((a, b) => {
            if (a.isDirectory() && !b.isDirectory()) return -1
            if (!a.isDirectory() && b.isDirectory()) return 1
            return a.name.localeCompare(b.name)
        })

        for (const entry of sorted) {
            if (entry.name.startsWith('.') || IGNORED_DIRS.has(entry.name)) continue

            const fullPath = path.join(dirPath, entry.name)
            if (entry.isDirectory()) {
                results.push({
                    name: entry.name,
                    path: fullPath,
                    type: 'directory',
                    children: getFileTree(fullPath, depth + 1, maxDepth),
                })
            } else {
                let size = 0
                try { size = statSync(fullPath).size } catch { }
                results.push({
                    name: entry.name,
                    path: fullPath,
                    type: 'file',
                    size,
                    extension: path.extname(entry.name).toLowerCase(),
                })
            }
        }
        return results
    } catch {
        return []
    }
}

// Search across files
function searchInFiles(rootDir: string, query: string, maxResults: number = 100): any[] {
    const results: any[] = []
    const lowerQuery = query.toLowerCase()

    function walkDir(dir: string) {
        if (results.length >= maxResults) return
        try {
            const entries = readdirSync(dir, { withFileTypes: true })
            for (const entry of entries) {
                if (results.length >= maxResults) return
                if (entry.name.startsWith('.') || IGNORED_DIRS.has(entry.name)) continue

                const fullPath = path.join(dir, entry.name)
                if (entry.isDirectory()) {
                    walkDir(fullPath)
                } else {
                    const ext = path.extname(entry.name).toLowerCase()
                    if (BINARY_EXTENSIONS.has(ext)) continue

                    try {
                        const stat = statSync(fullPath)
                        if (stat.size > 1024 * 512) continue  // Skip files > 512KB

                        const content = readFileSync(fullPath, 'utf-8')
                        const lines = content.split('\n')
                        const matches: any[] = []

                        for (let i = 0; i < lines.length; i++) {
                            if (lines[i].toLowerCase().includes(lowerQuery)) {
                                matches.push({
                                    line: i + 1,
                                    text: lines[i].trim().substring(0, 200),
                                    column: lines[i].toLowerCase().indexOf(lowerQuery),
                                })
                                if (matches.length >= 5) break  // Max 5 matches per file
                            }
                        }

                        if (matches.length > 0) {
                            results.push({
                                filePath: fullPath,
                                fileName: entry.name,
                                relativePath: path.relative(rootDir, fullPath),
                                matches,
                            })
                        }
                    } catch {
                        // Skip unreadable files
                    }
                }
            }
        } catch {
            // Skip unreadable directories
        }
    }

    walkDir(rootDir)
    return results
}

// Git operations
function runGitCommand(cwd: string, args: string[]): Promise<{ stdout: string; stderr: string; code: number }> {
    return new Promise((resolve) => {
        const proc = spawn('git', args, { cwd, shell: true })
        let stdout = ''
        let stderr = ''
        proc.stdout?.on('data', (d: Buffer) => { stdout += d.toString() })
        proc.stderr?.on('data', (d: Buffer) => { stderr += d.toString() })
        proc.on('close', (code) => resolve({ stdout, stderr, code: code || 0 }))
        proc.on('error', () => resolve({ stdout: '', stderr: 'git not found', code: 1 }))
    })
}

// =============================================================================
// Window
// =============================================================================

function createWindow() {
    win = new BrowserWindow({
        width: 1440,
        height: 900,
        minWidth: 1024,
        minHeight: 700,
        icon: path.join(process.env.VITE_PUBLIC || '', 'electron-vite.svg'),
        webPreferences: {
            preload: path.join(__dirname, 'preload.js'),
            webviewTag: true,
            nodeIntegration: false,
            contextIsolation: true,
        },
        autoHideMenuBar: true,
        title: 'Neural Studio IDE',
        frame: false,
        titleBarStyle: 'hidden',
        titleBarOverlay: {
            color: '#0D0D0D',
            symbolColor: '#888',
            height: 36,
        },
        backgroundColor: '#0D0D0D',
    })

    win.webContents.on('did-finish-load', () => {
        win?.webContents.send('main-process-message', (new Date()).toLocaleString())
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
        terminalProcesses.forEach((proc) => {
            if (proc.pid) {
                if (process.platform === 'win32') {
                    spawn('taskkill', ['/pid', String(proc.pid), '/f', '/t'], { shell: true })
                } else {
                    proc.kill('SIGTERM')
                }
            }
        })
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
    // ── File Dialog IPC ──
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
    ipcMain.handle('dialog:openDirectory', async () => {
        const { canceled, filePaths } = await dialog.showOpenDialog({ properties: ['openDirectory'] })
        if (!canceled) return filePaths[0]
        return null
    })

    // ── Server IPC ──
    ipcMain.handle('server:status', () => {
        return { running: serverProcess !== null, pid: serverProcess?.pid }
    })
    ipcMain.handle('server:restart', () => {
        stopServer()
        setTimeout(startServer, 500)
        return { status: 'restarting' }
    })

    // ── File System IPC ──
    ipcMain.handle('fs:readDir', (_event, dirPath: string) => {
        return getFileTree(dirPath, 0, 4)
    })
    ipcMain.handle('fs:readFile', (_event, filePath: string) => {
        try {
            return readFileSync(filePath, 'utf-8')
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:writeFile', (_event, filePath: string, content: string) => {
        try {
            writeFileSync(filePath, content, 'utf-8')
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:createFile', (_event, filePath: string) => {
        try {
            writeFileSync(filePath, '', 'utf-8')
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:createDir', (_event, dirPath: string) => {
        try {
            mkdirSync(dirPath, { recursive: true })
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:delete', (_event, targetPath: string) => {
        try {
            const stat = statSync(targetPath)
            if (stat.isDirectory()) {
                if (process.platform === 'win32') {
                    exec(`rmdir /s /q "${targetPath}"`)
                } else {
                    exec(`rm -rf "${targetPath}"`)
                }
            } else {
                unlinkSync(targetPath)
            }
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:rename', (_event, oldPath: string, newPath: string) => {
        try {
            renameSync(oldPath, newPath)
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:stat', (_event, filePath: string) => {
        try {
            const s = statSync(filePath)
            return {
                size: s.size,
                isFile: s.isFile(),
                isDirectory: s.isDirectory(),
                modified: s.mtime.toISOString(),
                created: s.birthtime.toISOString(),
            }
        } catch (e: any) {
            return { error: e.message }
        }
    })
    ipcMain.handle('fs:getProjectRoot', () => {
        return path.join(process.env.APP_ROOT!, '..')
    })

    // ── Search IPC ──
    ipcMain.handle('search:files', (_event, rootDir: string, query: string) => {
        if (!query || query.length < 2) return []
        return searchInFiles(rootDir, query)
    })

    // ── Git IPC ──
    ipcMain.handle('git:status', async (_event, cwd: string) => {
        const result = await runGitCommand(cwd, ['status', '--porcelain', '-b'])
        if (result.code !== 0) return { error: result.stderr }
        const lines = result.stdout.trim().split('\n')
        const branchLine = lines[0] || ''
        const branch = branchLine.replace('## ', '').split('...')[0] || 'unknown'
        const changes = lines.slice(1).filter(l => l.trim()).map(line => {
            const status = line.substring(0, 2).trim()
            const file = line.substring(3).trim()
            return { status, file, staged: line[0] !== ' ' && line[0] !== '?' }
        })
        return { branch, changes }
    })
    ipcMain.handle('git:log', async (_event, cwd: string, count: number = 10) => {
        const result = await runGitCommand(cwd, ['log', `--pretty=format:%H|||%an|||%s|||%ar`, `-${count}`])
        if (result.code !== 0) return []
        return result.stdout.trim().split('\n').filter(l => l).map(line => {
            const [hash, author, message, date] = line.split('|||')
            return { hash: hash?.substring(0, 7), author, message, date }
        })
    })
    ipcMain.handle('git:stage', async (_event, cwd: string, filePath: string) => {
        const result = await runGitCommand(cwd, ['add', filePath])
        return { success: result.code === 0, error: result.stderr }
    })
    ipcMain.handle('git:unstage', async (_event, cwd: string, filePath: string) => {
        const result = await runGitCommand(cwd, ['reset', 'HEAD', filePath])
        return { success: result.code === 0, error: result.stderr }
    })
    ipcMain.handle('git:commit', async (_event, cwd: string, message: string) => {
        const result = await runGitCommand(cwd, ['commit', '-m', `"${message}"`])
        return { success: result.code === 0, output: result.stdout, error: result.stderr }
    })
    ipcMain.handle('git:stageAll', async (_event, cwd: string) => {
        const result = await runGitCommand(cwd, ['add', '-A'])
        return { success: result.code === 0 }
    })
    ipcMain.handle('git:diff', async (_event, cwd: string, filePath: string) => {
        const result = await runGitCommand(cwd, ['diff', filePath])
        return result.stdout
    })
    ipcMain.handle('git:getFile', async (_event, cwd: string, ref: string, filePath: string) => {
        // Need to normalize filePath for git show (use forward slashes and relative to git root)
        // For simplicity, we assume filePath here is already relative or we just use `show ref:./filePath`
        // Actually, git show HEAD:./path/to/file works if cwd is project root.
        const normalizedPath = filePath.replace(/\\/g, '/')
        const result = await runGitCommand(cwd, ['show', `${ref}:${normalizedPath}`])
        if (result.code !== 0) return { error: result.stderr }
        return { content: result.stdout }
    })

    // ── Terminal IPC ──
    ipcMain.handle('terminal:spawn', (_event, id: string, cwd?: string) => {
        const shellCmd = process.platform === 'win32' ? 'powershell.exe' : (process.env.SHELL || '/bin/bash')
        const defaultCwd = cwd || path.join(process.env.APP_ROOT!, '..')

        const proc = spawn(shellCmd, [], {
            cwd: defaultCwd,
            stdio: ['pipe', 'pipe', 'pipe'],
            env: { ...process.env, TERM: 'xterm-256color' },
            shell: false,
        })

        terminalProcesses.set(id, proc)

        proc.stdout?.on('data', (data: Buffer) => {
            if (win) {
                win.webContents.send(`terminal:data:${id}`, data.toString())
            }
        })

        proc.stderr?.on('data', (data: Buffer) => {
            if (win) {
                win.webContents.send(`terminal:data:${id}`, data.toString())
            }
        })

        proc.on('exit', (code) => {
            terminalProcesses.delete(id)
            if (win) {
                win.webContents.send(`terminal:exit:${id}`, code)
            }
        })

        return { pid: proc.pid, id }
    })

    ipcMain.handle('terminal:write', (_event, id: string, data: string) => {
        const proc = terminalProcesses.get(id)
        if (proc && proc.stdin) {
            proc.stdin.write(data)
            return { success: true }
        }
        return { error: 'Terminal not found' }
    })

    ipcMain.handle('terminal:kill', (_event, id: string) => {
        const proc = terminalProcesses.get(id)
        if (proc) {
            if (process.platform === 'win32' && proc.pid) {
                spawn('taskkill', ['/pid', String(proc.pid), '/f', '/t'], { shell: true })
            } else {
                proc.kill('SIGTERM')
            }
            terminalProcesses.delete(id)
            return { success: true }
        }
        return { error: 'Terminal not found' }
    })

    // ── Shell IPC ──
    ipcMain.handle('shell:openExternal', (_event, url: string) => {
        shell.openExternal(url)
    })
    ipcMain.handle('shell:openPath', (_event, p: string) => {
        shell.openPath(p)
    })

    // ── App Info ──
    ipcMain.handle('app:platform', () => platform())
    ipcMain.handle('app:homedir', () => homedir())

    // ── File Watcher IPC ──
    ipcMain.handle('fs:watch', (_event, dirPath: string) => {
        // Clean up previous watcher
        if (fileWatcher) {
            fileWatcher.close()
            fileWatcher = null
        }

        if (!existsSync(dirPath)) return { error: 'Directory not found' }

        let debounceTimer: ReturnType<typeof setTimeout> | null = null

        try {
            fileWatcher = watch(dirPath, { recursive: true }, (eventType, filename) => {
                if (!filename) return
                // Ignore common noisy directories
                const ignored = ['node_modules', '.git', 'dist', 'dist-electron', '__pycache__', '.next', 'build', 'out']
                if (ignored.some(d => filename.includes(d))) return

                // Debounce to avoid rapid-fire events
                if (debounceTimer) clearTimeout(debounceTimer)
                debounceTimer = setTimeout(() => {
                    if (win) {
                        win.webContents.send('fs:change', eventType, filename)
                    }
                }, 300)
            })
            return { success: true }
        } catch (e: any) {
            return { error: e.message }
        }
    })

    ipcMain.handle('fs:unwatch', () => {
        if (fileWatcher) {
            fileWatcher.close()
            fileWatcher = null
        }
        return { success: true }
    })

    // Start the Python server automatically
    startServer()

    // Create the window
    createWindow()
})
