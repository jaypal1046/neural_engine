"use strict";
Object.defineProperty(exports, Symbol.toStringTag, { value: "Module" });
const electron = require("electron");
const node_url = require("node:url");
const path = require("node:path");
const node_child_process = require("node:child_process");
const node_fs = require("node:fs");
const node_os = require("node:os");
var _documentCurrentScript = typeof document !== "undefined" ? document.currentScript : null;
const __dirname$1 = path.dirname(node_url.fileURLToPath(typeof document === "undefined" ? require("url").pathToFileURL(__filename).href : _documentCurrentScript && _documentCurrentScript.tagName.toUpperCase() === "SCRIPT" && _documentCurrentScript.src || new URL("main.js", document.baseURI).href));
process.env.APP_ROOT = path.join(__dirname$1, "..");
const VITE_DEV_SERVER_URL = process.env["VITE_DEV_SERVER_URL"];
const MAIN_DIST = path.join(process.env.APP_ROOT, "dist-electron");
const RENDERER_DIST = path.join(process.env.APP_ROOT, "dist");
process.env.VITE_PUBLIC = VITE_DEV_SERVER_URL ? path.join(process.env.APP_ROOT, "public") : RENDERER_DIST;
let win;
let serverProcess = null;
let fileWatcher = null;
const terminalProcesses = /* @__PURE__ */ new Map();
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
const IGNORED_DIRS = /* @__PURE__ */ new Set([".git", "node_modules", "__pycache__", "dist-electron", "dist", ".next", ".vscode", "build", "out", ".svn", "coverage", ".angular"]);
const BINARY_EXTENSIONS = /* @__PURE__ */ new Set([".jpg", ".jpeg", ".png", ".gif", ".ico", ".webp", ".mp3", ".mp4", ".avi", ".mov", ".pdf", ".zip", ".rar", ".7z", ".tar", ".gz", ".exe", ".dll", ".so", ".woff", ".woff2", ".ttf", ".eot", ".otf"]);
function getFileTree(dirPath, depth = 0, maxDepth = 3) {
  if (depth > maxDepth)
    return [];
  try {
    const entries = node_fs.readdirSync(dirPath, { withFileTypes: true });
    const results = [];
    const sorted = entries.sort((a, b) => {
      if (a.isDirectory() && !b.isDirectory())
        return -1;
      if (!a.isDirectory() && b.isDirectory())
        return 1;
      return a.name.localeCompare(b.name);
    });
    for (const entry of sorted) {
      if (entry.name.startsWith(".") || IGNORED_DIRS.has(entry.name))
        continue;
      const fullPath = path.join(dirPath, entry.name);
      if (entry.isDirectory()) {
        results.push({
          name: entry.name,
          path: fullPath,
          type: "directory",
          children: getFileTree(fullPath, depth + 1, maxDepth)
        });
      } else {
        let size = 0;
        try {
          size = node_fs.statSync(fullPath).size;
        } catch {
        }
        results.push({
          name: entry.name,
          path: fullPath,
          type: "file",
          size,
          extension: path.extname(entry.name).toLowerCase()
        });
      }
    }
    return results;
  } catch {
    return [];
  }
}
function searchInFiles(rootDir, query, maxResults = 100) {
  const results = [];
  const lowerQuery = query.toLowerCase();
  function walkDir(dir) {
    if (results.length >= maxResults)
      return;
    try {
      const entries = node_fs.readdirSync(dir, { withFileTypes: true });
      for (const entry of entries) {
        if (results.length >= maxResults)
          return;
        if (entry.name.startsWith(".") || IGNORED_DIRS.has(entry.name))
          continue;
        const fullPath = path.join(dir, entry.name);
        if (entry.isDirectory()) {
          walkDir(fullPath);
        } else {
          const ext = path.extname(entry.name).toLowerCase();
          if (BINARY_EXTENSIONS.has(ext))
            continue;
          try {
            const stat = node_fs.statSync(fullPath);
            if (stat.size > 1024 * 512)
              continue;
            const content = node_fs.readFileSync(fullPath, "utf-8");
            const lines = content.split("\n");
            const matches = [];
            for (let i = 0; i < lines.length; i++) {
              if (lines[i].toLowerCase().includes(lowerQuery)) {
                matches.push({
                  line: i + 1,
                  text: lines[i].trim().substring(0, 200),
                  column: lines[i].toLowerCase().indexOf(lowerQuery)
                });
                if (matches.length >= 5)
                  break;
              }
            }
            if (matches.length > 0) {
              results.push({
                filePath: fullPath,
                fileName: entry.name,
                relativePath: path.relative(rootDir, fullPath),
                matches
              });
            }
          } catch {
          }
        }
      }
    } catch {
    }
  }
  walkDir(rootDir);
  return results;
}
function runGitCommand(cwd, args) {
  return new Promise((resolve) => {
    var _a, _b;
    const proc = node_child_process.spawn("git", args, { cwd, shell: true });
    let stdout = "";
    let stderr = "";
    (_a = proc.stdout) == null ? void 0 : _a.on("data", (d) => {
      stdout += d.toString();
    });
    (_b = proc.stderr) == null ? void 0 : _b.on("data", (d) => {
      stderr += d.toString();
    });
    proc.on("close", (code) => resolve({ stdout, stderr, code: code || 0 }));
    proc.on("error", () => resolve({ stdout: "", stderr: "git not found", code: 1 }));
  });
}
function createWindow() {
  win = new electron.BrowserWindow({
    width: 1440,
    height: 900,
    minWidth: 1024,
    minHeight: 700,
    icon: path.join(process.env.VITE_PUBLIC || "", "electron-vite.svg"),
    webPreferences: {
      preload: path.join(__dirname$1, "preload.js"),
      webviewTag: true,
      nodeIntegration: false,
      contextIsolation: true
    },
    autoHideMenuBar: true,
    title: "Neural Studio IDE",
    frame: false,
    titleBarStyle: "hidden",
    titleBarOverlay: {
      color: "#0D0D0D",
      symbolColor: "#888",
      height: 36
    },
    backgroundColor: "#0D0D0D"
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
    terminalProcesses.forEach((proc) => {
      if (proc.pid) {
        if (process.platform === "win32") {
          node_child_process.spawn("taskkill", ["/pid", String(proc.pid), "/f", "/t"], { shell: true });
        } else {
          proc.kill("SIGTERM");
        }
      }
    });
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
  electron.ipcMain.handle("dialog:openDirectory", async () => {
    const { canceled, filePaths } = await electron.dialog.showOpenDialog({ properties: ["openDirectory"] });
    if (!canceled)
      return filePaths[0];
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
  electron.ipcMain.handle("fs:readDir", (_event, dirPath) => {
    return getFileTree(dirPath, 0, 4);
  });
  electron.ipcMain.handle("fs:readFile", (_event, filePath) => {
    try {
      return node_fs.readFileSync(filePath, "utf-8");
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:readFileBase64", (_event, filePath) => {
    try {
      const buffer = node_fs.readFileSync(filePath);
      return buffer.toString("base64");
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:writeFile", (_event, filePath, content) => {
    try {
      node_fs.writeFileSync(filePath, content, "utf-8");
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:createFile", (_event, filePath) => {
    try {
      node_fs.writeFileSync(filePath, "", "utf-8");
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:createDir", (_event, dirPath) => {
    try {
      node_fs.mkdirSync(dirPath, { recursive: true });
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:delete", (_event, targetPath) => {
    try {
      const stat = node_fs.statSync(targetPath);
      if (stat.isDirectory()) {
        if (process.platform === "win32") {
          node_child_process.exec(`rmdir /s /q "${targetPath}"`);
        } else {
          node_child_process.exec(`rm -rf "${targetPath}"`);
        }
      } else {
        node_fs.unlinkSync(targetPath);
      }
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:rename", (_event, oldPath, newPath) => {
    try {
      node_fs.renameSync(oldPath, newPath);
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:stat", (_event, filePath) => {
    try {
      const s = node_fs.statSync(filePath);
      return {
        size: s.size,
        isFile: s.isFile(),
        isDirectory: s.isDirectory(),
        modified: s.mtime.toISOString(),
        created: s.birthtime.toISOString()
      };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:getProjectRoot", () => {
    return path.join(process.env.APP_ROOT, "..");
  });
  electron.ipcMain.handle("search:files", (_event, rootDir, query) => {
    if (!query || query.length < 2)
      return [];
    return searchInFiles(rootDir, query);
  });
  electron.ipcMain.handle("git:status", async (_event, cwd) => {
    const result = await runGitCommand(cwd, ["status", "--porcelain", "-b"]);
    if (result.code !== 0)
      return { error: result.stderr };
    const lines = result.stdout.trim().split("\n");
    const branchLine = lines[0] || "";
    const branch = branchLine.replace("## ", "").split("...")[0] || "unknown";
    const changes = lines.slice(1).filter((l) => l.trim()).map((line) => {
      const status = line.substring(0, 2).trim();
      const file = line.substring(3).trim();
      return { status, file, staged: line[0] !== " " && line[0] !== "?" };
    });
    return { branch, changes };
  });
  electron.ipcMain.handle("git:log", async (_event, cwd, count = 10) => {
    const result = await runGitCommand(cwd, ["log", `--pretty=format:%H|||%an|||%s|||%ar`, `-${count}`]);
    if (result.code !== 0)
      return [];
    return result.stdout.trim().split("\n").filter((l) => l).map((line) => {
      const [hash, author, message, date] = line.split("|||");
      return { hash: hash == null ? void 0 : hash.substring(0, 7), author, message, date };
    });
  });
  electron.ipcMain.handle("git:stage", async (_event, cwd, filePath) => {
    const result = await runGitCommand(cwd, ["add", filePath]);
    return { success: result.code === 0, error: result.stderr };
  });
  electron.ipcMain.handle("git:unstage", async (_event, cwd, filePath) => {
    const result = await runGitCommand(cwd, ["reset", "HEAD", filePath]);
    return { success: result.code === 0, error: result.stderr };
  });
  electron.ipcMain.handle("git:commit", async (_event, cwd, message) => {
    const result = await runGitCommand(cwd, ["commit", "-m", `"${message}"`]);
    return { success: result.code === 0, output: result.stdout, error: result.stderr };
  });
  electron.ipcMain.handle("git:stageAll", async (_event, cwd) => {
    const result = await runGitCommand(cwd, ["add", "-A"]);
    return { success: result.code === 0 };
  });
  electron.ipcMain.handle("git:diff", async (_event, cwd, filePath) => {
    const result = await runGitCommand(cwd, ["diff", filePath]);
    return result.stdout;
  });
  electron.ipcMain.handle("git:getFile", async (_event, cwd, ref, filePath) => {
    const normalizedPath = filePath.replace(/\\/g, "/");
    const result = await runGitCommand(cwd, ["show", `${ref}:${normalizedPath}`]);
    if (result.code !== 0)
      return { error: result.stderr };
    return { content: result.stdout };
  });
  electron.ipcMain.handle("terminal:spawn", (_event, id, cwd) => {
    var _a, _b;
    const shellCmd = process.platform === "win32" ? "powershell.exe" : process.env.SHELL || "/bin/bash";
    const defaultCwd = cwd || path.join(process.env.APP_ROOT, "..");
    const proc = node_child_process.spawn(shellCmd, [], {
      cwd: defaultCwd,
      stdio: ["pipe", "pipe", "pipe"],
      env: { ...process.env, TERM: "xterm-256color" },
      shell: false
    });
    terminalProcesses.set(id, proc);
    (_a = proc.stdout) == null ? void 0 : _a.on("data", (data) => {
      if (win) {
        win.webContents.send(`terminal:data:${id}`, data.toString());
      }
    });
    (_b = proc.stderr) == null ? void 0 : _b.on("data", (data) => {
      if (win) {
        win.webContents.send(`terminal:data:${id}`, data.toString());
      }
    });
    proc.on("exit", (code) => {
      terminalProcesses.delete(id);
      if (win) {
        win.webContents.send(`terminal:exit:${id}`, code);
      }
    });
    return { pid: proc.pid, id };
  });
  electron.ipcMain.handle("terminal:write", (_event, id, data) => {
    const proc = terminalProcesses.get(id);
    if (proc && proc.stdin) {
      proc.stdin.write(data);
      return { success: true };
    }
    return { error: "Terminal not found" };
  });
  electron.ipcMain.handle("terminal:kill", (_event, id) => {
    const proc = terminalProcesses.get(id);
    if (proc) {
      if (process.platform === "win32" && proc.pid) {
        node_child_process.spawn("taskkill", ["/pid", String(proc.pid), "/f", "/t"], { shell: true });
      } else {
        proc.kill("SIGTERM");
      }
      terminalProcesses.delete(id);
      return { success: true };
    }
    return { error: "Terminal not found" };
  });
  electron.ipcMain.handle("shell:openExternal", (_event, url) => {
    electron.shell.openExternal(url);
  });
  electron.ipcMain.handle("shell:openPath", (_event, p) => {
    electron.shell.openPath(p);
  });
  electron.ipcMain.handle("app:platform", () => node_os.platform());
  electron.ipcMain.handle("app:homedir", () => node_os.homedir());
  electron.ipcMain.handle("fs:watch", (_event, dirPath) => {
    if (fileWatcher) {
      fileWatcher.close();
      fileWatcher = null;
    }
    if (!node_fs.existsSync(dirPath))
      return { error: "Directory not found" };
    let debounceTimer = null;
    try {
      fileWatcher = node_fs.watch(dirPath, { recursive: true }, (eventType, filename) => {
        if (!filename)
          return;
        const ignored = ["node_modules", ".git", "dist", "dist-electron", "__pycache__", ".next", "build", "out"];
        if (ignored.some((d) => filename.includes(d)))
          return;
        if (debounceTimer)
          clearTimeout(debounceTimer);
        debounceTimer = setTimeout(() => {
          if (win) {
            win.webContents.send("fs:change", eventType, filename);
          }
        }, 300);
      });
      return { success: true };
    } catch (e) {
      return { error: e.message };
    }
  });
  electron.ipcMain.handle("fs:unwatch", () => {
    if (fileWatcher) {
      fileWatcher.close();
      fileWatcher = null;
    }
    return { success: true };
  });
  startServer();
  createWindow();
});
exports.MAIN_DIST = MAIN_DIST;
exports.RENDERER_DIST = RENDERER_DIST;
exports.VITE_DEV_SERVER_URL = VITE_DEV_SERVER_URL;
