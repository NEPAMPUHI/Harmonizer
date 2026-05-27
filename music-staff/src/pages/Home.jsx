import { useState, useEffect, useCallback, useRef } from 'react'
import Staff from '../Staff'
import NoteToolbar from '../NoteToolbar'
import { canAdd, canAddTriplet, measureCapacity, noteTicks, NOTE_TICKS } from '../capacity'
import { DEFAULT_TONALITY } from '../tonalities'
import { getKeyAccidentals, getEffectiveSemitones } from '../pitchUtils'
import { downloadScoreJson, scoreToJson } from '../scoreToJson'
import { harmonizeScore } from '../api'

const DURATIONS = [
  { id: 'w', label: 'Ціла' },
  { id: 'h', label: 'Половинна' },
  { id: 'q', label: 'Чвертна' },
  { id: '8', label: 'Восьма' },
  { id: '16', label: 'Шістнадцята' },
]
const TIME_SIGNATURES  = ['2/4', '3/4', '4/4', '3/8', '6/8', '9/8', '12/8']
const DIATONIC         = ['c', 'd', 'e', 'f', 'g', 'a', 'b']

const FORBIDDEN_RULES = [
  { id: 'parallel_fifths',      label: 'паралельні квінти' },
  { id: 'parallel_octaves',     label: 'паралельні октави і прими' },
  { id: 'parallel_seconds',     label: 'паралельні секунди і септіми' },
  { id: 'all_voices_same_dir',  label: 'всі голоси в одному напрямку' },
  { id: 's_after_d',            label: 'S після D' },
  { id: 'hidden_octaves',       label: 'приховані октави і квінти' },
  { id: 'voice_crossing',       label: 'перехрещування голосів' },
  { id: 'chromatic_transfer',   label: 'передача хроматичного півтона в інший голос' },
  { id: 'bass_leap_sequence',   label: '2 послідовні ходи по квартам/квінтам в басу' },
  { id: 'large_interval_sa_at', label: 'більше октави між S і A, A і T' },
  { id: 'double_third',         label: 'подвоєння терцового тону' },
]

const ALLOWED_CHORDS = [
  'T53', 'S53', 'D53', 'К64', 'T6', 'S6', 'D6', 'T64', 'S64', 'D64',
  'D7', 'D65', 'D43', 'D2',
  'II53', 'II6', 'VI53', 'II7', 'II65', 'II43', 'II2',
  'VII7', 'VII65', 'VII43', 'VII2',
  'D9', 'II9', 'VII6', 'III53', 'D+6',
].map(id => ({ id, label: id }))

const MAX_MEASURES   = 64
const MAX_UNDO       = 50
const EMPTY_MEASURE  = () => ({ treble: [], bass: [] })
const PICKUP_MEASURE = () => ({ treble: [], bass: [], isPickup: true })

function findNextAfterDelete(ms, deletedId) {
  let foundClef = null
  for (const m of ms) {
    for (const clef of ['treble', 'bass']) {
      if (m[clef].some(n => n.id === deletedId)) { foundClef = clef; break }
    }
    if (foundClef) break
  }
  if (!foundClef) return null

  const flat = ms.flatMap(m => m[foundClef].filter(n => !n.isTripletPlaceholder))
  const curIdx = flat.findIndex(n => n.id === deletedId)
  if (curIdx === -1) return null

  const target = flat[curIdx]
  const deletedIds = new Set()
  if (target.triplet && target.tripletGroup != null) {
    ms.forEach(m => m[foundClef].forEach(n => {
      if (n.tripletGroup === target.tripletGroup) deletedIds.add(n.id)
    }))
  } else {
    deletedIds.add(deletedId)
  }

  const remaining = flat.filter(n => !deletedIds.has(n.id))
  const newIdx = Math.min(curIdx, remaining.length - 1)
  return newIdx >= 0 ? remaining[newIdx].id : null
}

