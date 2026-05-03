import { useEffect, useRef, useState } from 'react'
import type { Config } from '../App'
import './Page.css'

type Char = { name: string; path: string; previewUrl: string }

type Props = {
  config: Config
  update: (p: Partial<Config> | ((prev: Config) => Config)) => void
  api: {
    getCharacters: () => Promise<Char[]>
    selectGif: () => Promise<string | null>
    openCharactersFolder: () => Promise<void>
    saveConfig: (c: Config) => Promise<boolean>
  }
}

export default function CharactersPage({ config, update, api }: Props) {
  const [chars, setChars] = useState<Char[]>([])
  const [menuOpen, setMenuOpen] = useState<string | null>(null)
  const menuRef = useRef<HTMLDivElement | null>(null)

  const reload = () => api.getCharacters().then(setChars)
  useEffect(() => { reload() }, [])

  // Close menu on outside click
  useEffect(() => {
    const handler = (e: MouseEvent) => {
      if (menuRef.current && !menuRef.current.contains(e.target as Node))
        setMenuOpen(null)
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [])

  const normPath = (p: string) => p.replace(/\//g, '\\').toLowerCase()

  const selectChar = (path: string) =>
    update(prev => ({ ...prev, character: { ...prev.character, path } }))

  const browseGif = async () => {
    const path = await api.selectGif()
    if (path) {
      update(prev => ({ ...prev, character: { ...prev.character, path } }))
      reload()
    }
  }

  const setWalkGif = async (charPath: string) => {
    setMenuOpen(null)
    const path = await api.selectGif()
    if (!path) return
    update(prev => ({
      ...prev,
      walkPaths: { ...prev.walkPaths, [charPath]: path }
    }))
  }

  const clearWalkGif = (charPath: string) => {
    setMenuOpen(null)
    update(prev => {
      const next = { ...prev.walkPaths }
      delete next[charPath]
      return { ...prev, walkPaths: next }
    })
  }

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h2 className="page-title">Personaggi</h2>
          <p className="page-sub">Seleziona il GIF da mostrare sull'overlay</p>
        </div>
        <div style={{ display: 'flex', gap: 8 }}>
          <button className="btn btn-secondary" onClick={() => api.openCharactersFolder()}>
            📁 Apri cartella
          </button>
          <button className="btn btn-primary" onClick={browseGif}>
            + Aggiungi GIF
          </button>
        </div>
      </div>

      <div className="card" style={{ marginBottom: 16 }}>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Mostra personaggio</div>
            <div className="toggle-sub">Visualizza il GIF animato sull'overlay</div>
          </div>
          <label className="toggle">
            <input
              type="checkbox"
              checked={config.character.enabled}
              onChange={e => update(prev => ({ ...prev, character: { ...prev.character, enabled: e.target.checked } }))}
            />
            <div className="toggle-track" />
            <div className="toggle-thumb" />
          </label>
        </div>
      </div>

      {chars.length === 0 ? (
        <div className="empty-state">
          <div className="empty-icon">🎭</div>
          <div className="empty-title">Nessun personaggio trovato</div>
          <div className="empty-sub">
            Aggiungi GIF nella cartella <code>characters/</code> o usa il pulsante sopra
          </div>
        </div>
      ) : (
        <div className="char-grid">
          {chars.map(ch => {
            const active = normPath(config.character.path) === normPath(ch.path)
            const hasWalk = !!config.walkPaths?.[ch.path]
            const isMenuOpen = menuOpen === ch.path
            return (
              <div key={ch.path} style={{ position: 'relative' }}>
                <button
                  className={`char-card ${active ? 'char-card--active' : ''}`}
                  onClick={() => selectChar(ch.path)}
                >
                  <div className="char-preview">
                    <img src={ch.previewUrl} alt={ch.name} />
                  </div>
                  <div className="char-name">{ch.name}</div>
                  {active && <div className="char-badge">✓ Attivo</div>}
                  {hasWalk && (
                    <div className="char-walk-badge" title="Ha animazione camminata">🏃</div>
                  )}
                </button>
                {/* Three-dots menu button */}
                <button
                  className="char-menu-btn"
                  onClick={e => {
                    e.stopPropagation()
                    setMenuOpen(isMenuOpen ? null : ch.path)
                  }}
                  title="Opzioni"
                >
                  ⋯
                </button>
                {isMenuOpen && (
                  <div className="char-menu" ref={menuRef}>
                    {!active && (
                      <button className="char-menu-item" onClick={() => { selectChar(ch.path); setMenuOpen(null) }}>
                        ✓ Seleziona
                      </button>
                    )}
                    <button className="char-menu-item" onClick={() => setWalkGif(ch.path)}>
                      🏃 {hasWalk ? 'Cambia walk GIF' : 'Imposta walk GIF'}
                    </button>
                    {hasWalk && (
                      <button className="char-menu-item char-menu-item--danger" onClick={() => clearWalkGif(ch.path)}>
                        ✕ Rimuovi walk GIF
                      </button>
                    )}
                  </div>
                )}
              </div>
            )
          })}
        </div>
      )}

      {chars.length > 0 && (
        <div className="card" style={{ marginTop: 16 }}>
          <div className="section-label">Dimensioni</div>
          <div className="slider-row">
            <div className="slider-header">
              <span>Larghezza massima</span>
              <span className="slider-val">{config.character.maxWidth}px</span>
            </div>
            <input type="range" min={60} max={480} value={config.character.maxWidth}
              onChange={e => update(prev => ({ ...prev, character: { ...prev.character, maxWidth: +e.target.value } }))} />
          </div>
          <div className="slider-row">
            <div className="slider-header">
              <span>Altezza massima</span>
              <span className="slider-val">{config.character.maxHeight}px</span>
            </div>
            <input type="range" min={60} max={480} value={config.character.maxHeight}
              onChange={e => update(prev => ({ ...prev, character: { ...prev.character, maxHeight: +e.target.value } }))} />
          </div>
          <div className="slider-row">
            <div className="slider-header">
              <span>Margine bordo</span>
              <span className="slider-val">{config.character.margin}px</span>
            </div>
            <input type="range" min={0} max={80} value={config.character.margin}
              onChange={e => update(prev => ({ ...prev, character: { ...prev.character, margin: +e.target.value } }))} />
          </div>
        </div>
      )}
    </div>
  )
}
