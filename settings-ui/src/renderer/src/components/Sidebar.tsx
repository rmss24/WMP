import './Sidebar.css'

type Page = 'characters' | 'display' | 'animation' | 'system'

const NAV = [
  { id: 'characters' as Page, icon: '🎭', label: 'Personaggi' },
  { id: 'display'    as Page, icon: '🎯', label: 'Display'     },
  { id: 'animation'  as Page, icon: '🏃', label: 'Animazioni'  },
  { id: 'system'     as Page, icon: '⚙️', label: 'Sistema'     },
]

type Props = { page: Page; onNavigate: (p: Page) => void }

export default function Sidebar({ page, onNavigate }: Props) {
  return (
    <nav className="sidebar">
      <div className="sidebar-top">
        <div className="sidebar-brand">WMP</div>
        <div className="sidebar-version">v1.0</div>
      </div>
      <ul className="sidebar-nav">
        {NAV.map(item => (
          <li key={item.id}>
            <button
              className={`nav-item ${page === item.id ? 'nav-item--active' : ''}`}
              onClick={() => onNavigate(item.id)}
            >
              <span className="nav-icon">{item.icon}</span>
              <span className="nav-label">{item.label}</span>
            </button>
          </li>
        ))}
      </ul>
    </nav>
  )
}