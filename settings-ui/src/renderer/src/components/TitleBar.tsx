import './TitleBar.css'

type Props = {
  saved: boolean
  onMinimize: () => void
  onClose: () => void
}

export default function TitleBar({ saved, onMinimize, onClose }: Props) {
  return (
    <div className="titlebar">
      <div className="titlebar-drag">
        <span className="titlebar-icon">⬡</span>
        <span className="titlebar-title">Overlay Settings</span>
        {saved && <span className="titlebar-saved">Salvato ✓</span>}
      </div>
      <div className="titlebar-controls">
        <button className="tb-btn" onClick={onMinimize} title="Minimizza">─</button>
        <button className="tb-btn tb-close" onClick={onClose} title="Chiudi">✕</button>
      </div>
    </div>
  )
}