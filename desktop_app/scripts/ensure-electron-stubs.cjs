const fs = require('node:fs');
const path = require('node:path');

const projectRoot = path.resolve(__dirname, '..');
const distElectronDir = path.join(projectRoot, 'dist-electron');
const bundlesDir = path.join(distElectronDir, 'bundles');

function writeFileIfChanged(targetPath, content) {
  const current = fs.existsSync(targetPath) ? fs.readFileSync(targetPath, 'utf8') : null;
  if (current === content) {
    return;
  }
  fs.writeFileSync(targetPath, content, 'utf8');
}

const sharedLoader = `
const fs = require('node:fs');
const path = require('node:path');

function resolveLatestBundle(prefix) {
  const bundlesDir = path.join(__dirname, 'bundles');
  if (!fs.existsSync(bundlesDir)) {
    throw new Error('Electron bundle directory not found: ' + bundlesDir);
  }

  const entries = fs.readdirSync(bundlesDir)
    .filter((name) => name.startsWith(prefix + '-') && name.endsWith('.js'))
    .map((name) => ({
      name,
      fullPath: path.join(bundlesDir, name),
      mtimeMs: fs.statSync(path.join(bundlesDir, name)).mtimeMs,
    }))
    .sort((left, right) => right.mtimeMs - left.mtimeMs);

  if (!entries.length) {
    throw new Error('No built Electron bundle found for prefix "' + prefix + '" in ' + bundlesDir);
  }

  return entries[0].fullPath;
}
`.trim();

const mainStub = `
${sharedLoader}

module.exports = require(resolveLatestBundle('main'));
`.trimStart();

const preloadStub = `
${sharedLoader}

require(resolveLatestBundle('preload'));
`.trimStart();

fs.mkdirSync(distElectronDir, { recursive: true });
fs.mkdirSync(bundlesDir, { recursive: true });

writeFileIfChanged(path.join(distElectronDir, 'main.js'), mainStub);
writeFileIfChanged(path.join(distElectronDir, 'preload.js'), preloadStub);
