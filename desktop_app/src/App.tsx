import { useState } from 'react'
import './App.css'

import { Sidebar } from './components/Sidebar'
import { Header } from './components/Header'
import { CompressView } from './components/CompressView'
import { DecompressView } from './components/DecompressView'
import { BrowserView } from './components/BrowserView'
import { HelperView } from './components/HelperView'
import { ConsoleOutput } from './components/ConsoleOutput'
import { SearchView } from './components/SearchView'
import { ScriptsView } from './components/ScriptsView'

declare global {
  interface Window {
    ws: any;
    ipcRenderer: any;
  }
}

function App() {
  const [activeTab, setActiveTab] = useState('helper')

  const showConsole = activeTab === 'compress' || activeTab === 'decompress' || activeTab === 'browser';

  return (
    <div className="neural-studio">
      <Sidebar activeTab={activeTab} setActiveTab={setActiveTab} />
      <main className="content">
        <Header activeTab={activeTab} />
        <div className="main-area">
          {activeTab === 'compress' && <CompressView />}
          {activeTab === 'decompress' && <DecompressView />}
          {activeTab === 'browser' && <BrowserView />}
          {activeTab === 'search' && <SearchView />}
          {activeTab === 'scripts' && <ScriptsView />}
          {showConsole && <ConsoleOutput />}
          {activeTab === 'helper' && <HelperView />}
        </div>
      </main>
    </div>
  )
}

export default App