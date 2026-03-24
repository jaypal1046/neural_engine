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

try {
  require(resolveLatestBundle('preload'));
} catch (e) {
  fs.writeFileSync(path.join(__dirname, '..', 'preload-error.log'), e.stack || e.message);
}
