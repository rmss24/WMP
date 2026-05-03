import type { Config } from '../App'
import './Page.css'
import './AnimationsPage.css'

type Char = { name: string; path: string; previewUrl: string }

type Props = {
  config: Config
  update: (p: Partial<Config> | ((prev: Config) => Config)) => void
  api: { getCharacters: () => Promise<Char[]>; selectGif: () => Promise<string | null> }
}

const ANCHORS: { ax: Config['character']['anchorX']; ay: Config['character']['anchorY']; label: string }[] = [
  { ax: 'left',   ay: 'top',    label: '↖' },
  { ax: 'center', ay: 'top',    label: '↑' },
  { ax: 'right',  ay: 'top',    label: '↗' },
  { ax: 'left',   ay: 'center', label: '←' },
  { ax: 'center', ay: 'center', label: '●' },
  { ax: 'right',  ay: 'center', label: '→' },
  { ax: 'left',   ay: 'bottom', label: '↙' },
  { ax: 'center', ay: 'bottom', label: '↓' },
  { ax: 'right',  ay: 'bottom', label: '↘' },
]

const DIR_OPTIONS: { value: Config['animation']['walkDirection']; label: string }[] = [
  { value: 'left',  label: '← Sinistra' },
  { value: 'both',  label: '↔ Entrambe' },
  { value: 'right', label: '→ Destra' },
]

export default function AnimationsPage({ config, update }: Props) {
  const setCharProp = <K extends keyof Config['character']>(k: K, v: Config['character'][K]) =>
    update(prev => ({ ...prev, character: { ...prev.character, [k]: v } }))

  const setAnimProp = <K extends keyof Config['animation']>(k: K, v: Config['animation'][K]) =>
    update(prev => ({ ...prev, animation: { ...prev.animation, [k]: v } }))

  const currentAx = config.character.anchorX ?? 'right'
  const currentAy = config.character.anchorY ?? 'bottom'

  return (
    <div className="page">
      <div className="page-header">
        <div>
          <h2 className="page-title">Animazioni</h2>
          <p className="page-sub">Posizione e movimento del personaggio sullo schermo</p>
        </div>
      </div>

      {/* ── Position ── */}
      <div className="card" style={{ marginBottom: 16 }}>
        <div className="section-label">Posizione di default</div>
        <p className="section-hint">Scegli il punto di ancoraggio dello schermo</p>

        <div className="anchor-grid">
          {ANCHORS.map(a => {
            const active = a.ax === currentAx && a.ay === currentAy
            return (
              <button
                key={`${a.ax}-${a.ay}`}
                className={`anchor-cell ${active ? 'anchor-cell--active' : ''}`}
                onClick={() => {
                  setCharProp('anchorX', a.ax)
                  setCharProp('anchorY', a.ay)
                }}
                title={`${a.ay} ${a.ax}`}
              >
                {a.label}
              </button>
            )
          })}
        </div>

        <div className="slider-row" style={{ marginTop: 16 }}>
          <div className="slider-header">
            <span>Offset X</span>
            <span className="slider-val">{config.character.offsetX ?? 0}px</span>
          </div>
          <input
            type="range" min={-300} max={300}
            value={config.character.offsetX ?? 0}
            onChange={e => setCharProp('offsetX', +e.target.value)}
          />
        </div>
        <div className="slider-row">
          <div className="slider-header">
            <span>Offset Y</span>
            <span className="slider-val">{config.character.offsetY ?? 0}px</span>
          </div>
          <input
            type="range" min={-300} max={300}
            value={config.character.offsetY ?? 0}
            onChange={e => setCharProp('offsetY', +e.target.value)}
          />
        </div>
      </div>

      {/* ── Walk animation ── */}
      <div className="card">
        <div className="toggle-row" style={{ marginBottom: 16 }}>
          <div>
            <div className="toggle-label">Camminata attiva</div>
            <div className="toggle-sub">Il personaggio cammina periodicamente sullo schermo</div>
          </div>
          <label className="toggle">
            <input
              type="checkbox"
              checked={config.animation.enabled}
              onChange={e => setAnimProp('enabled', e.target.checked)}
            />
            <div className="toggle-track" />
            <div className="toggle-thumb" />
          </label>
        </div>

        <div className={`anim-fields ${config.animation.enabled ? '' : 'anim-fields--disabled'}`}>
          <div className="section-hint" style={{ marginBottom: 12 }}>
            Usa i 3 puntini sui personaggi per assegnare un GIF di camminata dedicato
          </div>

          <div className="slider-row">
            <div className="slider-header">
              <span>Frequenza</span>
              <span className="slider-val">ogni {config.animation.walkFrequency}s</span>
            </div>
            <input
              type="range" min={5} max={300}
              value={config.animation.walkFrequency}
              disabled={!config.animation.enabled}
              onChange={e => setAnimProp('walkFrequency', +e.target.value)}
            />
          </div>

          <div className="slider-row">
            <div className="slider-header">
              <span>Velocità</span>
              <span className="slider-val">{config.animation.walkSpeed}px/s</span>
            </div>
            <input
              type="range" min={30} max={500}
              value={config.animation.walkSpeed}
              disabled={!config.animation.enabled}
              onChange={e => setAnimProp('walkSpeed', +e.target.value)}
            />
          </div>

          <div className="section-label" style={{ marginTop: 16, marginBottom: 8 }}>Direzione</div>
          <div className="dir-group">
            {DIR_OPTIONS.map(opt => (
              <button
                key={opt.value}
                className={`dir-btn ${config.animation.walkDirection === opt.value ? 'dir-btn--active' : ''}`}
                disabled={!config.animation.enabled}
                onClick={() => setAnimProp('walkDirection', opt.value)}
              >
                {opt.label}
              </button>
            ))}
          </div>
        </div>
      </div>
    </div>
  )
}
