import { useState, useRef, useEffect } from 'react'
import { TONALITIES } from './tonalities'

export default function KeyPicker({ value, onChange }) {
  const [open, setOpen] = useState(false)
  const containerRef = useRef(null)

  useEffect(() => {
    if (!open) return
    const handler = (e) => {
      if (!containerRef.current?.contains(e.target)) setOpen(false)
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [open])

  function select(t) {
    onChange(t)
    setOpen(false)
  }

  return (
    <div className="key-picker" ref={containerRef}>
      <button
        className={`btn-key-trigger${open ? ' open' : ''}`}
        onClick={() => setOpen(o => !o)}
        title="Вибрати тональність"
      >
        <span className="key-trigger-label">{value.label}</span>
        <span className="key-trigger-arrow">{open ? '▲' : '▼'}</span>
      </button>

      {open && (
        <div className="key-dropdown">
          <ul className="key-list">
            {TONALITIES.map(t => (
              <li
                key={t.key}
                className={[
                  'key-item',
                  t.major ? 'key-major' : 'key-minor',
                  value.key === t.key ? 'key-selected' : '',
                ].join(' ')}
                onClick={() => select(t)}
              >
                <span className="key-item-label">{t.label}</span>
              </li>
            ))}
          </ul>
        </div>
      )}
    </div>
  )
}