export default function Home() {
  const [measures,       setMeasures]       = useState([EMPTY_MEASURE()])
  const [selectedNote,   setSelectedNote]   = useState({ pitch: 'b', octave: 4, duration: 'q' })
  const [selectedStaff,  setSelectedStaff]  = useState('treble')
  const [timeSignature,  setTimeSignature]  = useState('4/4')
  const [tonality,       setTonality]       = useState(DEFAULT_TONALITY)
  const [drag,           setDrag]           = useState({ duration: 'q', isRest: false, dotted: false, triplet: false })
  const [isRest,         setIsRest]         = useState(false)
  const [accidental,     setAccidental]     = useState(null)
  const [isDotted,       setIsDotted]       = useState(false)
  const [isTie,          setIsTie]          = useState(false)
  const [anacrusis,      setAnacrusis]      = useState({ enabled: false, q: 0, e: 0, s: 0 })
  const [selectedNoteId, setSelectedNoteId] = useState(null)
  const [isEditMode,     setIsEditMode]     = useState(false)
  const [mode,           setMode]           = useState('harmonize')
  const [clefMode,       setClefMode]       = useState('treble')

  // Always-current ref used by the key handler and undo to read state without stale closures
  const measuresRef = useRef(measures)
  useEffect(() => { measuresRef.current = measures }, [measures])

  // ── Undo stack ────────────────────────────────────────────────────
  const [undoStack, setUndoStack] = useState([])

  function pushUndo() {
    setUndoStack(prev => [...prev.slice(-(MAX_UNDO - 1)), measuresRef.current])
  }

  // ── Triplet mode ─────────────────────────────────────────────────
  const [isTriplet,      setIsTriplet]      = useState(false)
  const [pendingTriplet, setPendingTriplet] = useState(null)
  const nextGroupIdRef  = useRef(1)
  const tripletRefRef   = useRef('8')

  const tripletCount = 3 - (pendingTriplet?.remainingUnits ?? 0)

  // ── Harmonization results ────────────────────────────────────────
  const [harmonizeVariants,   setHarmonizeVariants]   = useState([])
  const [isHarmonizing,       setIsHarmonizing]       = useState(false)
  const [harmonizeError,      setHarmonizeError]      = useState(null)
  const [selectedVariantIdx,  setSelectedVariantIdx]  = useState(0)

  // ── Check mode ───────────────────────────────────────────────────
  const [isChecking, setIsChecking] = useState(false)

  const [selectedForbiddenRules, setSelectedForbiddenRules] = useState(
    () => FORBIDDEN_RULES.map(r => r.id)
  )
  const [selectedAllowedChords, setSelectedAllowedChords] = useState(
    () => ALLOWED_CHORDS.map(c => c.id)
  )

  // ── Anacrusis computed values ────────────────────────────────────
  const normalCap        = measureCapacity(timeSignature)
  const rawAnacruisTicks = anacrusis.q * 4 + anacrusis.e * 2 + anacrusis.s * 1
  const anacruisTicks    = anacrusis.enabled && rawAnacruisTicks > 0 && rawAnacruisTicks < normalCap
    ? rawAnacruisTicks : 0
  const hasAnacrusis = anacruisTicks > 0

  function getMeasureCap(measureIdx, totalMeasures) {
    if (!hasAnacrusis) return normalCap
    if (measureIdx === 0) return anacruisTicks
    if (measureIdx === totalMeasures - 1 && totalMeasures > 1) return normalCap - anacruisTicks
    return normalCap
  }

  // ── Anacrusis structure sync ─────────────────────────────────────
  useEffect(() => {
    setMeasures(prev => {
      const firstIsPickup = prev[0]?.isPickup === true
      if (hasAnacrusis && !firstIsPickup) return [PICKUP_MEASURE(), ...prev]
      if (!hasAnacrusis && firstIsPickup) {
        const rest = prev.slice(1)
        return rest.length > 0 ? rest : [EMPTY_MEASURE()]
      }
      if (hasAnacrusis && firstIsPickup) {
        const trim = (notes) => {
          let total = 0
          return notes.filter(n => {
            const t = noteTicks(n)
            if (total + t <= anacruisTicks) { total += t; return true }
            return false
          })
        }
        return prev.map((m, i) =>
          i === 0 ? { ...m, treble: trim(m.treble), bass: trim(m.bass) } : m
        )
      }
      return prev
    })
  }, [hasAnacrusis, anacruisTicks])   // eslint-disable-line react-hooks/exhaustive-deps

  // ── Clear selection if selected note no longer exists ────────────
  useEffect(() => {
    if (!selectedNoteId) return
    let found = false
    for (const m of measures) {
      for (const clef of ['treble', 'bass']) {
        if (m[clef].some(n => n.id === selectedNoteId)) { found = true; break }
      }
      if (found) break
    }
    if (!found) setSelectedNoteId(null)
  }, [measures, selectedNoteId])

  useEffect(() => {
    setDrag(prev => prev ? { ...prev, dotted: isDotted } : prev)
  }, [isDotted])   // eslint-disable-line react-hooks/exhaustive-deps

  useEffect(() => {
    setDrag(prev => prev ? { ...prev, triplet: isTriplet } : prev)
  }, [isTriplet])   // eslint-disable-line react-hooks/exhaustive-deps

  // ── Arrow key handling for selected note ─────────────────────────
  useEffect(() => {
    if (!selectedNoteId) return
    function handleKeyDown(e) {
      if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return

      if (e.key === 'Delete' && isEditMode) {
        e.preventDefault()
        const ms = measuresRef.current
        // Verify note still exists before pushing undo
        let noteExists = false
        for (const m of ms) {
          for (const clef of ['treble', 'bass']) {
            if (m[clef].some(n => n.id === selectedNoteId)) { noteExists = true; break }
          }
          if (noteExists) break
        }
        if (!noteExists) return
        pushUndo()
        const nextId = findNextAfterDelete(ms, selectedNoteId)
        setMeasures(prev => {
          for (let mIdx = 0; mIdx < prev.length; mIdx++) {
            for (const clef of ['treble', 'bass']) {
              const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
              if (noteIdx === -1) continue
              const note = prev[mIdx][clef][noteIdx]
              if (note.triplet && note.tripletGroup != null) {
                const gid = note.tripletGroup
                return prev.map((m, mi) =>
                  mi !== mIdx ? m : { ...m, [clef]: m[clef].filter(n => n.tripletGroup !== gid) }
                )
              }
              return prev.map((m, mi) =>
                mi !== mIdx ? m : { ...m, [clef]: m[clef].filter((_, ni) => ni !== noteIdx) }
              )
            }
          }
          return prev
        })
        setSelectedNoteId(nextId)
        return
      }

      if (e.key === 'ArrowLeft' || e.key === 'ArrowRight') {
        e.preventDefault()
        const ms = measuresRef.current
        let foundClef = null
        outer:
        for (const m of ms) {
          for (const clef of ['treble', 'bass']) {
            if (m[clef].some(n => n.id === selectedNoteId && !n.isTripletPlaceholder)) {
              foundClef = clef; break outer
            }
          }
        }
        if (!foundClef) return
        const flat = ms.flatMap(m => m[foundClef].filter(n => !n.isTripletPlaceholder).map(n => n.id))
        const curIdx = flat.indexOf(selectedNoteId)
        if (curIdx === -1) return
        const newIdx = curIdx + (e.key === 'ArrowRight' ? 1 : -1)
        if (newIdx < 0 || newIdx >= flat.length) return
        setSelectedNoteId(flat[newIdx])
        return
      }

      if (e.key !== 'ArrowUp' && e.key !== 'ArrowDown') return
      e.preventDefault()
      const delta = e.key === 'ArrowUp' ? 1 : -1
      // Check note exists and is not a rest before pushing
      const ms = measuresRef.current
      let isMovable = false
      for (const m of ms) {
        for (const clef of ['treble', 'bass']) {
          const note = m[clef].find(n => n.id === selectedNoteId)
          if (note && !note.isRest) { isMovable = true; break }
        }
        if (isMovable) break
      }
      if (!isMovable) return
      pushUndo()
      setMeasures(prev => {
        for (let mIdx = 0; mIdx < prev.length; mIdx++) {
          for (const clef of ['treble', 'bass']) {
            const notes = prev[mIdx][clef]
            const noteIdx = notes.findIndex(n => n.id === selectedNoteId)
            if (noteIdx === -1) continue
            const note = notes[noteIdx]
            if (note.isRest) return prev
            const pitchIdx  = DIATONIC.indexOf(note.pitch)
            const totalIdx  = pitchIdx + note.octave * 7 + delta
            const newOctave = Math.floor(totalIdx / 7)
            if (newOctave < 1 || newOctave > 8) return prev
            const newPitchIdx = ((totalIdx % 7) + 7) % 7
            const newNote = { ...note, pitch: DIATONIC[newPitchIdx], octave: newOctave }
            return prev.map((m, mi) =>
              mi !== mIdx ? m : { ...m, [clef]: notes.map((n, ni) => ni === noteIdx ? newNote : n) }
            )
          }
        }
        return prev
      })
    }
    document.addEventListener('keydown', handleKeyDown)
    return () => document.removeEventListener('keydown', handleKeyDown)
  }, [selectedNoteId, isEditMode])   // eslint-disable-line react-hooks/exhaustive-deps

  // ── Sync toolbar to selected note when navigating in edit mode ────
  useEffect(() => {
    if (!isEditMode || !selectedNoteId) return
    const ms = measuresRef.current
    for (const m of ms) {
      for (const clef of ['treble', 'bass']) {
        const note = m[clef].find(n => n.id === selectedNoteId && !n.isTripletPlaceholder)
        if (note) {
          setSelectedNote(prev => ({ ...prev, duration: note.duration }))
          setAccidental(note.accidental ?? null)
          setIsDotted(note.dotted ?? false)
          setIsTie(note.tieAfter ?? false)
          setIsRest(note.isRest ?? false)
          setIsTriplet(note.triplet ?? false)
          return
        }
      }
    }
  }, [isEditMode, selectedNoteId])  // eslint-disable-line react-hooks/exhaustive-deps

  // ── Set absolute pitch of a note (called on every mousemove during drag) ─
  // Undo is pushed by Staff via onNoteDragStart, not here.
  function setNotePitch(id, newPitch, newOctave) {
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const notes = prev[mIdx][clef]
          const noteIdx = notes.findIndex(n => n.id === id)
          if (noteIdx === -1) continue
          const note = notes[noteIdx]
          if (note.isRest) return prev
          const newNote = { ...note, pitch: newPitch, octave: newOctave }
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: notes.map((n, ni) => ni === noteIdx ? newNote : n) }
          )
        }
      }
      return prev
    })
  }

  // ── Triplet helpers ──────────────────────────────────────────────
  function makePlaceholders(groupId, refN, count, baseId) {
    return Array.from({ length: count }, (_, i) => ({
      pitch: 'b', octave: 4, duration: refN,
      isRest: true, triplet: true, tripletGroup: groupId, tripletRef: refN,
      isTripletPlaceholder: true, id: baseId + i,
    }))
  }

  // Cancels in-progress triplet group and also pops its undo entry so one
  // Undo press is enough to reach the state before the triplet was started.
  function cancelTripletBuffer() {
    const pt = pendingTriplet
    if (pt) {
      setMeasures(prev => prev.map((m, mi) =>
        mi !== pt.measureIdx ? m
        : { ...m, [pt.clef]: m[pt.clef].filter(n => n.tripletGroup !== pt.groupId) }
      ))
      setUndoStack(prev => prev.length > 0 ? prev.slice(0, -1) : prev)
    }
    setPendingTriplet(null)
  }

  // ── Undo ─────────────────────────────────────────────────────────
  function undo() {
    if (pendingTriplet) {
      cancelTripletBuffer()   // also pops the stack entry for the triplet start
      return
    }
    if (undoStack.length === 0) return
    const snapshot = undoStack[undoStack.length - 1]
    setUndoStack(prev => prev.slice(0, -1))
    setMeasures(snapshot)
    // selectedNoteId is validated by the existing useEffect
  }

  // ── Selected-note editing ────────────────────────────────────────
  function deleteSelectedNote() {
    if (!selectedNoteId) return
    pushUndo()
    const nextId = findNextAfterDelete(measures, selectedNoteId)
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.triplet && note.tripletGroup != null) {
            const gid = note.tripletGroup
            return prev.map((m, mi) =>
              mi !== mIdx ? m : { ...m, [clef]: m[clef].filter(n => n.tripletGroup !== gid) }
            )
          }
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].filter((_, ni) => ni !== noteIdx) }
          )
        }
      }
      return prev
    })
    setSelectedNoteId(nextId)
  }

  function editSelectedNoteDuration(newDuration) {
    pushUndo()
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.triplet || note.isTripletPlaceholder) return prev
          const cap = getMeasureCap(mIdx, prev.length)
          const newDotted  = newDuration === '16' ? false : (note.dotted ?? false)
          const baseTicks  = NOTE_TICKS[newDuration] ?? 4
          const newTicks   = newDotted ? baseTicks * 1.5 : baseTicks
          const otherTicks = prev[mIdx][clef].reduce((s, n, ni) =>
            ni !== noteIdx ? s + noteTicks(n) : s, 0)
          if (otherTicks + newTicks > cap) return prev
          const newNote = { ...note, duration: newDuration, dotted: newDotted || undefined }
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni === noteIdx ? newNote : n) }
          )
        }
      }
      return prev
    })
    setSelectedNote(prev => ({ ...prev, duration: newDuration }))
    if (newDuration === '16') setIsDotted(false)
  }

  function editSelectedNoteAccidental(accId) {
    pushUndo()
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.isRest || note.isTripletPlaceholder) return prev
          const newAcc = note.accidental === accId ? undefined : accId
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) =>
              ni !== noteIdx ? n : { ...note, accidental: newAcc }
            )}
          )
        }
      }
      return prev
    })
    setAccidental(prev => prev === accId ? null : accId)
  }

  function editSelectedNoteDot() {
    pushUndo()
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.isRest || note.triplet || note.duration === '16' || note.isTripletPlaceholder) return prev
          const willBeDotted = !(note.dotted ?? false)
          if (willBeDotted) {
            const cap        = getMeasureCap(mIdx, prev.length)
            const baseTicks  = NOTE_TICKS[note.duration] ?? 4
            const otherTicks = prev[mIdx][clef].reduce((s, n, ni) =>
              ni !== noteIdx ? s + noteTicks(n) : s, 0)
            if (otherTicks + baseTicks * 1.5 > cap) return prev
          }
          const newNote = { ...note, dotted: willBeDotted || undefined }
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni !== noteIdx ? n : newNote) }
          )
        }
      }
      return prev
    })
    setIsDotted(prev => !prev)
  }

  function editSelectedNoteTie() {
    const keyAcc = getKeyAccidentals(tonality?.acc ?? 0)
    for (let mIdx = 0; mIdx < measures.length; mIdx++) {
      for (const clef of ['treble', 'bass']) {
        const noteIdx = measures[mIdx][clef].findIndex(n => n.id === selectedNoteId)
        if (noteIdx === -1) continue
        const note = measures[mIdx][clef][noteIdx]
        if (note.isRest || note.isTripletPlaceholder) return
        const newTieAfter = !(note.tieAfter ?? false)
        if (newTieAfter) {
          const sameM    = measures[mIdx][clef]
          const nextM    = measures[mIdx + 1]?.[clef] ?? []
          const inSame   = noteIdx + 1 < sameM.length
          const nextNote = inSame ? sameM[noteIdx + 1] : nextM[0]
          const nextPrev = inSame ? sameM.slice(0, noteIdx + 1) : []
          if (
            !nextNote || nextNote.isRest ||
            getEffectiveSemitones(note, sameM.slice(0, noteIdx), keyAcc) !==
            getEffectiveSemitones(nextNote, nextPrev, keyAcc)
          ) return
        }
        // All guards passed — will mutate
        pushUndo()
        const newNote = { ...note, tieAfter: newTieAfter || undefined }
        setMeasures(prev =>
          prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni !== noteIdx ? n : newNote) }
          )
        )
        setIsTie(prev => !prev)
        return
      }
    }
  }

  function editSelectedNoteTriplet() {
    pushUndo()
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(
            n => n.id === selectedNoteId && !n.isTripletPlaceholder
          )
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (!note.triplet || note.tripletGroup == null) return prev
          const gid = note.tripletGroup
          return prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].filter(n => n.tripletGroup !== gid) }
          )
        }
      }
      return prev
    })
    setSelectedNoteId(null)
    setIsTriplet(false)
  }

  // ── Edit mode ────────────────────────────────────────────────────
  function handleToggleEditMode() {
    const entering = !isEditMode
    setIsEditMode(entering)
    if (entering) {
      setDrag(null)
      if (pendingTriplet) cancelTripletBuffer()
    } else {
      setSelectedNoteId(null)
      const dur = selectedNote.duration || 'q'
      const effectiveDotted = dur === '16' ? false : isDotted
      setDrag({ duration: dur, isRest, dotted: effectiveDotted, triplet: isTriplet })
    }
  }

  // ── Tool selection ───────────────────────────────────────────────
  function startDrag(duration, isRestDrag) {
    if (isEditMode && selectedNoteId) {
      editSelectedNoteDuration(duration)
      return
    }
    setIsEditMode(false)
    setSelectedNoteId(null)
    if (pendingTriplet) cancelTripletBuffer()
    if (isTriplet) tripletRefRef.current = duration
    setSelectedNote(prev => ({ ...prev, duration }))
    setIsRest(isRestDrag)
    const effectiveDotted = duration === '16' ? false : isDotted
    if (duration === '16' && isDotted) setIsDotted(false)
    setDrag({ duration, isRest: isRestDrag, dotted: effectiveDotted, triplet: isTriplet })
  }

  // ── Time signature ───────────────────────────────────────────────
  function changeTimeSig(ts) {
    setUndoStack([])
    setTimeSignature(ts)
    setAnacrusis({ enabled: false, q: 0, e: 0, s: 0 })
    setMeasures([EMPTY_MEASURE()])
    setSelectedNoteId(null)
    setHarmonizeVariants([])
    setHarmonizeError(null)
    cancelTripletBuffer()
  }

  // ── Note mutations ───────────────────────────────────────────────
  function addNoteByDrop({ measureIdx, clef, pitch, octave }) {
    if (!drag) return
    const { duration, isRest: dropRest, dotted: dropDotted } = drag
    const effectiveDotted = duration === '16' ? false : dropDotted

    // ── Triplet path ─────────────────────────────────────────────
    if (isTriplet && !dropRest) {
      if (!pendingTriplet) {
        const referenceN = tripletRefRef.current
        const refTicks   = NOTE_TICKS[referenceN] ?? 2
        const durTicks   = NOTE_TICKS[duration]   ?? 2
        if (durTicks < refTicks || durTicks % refTicks !== 0) return
        const firstUnits = durTicks / refTicks
        if (firstUnits > 3) return
        const notesList = measures[measureIdx]?.[clef]
        if (!notesList) return
        const cap = getMeasureCap(measureIdx, measures.length)
        if (!canAddTriplet(notesList, referenceN, cap)) return

        // All guards passed — push undo before mutating
        pushUndo()

        const groupId        = nextGroupIdRef.current++
        const remainingUnits = 3 - firstUnits
        const t = Date.now()
        const newNote = {
          pitch, octave, duration,
          accidental: accidental || undefined,
          triplet: true, tripletGroup: groupId, tripletRef: referenceN, id: t,
        }
        const placeholders = makePlaceholders(groupId, referenceN, remainingUnits, t + 1)
        setMeasures(prev => prev.map((m, mi) =>
          mi !== measureIdx ? m
          : { ...m, [clef]: [...m[clef], newNote, ...placeholders] }
        ))
        setPendingTriplet({ groupId, clef, measureIdx, referenceN, remainingUnits })
        setSelectedStaff(clef)

      } else {
        // Triplet continuation — no undo push (it's part of the same triplet group)
        const pt = pendingTriplet
        if (measureIdx !== pt.measureIdx || clef !== pt.clef) return
        const noteDurTicks = NOTE_TICKS[duration] ?? 2
        const refTicks     = NOTE_TICKS[pt.referenceN] ?? 2
        if (noteDurTicks < refTicks || noteDurTicks % refTicks !== 0) return
        const noteUnits = noteDurTicks / refTicks
        if (noteUnits > pt.remainingUnits) return

        const t = Date.now()
        const newNote = {
          pitch, octave, duration,
          accidental: accidental || undefined,
          triplet: true, tripletGroup: pt.groupId, tripletRef: pt.referenceN, id: t,
        }
        setMeasures(prev => prev.map((m, mi) => {
          if (mi !== pt.measureIdx) return m
          let consumed = 0
          let inserted = false
          const result = []
          for (const n of m[pt.clef]) {
            if (n.tripletGroup === pt.groupId && n.isTripletPlaceholder && consumed < noteUnits) {
              if (!inserted) { result.push(newNote); inserted = true }
              consumed++
            } else {
              result.push(n)
            }
          }
          return { ...m, [pt.clef]: result }
        }))

        const newRemaining = pt.remainingUnits - noteUnits
        setPendingTriplet(newRemaining === 0 ? null : { ...pt, remainingUnits: newRemaining })
        setSelectedStaff(pt.clef)
      }
      return
    }

    if (pendingTriplet !== null) return

    // ── Normal note/rest path — pre-check capacity before pushing ──
    if (measureIdx >= measures.length) return
    const cap = getMeasureCap(measureIdx, measures.length)
    if (!canAdd(measures[measureIdx]?.[clef], duration, cap, effectiveDotted)) return

    pushUndo()
    setSelectedStaff(clef)
    setMeasures(prev => {
      if (measureIdx >= prev.length) return prev
      const c = getMeasureCap(measureIdx, prev.length)
      if (!canAdd(prev[measureIdx][clef], duration, c, effectiveDotted)) return prev
      const note = dropRest
        ? { pitch: 'b', octave: 4, duration, isRest: true, id: Date.now() }
        : { pitch, octave, duration, accidental: accidental || undefined, dotted: effectiveDotted || undefined, tieAfter: isTie || undefined, id: Date.now() }
      return prev.map((m, i) =>
        i !== measureIdx ? m : { ...m, [clef]: [...m[clef], note] }
      )
    })
  }

  function addMeasure() {
    if (measures.length >= MAX_MEASURES) return
    pushUndo()
    setMeasures(prev => prev.length >= MAX_MEASURES ? prev : [...prev, EMPTY_MEASURE()])
  }

  function removeMeasure() {
    const minLen = hasAnacrusis ? 2 : 1
    if (measures.length <= minLen) return
    pushUndo()
    setMeasures(prev => {
      const ml = hasAnacrusis ? 2 : 1
      if (prev.length <= ml) return prev.length === 1 ? [EMPTY_MEASURE()] : prev
      return prev.slice(0, -1)
    })
  }

  function handleSetMeasureCount(n) {
    const minAllowed = hasAnacrusis ? 2 : 1
    const effectiveTarget = Math.min(MAX_MEASURES, Math.max(minAllowed, n))
    if (effectiveTarget <= measures.length) return
    pushUndo()
    setMeasures(prev => [
      ...prev,
      ...Array.from({ length: effectiveTarget - prev.length }, () => EMPTY_MEASURE()),
    ])
  }

  function clearAll() {
    setUndoStack([])
    setSelectedNoteId(null)
    setMeasures(hasAnacrusis ? [PICKUP_MEASURE(), EMPTY_MEASURE()] : [EMPTY_MEASURE()])
    setHarmonizeVariants([])
    setHarmonizeError(null)
    cancelTripletBuffer()
  }

  function exportJson() {
    downloadScoreJson({ measures, timeSignature, tonality, anacruisTicks })
  }

  // ── Harmonization ────────────────────────────────────────────────
  async function requestHarmonize() {
    setIsHarmonizing(true)
    setHarmonizeError(null)
    try {
      const scoreJson = scoreToJson({ measures, timeSignature, tonality, anacruisTicks })
      const variants  = await harmonizeScore(scoreJson)
      setHarmonizeVariants(variants)
      setSelectedVariantIdx(0)
    } catch (err) {
      setHarmonizeError(err.message)
      setHarmonizeVariants([])
    } finally {
      setIsHarmonizing(false)
    }
  }

  function toggleForbiddenRule(id) {
    setSelectedForbiddenRules(prev =>
      prev.includes(id) ? prev.filter(x => x !== id) : [...prev, id]
    )
  }

  async function requestCheck() {
    setIsChecking(true)
    // TODO: implement check logic
    setIsChecking(false)
  }

  function toggleAllowedChord(id) {
    setSelectedAllowedChords(prev =>
      prev.includes(id) ? prev.filter(x => x !== id) : [...prev, id]
    )
  }

  function handleSelectClef(clef) {
    setClefMode(clef)
    setSelectedStaff(clef)
  }

  function handleToggleTriplet() {
    if (isTriplet && pendingTriplet) cancelTripletBuffer()
    if (!isTriplet) {
      setIsDotted(false)
      tripletRefRef.current = selectedNote.duration
    }
    setIsTriplet(prev => !prev)
  }

  // ── Derived state for toolbar ────────────────────────────────────
  const canUndo   = pendingTriplet !== null || undoStack.length > 0
  const canRemove = measures.length > (hasAnacrusis ? 2 : 1)

  return (
    <div className="app">
      <div className="workspace">
        <div className="mode-bar">
          <button
            className={`mode-tab${mode === 'harmonize' ? ' mode-active' : ''}`}
            onClick={() => setMode('harmonize')}
          >ГАРМОНІЗУВАТИ</button>
          <button
            className={`mode-tab${mode === 'check' ? ' mode-active' : ''}`}
            onClick={() => setMode('check')}
          >ПЕРЕВІРИТИ</button>
        </div>

      <NoteToolbar
        durations={DURATIONS}
        timeSigs={TIME_SIGNATURES}
        selected={selectedNote}
        timeSignature={timeSignature}
        isRest={isRest}
        onSelectTimeSig={changeTimeSig}
        onStartDrag={startDrag}
        onUndo={undo}
        onClear={clearAll}
        tonality={tonality}
        onSelectTonality={setTonality}
        accidental={accidental}
        onSelectAccidental={(a) => isEditMode && selectedNoteId
          ? editSelectedNoteAccidental(a)
          : setAccidental(prev => prev === a ? null : a)}
        isDotted={isDotted}
        onToggleDot={() => isEditMode && selectedNoteId ? editSelectedNoteDot() : setIsDotted(p => !p)}
        isTie={isTie}
        onToggleTie={() => isEditMode && selectedNoteId ? editSelectedNoteTie() : setIsTie(p => !p)}
        isTriplet={isTriplet}
        tripletCount={tripletCount}
        onToggleTriplet={() => isEditMode && selectedNoteId ? editSelectedNoteTriplet() : handleToggleTriplet()}
        anacrusis={anacrusis}
        anacruisTicks={anacruisTicks}
        normalCap={normalCap}
        onChangeAnacrusis={setAnacrusis}
        onAddMeasure={addMeasure}
        onRemoveMeasure={removeMeasure}
        canUndo={canUndo}
        canRemoveMeasure={canRemove}
        isHarmonize={mode === 'harmonize'}
        clefMode={clefMode}
        onSelectClef={handleSelectClef}
        isEditMode={isEditMode}
        onToggleEditMode={handleToggleEditMode}
        onDeleteSelected={deleteSelectedNote}
        canDeleteNote={isEditMode && !!selectedNoteId}
        onHarmonize={requestHarmonize}
        isHarmonizing={isHarmonizing}
        isCheck={mode === 'check'}
        onCheck={requestCheck}
        isChecking={isChecking}
        forbiddenRules={FORBIDDEN_RULES}
        selectedForbiddenRules={selectedForbiddenRules}
        onToggleForbiddenRule={toggleForbiddenRule}
        allowedChords={ALLOWED_CHORDS}
        selectedAllowedChords={selectedAllowedChords}
        onToggleAllowedChord={toggleAllowedChord}
        measuresCount={measures.length}
        onSetMeasureCount={handleSetMeasureCount}
      />

      <Staff
        measures={measures}
        timeSignature={timeSignature}
        keySignature={tonality.vexKey}
        drag={drag}
        onDrop={addNoteByDrop}
        anacruisTicks={anacruisTicks}
        selectedNoteId={selectedNoteId}
        onSelectNote={setSelectedNoteId}
        onSetNotePitch={setNotePitch}
        onNoteDragStart={pushUndo}
        showBass={mode === 'check'}
        singleClef={clefMode}
        isTriplet={isTriplet}
        tripletCount={tripletCount}
        pendingTriplet={pendingTriplet}
      />
      </div>

      {isHarmonizing && (
        <div className="harmonize-loading">
          <span className="harmonize-spinner" />
          Гармонізую мелодію…
        </div>
      )}

      {harmonizeError && (
        <div className="harmonize-error">
          Помилка: {harmonizeError}
        </div>
      )}

      {harmonizeVariants.length > 0 && (() => {
        const variant = harmonizeVariants[selectedVariantIdx]
        return (
          <section className="harmonize-results">
            <h2 className="harmonize-results-title">Варіанти гармонізації</h2>

            <div className="variant-tabs">
              {harmonizeVariants.map((v, i) => (
                <button
                  key={v.id}
                  className={`variant-tab${i === selectedVariantIdx ? ' active' : ''}`}
                  onClick={() => setSelectedVariantIdx(i)}
                >
                  {v.name}
                </button>
              ))}
            </div>

            <div className="variant-card">
              <div className="variant-header">
                <div className="variant-meta">
                  <span className="variant-name">{variant.name}</span>
                  <span className="variant-desc">{variant.description}</span>
                </div>
                <a
                  className="btn-action btn-download"
                  href={variant.downloadUrl}
                  download={variant.filename}
                >
                  Завантажити MusicXML
                </a>
              </div>

              <Staff
                measures={variant.measures}
                timeSignature={timeSignature}
                keySignature={tonality.vexKey}
                drag={null}
                onDrop={() => {}}
                anacruisTicks={anacruisTicks}
                selectedNoteId={null}
                onSelectNote={() => {}}
                onSetNotePitch={() => {}}
                showBass={true}
              />
            </div>
          </section>
        )
      })()}
    </div>
  )
}
