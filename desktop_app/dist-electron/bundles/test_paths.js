import { fileURLToPath } from 'node:url'
import path from 'node:path'

const __dirname = path.dirname(fileURLToPath(import.meta.url))
console.log('__dirname:', __dirname)

const isBundlesDir = path.basename(__dirname) === 'bundles'
const APP_ROOT = path.join(__dirname, isBundlesDir ? '../..' : '..')
console.log('isBundlesDir:', isBundlesDir)
console.log('APP_ROOT:', APP_ROOT)

const MAIN_DIST = path.join(APP_ROOT, 'dist-electron')
console.log('MAIN_DIST:', MAIN_DIST)
console.log('preload path:', path.join(MAIN_DIST, 'preload.js'))
