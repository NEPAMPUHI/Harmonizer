import { useState, useEffect, useCallback, useRef } from 'react'
import Staff from '../Staff'
import NoteToolbar from '../NoteToolbar'
import { canAdd, canAddTriplet, measureCapacity, noteTicks, usedTicks, NOTE_TICKS } from '../capacity'
import { DEFAULT_TONALITY } from '../tonalities'
import { getKeyAccidentals, getEffectiveSemitones } from '../pitchUtils'
import { downloadScoreJson, scoreToJson } from '../scoreToJson'
import { harmonizeScore } from '../api'
import { usePlayback, getPlaybackBpm } from '../usePlayback'
import { exportSvgToPng } from '../exportPng'

// Allowed note range per clef (diatonic totals = octave×7 + pitch_index)
const NOTE_RANGE = {
  treble: { min: 25, max: 42 },   // G3–C6
  bass:   { min: 14, max: 31 },   // C2–F4
}

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
]

const ALLOWED_CHORDS = [
  'T53', 'S53', 'D53', 'К64', 'T6', 'S6', 'D6', 'T64', 'S64', 'D64',
  'D7', 'D65', 'D43', 'D2',
  'II53', 'II6', 'VI53', 'II7', 'II65', 'II43', 'II2',
  'VII7', 'VII65', 'VII43', 'VII2',
  'D9', 'VII6', 'III53',
].map(id => ({ id, label: id }))

// Durations where two voices at the same pitch share a single note head
const SHARED_HEAD_DURS = new Set(['q', '8', '16'])

// In check mode, if `noteId` is part of a unison pair (same pitch, same tick,
// compatible durations), returns the partner note's id; otherwise null.
function findUnisonPartner(measures, noteId) {
  for (const m of measures) {
    for (const clef of ['treble', 'bass']) {
      const noteIdx = m[clef].findIndex(n => n.id === noteId)
      if (noteIdx === -1) continue
      const note = m[clef][noteIdx]
      if (note.isRest || note.positionTick == null || note.stemDir == null) return null
      const partner = m[clef].find(n =>
        n.id !== noteId &&
        !n.isRest &&
        n.positionTick === note.positionTick &&
        n.pitch === note.pitch &&
        n.octave === note.octave &&
        n.stemDir !== note.stemDir &&
        (n.duration === note.duration ||
          // Both in {q,8,16} AND dot presence is the same → shared head
          (SHARED_HEAD_DURS.has(n.duration) && SHARED_HEAD_DURS.has(note.duration) &&
           !!n.dotted === !!note.dotted))
      )
      return partner?.id ?? null
    }
  }
  return null
}

const MAX_MEASURES   = 64
const MAX_UNDO       = 50
const EMPTY_MEASURE  = () => ({ treble: [], bass: [] })
const PICKUP_MEASURE = () => ({ treble: [], bass: [], isPickup: true })

// Split a flat note array into measures of given tick capacity.
// Triplet groups are kept atomic (never split across a measure boundary).
function splitIntoMeasures(notes, cap) {
  // Build atomic units: single notes or complete triplet groups
  const units = []
  let i = 0
  while (i < notes.length) {
    const note = notes[i]
    if (note.triplet && note.tripletGroup != null) {
      const gid = note.tripletGroup
      const group = []
      while (i < notes.length && notes[i].tripletGroup === gid) {
        group.push(notes[i])
        i++
      }
      units.push({ notes: group, ticks: group.reduce((s, n) => s + noteTicks(n), 0) })
    } else {
      units.push({ notes: [note], ticks: noteTicks(note) })
      i++
    }
  }

  const result = [[]]
  let used = 0

  for (const unit of units) {
    const projected = Math.round((used + unit.ticks) * 10000) / 10000
    // Start a new measure only if there are already notes in the current one
    if (used > 0 && projected > cap) {
      result.push([])
      used = 0
    }
    result[result.length - 1].push(...unit.notes)
    used = Math.round((used + unit.ticks) * 10000) / 10000
  }

  return result
}

// Rest sizes used for decomposing merged tick spans into standard note values (largest first)
const MERGE_REST_SIZES = [
  { duration: 'w',  dotted: false, ticks: 16 },
  { duration: 'h',  dotted: true,  ticks: 12 },
  { duration: 'h',  dotted: false, ticks:  8 },
  { duration: 'q',  dotted: true,  ticks:  6 },
  { duration: 'q',  dotted: false, ticks:  4 },
  { duration: '8',  dotted: true,  ticks:  3 },
  { duration: '8',  dotted: false, ticks:  2 },
  { duration: '16', dotted: false, ticks:  1 },
]

// Decomposes [startTick, startTick+totalTicks) into standard deletion-rest objects.
// All resulting rests are tagged deletionRest:true so they act as overwritable placeholders.
function decomposeRestTicks(startTick, totalTicks, voice, stemDir) {
  const result = []
  let rem    = Math.round(totalTicks * 10000) / 10000
  let cursor = startTick
  let id     = Date.now()
  for (const { duration, dotted, ticks } of MERGE_REST_SIZES) {
    while (rem >= ticks - 0.0001) {
      result.push({
        pitch: 'b', octave: 4, duration, dotted: dotted || undefined,
        isRest: true, deletionRest: true,
        voice, positionTick: Math.round(cursor * 10000) / 10000,
        stemDir, id: id++,
      })
      cursor = Math.round((cursor + ticks) * 10000) / 10000
      rem    = Math.round((rem    - ticks) * 10000) / 10000
    }
  }
  return result
}

// Merges contiguous same-voice rests in a check-mode clef note array.
// Each voice (stemDir) is processed independently so rests from different voices
// never merge into each other, and a note between two rests blocks the merge.
// All merged rests are tagged deletionRest:true so they remain overwritable.
// Returns a new array sorted by positionTick.
function mergeAdjacentVoiceRests(notes) {
  function processVoice(voiceNotes) {
    const sorted = [...voiceNotes].sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
    const result = []
    let i = 0
    while (i < sorted.length) {
      const n = sorted[i]
      if (!n.isRest) { result.push(n); i++; continue }

      const runVoice   = n.voice
      const runStemDir = n.stemDir
      const runStart   = n.positionTick ?? 0
      let runTicks = 0
      let j = i
      while (j < sorted.length && sorted[j].isRest) {
        const m        = sorted[j]
        const expected = Math.round((runStart + runTicks) * 10000) / 10000
        const mStart   = Math.round((m.positionTick ?? 0) * 10000) / 10000
        if (Math.abs(mStart - expected) > 0.0001) break
        runTicks = Math.round((runTicks + noteTicks(m)) * 10000) / 10000
        j++
      }

      result.push(...decomposeRestTicks(runStart, runTicks, runVoice, runStemDir))
      i = j
    }
    return result
  }

  const upper = notes.filter(n => n.stemDir !== -1)
  const lower = notes.filter(n => n.stemDir === -1)
  return [...processVoice(upper), ...processVoice(lower)]
    .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
}

