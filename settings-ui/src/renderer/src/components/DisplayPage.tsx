import type { Config } from '../App'
import './Page.css'

type Props = {
  config: Config
  update: (p: Partial<Config> | ((prev: Config) => Config)) => void
}

export default function DisplayPage({ config, update }: Props) {
  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h2 className="page-title">Display</h2>
          <p className="page-sub">Personalizza mirino, bordo e HUD</p>
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">🎯 Mirino</div>
        <div className="toggle-row">
          <div className="toggle-label">Attivo</div>
          <label className="toggle">
            <input type="checkbox" checked={config.crosshair.enabled}
              onChange={e => update(prev => ({ ...prev, crosshair: { ...prev.crosshair, enabled: e.target.checked } }))} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
        <div className="color-row">
          <span>Colore</span>
          <div className="color-swatch">
            <input type="color" value={config.crosshair.color}
              onChange={e => update(prev => ({ ...prev, crosshair: { ...prev.crosshair, color: e.target.value } }))} />
          </div>
        </div>
        <div className="slider-row">
          <div className="slider-header"><span>Lunghezza</span><span className="slider-val">{config.crosshair.length}px</span></div>
          <input type="range" min={4} max={60} value={config.crosshair.length}
            onChange={e => update(prev => ({ ...prev, crosshair: { ...prev.crosshair, length: +e.target.value } }))} />
        </div>
        <div className="slider-row">
          <div className="slider-header"><span>Gap centro</span><span className="slider-val">{config.crosshair.gap}px</span></div>
          <input type="range" min={0} max={30} value={config.crosshair.gap}
            onChange={e => update(prev => ({ ...prev, crosshair: { ...prev.crosshair, gap: +e.target.value } }))} />
        </div>
        <div className="slider-row">
          <div className="slider-header"><span>Spessore</span><span className="slider-val">{config.crosshair.thickness}px</span></div>
          <input type="range" min={1} max={10} value={config.crosshair.thickness}
            onChange={e => update(prev => ({ ...prev, crosshair: { ...prev.crosshair, thickness: +e.target.value } }))} />
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">🔲 Bordo schermo</div>
        <div className="toggle-row">
          <div className="toggle-label">Attivo</div>
          <label className="toggle">
            <input type="checkbox" checked={config.border.enabled}
              onChange={e => update(prev => ({ ...prev, border: { ...prev.border, enabled: e.target.checked } }))} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
        <div className="color-row">
          <span>Colore</span>
          <div className="color-swatch">
            <input type="color" value={config.border.color}
              onChange={e => update(prev => ({ ...prev, border: { ...prev.border, color: e.target.value } }))} />
          </div>
        </div>
        <div className="slider-row">
          <div className="slider-header"><span>Spessore</span><span className="slider-val">{config.border.thickness}px</span></div>
          <input type="range" min={1} max={12} value={config.border.thickness}
            onChange={e => update(prev => ({ ...prev, border: { ...prev.border, thickness: +e.target.value } }))} />
        </div>
      </div>

      <div className="card section-card">
        <div className="section-label">📝 HUD testo</div>
        <div className="toggle-row">
          <div className="toggle-label">Mostra HUD</div>
          <label className="toggle">
            <input type="checkbox" checked={config.hud.enabled}
              onChange={e => update(prev => ({ ...prev, hud: { ...prev.hud, enabled: e.target.checked } }))} />
            <div className="toggle-track" /><div className="toggle-thumb" />
          </label>
        </div>
        <div className="slider-row">
          <div className="slider-header"><span>Dimensione font</span><span className="slider-val">{config.hud.fontSize}px</span></div>
          <input type="range" min={10} max={28} value={config.hud.fontSize}
            onChange={e => update(prev => ({ ...prev, hud: { ...prev.hud, fontSize: +e.target.value } }))} />
        </div>
      </div>
    </div>
  )
}
