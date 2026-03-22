import { ipcRenderer, contextBridge } from 'electron'

// ── Expose rich API to the Renderer process ──
contextBridge.exposeInMainWorld('ipcRenderer', {
    on(...args: Parameters<typeof ipcRenderer.on>) {
        const [channel, listener] = args
        return ipcRenderer.on(channel, (event, ...args) => listener(event, ...args))
    },
    off(...args: Parameters<typeof ipcRenderer.off>) {
        const [channel, ...omit] = args
        return ipcRenderer.off(channel, ...omit)
    },
    send(...args: Parameters<typeof ipcRenderer.send>) {
        const [channel, ...omit] = args
        return ipcRenderer.send(channel, ...omit)
    },
    invoke(...args: Parameters<typeof ipcRenderer.invoke>) {
        const [channel, ...omit] = args
        return ipcRenderer.invoke(channel, ...omit)
    },
    selectFile: () => ipcRenderer.invoke('dialog:openFile'),
    selectSaveFile: () => ipcRenderer.invoke('dialog:saveFile'),
    selectDirectory: () => ipcRenderer.invoke('dialog:openDirectory'),
})

// ── File System API ──
contextBridge.exposeInMainWorld('fs', {
    readDir: (dirPath: string) => ipcRenderer.invoke('fs:readDir', dirPath),
    readFile: (filePath: string) => ipcRenderer.invoke('fs:readFile', filePath),
    readFileBase64: (filePath: string) => ipcRenderer.invoke('fs:readFileBase64', filePath),
    writeFile: (filePath: string, content: string) => ipcRenderer.invoke('fs:writeFile', filePath, content),
    createFile: (filePath: string) => ipcRenderer.invoke('fs:createFile', filePath),
    createDir: (dirPath: string) => ipcRenderer.invoke('fs:createDir', dirPath),
    delete: (targetPath: string) => ipcRenderer.invoke('fs:delete', targetPath),
    rename: (oldPath: string, newPath: string) => ipcRenderer.invoke('fs:rename', oldPath, newPath),
    stat: (filePath: string) => ipcRenderer.invoke('fs:stat', filePath),
    getProjectRoot: () => ipcRenderer.invoke('fs:getProjectRoot'),
    selectDirectory: () => ipcRenderer.invoke('dialog:openDirectory'),
})

// ── Search API ──
contextBridge.exposeInMainWorld('searchApi', {
    searchFiles: (rootDir: string, query: string) => ipcRenderer.invoke('search:files', rootDir, query),
})

// ── Git API ──
contextBridge.exposeInMainWorld('gitApi', {
    status: (cwd: string) => ipcRenderer.invoke('git:status', cwd),
    log: (cwd: string, count?: number) => ipcRenderer.invoke('git:log', cwd, count),
    stage: (cwd: string, filePath: string) => ipcRenderer.invoke('git:stage', cwd, filePath),
    unstage: (cwd: string, filePath: string) => ipcRenderer.invoke('git:unstage', cwd, filePath),
    commit: (cwd: string, message: string) => ipcRenderer.invoke('git:commit', cwd, message),
    stageAll: (cwd: string) => ipcRenderer.invoke('git:stageAll', cwd),
    diff: (cwd: string, filePath: string) => ipcRenderer.invoke('git:diff', cwd, filePath),
    getFile: (cwd: string, ref: string, filePath: string) => ipcRenderer.invoke('git:getFile', cwd, ref, filePath)
})

// ── Terminal API ──
contextBridge.exposeInMainWorld('terminal', {
    spawn: (id: string, cwd?: string) => ipcRenderer.invoke('terminal:spawn', id, cwd),
    write: (id: string, data: string) => ipcRenderer.invoke('terminal:write', id, data),
    kill: (id: string) => ipcRenderer.invoke('terminal:kill', id),
    onData: (id: string, callback: (data: string) => void) => {
        const handler = (_event: any, data: string) => callback(data)
        ipcRenderer.on(`terminal:data:${id}`, handler)
        return () => ipcRenderer.off(`terminal:data:${id}`, handler)
    },
    onExit: (id: string, callback: (code: number) => void) => {
        const handler = (_event: any, code: number) => callback(code)
        ipcRenderer.on(`terminal:exit:${id}`, handler)
        return () => ipcRenderer.off(`terminal:exit:${id}`, handler)
    },
})

// ── Shell API ──
contextBridge.exposeInMainWorld('shell', {
    openExternal: (url: string) => ipcRenderer.invoke('shell:openExternal', url),
    openPath: (p: string) => ipcRenderer.invoke('shell:openPath', p),
})

// ── App API ──
contextBridge.exposeInMainWorld('appApi', {
    platform: () => ipcRenderer.invoke('app:platform'),
    homedir: () => ipcRenderer.invoke('app:homedir'),
    serverStatus: () => ipcRenderer.invoke('server:status'),
    serverRestart: () => ipcRenderer.invoke('server:restart'),
    setWorkspaceRoot: (workspaceRoot: string) => ipcRenderer.invoke('workspace:setRoot', workspaceRoot),
    getWorkspaceMemory: (workspaceRoot?: string) => ipcRenderer.invoke('workspace:getMemory', workspaceRoot),
    updateEditorContext: (payload: Record<string, any>) => ipcRenderer.invoke('workspace:updateEditorContext', payload),
})

// ── File Watcher API ──
contextBridge.exposeInMainWorld('fileWatcher', {
    watch: (dirPath: string) => ipcRenderer.invoke('fs:watch', dirPath),
    unwatch: () => ipcRenderer.invoke('fs:unwatch'),
    onChange: (callback: (event: string, filename: string) => void) => {
        const handler = (_event: any, ev: string, filename: string) => callback(ev, filename)
        ipcRenderer.on('fs:change', handler)
        return () => ipcRenderer.off('fs:change', handler)
    },
})
