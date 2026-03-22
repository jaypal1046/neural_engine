import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'
import electron from 'vite-plugin-electron'
import renderer from 'vite-plugin-electron-renderer'

// Note: "type": "module" was removed from package.json so vite-plugin-electron
// compiles electron main/preload as CJS (needed for Electron's module loader).
export default defineConfig({
  plugins: [
    react(),
    electron([
      {
        entry: 'electron/main.ts',
        onstart(options) {
          options.startup()
        },
        vite: {
          build: {
            rollupOptions: {
              output: {
                entryFileNames: 'bundles/[name]-[hash].js',
                chunkFileNames: 'bundles/chunks/[name]-[hash].js',
                assetFileNames: 'bundles/assets/[name]-[hash][extname]',
              },
            },
          },
        },
      },
      {
        entry: 'electron/preload.ts',
        vite: {
          build: {
            rollupOptions: {
              output: {
                entryFileNames: 'bundles/[name]-[hash].js',
                chunkFileNames: 'bundles/chunks/[name]-[hash].js',
                assetFileNames: 'bundles/assets/[name]-[hash][extname]',
              },
            },
          },
        },
        onstart(options) {
          options.reload()
        },
      },
    ]),
    renderer(),
  ],
})
