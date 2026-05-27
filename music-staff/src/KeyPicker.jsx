import { useState, useRef, useEffect } from 'react'
import { TONALITIES, accLabel } from './tonalities'

export default function KeyPicker({ value, onChange }) {
  const [open, setOpen] = useState(false)
  const containerRef = useRef(null)

  // close on outside click
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
        <span className="key-trigger-name">{value.key}</span>
        <span className="key-trigger-label">{value.label}</span>
        <span className="key-trigger-acc">{accLabel(value.acc)}</span>
        <span className="key-trigger-arrow">{open ? '▲' : '▼'}</span>
      </button>

      {open && (
        <div className="key-dropdown">
          <div className="key-dropdown-header">
            <span>Назва</span>
            <span>Тональність</span>
            <span>Знаки</span>
          </div>
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
                <span className="key-item-name">{t.key}</span>
                <span className="key-item-label">{t.label}</span>
                <span className="key-item-acc">{accLabel(t.acc)}</span>
              </li>
            ))}
          </ul>
        </div>
      )}
    </div>
  )
}
