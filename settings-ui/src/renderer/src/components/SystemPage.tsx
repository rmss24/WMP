import { useEffect, useState } from 'react'
import type { Config } from '../App'
import './Page.css'

type Props = {
  config: Config
  update: (p: Partial<Config> | ((prev: Config) => Config)) => void
  api: {
    setAutostart: (v: boolean) => Promise<boolean>
    getAutostart: () => Promise<boolean>
    launchOverlay: () => Promise<boolean>
  }
}

export default function SystemPage({ config, update, api }: Props) {
  const [autostart, setAutostart] = useState(false)
  const [launching, setLaunching] = useState(false)

  useEffect(() => { api.getAutostart().then(setAutostart) }, [])

  const toggleAutostart = async (v: boolean) => {
    const ok = await api.setAutostart(v)
    if (ok) setAutostart(v)
  }

  const launch = async () => {
    setLaunching(true)
    await api.launchOverlay()
    setTimeout(() => setLaunching(false), 2000)
  }

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h2 className="page-title">Sistema</h2>
          <p className="page-sub">Avvio, debug e comportamento dell'overlay</p>
        </div>
        <button className="btn btn-primary" onClick={launch} disabled={launching}>
          {launching ? '▶ Avviato' : '▶ Avvia Overlay'}
        </button>
      </div>

      <div className="card section-card">
        <div className="section-label">🚀 Avvio</div>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Avvio automatico con Windows</div>
            <div className="toggle-sub">Aggiunge overlay.exe al registro di avvio</div>
          </div>
          <label className="toggle">
            <input type="checkbox" checked={autostart}
              onChange={e => toggleAutostart(e.target.checked)} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Apri Settings all'avvio overlay</div>
            <div className="toggle-sub">Mostra questa finestra quando si lancia overlay.exe</div>
          </div>
          <label className="toggle">
            <input type="checkbox" checked={config.launchSettingsOnStart}
              onChange={e => update({ launchSettingsOnStart: e.target.checked })} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">🖥️ Overlay</div>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Visibile all'avvio</div>
            <div className="toggle-sub">L'overlay è attivo quando si apre</div>
          </div>
          <label className="toggle">
            <input type="checkbox" checked={config.overlay.visible}
              onChange={e => update(prev => ({ ...prev, overlay: { ...prev.overlay, visible: e.target.checked } }))} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Click-through attivo all'avvio</div>
            <div className="toggle-sub">Il mouse passa attraverso l'overlay</div>
          </div>
          <label className="toggle">
            <input type="checkbox" checked={config.overlay.clickThrough}
              onChange={e => update(prev => ({ ...prev, overlay: { ...prev.overlay, clickThrough: e.target.checked } }))} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">🐛 Debug</div>
        <div className="toggle-row">
          <div>
            <div className="toggle-label">Modalità debug</div>
            <div className="toggle-sub">Mostra info aggiuntive sull'overlay (FPS, config)</div>
          </div>
          <label className="toggle">
            <input type="checkbox" checked={config.debugMode}
              onChange={e => update({ debugMode: e.target.checked })} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">⌨️ Scorciatoie</div>
        <div className="hotkey-list">
          {[
            ['Ctrl+Shift+H', 'Attiva/disattiva overlay'],
            ['Ctrl+Shift+T', 'Toggle click-through'],
            ['Ctrl+Shift+S', 'Apri impostazioni'],
            ['Ctrl+Shift+Q', 'Esci dall\'overlay'],
          ].map(([keys, desc]) => (
            <div key={keys} className="hotkey-row">
              <span className="hotkey-desc">{desc}</span>
              <span className="hotkey">
                {keys.split('+').map((k, i) => (
                  <span key={i}>{i > 0 && <span style={{ color: 'var(--text-3)', margin: '0 2px' }}>+</span>}<kbd>{k}</kbd></span>
                ))}
              </span>
            </div>
          ))}
        </div>
      </div>
    </div>
  )
}
