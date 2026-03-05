// Bootstrap: run Vite from desktop_app subdirectory (Windows ESM-compatible)
const path = require('path');
const { pathToFileURL } = require('url');
const desktopDir = path.join(__dirname, 'desktop_app');
process.chdir(desktopDir);
const vite = path.join(desktopDir, 'node_modules', 'vite', 'bin', 'vite.js');
import(pathToFileURL(vite).href);
