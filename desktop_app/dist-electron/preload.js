"use strict";
const electron = require("electron");
electron.contextBridge.exposeInMainWorld("ipcRenderer", {
  on(...args) {
    const [channel, listener] = args;
    return electron.ipcRenderer.on(channel, (event, ...args2) => listener(event, ...args2));
  },
  off(...args) {
    const [channel, ...omit] = args;
    return electron.ipcRenderer.off(channel, ...omit);
  },
  send(...args) {
    const [channel, ...omit] = args;
    return electron.ipcRenderer.send(channel, ...omit);
  },
  invoke(...args) {
    const [channel, ...omit] = args;
    return electron.ipcRenderer.invoke(channel, ...omit);
  },
  selectFile: () => electron.ipcRenderer.invoke("dialog:openFile"),
  selectSaveFile: () => electron.ipcRenderer.invoke("dialog:saveFile"),
  selectDirectory: () => electron.ipcRenderer.invoke("dialog:openDirectory")
});
electron.contextBridge.exposeInMainWorld("fs", {
  readDir: (dirPath) => electron.ipcRenderer.invoke("fs:readDir", dirPath),
  readFile: (filePath) => electron.ipcRenderer.invoke("fs:readFile", filePath),
  writeFile: (filePath, content) => electron.ipcRenderer.invoke("fs:writeFile", filePath, content),
  createFile: (filePath) => electron.ipcRenderer.invoke("fs:createFile", filePath),
  createDir: (dirPath) => electron.ipcRenderer.invoke("fs:createDir", dirPath),
  delete: (targetPath) => electron.ipcRenderer.invoke("fs:delete", targetPath),
  rename: (oldPath, newPath) => electron.ipcRenderer.invoke("fs:rename", oldPath, newPath),
  stat: (filePath) => electron.ipcRenderer.invoke("fs:stat", filePath),
  getProjectRoot: () => electron.ipcRenderer.invoke("fs:getProjectRoot")
});
electron.contextBridge.exposeInMainWorld("searchApi", {
  searchFiles: (rootDir, query) => electron.ipcRenderer.invoke("search:files", rootDir, query)
});
electron.contextBridge.exposeInMainWorld("gitApi", {
  status: (cwd) => electron.ipcRenderer.invoke("git:status", cwd),
  log: (cwd, count) => electron.ipcRenderer.invoke("git:log", cwd, count),
  stage: (cwd, filePath) => electron.ipcRenderer.invoke("git:stage", cwd, filePath),
  unstage: (cwd, filePath) => electron.ipcRenderer.invoke("git:unstage", cwd, filePath),
  commit: (cwd, message) => electron.ipcRenderer.invoke("git:commit", cwd, message),
  stageAll: (cwd) => electron.ipcRenderer.invoke("git:stageAll", cwd),
  diff: (cwd, filePath) => electron.ipcRenderer.invoke("git:diff", cwd, filePath),
  getFile: (cwd, ref, filePath) => electron.ipcRenderer.invoke("git:getFile", cwd, ref, filePath)
});
electron.contextBridge.exposeInMainWorld("terminal", {
  spawn: (id, cwd) => electron.ipcRenderer.invoke("terminal:spawn", id, cwd),
  write: (id, data) => electron.ipcRenderer.invoke("terminal:write", id, data),
  kill: (id) => electron.ipcRenderer.invoke("terminal:kill", id),
  onData: (id, callback) => {
    const handler = (_event, data) => callback(data);
    electron.ipcRenderer.on(`terminal:data:${id}`, handler);
    return () => electron.ipcRenderer.off(`terminal:data:${id}`, handler);
  },
  onExit: (id, callback) => {
    const handler = (_event, code) => callback(code);
    electron.ipcRenderer.on(`terminal:exit:${id}`, handler);
    return () => electron.ipcRenderer.off(`terminal:exit:${id}`, handler);
  }
});
electron.contextBridge.exposeInMainWorld("shell", {
  openExternal: (url) => electron.ipcRenderer.invoke("shell:openExternal", url),
  openPath: (p) => electron.ipcRenderer.invoke("shell:openPath", p)
});
electron.contextBridge.exposeInMainWorld("appApi", {
  platform: () => electron.ipcRenderer.invoke("app:platform"),
  homedir: () => electron.ipcRenderer.invoke("app:homedir"),
  serverStatus: () => electron.ipcRenderer.invoke("server:status"),
  serverRestart: () => electron.ipcRenderer.invoke("server:restart")
});
electron.contextBridge.exposeInMainWorld("fileWatcher", {
  watch: (dirPath) => electron.ipcRenderer.invoke("fs:watch", dirPath),
  unwatch: () => electron.ipcRenderer.invoke("fs:unwatch"),
  onChange: (callback) => {
    const handler = (_event, ev, filename) => callback(ev, filename);
    electron.ipcRenderer.on("fs:change", handler);
    return () => electron.ipcRenderer.off("fs:change", handler);
  }
});
