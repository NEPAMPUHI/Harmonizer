const LINKS = [
  { id: 'home',     label: 'Головна' },
  { id: 'about',    label: 'Про проєкт' },
  { id: 'contacts', label: 'Контакти' },
]

export default function Nav({ current, onNavigate }) {
  return (
    <nav className="main-nav">
      {LINKS.map(l => (
        <a
          key={l.id}
          href="#"
          className={`nav-link${current === l.id ? ' nav-active' : ''}`}
          onClick={e => { e.preventDefault(); onNavigate(l.id) }}
        >
          {l.label}
        </a>
      ))}
    </nav>
  )
}
