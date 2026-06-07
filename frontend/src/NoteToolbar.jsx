import { useState, useRef, useEffect } from 'react'
import KeyPicker from './KeyPicker'
import { getPlaybackBpm } from './usePlayback'

// Number spinner whose value can only be changed via the native up/down arrow buttons.
// Clicking the text area never shows a caret: focus is immediately blurred on entry.
// The spinner's change event fires during mousedown (before focus), so the value
// update is captured before the blur — arrows stay fully functional.
// The onChange prop receives a clamped integer in [0, maxValue].
function AnacrusisInput({ value, maxValue, onChange }) {
  const safeMax = Math.max(0, maxValue)
  return (
    <input
      type="number"
      className="anacrusis-input"
      value={value}
      min={0}
      max={safeMax}
      tabIndex={-1}
      onFocus={e => e.target.blur()}
      onChange={e => {
        const parsed = parseInt(e.target.value, 10)
        if (!isNaN(parsed)) onChange(Math.max(0, Math.min(safeMax, parsed)))
      }}
    />
  )
}

const SCALE_MODES = [
  { id: 'natural',  label: 'натуральний' },
  { id: 'harmonic', label: 'гармонічний' },
  { id: 'melodic',  label: 'мелодичний' },
]

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
  anacrusis, anacruisTicks, maxAnacruisTicks, onChangeAnacrusis,
  tonality, onSelectTonality,
  onSelectTimeSig, onStartDrag, onUndo, onClear,
  onAddMeasure, onRemoveMeasure,
  canUndo, canRemoveMeasure,
  isHarmonize, clefMode, onSelectClef,
  isEditMode, hasSelectedNote, onToggleEditMode,
  onExport,
  selectedModes = ['natural', 'harmonic', 'melodic'],
  onToggleMode,
  onHarmonize, isHarmonizing,
  isCheck, onCheck, isChecking,
  forbiddenRules = [], selectedForbiddenRules = [], onToggleForbiddenRule,
  allowedChords  = [], selectedAllowedChords  = [], onToggleAllowedChord,
  measuresCount = 1, canAddMeasure = true, onSetMeasureCount,
  playbackState = 'idle', onPlay, onPause, onStop,
  playbackSpeedMode = 'fast', onSetSpeedMode,
}) {

  const [measureInputVal, setMeasureInputVal] = useState(String(measuresCount))
  const isMeasureInputFocused = useRef(false)

  useEffect(() => {
    if (!isMeasureInputFocused.current) {
      setMeasureInputVal(String(measuresCount))
    }
  }, [measuresCount])

  const MAX_MEASURES = 64

  function handleMeasureInputChange(e) {
    const val = e.target.value
    if (val !== '' && !/^\d+$/.test(val)) return
    setMeasureInputVal(val)
  }

  function handleMeasureInputFocus(e) {
    isMeasureInputFocused.current = true
    e.target.select()
  }

  function handleMeasureInputBlur() {
    isMeasureInputFocused.current = false
    const parsed = parseInt(measureInputVal, 10)
    const clamped = !isNaN(parsed) && parsed >= 1 ? Math.min(MAX_MEASURES, parsed) : 1
    if (onSetMeasureCount) onSetMeasureCount(clamped)
    setMeasureInputVal(String(clamped))
  }

  const measureCountRow = (
    <div className={`measure-count-row${isHarmonize ? ' measure-count-row--harmonize' : ''}${isCheck ? ' measure-count-row--check' : ''}`}>
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
        onFocus={handleMeasureInputFocus}
        onBlur={handleMeasureInputBlur}
        onKeyDown={e => { if (e.key === 'Enter') e.target.blur() }}
      />
      <button
        className="btn-measure-step"
        onClick={() => onAddMeasure && onAddMeasure()}
        disabled={!canAddMeasure}
        title="Збільшити кількість тактів"
      >+</button>
    </div>
  )

  const playbackButtons = (
    <div className="toolbar-ctrl-buttons">
      <button
        className={`btn-ctrl btn-ctrl-speed${playbackSpeedMode === 'slow' ? ' active' : ''}`}
        onClick={() => onSetSpeedMode && onSetSpeedMode('slow')}
        title={`Повільний темп (${getPlaybackBpm(timeSignature, 'slow')} BPM)`}
      >slow</button>
      <button
        className={`btn-ctrl btn-ctrl-speed${playbackSpeedMode === 'fast' ? ' active' : ''}`}
        onClick={() => onSetSpeedMode && onSetSpeedMode('fast')}
        title={`Швидкий темп (${getPlaybackBpm(timeSignature, 'fast')} BPM)`}
      >fast</button>
      <button
        className={`btn-ctrl btn-ctrl-play${playbackState === 'playing' ? ' playing' : ''}`}
        onClick={onPlay}
        disabled={playbackState === 'playing'}
        title={playbackState === 'paused' ? 'Продовжити' : 'Відтворити'}
      >▶</button>
      <button
        className="btn-ctrl btn-ctrl-pause"
        onClick={onPause}
        disabled={playbackState !== 'playing'}
        title="Пауза"
      >⏸</button>
      <button
        className="btn-ctrl btn-ctrl-stop"
        onClick={onStop}
        disabled={playbackState === 'idle'}
        title="Зупинити"
      >⏹</button>
    </div>
  )

  const ctrlButtons = (
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
      >✎ Редагувати</button>
      <button
        className="btn-ctrl btn-ctrl-clear"
        onClick={onClear}
        title={isEditMode && hasSelectedNote ? 'Видалити ноту' : 'Очистити нотний стан'}
      >🗑</button>
    </div>
  )

  return (
    <div className="toolbar">

      {/* ─── Col 1 (harmonize): Voice + MeasureCount ─────────────── */}
      {isHarmonize && (
        <div className="toolbar-col">
          <div className="toolbar-group">
            <div className={`clef-switcher${isHarmonize ? ' clef-switcher--harmonize' : ''}`}>
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
          <div className="toolbar-group">
            <span className="toolbar-label">Кількість тактів</span>
            {measureCountRow}
          </div>
        </div>
      )}

      {/* ─── Col 2/1: Tonality + [Mode (harmonize) | MeasureCount (check)] ── */}
      <div className="toolbar-col toolbar-col--key">
        <div className="toolbar-group">
          <span className="toolbar-label">Тональність</span>
          <KeyPicker value={tonality} onChange={onSelectTonality} />
        </div>
        {isHarmonize && (
          <div className="toolbar-group">
            <span className="toolbar-label">Лад</span>
            <MultiCheckDropdown
              label="Лад"
              options={SCALE_MODES}
              selected={selectedModes}
              onToggle={onToggleMode}
            />
          </div>
        )}
        {isCheck && (
          <div className="toolbar-group">
            <span className="toolbar-label">Кількість тактів</span>
            {measureCountRow}
          </div>
        )}
      </div>

      {/* ─── Col 3/2: TimeSig + Anacrusis ──────────────────────────── */}
      <div className="toolbar-col">
        <div className="toolbar-group">
          <span className="toolbar-label">Розмір</span>
          <TimeSigPicker value={timeSignature} onChange={onSelectTimeSig} options={timeSigs} />
        </div>
        <div className="anacrusis-group">
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
                <span>♪</span>
                <AnacrusisInput
                  value={anacrusis.e}
                  maxValue={Math.floor((maxAnacruisTicks - anacrusis.s) / 2)}
                  onChange={newE => onChangeAnacrusis({ ...anacrusis, e: newE })}
                />
              </label>
              <label className="anacrusis-field">
                <span>{'\u{1D161}'}</span>
                <AnacrusisInput
                  value={anacrusis.s}
                  maxValue={maxAnacruisTicks - anacrusis.e * 2}
                  onChange={newS => onChangeAnacrusis({ ...anacrusis, s: newS })}
                />
              </label>
            </div>
          )}
        </div>
      </div>

      {/* ─── Col 4/3: Note symbols ─────────────────────────────────── */}
      <div className="toolbar-col">
        <div className="toolbar-group">
          <span className="toolbar-label">Нотні символи</span>
          <div className="note-symbols-grid">

            {/* Row 1: accidentals + triplet */}
            {ACCIDENTALS.map(a => (
              <button
                key={a.id}
                className={`btn-dur${accidental === a.id ? ' active' : ''}`}
                title={a.title}
                onClick={() => onSelectAccidental(a.id)}
              >{a.label}</button>
            ))}
            {(() => {
              const tripletDisabled = parseInt(timeSignature?.split('/')[1] ?? '4', 10) === 8
              return (
                <button
                  className={`btn-dur${isTriplet ? ' active' : ''}`}
                  title={tripletDisabled ? 'Тріоль недоступна для розмірів x/8' : 'Тріоль — три ноти замість двох (3:2)'}
                  onClick={tripletDisabled ? undefined : onToggleTriplet}
                  disabled={tripletDisabled}
                  style={{ fontSize: isTriplet && tripletCount > 0 ? '0.7rem' : '0.9rem', fontWeight: 700 }}
                >{isTriplet && tripletCount > 0 ? `${tripletCount}/3` : '³'}</button>
              )
            })()}

            {/* Row 2: rests + tie */}
            {durations.map(d => (
              <button
                key={`r-${d.id}`}
                className={`btn-dur ${isRest && selected.duration === d.id ? 'active' : ''}`}
                title={`${d.label} (пауза)`}
                onClick={() => onStartDrag(d.id, true)}
              >{REST_SYMBOLS[d.id]}</button>
            ))}
            <button
              className={`btn-dur${isTie ? ' active' : ''}`}
              title="Ліга"
              onClick={onToggleTie}
            >⌢</button>

            {/* Row 3: notes + dot */}
            {durations.map(d => (
              <button
                key={`n-${d.id}`}
                className={`btn-dur ${!isRest && selected.duration === d.id ? 'active' : ''}`}
                title={d.label}
                onClick={() => onStartDrag(d.id, false)}
              >{DURATION_SYMBOLS[d.id]}</button>
            ))}
            <button
              className={`btn-dur${isDotted ? ' active' : ''}`}
              title="Точка (×1.5)"
              disabled={selected.duration === '16' || isTriplet}
              onClick={onToggleDot}
            >•</button>

          </div>
        </div>
      </div>

      {/* ─── Col 5/4 (harmonize only): Restrictions + Allowed chords ── */}
      {isHarmonize && (
        <div className="toolbar-col">
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
      )}

      {/* ─── Col 6 (harmonize): Harmonize button + Playback + Ctrl buttons ── */}
      {isHarmonize && (
        <div className="toolbar-col toolbar-col--actions">
          <button
            className="btn-action btn-harmonize"
            onClick={onHarmonize}
            disabled={isHarmonizing}
            title="Надіслати мелодію на сервер і отримати варіанти гармонізації"
          >
            {isHarmonizing ? 'Гармонізую...' : 'Гармонізувати!'}
          </button>
          {playbackButtons}
          {ctrlButtons}
        </div>
      )}

      {/* ─── Col 5 (check): Check button + Playback + Ctrl buttons ──── */}
      {isCheck && (
        <div className="toolbar-col toolbar-col--actions">
          <button
            className="btn-action btn-check"
            onClick={onCheck}
            disabled={isChecking}
            title="Перевірити гармонізацію"
          >
            {isChecking ? 'Перевіряю...' : 'Перевірити!'}
          </button>
          {playbackButtons}
          {ctrlButtons}
        </div>
      )}

    </div>
  )
}
