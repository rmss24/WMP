import { useState, useEffect, useCallback, useRef } from 'react'
import TitleBar from './components/TitleBar'
import Sidebar from './components/Sidebar'
import CharactersPage from './components/CharactersPage'
import DisplayPage from './components/DisplayPage'
import SystemPage from './components/SystemPage'
import './App.css'

export type Config = {
  version: number
  autoStart: boolean
  debugMode: boolean
  launchSettingsOnStart: boolean
  character: { enabled: boolean; path: string; maxWidth: number; maxHeight: number; margin: number }
  crosshair: { enabled: boolean; length: number; gap: number; thickness: number; color: string }
  border: { enabled: boolean; thickness: number; color: string }
  hud: { enabled: boolean; fontSize: number }
  overlay: { visible: boolean; clickThrough: boolean }
}

const api = (window as unknown as { api: {
  getConfig: () => Promise<Config>
  saveConfig: (c: Config) => Promise<boolean>
  selectGif: () => Promise<string | null>
  getCharacters: () => Promise<{ name: string; path: string; previewUrl: string }[]>
  openCharactersFolder: () => Promise<void>
  setAutostart: (v: boolean) => Promise<boolean>
  getAutostart: () => Promise<boolean>
  launchOverlay: () => Promise<boolean>
  minimize: () => void
  close: () => void
  onConfigChanged: (cb: (cfg: Config) => void) => () => void
}}).api

type Page = 'characters' | 'display' | 'system'

export default function App() {
  const [config, setConfig] = useState<Config | null>(null)
  const [page, setPage] = useState<Page>('characters')
  const [saved, setSaved] = useState(false)
  const saveTimer = useRef<ReturnType<typeof setTimeout> | null>(null)

  useEffect(() => {
    api.getConfig().then(setConfig)
    const unsub = api.onConfigChanged((fresh) => {
      setConfig(prev => {
        if (!saveTimer.current) return fresh
        return prev
      })
    })
    return unsub
  }, [])

  const updateConfig = useCallback((partial: Partial<Config> | ((prev: Config) => Config)) => {
    setConfig(prev => {
      if (!prev) return prev
      const next = typeof partial === 'function' ? partial(prev) : { ...prev, ...partial }
      if (saveTimer.current) clearTimeout(saveTimer.current)
      saveTimer.current = setTimeout(() => {
        api.saveConfig(next).then(() => {
          setSaved(true)
          setTimeout(() => setSaved(false), 1500)
        })
      }, 400)
      return next
    })
  }, [])

  if (!config) {
    return (
      <div className="app">
        <div className="loading">Caricamento…</div>
      </div>
    )
  }

  return (
    <div className="app">
      <TitleBar saved={saved} onMinimize={api.minimize} onClose={api.close} />
      <div className="body">
        <Sidebar page={page} onNavigate={setPage} />
        <main className="content">
          {page === 'characters' && (
            <CharactersPage config={config} update={updateConfig} api={api} />
          )}
          {page === 'display' && (
            <DisplayPage config={config} update={updateConfig} />
          )}
          {page === 'system' && (
            <SystemPage config={config} update={updateConfig} api={api} />
          )}
        </main>
      </div>
    </div>
  )
}
