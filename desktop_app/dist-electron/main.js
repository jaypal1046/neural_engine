"use strict";
Object.defineProperty(exports, Symbol.toStringTag, { value: "Module" });
const electron = require("electron");
const node_url = require("node:url");
const path = require("node:path");
const node_child_process = require("node:child_process");
const node_fs = require("node:fs");
var _documentCurrentScript = typeof document !== "undefined" ? document.currentScript : null;
const __dirname$1 = path.dirname(node_url.fileURLToPath(typeof document === "undefined" ? require("url").pathToFileURL(__filename).href : _documentCurrentScript && _documentCurrentScript.tagName.toUpperCase() === "SCRIPT" && _documentCurrentScript.src || new URL("main.js", document.baseURI).href));
process.env.APP_ROOT = path.join(__dirname$1, "..");
const VITE_DEV_SERVER_URL = process.env["VITE_DEV_SERVER_URL"];
const MAIN_DIST = path.join(process.env.APP_ROOT, "dist-electron");
const RENDERER_DIST = path.join(process.env.APP_ROOT, "dist");
process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL ? path.join(process.env.APP_ROOT, "public") : RENDERER_DIST;
let win;
let serverProcess = null;
function findPython() {
  const candidates = ["python", "python3", "py"];
  return candidates[0];
}
function startServer() {
  var _a, _b;
  const projectRoot = path.join(process.env.APP_ROOT, "..");
  const serverScript = path.join(projectRoot, "server", "main.py");
  if (!node_fs.existsSync(serverScript)) {
    console.error(`[Neural Studio] Server script not found: ${serverScript}`);
    return;
  }
  console.log(`[Neural Studio] Starting Python server: ${serverScript}`);
  const pythonCmd = findPython();
  serverProcess = node_child_process.spawn(pythonCmd, [serverScript], {
    cwd: path.join(projectRoot, "server"),
    stdio: ["pipe", "pipe", "pipe"],
    env: { ...process.env }
  });
  (_a = serverProcess.stdout) == null ? void 0 : _a.on("data", (data) => {
    const msg = data.toString().trim();
    if (msg)
      console.log(`[Server] ${msg}`);
    if (win) {
      win.webContents.send("server-log", msg);
    }
  });
  (_b = serverProcess.stderr) == null ? void 0 : _b.on("data", (data) => {
    const msg = data.toString().trim();
    if (msg)
      console.log(`[Server:ERR] ${msg}`);
  });
  serverProcess.on("exit", (code) => {
    console.log(`[Neural Studio] Server process exited with code ${code}`);
    serverProcess = null;
  });
  serverProcess.on("error", (err) => {
    console.error(`[Neural Studio] Failed to start server:`, err.message);
    serverProcess = null;
  });
}
function stopServer() {
  if (serverProcess) {
    console.log("[Neural Studio] Stopping Python server...");
    if (process.platform === "win32" && serverProcess.pid) {
      node_child_process.spawn("taskkill", ["/pid", String(serverProcess.pid), "/f", "/t"], { shell: true });
    } else {
      serverProcess.kill("SIGTERM");
    }
    serverProcess = null;
  }
}
function createWindow() {
  win = new electron.BrowserWindow({
    width: 1220,
    height: 780,
    minWidth: 900,
    minHeight: 600,
    icon: path.join(process.env.VITE_PUBLIC || "", "electron-vite.svg"),
    webPreferences: {
      preload: path.join(__dirname$1, "preload.js")
    },
    autoHideMenuBar: true,
    title: "Neural Studio V10"
  });
  win.webContents.on("did-finish-load", () => {
    win == null ? void 0 : win.webContents.send("main-process-message", (/* @__PURE__ */ new Date()).toLocaleString());
  });
  if (VITE_DEV_SERVER_URL) {
    win.loadURL(VITE_DEV_SERVER_URL);
  } else {
    win.loadFile(path.join(RENDERER_DIST, "index.html"));
  }
}
electron.app.on("window-all-closed", () => {
  if (process.platform !== "darwin") {
    stopServer();
    electron.app.quit();
    win = null;
  }
});
electron.app.on("before-quit", () => {
  stopServer();
});
electron.app.on("activate", () => {
  if (electron.BrowserWindow.getAllWindows().length === 0) {
    createWindow();
  }
});
electron.app.whenReady().then(() => {
  electron.ipcMain.handle("dialog:openFile", async () => {
    const { canceled, filePaths } = await electron.dialog.showOpenDialog({});
    if (!canceled)
      return filePaths[0];
    return null;
  });
  electron.ipcMain.handle("dialog:saveFile", async () => {
    const { canceled, filePath } = await electron.dialog.showSaveDialog({});
    if (!canceled)
      return filePath;
    return null;
  });
  electron.ipcMain.handle("server:status", () => {
    return { running: serverProcess !== null, pid: serverProcess == null ? void 0 : serverProcess.pid };
  });
  electron.ipcMain.handle("server:restart", () => {
    stopServer();
    setTimeout(startServer, 500);
    return { status: "restarting" };
  });
  startServer();
  createWindow();
});
exports.MAIN_DIST = MAIN_DIST;
exports.RENDERER_DIST = RENDERER_DIST;
exports.VITE_DEV_SERVER_URL = VITE_DEV_SERVER_URL;