// Returns { newMeasures, nextSelectedId } after deleting the note from measures.
// Harmonize mode: removes note (or whole triplet group), then:
//   - selects the note that was immediately after (shifts left) if any remain in the measure
//   - otherwise selects the last note of the nearest previous non-empty measure
// Check mode: replaces deleted note with an explicit rest (same duration/tick/voice/stemDir),
//   does NOT shift subsequent notes, then selects:
//   1) next note in same voice (cross-measure, forward)
//   2) previous note in same voice (cross-measure, backward)
//   3) nearest note in the other voice of the same clef (by positionTick distance)
function computeDeleteResult(ms, deletedId, currentMode) {
  let foundMIdx = -1, foundClef = null, foundNoteIdx = -1
  outer: for (let mi = 0; mi < ms.length; mi++) {
    for (const clef of ['treble', 'bass']) {
      const idx = ms[mi][clef].findIndex(n => n.id === deletedId)
      if (idx !== -1) { foundMIdx = mi; foundClef = clef; foundNoteIdx = idx; break outer }
    }
  }
  if (foundMIdx === -1) return null

  const deletedNote = ms[foundMIdx][foundClef][foundNoteIdx]

  if (currentMode !== 'check') {
    // ── Harmonize mode ────────────────────────────────────────────
    const deleteIds = new Set()
    if (deletedNote.triplet && deletedNote.tripletGroup != null) {
      ms[foundMIdx][foundClef].forEach(n => {
        if (n.tripletGroup === deletedNote.tripletGroup) deleteIds.add(n.id)
      })
    } else {
      deleteIds.add(deletedId)
    }

    const realNotes = ms[foundMIdx][foundClef].filter(n => !n.isTripletPlaceholder)
    const deletedRealIndices = realNotes
      .map((n, i) => deleteIds.has(n.id) ? i : -1)
      .filter(i => i !== -1)

    if (deletedRealIndices.length === 0) {
      return {
        newMeasures: ms.map((m, mi) =>
          mi !== foundMIdx ? m : { ...m, [foundClef]: m[foundClef].filter(n => !deleteIds.has(n.id)) }
        ),
        nextSelectedId: null,
      }
    }

    const firstIdx = Math.min(...deletedRealIndices)
    const lastIdx  = Math.max(...deletedRealIndices)
    const hasNotesAfter = realNotes.some((n, i) => i > lastIdx && !deleteIds.has(n.id))

    const remaining = realNotes.filter(n => !deleteIds.has(n.id))
    let nextId = null
    if (hasNotesAfter) {
      nextId = remaining[firstIdx]?.id ?? null
    } else {
      // No notes after — go to the nearest previous note
      if (firstIdx > 0) {
        nextId = remaining[firstIdx - 1]?.id ?? null
      } else {
        for (let mi = foundMIdx - 1; mi >= 0; mi--) {
          const prev = ms[mi][foundClef].filter(n => !n.isTripletPlaceholder)
          if (prev.length > 0) { nextId = prev[prev.length - 1].id; break }
        }
      }
    }

    const newMeasures = ms.map((m, mi) =>
      mi !== foundMIdx ? m : { ...m, [foundClef]: m[foundClef].filter(n => !deleteIds.has(n.id)) }
    )
    return { newMeasures, nextSelectedId: nextId }

  } else {
    // ── Check mode ────────────────────────────────────────────────
    const deletedTick    = deletedNote.positionTick ?? 0
    const deletedStemDir = deletedNote.stemDir

    // Replace the deleted note with an explicit rest of the same duration/position/voice.
    // Subsequent notes are NOT shifted — the voice timeline is preserved.
    // deletionRest:true marks it as an overwritable placeholder (not a user-placed rest).
    const restNote = {
      pitch: 'b', octave: 4,
      duration: deletedNote.duration,
      dotted: deletedNote.dotted || undefined,
      isRest: true, deletionRest: true,
      voice: deletedNote.voice,
      positionTick: deletedTick,
      stemDir: deletedStemDir,
      id: Date.now(),
    }

    const newMeasures = ms.map((m, mi) => {
      if (mi !== foundMIdx) return m
      return { ...m, [foundClef]: m[foundClef].map(n => n.id === deletedId ? restNote : n) }
    })

    const isSameVoiceNote = n =>
      n.id !== deletedId && !n.isRest && n.stemDir === deletedStemDir

    // 1) Next note in same voice, searching forward across all measures
    let nextId = null
    const nextInCurrent = ms[foundMIdx][foundClef]
      .filter(n => isSameVoiceNote(n) && n.positionTick != null && n.positionTick > deletedTick + 0.0001)
      .sort((a, b) => a.positionTick - b.positionTick)
    if (nextInCurrent.length > 0) {
      nextId = nextInCurrent[0].id
    } else {
      for (let mi = foundMIdx + 1; mi < ms.length; mi++) {
        const ahead = ms[mi][foundClef]
          .filter(n => !n.isRest && n.stemDir === deletedStemDir)
          .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
        if (ahead.length > 0) { nextId = ahead[0].id; break }
      }
    }

    // 2) Previous note in same voice, searching backward across all measures
    if (!nextId) {
      const prevInCurrent = ms[foundMIdx][foundClef]
        .filter(n => isSameVoiceNote(n) && n.positionTick != null && n.positionTick < deletedTick - 0.0001)
        .sort((a, b) => b.positionTick - a.positionTick)
      if (prevInCurrent.length > 0) {
        nextId = prevInCurrent[0].id
      } else {
        for (let mi = foundMIdx - 1; mi >= 0; mi--) {
          const behind = ms[mi][foundClef]
            .filter(n => !n.isRest && n.stemDir === deletedStemDir)
            .sort((a, b) => (b.positionTick ?? 0) - (a.positionTick ?? 0))
          if (behind.length > 0) { nextId = behind[0].id; break }
        }
      }
    }

    // 3) Nearest note in the other voice of the same clef (across all measures)
    if (!nextId) {
      const otherStemDir = deletedStemDir === 1 ? -1 : 1
      let closestId = null, closestDist = Infinity
      for (const m of ms) {
        for (const n of m[foundClef]) {
          if (n.isRest || n.stemDir !== otherStemDir) continue
          const dist = Math.abs((n.positionTick ?? 0) - deletedTick)
          if (dist < closestDist) { closestDist = dist; closestId = n.id }
        }
      }
      nextId = closestId
    }

    // Merge adjacent same-voice rests in the affected measure/clef
    const mergedMeasures = newMeasures.map((m, mi) => {
      if (mi !== foundMIdx) return m
      return { ...m, [foundClef]: mergeAdjacentVoiceRests(m[foundClef]) }
    })

    return { newMeasures: mergedMeasures, nextSelectedId: nextId }
  }
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
  const [anacrusis,      setAnacrusis]      = useState({ enabled: false, e: 0, s: 0 })
  const [selectedNoteId, setSelectedNoteId] = useState(null)
  const [isEditMode,       setIsEditMode]       = useState(false)
  const [mode,             setMode]             = useState('harmonize')
  const [clefMode,         setClefMode]         = useState('treble')
  const [playbackSpeedMode, setPlaybackSpeedMode] = useState('fast')

  // Always-current ref used by the key handler and undo to read state without stale closures
  const measuresRef = useRef(measures)
  useEffect(() => { measuresRef.current = measures }, [measures])

  const anacrusisRef = useRef(anacrusis)
  useEffect(() => { anacrusisRef.current = anacrusis }, [anacrusis])

  const modeRef = useRef(mode)
  useEffect(() => { modeRef.current = mode }, [mode])

  const timeSignatureRef = useRef(timeSignature)
  useEffect(() => { timeSignatureRef.current = timeSignature }, [timeSignature])

  // Stores the full 4-voice check-mode state so it can be restored after a round-trip through harmonize mode
  const savedCheckMeasuresRef = useRef(null)

  // ── Undo stack ────────────────────────────────────────────────────
  const [undoStack, setUndoStack] = useState([])

  function pushUndo() {
    setUndoStack(prev => [...prev.slice(-(MAX_UNDO - 1)), {
      measures: measuresRef.current,
      anacrusis: anacrusisRef.current,
      timeSignature: timeSignatureRef.current,
      savedCheckMeasures: savedCheckMeasuresRef.current,
    }])
  }

  function handleChangeAnacrusis(newAnacrusis) {
    pushUndo()
    setAnacrusis(newAnacrusis)
  }

  // ── Triplet mode ─────────────────────────────────────────────────
  const [isTriplet,      setIsTriplet]      = useState(false)
  const [pendingTriplet, setPendingTriplet] = useState(null)
  const nextGroupIdRef  = useRef(1)
  const tripletRefRef   = useRef('8')

  const tripletCount = 3 - (pendingTriplet?.remainingUnits ?? 0)

  // ── Harmonization results ────────────────────────────────────────
  // uiState controls which panel is shown in the main workspace area:
  //   'editing'               — NoteToolbar + Staff (default)
  //   'harmonizingLoading'    — loading spinner while waiting for the backend
  //   'harmonizationResults'  — variant tabs + result Staff
  const [uiState,             setUiState]             = useState('editing')
  const [harmonizeVariants,   setHarmonizeVariants]   = useState([])
  const [harmonizeError,      setHarmonizeError]      = useState(null)
  const [selectedVariantIdx,  setSelectedVariantIdx]  = useState(0)
  const [dlDropdownOpen,      setDlDropdownOpen]      = useState(false)
  const dlDropdownRef   = useRef(null)
  const resultStaffRef  = useRef(null)

  // ── Check mode ───────────────────────────────────────────────────
  const [isChecking, setIsChecking] = useState(false)

  // ── Scale modes (harmonize mode) ────────────────────────────────
  const [selectedModes, setSelectedModes] = useState(['natural', 'harmonic', 'melodic'])

  const [selectedForbiddenRules, setSelectedForbiddenRules] = useState(
    () => FORBIDDEN_RULES.map(r => r.id)
  )
  const [selectedAllowedChords, setSelectedAllowedChords] = useState(
    () => ALLOWED_CHORDS.map(c => c.id)
  )

  // ── Anacrusis computed values ────────────────────────────────────
  const normalCap        = measureCapacity(timeSignature)
  const rawAnacruisTicks = anacrusis.e * 2 + anacrusis.s * 1
  const anacruisTicks    = anacrusis.enabled && rawAnacruisTicks > 0 && rawAnacruisTicks < normalCap
    ? rawAnacruisTicks : 0
  const hasAnacrusis = anacruisTicks > 0

  // ── Playback ─────────────────────────────────────────────────────
  const isResultsMode = uiState === 'harmonizationResults'
  const playbackMeasures = isResultsMode
    ? (harmonizeVariants[selectedVariantIdx]?.measures ?? [])
    : measures
  const {
    playbackState,
    currentTick,
    totalTicks,
    timeline,
    play:  handlePlay,
    pause: handlePause,
    stop:  handleStop,
  } = usePlayback({
    measures:   playbackMeasures,
    timeSignature,
    tonality,
    anacruisTicks,
    mode:       isResultsMode ? 'harmonize' : mode,
    audioClef:  isResultsMode ? null : (mode === 'harmonize' ? clefMode : null),
    bpm: getPlaybackBpm(timeSignature, playbackSpeedMode),
  })

  // Maximum allowed anacrusis ticks.
  // When rawAnacruisTicks is already > 0 the pickup exists and the last measure has a
  // reduced capacity — increasing further must leave room for any notes already there.
  // When rawAnacruisTicks is 0 the user is creating the anacrusis from scratch: allow
  // any value < normalCap; the useEffect will add a new empty measure if needed (case 4.2).
  const _lastM            = measures[measures.length - 1]
  const _lastMaxUsed      = Math.max(usedTicks(_lastM?.treble ?? []), usedTicks(_lastM?.bass ?? []))
  const maxAnacruisTicks  = rawAnacruisTicks === 0
    ? normalCap - 1
    : normalCap - Math.max(1, _lastMaxUsed)

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

      if (hasAnacrusis && !firstIsPickup) {
        // Case 4.1: last measure has room for the complement → just prepend the pickup.
        // Case 4.2: last measure is too full → append a new empty measure first so
        //           notes are never lost, then prepend the pickup.
        const last = prev[prev.length - 1]
        const reducedCap = normalCap - anacruisTicks
        const lastFits = usedTicks(last.treble) <= reducedCap && usedTicks(last.bass) <= reducedCap
        return lastFits
          ? [PICKUP_MEASURE(), ...prev]
          : [PICKUP_MEASURE(), ...prev, EMPTY_MEASURE()]
      }

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
            const range = NOTE_RANGE[clef] ?? NOTE_RANGE.treble
            if (totalIdx < range.min || totalIdx > range.max) return prev
            const newPitchIdx = ((totalIdx % 7) + 7) % 7
            // Check mode: prevent voice crossing with concurrent opposite-voice notes
            if (modeRef.current === 'check' && note.stemDir != null && note.positionTick != null) {
              const isUpper   = note.stemDir !== -1
              const noteStart = note.positionTick
              const noteEnd   = Math.round((noteStart + noteTicks(note)) * 10000) / 10000
              const dtFn      = (p, o) => o * 7 + DIATONIC.indexOf(p)
              const newDT     = dtFn(DIATONIC[newPitchIdx], newOctave)
              const oppDir    = isUpper ? -1 : 1
              const concurrent = notes.filter(n => {
                if (n.id === selectedNoteId || n.isRest || n.stemDir !== oppDir || n.positionTick == null) return false
                const nStart = n.positionTick
                const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
                return nStart < noteEnd && nEnd > noteStart
              })
              if (concurrent.length > 0) {
                const cDTs = concurrent.map(n => dtFn(n.pitch, n.octave))
                if ( isUpper && newDT < Math.max(...cDTs)) return prev
                if (!isUpper && newDT > Math.min(...cDTs)) return prev
              }
            }
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

  // ── Global keyboard shortcuts: Ctrl+Z (undo) and Delete/Backspace (clear) ──
  const globalShortcutRef = useRef(null)
  globalShortcutRef.current = (e) => {
    if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA' || e.target.tagName === 'SELECT') return

    if (e.ctrlKey && e.code === 'KeyZ') {
      e.preventDefault()
      undo()
      return
    }

    if (e.key === 'Delete' || e.key === 'Backspace') {
      e.preventDefault()
      if (isEditMode && selectedNoteId) {
        deleteSelectedNote()
      } else {
        clearAll()
      }
    }
  }

  useEffect(() => {
    const handler = (e) => globalShortcutRef.current(e)
    document.addEventListener('keydown', handler)
    return () => document.removeEventListener('keydown', handler)
  }, [])   // eslint-disable-line react-hooks/exhaustive-deps

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

  useEffect(() => {
    if (!dlDropdownOpen) return
    const handler = (e) => {
      if (!dlDropdownRef.current?.contains(e.target)) setDlDropdownOpen(false)
    }
    document.addEventListener('mousedown', handler)
    return () => document.removeEventListener('mousedown', handler)
  }, [dlDropdownOpen])

  // Stop playback when leaving results mode or switching variants
  useEffect(() => { handleStop() }, [uiState])            // eslint-disable-line react-hooks/exhaustive-deps
  useEffect(() => { handleStop() }, [selectedVariantIdx]) // eslint-disable-line react-hooks/exhaustive-deps

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

          // In check mode prevent voice crossing: upper voice must stay at or above
          // the highest concurrent lower-voice note, and lower voice must stay at or
          // below the lowest concurrent upper-voice note.
          if (mode === 'check' && note.stemDir != null && note.positionTick != null) {
            const isUpper   = note.stemDir !== -1
            const noteStart = note.positionTick
            const noteEnd   = Math.round((noteStart + noteTicks(note)) * 10000) / 10000
            const dt        = (p, o) => o * 7 + DIATONIC.indexOf(p)
            const newDT     = dt(newPitch, newOctave)
            const oppDir    = isUpper ? -1 : 1

            const concurrent = notes.filter(n => {
              if (n.id === id || n.isRest || n.stemDir !== oppDir || n.positionTick == null) return false
              const nStart = n.positionTick
              const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
              return nStart < noteEnd && nEnd > noteStart
            })

            if (concurrent.length > 0) {
              const concurrentDTs = concurrent.map(n => dt(n.pitch, n.octave))
              if (isUpper && newDT < Math.max(...concurrentDTs)) return prev
              if (!isUpper && newDT > Math.min(...concurrentDTs)) return prev
            }
          }

          const newNote = { ...note, pitch: newPitch, octave: newOctave }

          // In check mode, sync accidentals when a unison forms or breaks.
          // Dragged note's accidental becomes the shared accidental on entry;
          // partner's accidental is cleared to undefined on exit.
          if (mode === 'check' && note.stemDir != null && note.positionTick != null) {
            const partnerIdx = notes.findIndex(n =>
              n.id !== id &&
              !n.isRest &&
              n.stemDir !== note.stemDir &&
              n.positionTick === note.positionTick
            )
            if (partnerIdx !== -1) {
              const partner   = notes[partnerIdx]
              const wasUnison = note.pitch === partner.pitch && note.octave === partner.octave
              const isUnison  = newPitch   === partner.pitch && newOctave  === partner.octave
              if (isUnison !== wasUnison) {
                const updatedPartner = { ...partner,
                  accidental: isUnison ? newNote.accidental : undefined }
                return prev.map((m, mi) =>
                  mi !== mIdx ? m : {
                    ...m,
                    [clef]: notes.map((n, ni) =>
                      ni === noteIdx    ? newNote        :
                      ni === partnerIdx ? updatedPartner :
                      n
                    )
                  }
                )
              }
            }
          }

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
    setMeasures(snapshot.measures)
    setAnacrusis(snapshot.anacrusis)
    if (snapshot.timeSignature !== undefined) setTimeSignature(snapshot.timeSignature)
    if (snapshot.savedCheckMeasures !== undefined) savedCheckMeasuresRef.current = snapshot.savedCheckMeasures
    // selectedNoteId is validated by the existing useEffect
  }

  // ── Selected-note editing ────────────────────────────────────────
  function deleteSelectedNote() {
    if (!selectedNoteId) return
    const result = computeDeleteResult(measuresRef.current, selectedNoteId, mode)
    if (!result) return
    pushUndo()
    setMeasures(result.newMeasures)
    setSelectedNoteId(result.nextSelectedId)
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

          const cap      = getMeasureCap(mIdx, prev.length)
          const newDotted = newDuration === '16' ? false : (note.dotted ?? false)
          const baseTicks = NOTE_TICKS[newDuration] ?? 4
          const newTicks  = newDotted ? baseTicks * 1.5 : baseTicks
          const oldTicks  = noteTicks(note)
          const newNote   = { ...note, duration: newDuration, dotted: newDotted || undefined }

          // ── Harmonize mode: simple linear capacity check ───────
          if (mode !== 'check' || note.positionTick == null) {
            const otherTicks = prev[mIdx][clef].reduce((s, n, ni) =>
              ni !== noteIdx ? s + noteTicks(n) : s, 0)
            if (otherTicks + newTicks > cap) return prev
            return prev.map((m, mi) =>
              mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni === noteIdx ? newNote : n) }
            )
          }

          // ── Check mode: position-aware validation ──────────────
          const { positionTick, stemDir, voice } = note
          const newEnd = Math.round((positionTick + newTicks) * 10000) / 10000
          const oldEnd = Math.round((positionTick + oldTicks) * 10000) / 10000

          // Reject if note would exceed measure boundary
          if (newEnd > cap + 0.0001) return prev

          // Reject if new span overlaps a real/explicit note or rest of the same voice
          const hasOverlap = prev[mIdx][clef].some(n => {
            if (n.id === note.id || n.stemDir !== stemDir || n.positionTick == null) return false
            if (n.deletionRest) return false // auto-rests are overwritable
            const nStart = n.positionTick
            const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
            return nStart < newEnd - 0.0001 && nEnd > positionTick + 0.0001
          })
          if (hasOverlap) return prev

          // Apply the note change
          let updated = prev[mIdx][clef].map((n, ni) => ni === noteIdx ? newNote : n)

          if (newTicks < oldTicks) {
            // Note shortened: fill the freed gap [newEnd, oldEnd) with deletion-rests
            const newRests = decomposeRestTicks(newEnd, oldTicks - newTicks, voice, stemDir)
            updated = [...updated, ...newRests]
              .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
          } else if (newTicks > oldTicks) {
            // Note lengthened: remove deletion-rests consumed by [oldEnd, newEnd)
            updated = updated.filter(n => {
              if (!n.deletionRest || n.stemDir !== stemDir) return true
              const rs = n.positionTick ?? 0
              const re = Math.round((rs + noteTicks(n)) * 10000) / 10000
              return !(rs < newEnd - 0.0001 && re > oldEnd - 0.0001)
            })
          }

          const merged = mergeAdjacentVoiceRests(updated)
          return prev.map((m, mi) => mi !== mIdx ? m : { ...m, [clef]: merged })
        }
      }
      return prev
    })
    setSelectedNote(prev => ({ ...prev, duration: newDuration }))
    if (newDuration === '16') setIsDotted(false)
  }

  function editSelectedNoteAccidental(accId) {
    pushUndo()
    const partnerId = mode === 'check' ? findUnisonPartner(measuresRef.current, selectedNoteId) : null
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.isRest || note.isTripletPlaceholder) return prev
          const newAcc = note.accidental === accId ? undefined : accId
          let updated = prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) =>
              ni !== noteIdx ? n : { ...note, accidental: newAcc }
            )}
          )
          if (partnerId) {
            for (let pmi = 0; pmi < updated.length; pmi++) {
              const pIdx = updated[pmi][clef].findIndex(n => n.id === partnerId)
              if (pIdx === -1) continue
              const pNote = updated[pmi][clef][pIdx]
              if (pNote.isRest || pNote.isTripletPlaceholder) break
              updated = updated.map((m, mi) =>
                mi !== pmi ? m : { ...m, [clef]: m[clef].map((n, ni) =>
                  ni !== pIdx ? n : { ...pNote, accidental: newAcc }
                )}
              )
              break
            }
          }
          return updated
        }
      }
      return prev
    })
    setAccidental(prev => prev === accId ? null : accId)
  }

  function editSelectedNoteDot() {
    pushUndo()
    const partnerId = mode === 'check' ? findUnisonPartner(measuresRef.current, selectedNoteId) : null
    setMeasures(prev => {
      for (let mIdx = 0; mIdx < prev.length; mIdx++) {
        for (const clef of ['treble', 'bass']) {
          const noteIdx = prev[mIdx][clef].findIndex(n => n.id === selectedNoteId)
          if (noteIdx === -1) continue
          const note = prev[mIdx][clef][noteIdx]
          if (note.triplet || note.duration === '16' || note.isTripletPlaceholder) return prev
          const willBeDotted = !(note.dotted ?? false)
          const cap       = getMeasureCap(mIdx, prev.length)
          const baseTicks = NOTE_TICKS[note.duration] ?? 4
          const oldTicks  = noteTicks(note)
          const newTicks  = willBeDotted ? baseTicks * 1.5 : baseTicks

          // ── Harmonize mode: linear capacity check ──────────────
          if (mode !== 'check' || note.positionTick == null) {
            if (willBeDotted) {
              const otherTicks = prev[mIdx][clef].reduce((s, n, ni) =>
                ni !== noteIdx ? s + noteTicks(n) : s, 0)
              if (otherTicks + newTicks > cap) return prev
            }
            const newNote = { ...note, dotted: willBeDotted || undefined }
            let updated = prev.map((m, mi) =>
              mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni !== noteIdx ? n : newNote) }
            )
            if (partnerId) {
              for (let pmi = 0; pmi < updated.length; pmi++) {
                const pIdx = updated[pmi][clef].findIndex(n => n.id === partnerId)
                if (pIdx === -1) continue
                const pNote = updated[pmi][clef][pIdx]
                if (pNote.isRest || pNote.triplet || pNote.duration === '16' || pNote.isTripletPlaceholder) break
                updated = updated.map((m, mi) =>
                  mi !== pmi ? m : { ...m, [clef]: m[clef].map((n, ni) =>
                    ni !== pIdx ? n : { ...pNote, dotted: willBeDotted || undefined }
                  )}
                )
                break
              }
            }
            return updated
          }

          // ── Check mode: position-aware validation ──────────────
          const { positionTick, stemDir, voice } = note
          const newEnd = Math.round((positionTick + newTicks) * 10000) / 10000
          const oldEnd = Math.round((positionTick + oldTicks) * 10000) / 10000

          if (newEnd > cap + 0.0001) return prev

          const hasOverlap = prev[mIdx][clef].some(n => {
            if (n.id === note.id || n.stemDir !== stemDir || n.positionTick == null) return false
            if (n.deletionRest) return false
            const nStart = n.positionTick
            const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
            return nStart < newEnd - 0.0001 && nEnd > positionTick + 0.0001
          })
          if (hasOverlap) return prev

          let updated = prev[mIdx][clef].map((n, ni) =>
            ni === noteIdx ? { ...note, dotted: willBeDotted || undefined } : n
          )

          if (newTicks < oldTicks) {
            // Dot removed: fill freed gap [newEnd, oldEnd) with deletion-rests
            const newRests = decomposeRestTicks(newEnd, oldTicks - newTicks, voice, stemDir)
            updated = [...updated, ...newRests]
              .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
          } else if (newTicks > oldTicks) {
            // Dot added: consume deletion-rests in the extended span [oldEnd, newEnd)
            updated = updated.filter(n => {
              if (!n.deletionRest || n.stemDir !== stemDir) return true
              const rs = n.positionTick ?? 0
              const re = Math.round((rs + noteTicks(n)) * 10000) / 10000
              return !(rs < newEnd - 0.0001 && re > oldEnd - 0.0001)
            })
          }

          // Apply same change to unison partner (its voice is independent)
          if (partnerId) {
            const pIdx = updated.findIndex(n => n.id === partnerId)
            if (pIdx !== -1) {
              const pNote = updated[pIdx]
              if (!pNote.isRest && !pNote.triplet && pNote.duration !== '16' && !pNote.isTripletPlaceholder) {
                const { stemDir: pStemDir, voice: pVoice } = pNote
                const pHasOverlap = updated.some(n => {
                  if (n.id === partnerId || n.stemDir !== pStemDir || n.positionTick == null) return false
                  if (n.deletionRest) return false
                  const nStart = n.positionTick
                  const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
                  return nStart < newEnd - 0.0001 && nEnd > positionTick + 0.0001
                })
                if (!pHasOverlap) {
                  updated = updated.map((n, i) =>
                    i === pIdx ? { ...pNote, dotted: willBeDotted || undefined } : n
                  )
                  if (newTicks < oldTicks) {
                    const pRests = decomposeRestTicks(newEnd, oldTicks - newTicks, pVoice, pStemDir)
                    updated = [...updated, ...pRests]
                      .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
                  } else if (newTicks > oldTicks) {
                    updated = updated.filter(n => {
                      if (!n.deletionRest || n.stemDir !== pStemDir) return true
                      const rs = n.positionTick ?? 0
                      const re = Math.round((rs + noteTicks(n)) * 10000) / 10000
                      return !(rs < newEnd - 0.0001 && re > oldEnd - 0.0001)
                    })
                  }
                }
              }
            }
          }

          const merged = mergeAdjacentVoiceRests(updated)
          return prev.map((m, mi) => mi !== mIdx ? m : { ...m, [clef]: merged })
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
          const sameM = measures[mIdx][clef]
          let nextNote, nextPrev

          if (mode === 'check' && note.positionTick != null) {
            // Check mode: voice-aware search by positionTick, ignoring other voice and deletion-rests
            const endTick = Math.round((note.positionTick + noteTicks(note)) * 10000) / 10000
            const sameCandidates = sameM
              .filter(n =>
                !n.isRest && !n.isTripletPlaceholder &&
                n.stemDir === note.stemDir &&
                n.positionTick != null &&
                n.positionTick >= endTick - 0.0001 &&
                n.id !== note.id
              )
              .sort((a, b) => a.positionTick - b.positionTick)

            if (sameCandidates.length > 0) {
              nextNote = sameCandidates[0]
              nextPrev = sameM.slice(0, sameM.indexOf(nextNote))
            } else {
              nextNote = null; nextPrev = []
              for (let mi = mIdx + 1; mi < measures.length; mi++) {
                const mc = measures[mi][clef]
                const cx = mc
                  .filter(n => !n.isRest && !n.isTripletPlaceholder && n.stemDir === note.stemDir)
                  .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
                if (cx.length > 0) { nextNote = cx[0]; break }
              }
            }
          } else {
            // Harmonize mode: sequential array index
            const nextM  = measures[mIdx + 1]?.[clef] ?? []
            const inSame = noteIdx + 1 < sameM.length
            nextNote = inSame ? sameM[noteIdx + 1] : nextM[0]
            nextPrev = inSame ? sameM.slice(0, noteIdx + 1) : []
          }

          if (
            !nextNote || nextNote.isRest ||
            getEffectiveSemitones(note, sameM.slice(0, noteIdx), keyAcc) !==
            getEffectiveSemitones(nextNote, nextPrev, keyAcc)
          ) return
        }
        // All guards passed — will mutate
        pushUndo()
        const partnerId = mode === 'check' ? findUnisonPartner(measuresRef.current, selectedNoteId) : null
        const newNote = { ...note, tieAfter: newTieAfter || undefined }
        setMeasures(prev => {
          let updated = prev.map((m, mi) =>
            mi !== mIdx ? m : { ...m, [clef]: m[clef].map((n, ni) => ni !== noteIdx ? n : newNote) }
          )
          if (partnerId) {
            for (let pmi = 0; pmi < updated.length; pmi++) {
              const pIdx = updated[pmi][clef].findIndex(n => n.id === partnerId)
              if (pIdx === -1) continue
              const pNote = updated[pmi][clef][pIdx]
              if (pNote.isRest || pNote.isTripletPlaceholder) break
              // Validate tie for partner: next note in partner's voice must be same pitch
              const partnerSameM = updated[pmi][clef]
              let pNext, pNextPrev

              if (mode === 'check' && pNote.positionTick != null) {
                // Check mode: voice-aware search for partner's next note
                const pEndTick = Math.round((pNote.positionTick + noteTicks(pNote)) * 10000) / 10000
                const pSameCandidates = partnerSameM
                  .filter(n =>
                    !n.isRest && !n.isTripletPlaceholder &&
                    n.stemDir === pNote.stemDir &&
                    n.positionTick != null &&
                    n.positionTick >= pEndTick - 0.0001 &&
                    n.id !== pNote.id
                  )
                  .sort((a, b) => a.positionTick - b.positionTick)

                if (pSameCandidates.length > 0) {
                  pNext     = pSameCandidates[0]
                  pNextPrev = partnerSameM.slice(0, partnerSameM.indexOf(pNext))
                } else {
                  pNext = null; pNextPrev = []
                  for (let pmi2 = pmi + 1; pmi2 < updated.length; pmi2++) {
                    const mc = updated[pmi2][clef]
                    const cx = mc
                      .filter(n => !n.isRest && !n.isTripletPlaceholder && n.stemDir === pNote.stemDir)
                      .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
                    if (cx.length > 0) { pNext = cx[0]; break }
                  }
                }
              } else {
                // Harmonize mode: sequential array index for partner
                const partnerNextM = updated[pmi + 1]?.[clef] ?? []
                const pInSame      = pIdx + 1 < partnerSameM.length
                pNext     = pInSame ? partnerSameM[pIdx + 1] : partnerNextM[0]
                pNextPrev = pInSame ? partnerSameM.slice(0, pIdx + 1) : []
              }

              if (newTieAfter && (!pNext || pNext.isRest ||
                getEffectiveSemitones(pNote, partnerSameM.slice(0, pIdx), keyAcc) !==
                getEffectiveSemitones(pNext, pNextPrev, keyAcc))) break
              updated = updated.map((m, mi) =>
                mi !== pmi ? m : { ...m, [clef]: m[clef].map((n, ni) =>
                  ni !== pIdx ? n : { ...pNote, tieAfter: newTieAfter || undefined }
                )}
              )
              break
            }
          }
          return updated
        })
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
    // Push undo snapshot with clean measures (strip any in-progress triplet placeholders)
    const cleanMeasures = measuresRef.current.map(m => ({
      ...m,
      treble: m.treble.filter(n => !n.isTripletPlaceholder),
      bass:   m.bass.filter(n => !n.isTripletPlaceholder),
    }))
    setUndoStack(prev => [...prev.slice(-(MAX_UNDO - 1)), {
      measures: cleanMeasures,
      anacrusis: anacrusisRef.current,
      timeSignature: timeSignatureRef.current,
      savedCheckMeasures: savedCheckMeasuresRef.current,
    }])

    setTimeSignature(ts)
    setAnacrusis({ enabled: false, e: 0, s: 0 })
    setSelectedNoteId(null)
    setHarmonizeVariants([])
    setHarmonizeError(null)
    setUiState('editing')
    setPendingTriplet(null)
    savedCheckMeasuresRef.current = null
    if (parseInt(ts.split('/')[1], 10) === 8) setIsTriplet(false)

    setMeasures(prev => {
      const newCap    = measureCapacity(ts)
      const prevCount = prev.length

      // Collect all user-entered notes (exclude triplet placeholders)
      const allTreble = prev.flatMap(m => m.treble.filter(n => !n.isTripletPlaceholder))
      const allBass   = prev.flatMap(m => m.bass.filter(n => !n.isTripletPlaceholder))

      const trebleParts = splitIntoMeasures(allTreble, newCap)
      const bassParts   = splitIntoMeasures(allBass,   newCap)

      // Preserve user's measure count; expand if notes need more measures
      const neededCount = Math.max(prevCount, trebleParts.length, bassParts.length)
      const totalCount  = Math.min(MAX_MEASURES, neededCount)

      return Array.from({ length: totalCount }, (_, i) => ({
        treble: trebleParts[i] ?? [],
        bass:   bassParts[i]   ?? [],
      }))
    })
  }

  // ── Note mutations ───────────────────────────────────────────────

  // Check-mode only: insert a note at a specific time position (tick).
  // voiceHint (previewVoice from Staff) is the authoritative voice the note belongs to;
  // it supersedes pitch-based re-derivation so preview and click always agree.
  // Notes are kept sorted by positionTick within the clef array.
  function addNoteToCheckPosition(measureIdx, clef, pitch, octave, targetTick, voiceHint) {
    if (!drag || pendingTriplet !== null) return
    const { duration, isRest: dropRest, dotted: dropDotted } = drag
    const effectiveDotted = duration === '16' ? false : dropDotted
    const noteDurTicks    = NOTE_TICKS[duration] ?? 4
    const noteActualTicks = effectiveDotted ? noteDurTicks * 1.5 : noteDurTicks

    if (measureIdx >= measures.length) return
    const cap = getMeasureCap(measureIdx, measures.length)
    if (Math.round((targetTick + noteActualTicks) * 10000) / 10000 > cap) return

    const upperVoice = clef === 'treble' ? 'soprano' : 'tenor'
    const lowerVoice = clef === 'treble' ? 'alto'    : 'bass'
    const dt       = (p, o) => o * 7 + DIATONIC.indexOf(p)
    const getTotal = n => n.isRest ? -Infinity : dt(n.pitch, n.octave)
    const newTotal = dropRest ? -Infinity : dt(pitch, octave)
    const noteId   = Date.now()

    const buildNote = (voiceName, stemDirVal) => dropRest
      ? { pitch: 'b', octave: 4, duration, isRest: true, dotted: effectiveDotted || undefined, id: noteId,
          voice: voiceName, positionTick: targetTick, stemDir: stemDirVal }
      : { pitch, octave, duration,
          accidental: accidental || undefined,
          dotted: effectiveDotted || undefined,
          tieAfter: isTie || undefined,
          id: noteId,
          voice: voiceName, positionTick: targetTick, stemDir: stemDirVal }

    pushUndo()
    setSelectedStaff(clef)

    setMeasures(prev => {
      if (measureIdx >= prev.length) return prev
      const c = getMeasureCap(measureIdx, prev.length)
      if (Math.round((targetTick + noteActualTicks) * 10000) / 10000 > c) return prev

      // Preprocess: remove same-voice deletion-rests overlapping [targetTick, spanEnd),
      // splitting any remainder before/after the new note back into deletion-rests.
      const hintIsUpperPre = voiceHint != null ? voiceHint !== lowerVoice : null
      const spanEnd = targetTick + noteActualTicks
      let current = prev[measureIdx][clef]
      if (hintIsUpperPre !== null) {
        const tsd = hintIsUpperPre ? 1 : -1
        const splitRests = []
        current = current.filter(n => {
          if (!n.deletionRest || n.stemDir !== tsd) return true
          const rs = n.positionTick ?? 0
          const re = Math.round((rs + noteTicks(n)) * 10000) / 10000
          if (re <= targetTick + 0.0001 || rs >= spanEnd - 0.0001) return true  // no overlap
          if (rs < targetTick - 0.0001)
            splitRests.push(...decomposeRestTicks(rs, targetTick - rs, n.voice, tsd))
          if (re > spanEnd + 0.0001)
            splitRests.push(...decomposeRestTicks(spanEnd, re - spanEnd, n.voice, tsd))
          return false
        })
        if (splitRests.length > 0)
          current = [...current, ...splitRests]
            .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
      }

      const atTick  = current.filter(n => n.positionTick === targetTick)
      // Notes strictly inside the new note's span (not at targetTick itself)
      const inSpan  = current.filter(n => n.positionTick > targetTick && n.positionTick < spanEnd)
      const outside = current.filter(n => n.positionTick !== targetTick && (n.positionTick <= targetTick || n.positionTick >= spanEnd))

      let finalAtTick
      let isUpper

      // newIsUpper: trust voiceHint (from preview) when available; fall back to pitch comparison
      const hintIsUpper = voiceHint != null ? voiceHint !== lowerVoice : null

      if (atTick.length === 0) {
        isUpper = hintIsUpper ?? true
        finalAtTick = isUpper
          ? [buildNote(upperVoice,  1)]
          : [buildNote(lowerVoice, -1)]

      } else if (atTick.length === 1) {
        const existTotal = getTotal(atTick[0])
        isUpper    = hintIsUpper ?? (newTotal > existTotal)
        if (isUpper) {
          finalAtTick = [buildNote(upperVoice, 1), { ...atTick[0], voice: lowerVoice, stemDir: -1 }]
        } else {
          finalAtTick = [{ ...atTick[0], voice: upperVoice, stemDir: 1 }, buildNote(lowerVoice, -1)]
        }

      } else {
        // 2 notes at this tick — trust voiceHint or replace the closer-pitch voice
        const sorted   = [...atTick].sort((a, b) => getTotal(b) - getTotal(a))
        const [hi, lo] = sorted
        const hiTotal  = getTotal(hi)
        const loTotal  = getTotal(lo)

        isUpper = hintIsUpper ?? (() => {
          if (newTotal >= hiTotal) return true
          if (newTotal <= loTotal) return false
          return (hiTotal - newTotal) <= (newTotal - loTotal)
        })()

        if (isUpper) {
          finalAtTick = [buildNote(upperVoice, 1), { ...lo, voice: lowerVoice, stemDir: -1 }]
        } else {
          finalAtTick = [{ ...hi, voice: upperVoice, stemDir: 1 }, buildNote(lowerVoice, -1)]
        }
      }

      // If the new note is upper, demote any upper-voice notes within its span to lower voice.
      // This handles the case where short notes were provisionally placed in upper voice
      // before a longer upper-voice note was added above them.
      const finalInSpan = inSpan.map(n =>
        isUpper && n.stemDir !== -1
          ? { ...n, voice: lowerVoice, stemDir: -1 }
          : n
      )

      const newNotes = [...outside, ...finalAtTick, ...finalInSpan]
        .sort((a, b) => (a.positionTick ?? Infinity) - (b.positionTick ?? Infinity))

      // Block placement if the new note's pitch crosses any concurrent opposite-voice note.
      // This catches cases where the voice hint contradicts pitch order, or where a long
      // note in one voice overlaps a new note at a different tick in the other voice.
      if (!dropRest) {
        const newEntry   = newNotes.find(n => n.id === noteId)
        if (newEntry) {
          const isNewUpper = newEntry.stemDir !== -1
          const newDT      = dt(pitch, octave)
          const oppDir     = isNewUpper ? -1 : 1
          const concurrent = newNotes.filter(n => {
            if (n.id === noteId || n.isRest || n.stemDir !== oppDir || n.positionTick == null) return false
            const nStart = n.positionTick
            const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
            return nStart < spanEnd && nEnd > targetTick
          })
          if (concurrent.length > 0) {
            const cDTs = concurrent.map(n => dt(n.pitch, n.octave))
            if ( isNewUpper && newDT < Math.max(...cDTs)) return prev
            if (!isNewUpper && newDT > Math.min(...cDTs)) return prev
          }
        }
      }

      return prev.map((m, i) => i !== measureIdx ? m : { ...m, [clef]: newNotes })
    })
  }

  function addNoteByDrop({ measureIdx, clef, pitch, octave, targetTick, previewVoice }) {
    if (!drag) return
    const { duration, isRest: dropRest, dotted: dropDotted } = drag
    const effectiveDotted = duration === '16' ? false : dropDotted

    // In Check mode, non-triplet drops go through the time-position path.
    // Triplet notes bypass this so the triplet branch below handles them.
    if (mode === 'check' && targetTick != null && !(isTriplet && !dropRest)) {
      addNoteToCheckPosition(measureIdx, clef, pitch, octave, targetTick, previewVoice)
      return
    }

    // ── Triplet path ─────────────────────────────────────────────
    if (isTriplet && !dropRest) {
      if (!pendingTriplet) {
        const referenceN = tripletRefRef.current
        const refTicks   = NOTE_TICKS[referenceN] ?? 2
        const durTicks   = NOTE_TICKS[duration]   ?? 2
        if (durTicks < refTicks || durTicks % refTicks !== 0) return
        const firstUnits = durTicks / refTicks
        if (firstUnits > 3) return
        const cap = getMeasureCap(measureIdx, measures.length)

        if (mode === 'check') {
          // Per-voice check: the full group span [targetTick, targetTick + refTicks×2) must fit in the measure
          if (targetTick == null) return
          if (Math.round((targetTick + refTicks * 2) * 10000) / 10000 > cap + 0.0001) return
        } else {
          const notesList = measures[measureIdx]?.[clef]
          if (!notesList) return
          if (!canAddTriplet(notesList, referenceN, cap)) return
        }

        // All guards passed — push undo before mutating
        pushUndo()

        const groupId        = nextGroupIdRef.current++
        const remainingUnits = 3 - firstUnits
        const t = Date.now()

        if (mode === 'check') {
          const upperVoice = clef === 'treble' ? 'soprano' : 'tenor'
          const lowerVoice = clef === 'treble' ? 'alto'    : 'bass'
          const voiceName  = previewVoice === lowerVoice ? lowerVoice : upperVoice
          const stemDirVal = voiceName === upperVoice ? 1 : -1
          // Actual ticks per note: triplet duration = (base × 2) / 3
          const firstTicks = Math.round(((durTicks * 2) / 3) * 10000) / 10000
          const phTicks    = Math.round(((refTicks * 2) / 3) * 10000) / 10000
          const spanEnd    = Math.round((targetTick + refTicks * 2) * 10000) / 10000

          const newNote = {
            pitch, octave, duration,
            accidental: accidental || undefined,
            triplet: true, tripletGroup: groupId, tripletRef: referenceN, id: t,
            voice: voiceName, stemDir: stemDirVal,
            positionTick: Math.round(targetTick * 10000) / 10000,
          }
          const checkPlaceholders = Array.from({ length: remainingUnits }, (_, i) => ({
            pitch: 'b', octave: 4, duration: referenceN,
            isRest: true, triplet: true, tripletGroup: groupId, tripletRef: referenceN,
            isTripletPlaceholder: true, id: t + 1 + i,
            voice: voiceName, stemDir: stemDirVal,
            positionTick: Math.round((targetTick + firstTicks + i * phTicks) * 10000) / 10000,
          }))

          setMeasures(prev => prev.map((m, mi) => {
            if (mi !== measureIdx) return m
            // Remove deletion-rests of this voice that overlap the triplet's span
            const cleaned = m[clef].filter(n => {
              if (!n.deletionRest || n.stemDir !== stemDirVal) return true
              const rs = n.positionTick ?? 0
              const re = Math.round((rs + noteTicks(n)) * 10000) / 10000
              return re <= targetTick + 0.0001 || rs >= spanEnd - 0.0001
            })
            return {
              ...m,
              [clef]: [...cleaned, newNote, ...checkPlaceholders]
                .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0)),
            }
          }))
          setPendingTriplet({ groupId, clef, measureIdx, referenceN, remainingUnits,
            voice: voiceName, stemDir: stemDirVal })
        } else {
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
        }
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

        if (mode === 'check') {
          // Get positionTick from the first placeholder being consumed; voice/stemDir come from the group
          setMeasures(prev => prev.map((m, mi) => {
            if (mi !== pt.measureIdx) return m
            let consumed = 0
            let inserted = false
            let phTick = null
            const result = []
            for (const n of m[pt.clef]) {
              if (n.tripletGroup === pt.groupId && n.isTripletPlaceholder && consumed < noteUnits) {
                if (!inserted) {
                  phTick = n.positionTick
                  result.push({
                    pitch, octave, duration,
                    accidental: accidental || undefined,
                    triplet: true, tripletGroup: pt.groupId, tripletRef: pt.referenceN, id: t,
                    voice: pt.voice, stemDir: pt.stemDir, positionTick: phTick,
                  })
                  inserted = true
                }
                consumed++
              } else {
                result.push(n)
              }
            }
            return { ...m, [pt.clef]: result }
          }))
        } else {
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
        }

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
        ? { pitch: 'b', octave: 4, duration, isRest: true, dotted: effectiveDotted || undefined, id: Date.now() }
        : { pitch, octave, duration, accidental: accidental || undefined, dotted: effectiveDotted || undefined, tieAfter: isTie || undefined, id: Date.now() }
      return prev.map((m, i) =>
        i !== measureIdx ? m : { ...m, [clef]: [...m[clef], note] }
      )
    })
  }

  function addMeasure() {
    const maxLen = MAX_MEASURES + (hasAnacrusis ? 1 : 0)
    if (measures.length >= maxLen) return
    pushUndo()
    setMeasures(prev => {
      const ml = MAX_MEASURES + (hasAnacrusis ? 1 : 0)
      return prev.length >= ml ? prev : [...prev, EMPTY_MEASURE()]
    })
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

  function handleSetMeasureCount(displayedN) {
    // displayedN is the count excluding the pickup measure.
    // Convert to actual array length before comparing/adding.
    const offset = hasAnacrusis ? 1 : 0
    const target = Math.min(MAX_MEASURES + offset, Math.max(offset + 1, displayedN + offset))
    if (target === measures.length) return
    pushUndo()
    setMeasures(prev => {
      if (target > prev.length) {
        return [...prev, ...Array.from({ length: target - prev.length }, () => EMPTY_MEASURE())]
      }
      return prev.slice(0, target)
    })
  }

  function clearAll() {
    pushUndo()
    setPendingTriplet(null)
    setSelectedNoteId(null)
    setMeasures(hasAnacrusis ? [PICKUP_MEASURE(), EMPTY_MEASURE()] : [EMPTY_MEASURE()])
    setHarmonizeVariants([])
    setHarmonizeError(null)
    setUiState('editing')
    savedCheckMeasuresRef.current = null
  }

  function exportJson() {
    downloadScoreJson({ measures, timeSignature, tonality, anacruisTicks, selectedModes })
  }

  async function handleDownloadPng() {
    const svg = resultStaffRef.current?.querySelector('svg')
    const now = new Date()
    const dd   = String(now.getDate()).padStart(2, '0')
    const mm   = String(now.getMonth() + 1).padStart(2, '0')
    const yy   = String(now.getFullYear()).slice(-2)
    const hh   = String(now.getHours()).padStart(2, '0')
    const min  = String(now.getMinutes()).padStart(2, '0')
    const ss   = String(now.getSeconds()).padStart(2, '0')
    const filename = `harm_var_${selectedVariantIdx + 1}_${dd}${mm}${yy}_${hh}${min}${ss}.png`
    setDlDropdownOpen(false)
    await exportSvgToPng(svg, filename)
  }

  // ── Harmonization ────────────────────────────────────────────────
  async function requestHarmonize() {
    setUiState('harmonizingLoading')
    setHarmonizeError(null)
    try {
      const scoreJson = scoreToJson({ measures, timeSignature, tonality, anacruisTicks, selectedModes })
      const variants  = await harmonizeScore(scoreJson)
      setHarmonizeVariants(variants)
      setSelectedVariantIdx(0)
      setUiState('harmonizationResults')
    } catch (err) {
      setHarmonizeError(err.message)
      setHarmonizeVariants([])
      setUiState('editing')
    }
  }

  function goBackFromResults() {
    setUiState('editing')
    setHarmonizeVariants([])
    setHarmonizeError(null)
    setSelectedVariantIdx(0)
  }

  function selectVariantAndEdit() {
    const variant = harmonizeVariants[selectedVariantIdx]
    if (!variant) return
    const annotated = variant.measures.map(m => ({
      ...m,
      treble: annotateForCheck(m.treble, 'soprano',  1),
      bass:   annotateForCheck(m.bass,   'bass',    -1),
    }))
    pushUndo()
    setMeasures(annotated)
    savedCheckMeasuresRef.current = null
    setHarmonizeVariants([])
    setHarmonizeError(null)
    setSelectedVariantIdx(0)
    setUiState('editing')
    setMode('check')
  }

  function toggleMode(id) {
    setSelectedModes(prev => {
      if (prev.includes(id)) {
        const next = prev.filter(x => x !== id)
        return next.length === 0 ? ['natural'] : next
      }
      return [...prev, id]
    })
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

  // ── Mode switching ───────────────────────────────────────────────
  const annotateForCheck = (notes, voiceName, stemDirVal) => {
    let cursor = 0
    return notes.map(note => {
      const tick = cursor
      cursor = Math.round((cursor + noteTicks(note)) * 10000) / 10000
      return { ...note, voice: voiceName, stemDir: stemDirVal, positionTick: tick }
    })
  }

  // harmonize → check: stamp every note with positionTick / voice / stemDir.
  // If returning from a prior check→harmonize trip, merge soprano/bass from
  // harmonize mode back into the preserved alto/tenor from savedCheckMeasuresRef.
  function switchToCheck() {
    if (mode === 'harmonize') {
      const saved = savedCheckMeasuresRef.current
      if (saved) {
        setMeasures(prev => {
          const len = Math.max(prev.length, saved.length)
          return Array.from({ length: len }, (_, i) => {
            const hm = prev[i]  ?? EMPTY_MEASURE()
            const sc = saved[i] ?? EMPTY_MEASURE()
            const sopranoNotes = annotateForCheck(hm.treble, 'soprano',  1)
            const bassNotes    = annotateForCheck(hm.bass,   'bass',    -1)
            const altoNotes    = sc.treble.filter(n => n.voice === 'alto')
            const tenorNotes   = sc.bass.filter(n => n.voice === 'tenor')
            const newTreble    = [...sopranoNotes, ...altoNotes]
              .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
            const newBass      = [...tenorNotes, ...bassNotes]
              .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
            return {
              ...(hm.isPickup || sc.isPickup ? { isPickup: true } : {}),
              treble: newTreble,
              bass:   newBass,
            }
          })
        })
      } else {
        setMeasures(prev => prev.map(m => ({
          ...m,
          treble: annotateForCheck(m.treble, 'soprano',  1),
          bass:   annotateForCheck(m.bass,   'bass',    -1),
        })))
      }
    }
    setUiState('editing')
    setMode('check')
  }

  // check → harmonize: save the full 4-voice state, then keep only outer voices,
  // sort by positionTick, strip check-mode fields so the single-voice renderer sees a plain list.
  function switchToHarmonize() {
    if (mode === 'check') {
      savedCheckMeasuresRef.current = measuresRef.current
      setMeasures(prev => prev.map(m => {
        const cleanNotes = (notes, keepVoice) => {
          const filtered = notes
            .filter(n => n.voice == null || n.voice === keepVoice)
            .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
          return filtered.map(({ voice: _v, stemDir: _s, positionTick: _p, ...rest }) => rest)
        }
        return {
          ...m,
          treble: cleanNotes(m.treble, 'soprano'),
          bass:   cleanNotes(m.bass,   'bass'),
        }
      }))
    }
    setUiState('editing')
    setMode('harmonize')
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
  const isHarmonizing = uiState === 'harmonizingLoading'
  const canUndo      = pendingTriplet !== null || undoStack.length > 0
  const canRemove    = measures.length > (hasAnacrusis ? 2 : 1)
  const canAddMeasure = measures.length < MAX_MEASURES + (hasAnacrusis ? 1 : 0)

  return (
    <div className="app">
      <div className="workspace">
        <div className="workspace-toolbar">
          {uiState === 'harmonizationResults' ? (
            <div className="mode-bar mode-bar--results">
              <span className="results-heading">Результати гармонізації</span>
            </div>
          ) : (
            <div className="mode-bar">
              <button
                className={`mode-tab${mode === 'harmonize' ? ' mode-active' : ''}`}
                onClick={switchToHarmonize}
              >ГАРМОНІЗУВАТИ</button>
              <button
                className={`mode-tab${mode === 'check' ? ' mode-active' : ''}`}
                onClick={switchToCheck}
              >ПЕРЕВІРИТИ</button>
            </div>
          )}

          {uiState === 'editing' && (
            <NoteToolbar
              durations={DURATIONS}
              timeSigs={TIME_SIGNATURES}
              selected={selectedNote}
              timeSignature={timeSignature}
              isRest={isRest}
              onSelectTimeSig={changeTimeSig}
              onStartDrag={startDrag}
              onUndo={undo}
              hasSelectedNote={isEditMode && !!selectedNoteId}
              onClear={() => isEditMode && selectedNoteId ? deleteSelectedNote() : clearAll()}
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
              maxAnacruisTicks={maxAnacruisTicks}
              onChangeAnacrusis={handleChangeAnacrusis}
              onAddMeasure={addMeasure}
              onRemoveMeasure={removeMeasure}
              canUndo={canUndo}
              canRemoveMeasure={canRemove}
              isHarmonize={mode === 'harmonize'}
              clefMode={clefMode}
              onSelectClef={handleSelectClef}
              isEditMode={isEditMode}
              onToggleEditMode={handleToggleEditMode}
              selectedModes={selectedModes}
              onToggleMode={toggleMode}
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
              measuresCount={measures.length - (hasAnacrusis ? 1 : 0)}
              canAddMeasure={canAddMeasure}
              onSetMeasureCount={handleSetMeasureCount}
              playbackState={playbackState}
              onPlay={handlePlay}
              onPause={handlePause}
              onStop={handleStop}
              playbackSpeedMode={playbackSpeedMode}
              onSetSpeedMode={setPlaybackSpeedMode}
            />
          )}

          {uiState === 'harmonizationResults' && harmonizeVariants.length > 0 && (
            <div className="results-toolbar">

              {/* Колонка 1 — Назад */}
              <div className="results-col results-col--left">
                <button className="btn-back-results" onClick={goBackFromResults} title="Назад">
                  <svg viewBox="0 0 20 20" fill="none" xmlns="http://www.w3.org/2000/svg">
                    <path d="M12 5L7 10l5 5" stroke="currentColor" strokeWidth="2" strokeLinecap="round" strokeLinejoin="round"/>
                  </svg>
                </button>
              </div>

              {/* Колонка 2 — Варіанти */}
              <div className="results-col results-col--center">
                {harmonizeVariants.map((v, i) => (
                  <button
                    key={v.id}
                    className={`variant-tab${i === selectedVariantIdx ? ' active' : ''}`}
                    onClick={() => setSelectedVariantIdx(i)}
                  >
                    {i + 1}
                  </button>
                ))}
              </div>

              {/* Колонка 3 — Дії + Playback */}
              <div className="results-col results-col--right">
                <button className="btn-result-action btn-select" onClick={selectVariantAndEdit}>
                  Обрати і змінити
                </button>
                <div className="dl-dropdown-wrap" ref={dlDropdownRef}>
                  <button
                    className={`btn-result-action btn-download${dlDropdownOpen ? ' open' : ''}`}
                    onClick={() => setDlDropdownOpen(o => !o)}
                  >
                    <svg className="icon-download" viewBox="0 0 16 16" fill="none" xmlns="http://www.w3.org/2000/svg">
                      <path d="M8 1v9M4 7l4 4 4-4M2 14h12" stroke="currentColor" strokeWidth="1.7" strokeLinecap="round" strokeLinejoin="round"/>
                    </svg>
                    Завантажити
                    <svg className="icon-caret" viewBox="0 0 10 6" fill="none" xmlns="http://www.w3.org/2000/svg">
                      <path d="M1 1l4 4 4-4" stroke="currentColor" strokeWidth="1.5" strokeLinecap="round" strokeLinejoin="round"/>
                    </svg>
                  </button>
                  {dlDropdownOpen && (
                    <div className="dl-dropdown-menu">
                      <button className="dl-menu-item" onClick={handleDownloadPng}>
                        PNG
                      </button>
                      <button className="dl-menu-item" onClick={() => { window.open(harmonizeVariants[selectedVariantIdx]?.downloadUrl); setDlDropdownOpen(false) }}>
                        MusicXML
                      </button>
                    </div>
                  )}
                </div>
                <div className="toolbar-ctrl-buttons">
                  <button
                    className={`btn-ctrl btn-ctrl-speed${playbackSpeedMode === 'slow' ? ' active' : ''}`}
                    onClick={() => setPlaybackSpeedMode('slow')}
                    title={`Повільний темп (${getPlaybackBpm(timeSignature, 'slow')} BPM)`}
                  >slow</button>
                  <button
                    className={`btn-ctrl btn-ctrl-speed${playbackSpeedMode === 'fast' ? ' active' : ''}`}
                    onClick={() => setPlaybackSpeedMode('fast')}
                    title={`Швидкий темп (${getPlaybackBpm(timeSignature, 'fast')} BPM)`}
                  >fast</button>
                  <button
                    className={`btn-ctrl btn-ctrl-play${playbackState === 'playing' ? ' playing' : ''}`}
                    onClick={handlePlay}
                    disabled={playbackState === 'playing'}
                    title={playbackState === 'paused' ? 'Продовжити' : 'Відтворити'}
                  >▶</button>
                  <button
                    className="btn-ctrl btn-ctrl-pause"
                    onClick={handlePause}
                    disabled={playbackState !== 'playing'}
                    title="Пауза"
                  >⏸</button>
                  <button
                    className="btn-ctrl btn-ctrl-stop"
                    onClick={handleStop}
                    disabled={playbackState === 'idle'}
                    title="Зупинити"
                  >⏹</button>
                </div>
              </div>

            </div>
          )}
        </div>

        {uiState === 'editing' && (
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
            isCheckMode={mode === 'check'}
            currentTick={currentTick}
            totalTicks={totalTicks}
            playbackState={playbackState}
          />
        )}

        {uiState === 'harmonizingLoading' && (
          <div className="harmonize-loading">
            <span className="harmonize-spinner" />
            Гармонізую мелодію…
          </div>
        )}

        {uiState === 'harmonizationResults' && harmonizeVariants[selectedVariantIdx] && (
          <div ref={resultStaffRef} style={{ display: 'contents' }}>
            <Staff
              measures={harmonizeVariants[selectedVariantIdx].measures}
              timeSignature={timeSignature}
              keySignature={tonality.vexKey}
              drag={null}
              onDrop={() => {}}
              anacruisTicks={anacruisTicks}
              selectedNoteId={null}
              onSelectNote={() => {}}
              onSetNotePitch={() => {}}
              showBass={true}
              currentTick={currentTick}
              totalTicks={totalTicks}
              playbackState={playbackState}
            />
          </div>
        )}
      </div>
    </div>
  )
}
