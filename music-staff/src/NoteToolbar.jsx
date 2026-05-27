import { useState, useRef, useEffect } from 'react'
import KeyPicker from './KeyPicker'

const DURATION_SYMBOLS = { w: '𝅝', h: '𝅗𝅥', q: '♩', '8': '♪', '16': '\u{1D161}' }
const REST_SYMBOLS     = { w: '𝄻', h: '𝄼', q: '𝄽', '8': '𝄾', '16': '𝄿' }
const ACCIDENTALS = [
  { id: '#',  label: '♯', title: 'Дієз' },
  { id: 'b',  label: '♭', title: 'Бемоль' },
  { id: 'n',  label: '♮', title: 'Бекар' },
  { id: '##', label: '𝄪', title: 'Дубль-дієз' },
  { id: 'bb', label: '𝄫', title: 'Дубль-бемоль' },
]

function MultiCheckDropdown({ label, options, selected, onToggle }) {
  const [open, setOpen] = useState(false)
  const ref = useRef(null)

  useEffect(() => {
    if (!open) return
    const handler = (e) => { if (ref.current && !ref.current.contains(e.target)) setOpen(false) }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [open])

  return (
    <div className="mcd-picker" ref={ref}>
      <button
        className={`btn-mcd-trigger${open ? ' open' : ''}`}
        onClick={() => setOpen(o => !o)}
      >
        <span className="mcd-count">{selected.length}/{options.length} обрано</span>
        <span className="mcd-arrow">{open ? '▲' : '▼'}</span>
      </button>
      {open && (
        <div className="mcd-dropdown">
          <ul className="mcd-list">
            {options.map(opt => (
              <li key={opt.id} className="mcd-item" onClick={() => onToggle(opt.id)}>
                <input
                  type="checkbox"
                  checked={selected.includes(opt.id)}
                  onChange={() => onToggle(opt.id)}
                  onClick={e => e.stopPropagation()}
                />
                <span className="mcd-item-label">{opt.label}</span>
              </li>
            ))}
          </ul>
        </div>
      )}
    </div>
  )
}

function TimeSigPicker({ value, onChange, options }) {
  const [open, setOpen] = useState(false)
  const ref = useRef(null)

  useEffect(() => {
    if (!open) return
    const handler = (e) => { if (ref.current && !ref.current.contains(e.target)) setOpen(false) }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [open])

  return (
    <div className="timesig-picker" ref={ref}>
      <button
        className={`btn-timesig-trigger${open ? ' open' : ''}`}
        onClick={() => setOpen(o => !o)}
      >
        {value}
        <span className="timesig-trigger-arrow">{open ? '▲' : '▼'}</span>
      </button>
      {open && (
        <div className="timesig-dropdown">
          <ul className="timesig-list">
            {options.map(ts => (
              <li
                key={ts}
                className={`timesig-item${ts === value ? ' timesig-selected' : ''}`}
                onClick={() => { onChange(ts); setOpen(false) }}
              >
                {ts}
              </li>
            ))}
          </ul>
        </div>
      )}
    </div>
  )
}


export default function NoteToolbar({
  durations, timeSigs,
  selected, timeSignature, isRest,
  accidental, onSelectAccidental,
  isDotted, onToggleDot,
  isTie, onToggleTie,
  isTriplet, tripletCount, onToggleTriplet,
  anacrusis, anacruisTicks, normalCap, onChangeAnacrusis,
  tonality, onSelectTonality,
  onSelectTimeSig, onStartDrag, onUndo, onClear,
  onAddMeasure, onRemoveMeasure,
  canUndo, canRemoveMeasure,
  isHarmonize, clefMode, onSelectClef,
  isEditMode, onToggleEditMode,
  onDeleteSelected, canDeleteNote = false,
  onExport,
  onHarmonize, isHarmonizing,
  isCheck, onCheck, isChecking,
  forbiddenRules = [], selectedForbiddenRules = [], onToggleForbiddenRule,
  allowedChords  = [], selectedAllowedChords  = [], onToggleAllowedChord,
  measuresCount = 1, onSetMeasureCount,
}) {
  const rawSum = anacrusis.q * 4 + anacrusis.e * 2 + anacrusis.s * 1
  const anacruisInvalid = anacrusis.enabled && rawSum > 0 && anacruisTicks === 0

  const [measureInputVal, setMeasureInputVal] = useState(String(measuresCount))

  useEffect(() => {
    setMeasureInputVal(String(measuresCount))
  }, [measuresCount])

  const MAX_MEASURES = 64

  function handleMeasureInputChange(e) {
    const val = e.target.value
    setMeasureInputVal(val)
    if (onSetMeasureCount) {
      const parsed = parseInt(val) || 0
      const clamped = Math.min(MAX_MEASURES, Math.max(1, parsed))
      onSetMeasureCount(clamped)
    }
  }

  function handleMeasureInputBlur() {
    setMeasureInputVal(String(measuresCount))
  }

  return (
    <div className="toolbar">

      {/* Voice — harmonize only */}
      {isHarmonize && (
        <div className="toolbar-group">
          <span className="toolbar-label">Голос</span>
          <div className="clef-switcher">
            <button
              className={`btn-note${clefMode === 'treble' ? ' active' : ''}`}
              onClick={() => onSelectClef('treble')}
            >𝄞 Мелодія</button>
            <button
              className={`btn-note${clefMode === 'bass' ? ' active' : ''}`}
              onClick={() => onSelectClef('bass')}
            >𝄢 Бас</button>
          </div>
        </div>
      )}

      {/* Main 3-column area */}
      <div className="toolbar-main-cols">

        {/* Col 1: Tonality + Measure count (harmonize only) + Controls */}
        <div className="toolbar-group-stack">
          <div className="toolbar-group">
            <span className="toolbar-label">Тональність</span>
            <KeyPicker value={tonality} onChange={onSelectTonality} />
          </div>
          {(isHarmonize || isCheck) && (
            <div className="toolbar-group">
              <span className="toolbar-label">Кількість тактів</span>
              <div className="measure-count-row">
                <button
                  className="btn-measure-step"
                  onClick={() => onRemoveMeasure && onRemoveMeasure()}
                  disabled={!canRemoveMeasure}
                  title="Зменшити кількість тактів"
                >−</button>
                <input
                  type="text"
                  inputMode="numeric"
                  className="measure-count-input"
                  value={measureInputVal}
                  onChange={handleMeasureInputChange}
                  onBlur={handleMeasureInputBlur}
                />
                <button
                  className="btn-measure-step"
                  onClick={() => onAddMeasure && onAddMeasure()}
                  disabled={measuresCount >= MAX_MEASURES}
                  title="Збільшити кількість тактів"
                >+</button>
              </div>
            </div>
          )}
          <div className="toolbar-ctrl-buttons">
            <button
              className="btn-ctrl btn-ctrl-undo"
              onClick={onUndo}
              disabled={!canUndo}
              title="Скасувати останню дію"
            >↩</button>
            <button
              className={`btn-ctrl btn-ctrl-edit${isEditMode ? ' active' : ''}`}
              onClick={onToggleEditMode}
              title="Режим редагування"
            >✎</button>
            <button
              className="btn-ctrl btn-ctrl-delete"
              onClick={onDeleteSelected}
              disabled={!isEditMode || !canDeleteNote}
              title="Видалити вибрану ноту або паузу"
            >⌫</button>
            <button
              className="btn-ctrl btn-ctrl-clear"
              onClick={onClear}
              title="Очистити нотний стан"
            >🗑</button>
          </div>
        </div>

        {/* Col 2: Time signature + Anacrusis */}
        <div className="toolbar-group-stack">
          <div className="toolbar-group">
            <span className="toolbar-label">Розмір</span>
            <TimeSigPicker value={timeSignature} onChange={onSelectTimeSig} options={timeSigs} />
          </div>
          <label className="anacrusis-check">
            <input
              type="checkbox"
              checked={anacrusis.enabled}
              onChange={e => onChangeAnacrusis({ ...anacrusis, enabled: e.target.checked })}
            />
            <span className="toolbar-label">Затакт</span>
          </label>
          {anacrusis.enabled && (
            <div className="anacrusis-row">
              <label className="anacrusis-field">
                <span>♩</span>
                <input
                  type="number" min="0" max="99"
                  value={anacrusis.q}
                  onChange={e => onChangeAnacrusis({ ...anacrusis, q: Math.max(0, parseInt(e.target.value) || 0) })}
                />
              </label>
              <label className="anacrusis-field">
                <span>♪</span>
                <input
                  type="number" min="0" max="99"
                  value={anacrusis.e}
                  onChange={e => onChangeAnacrusis({ ...anacrusis, e: Math.max(0, parseInt(e.target.value) || 0) })}
                />
              </label>
              <label className="anacrusis-field">
                <span>{'\u{1D161}'}</span>
                <input
                  type="number" min="0" max="99"
                  value={anacrusis.s}
                  onChange={e => onChangeAnacrusis({ ...anacrusis, s: Math.max(0, parseInt(e.target.value) || 0) })}
                />
              </label>
              {anacruisInvalid && (
                <span className="anacrusis-warn">≥ розміру такту</span>
              )}
            </div>
          )}
        </div>

        {/* Col 3+: Note symbols (4×5 grid) + Restrictions */}
        <div className="toolbar-notes-and-rules">
          <div className="toolbar-group">
            <span className="toolbar-label">Нотні символи</span>
            <div className="note-symbols-grid">

              {/* Row 1: dot | tie | triplet | [progress or empty] | empty */}
              <button
                className={`btn-dur${isDotted ? ' active' : ''}`}
                title="Точка (×1.5)"
                disabled={selected.duration === '16' || isTriplet}
                onClick={onToggleDot}
              >•</button>
              <button
                className={`btn-dur${isTie ? ' active' : ''}`}
                title="Ліга"
                onClick={onToggleTie}
              >⌢</button>
              <button
                className={`btn-dur${isTriplet ? ' active' : ''}`}
                title="Тріоль — три ноти замість двох (3:2)"
                onClick={onToggleTriplet}
                style={{ fontSize: '0.9rem', fontWeight: 700 }}
              >³</button>
              {isTriplet && tripletCount > 0
                ? <div className="triplet-progress">{tripletCount}/3</div>
                : <div className="note-sym-empty" />
              }
              <div className="note-sym-empty" />

              {/* Row 2: accidentals */}
              {ACCIDENTALS.map(a => (
                <button
                  key={a.id}
                  className={`btn-dur${accidental === a.id ? ' active' : ''}`}
                  title={a.title}
                  onClick={() => onSelectAccidental(a.id)}
                >{a.label}</button>
              ))}

              {/* Row 3: rests */}
              {durations.map(d => (
                <button
                  key={`r-${d.id}`}
                  className={`btn-dur ${isRest && selected.duration === d.id ? 'active' : ''}`}
                  title={`${d.label} (пауза)`}
                  onClick={() => onStartDrag(d.id, true)}
                >{REST_SYMBOLS[d.id]}</button>
              ))}

              {/* Row 4: notes */}
              {durations.map(d => (
                <button
                  key={`n-${d.id}`}
                  className={`btn-dur ${!isRest && selected.duration === d.id ? 'active' : ''}`}
                  title={d.label}
                  onClick={() => onStartDrag(d.id, false)}
                >{DURATION_SYMBOLS[d.id]}</button>
              ))}

            </div>
          </div>

          <div className="toolbar-group-stack">
            <div className="toolbar-group">
              <span className="toolbar-label">Заборони</span>
              <MultiCheckDropdown
                label="Заборони"
                options={forbiddenRules}
                selected={selectedForbiddenRules}
                onToggle={onToggleForbiddenRule}
              />
            </div>

            <div className="toolbar-group">
              <span className="toolbar-label">Допустимі акорди</span>
              <MultiCheckDropdown
                label="Допустимі акорди"
                options={allowedChords}
                selected={selectedAllowedChords}
                onToggle={onToggleAllowedChord}
              />
            </div>
          </div>
        </div>

      </div>

      {/* Harmonize — rightmost */}
      {isHarmonize && (
        <div className="toolbar-group actions toolbar-harmonize-right">
          <button
            className="btn-action btn-harmonize"
            onClick={onHarmonize}
            disabled={isHarmonizing}
            title="Надіслати мелодію на сервер і отримати варіанти гармонізації"
          >
            {isHarmonizing ? 'Гармонізую...' : 'Гармонізувати!'}
          </button>
        </div>
      )}

      {/* Check — rightmost */}
      {isCheck && (
        <div className="toolbar-group actions toolbar-harmonize-right">
          <button
            className="btn-action btn-check"
            onClick={onCheck}
            disabled={isChecking}
            title="Перевірити гармонізацію"
          >
            {isChecking ? 'Перевіряю...' : 'Перевірити!'}
          </button>
        </div>
      )}

    </div>
  )
}
