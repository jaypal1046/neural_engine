import { contextBridge, ipcRenderer } from "electron";
contextBridge.exposeInMainWorld("ipcRenderer", {
  on(...args) {
    const [channel, listener] = args;
    return ipcRenderer.on(channel, (event, ...args2) => listener(event, ...args2));
  },
  off(...args) {
    const [channel, ...omit] = args;
    return ipcRenderer.off(channel, ...omit);
  },
  send(...args) {
    const [channel, ...omit] = args;
    return ipcRenderer.send(channel, ...omit);
  },
  invoke(...args) {
    const [channel, ...omit] = args;
    return ipcRenderer.invoke(channel, ...omit);
  },
  selectFile: () => ipcRenderer.invoke("dialog:openFile"),
  selectSaveFile: () => ipcRenderer.invoke("dialog:saveFile"),
  selectDirectory: () => ipcRenderer.invoke("dialog:openDirectory")
});
contextBridge.exposeInMainWorld("fs", {
  readDir: (dirPath) => ipcRenderer.invoke("fs:readDir", dirPath),
  readFile: (filePath) => ipcRenderer.invoke("fs:readFile", filePath),
  readFileBase64: (filePath) => ipcRenderer.invoke("fs:readFileBase64", filePath),
  writeFile: (filePath, content) => ipcRenderer.invoke("fs:writeFile", filePath, content),
  createFile: (filePath) => ipcRenderer.invoke("fs:createFile", filePath),
  createDir: (dirPath) => ipcRenderer.invoke("fs:createDir", dirPath),
  delete: (targetPath) => ipcRenderer.invoke("fs:delete", targetPath),
  rename: (oldPath, newPath) => ipcRenderer.invoke("fs:rename", oldPath, newPath),
  stat: (filePath) => ipcRenderer.invoke("fs:stat", filePath),
  getProjectRoot: () => ipcRenderer.invoke("fs:getProjectRoot"),
  selectDirectory: () => ipcRenderer.invoke("dialog:openDirectory")
});
contextBridge.exposeInMainWorld("searchApi", {
  searchFiles: (rootDir, query) => ipcRenderer.invoke("search:files", rootDir, query)
});
contextBridge.exposeInMainWorld("gitApi", {
  status: (cwd) => ipcRenderer.invoke("git:status", cwd),
  log: (cwd, count) => ipcRenderer.invoke("git:log", cwd, count),
  stage: (cwd, filePath) => ipcRenderer.invoke("git:stage", cwd, filePath),
  unstage: (cwd, filePath) => ipcRenderer.invoke("git:unstage", cwd, filePath),
  commit: (cwd, message) => ipcRenderer.invoke("git:commit", cwd, message),
  stageAll: (cwd) => ipcRenderer.invoke("git:stageAll", cwd),
  diff: (cwd, filePath) => ipcRenderer.invoke("git:diff", cwd, filePath),
  getFile: (cwd, ref, filePath) => ipcRenderer.invoke("git:getFile", cwd, ref, filePath)
});
contextBridge.exposeInMainWorld("terminal", {
  spawn: (id, cwd) => ipcRenderer.invoke("terminal:spawn", id, cwd),
  write: (id, data) => ipcRenderer.invoke("terminal:write", id, data),
  kill: (id) => ipcRenderer.invoke("terminal:kill", id),
  onData: (id, callback) => {
    const handler = (_event, data) => callback(data);
    ipcRenderer.on(`terminal:data:${id}`, handler);
    return () => ipcRenderer.off(`terminal:data:${id}`, handler);
  },
  onExit: (id, callback) => {
    const handler = (_event, code) => callback(code);
    ipcRenderer.on(`terminal:exit:${id}`, handler);
    return () => ipcRenderer.off(`terminal:exit:${id}`, handler);
  }
});
contextBridge.exposeInMainWorld("shell", {
  openExternal: (url) => ipcRenderer.invoke("shell:openExternal", url),
  openPath: (p) => ipcRenderer.invoke("shell:openPath", p)
});
contextBridge.exposeInMainWorld("appApi", {
  platform: () => ipcRenderer.invoke("app:platform"),
  homedir: () => ipcRenderer.invoke("app:homedir"),
  serverStatus: () => ipcRenderer.invoke("server:status"),
  serverRestart: () => ipcRenderer.invoke("server:restart"),
  setWorkspaceRoot: (workspaceRoot) => ipcRenderer.invoke("workspace:setRoot", workspaceRoot),
  getWorkspaceMemory: (workspaceRoot) => ipcRenderer.invoke("workspace:getMemory", workspaceRoot),
  updateEditorContext: (payload) => ipcRenderer.invoke("workspace:updateEditorContext", payload)
});
contextBridge.exposeInMainWorld("fileWatcher", {
  watch: (dirPath) => ipcRenderer.invoke("fs:watch", dirPath),
  unwatch: () => ipcRenderer.invoke("fs:unwatch"),
  onChange: (callback) => {
    const handler = (_event, ev, filename) => callback(ev, filename);
    ipcRenderer.on("fs:change", handler);
    return () => ipcRenderer.off("fs:change", handler);
  }
});
