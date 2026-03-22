const rendererWindow = window as any

function invoke(channel: string, ...args: any[]) {
    return rendererWindow.ipcRenderer?.invoke?.(channel, ...args)
}

export function getProjectRoot() {
    return rendererWindow.fs?.getProjectRoot?.() ?? invoke('fs:getProjectRoot')
}

export function selectFile() {
    return rendererWindow.ipcRenderer?.selectFile?.() ?? invoke('dialog:openFile')
}

export function selectSaveFile() {
    return rendererWindow.ipcRenderer?.selectSaveFile?.() ?? invoke('dialog:saveFile')
}

export function selectDirectory() {
    return rendererWindow.ipcRenderer?.selectDirectory?.()
        ?? rendererWindow.fs?.selectDirectory?.()
        ?? invoke('dialog:openDirectory')
}

export function readDir(dirPath: string) {
    return rendererWindow.fs?.readDir?.(dirPath) ?? invoke('fs:readDir', dirPath)
}

export function readFile(filePath: string) {
    return rendererWindow.fs?.readFile?.(filePath) ?? invoke('fs:readFile', filePath)
}

export function readFileBase64(filePath: string) {
    return rendererWindow.fs?.readFileBase64?.(filePath) ?? invoke('fs:readFileBase64', filePath)
}

export function writeFile(filePath: string, content: string) {
    return rendererWindow.fs?.writeFile?.(filePath, content) ?? invoke('fs:writeFile', filePath, content)
}

export function createFile(filePath: string) {
    return rendererWindow.fs?.createFile?.(filePath) ?? invoke('fs:createFile', filePath)
}

export function createDir(dirPath: string) {
    return rendererWindow.fs?.createDir?.(dirPath) ?? invoke('fs:createDir', dirPath)
}

export function deletePath(targetPath: string) {
    return rendererWindow.fs?.delete?.(targetPath) ?? invoke('fs:delete', targetPath)
}

export function renamePath(oldPath: string, newPath: string) {
    return rendererWindow.fs?.rename?.(oldPath, newPath) ?? invoke('fs:rename', oldPath, newPath)
}
