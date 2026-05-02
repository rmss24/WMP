import { useEffect, useState } from 'react'
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

  const reload = () => api.getCharacters().then(setChars)

  useEffect(() => { reload() }, [])

  const selectChar = (path: string) =>
    update(prev => ({ ...prev, character: { ...prev.character, path } }))

  const browseGif = async () => {
    const path = await api.selectGif()
    if (path) {
      update(prev => ({ ...prev, character: { ...prev.character, path } }))
      reload()
    }
  }

  const normPath = (p: string) => p.replace(/\//g, '\\').toLowerCase()

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
            return (
              <button
                key={ch.path}
                className={`char-card ${active ? 'char-card--active' : ''}`}
                onClick={() => selectChar(ch.path)}
              >
                <div className="char-preview">
                  <img src={ch.previewUrl} alt={ch.name} />
                </div>
                <div className="char-name">{ch.name}</div>
                {active && <div className="char-badge">✓ Attivo</div>}
              </button>
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