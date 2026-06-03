import { noteTicks, measureCapacity } from './capacity'
import { getKeyAccidentals, getEffectiveSemitones } from './pitchUtils'

// ─── Measure capacity helpers ────────────────────────────────────────────────

function getMeasureCap(measureIdx, totalMeasures, normalCap, anacruisTicks) {
  if (anacruisTicks > 0 && measureIdx === 0) return anacruisTicks
  if (anacruisTicks > 0 && measureIdx === totalMeasures - 1 && totalMeasures > 1) {
    return normalCap - anacruisTicks
  }
  return normalCap
}

// Cumulative global-tick offset for the start of each measure.
function buildMeasureOffsets(measures, normalCap, anacruisTicks) {
  const offsets = []
  let cursor = 0
  for (let i = 0; i < measures.length; i++) {
    offsets.push(cursor)
    cursor += getMeasureCap(i, measures.length, normalCap, anacruisTicks)
  }
  return offsets
}

// ─── Pitch → MIDI ────────────────────────────────────────────────────────────

// Returns MIDI note number (C4 = 60) or null for rests.
// getEffectiveSemitones returns octave*12 + pitchSemitones + alter, where C4 = 48.
// Adding 12 maps to the standard MIDI convention (C4 = 60).
function toMidi(note, precedingNotes, keyAcc) {
  if (note.isRest) return null
  return getEffectiveSemitones(note, precedingNotes, keyAcc) + 12
}

// ─── Tie merging ─────────────────────────────────────────────────────────────

// Collapses consecutive tied notes of the same pitch into a single event.
// Input events must be sorted by startTick within one voice.
// Events carry an internal _tieAfter flag that is stripped from the output.
function mergeTies(events) {
  if (events.length === 0) return []
  const result = []
  let i = 0
  while (i < events.length) {
    const ev = events[i]
    if (!ev._tieAfter || ev.midiNote === null) {
      // No tie or rest — emit as-is
      result.push({ startTick: ev.startTick, durationTicks: ev.durationTicks, midiNote: ev.midiNote, voice: ev.voice, clef: ev.clef })
      i++
    } else {
      // Accumulate duration while next note has the same midiNote
      let totalDuration = ev.durationTicks
      let j = i + 1
      while (j < events.length) {
        const next = events[j]
        if (next.midiNote !== ev.midiNote) break
        totalDuration += next.durationTicks
        j++
        if (!next._tieAfter) break   // chain ends at the note without tieAfter
      }
      result.push({ startTick: ev.startTick, durationTicks: totalDuration, midiNote: ev.midiNote, voice: ev.voice, clef: ev.clef })
      i = j
    }
  }
  return result
}

// ─── Raw event builder ───────────────────────────────────────────────────────

function addEvent(voiceEvents, voice, event) {
  ;(voiceEvents[voice] = voiceEvents[voice] ?? []).push(event)
}

function buildCheckModeEvents(measures, offsets, keyAcc, voiceEvents) {
  for (let mIdx = 0; mIdx < measures.length; mIdx++) {
    const globalOffset = offsets[mIdx]
    const measure = measures[mIdx]

    for (const clef of ['treble', 'bass']) {
      const allNotes = (measure[clef] ?? []).filter(n => !n.isTripletPlaceholder)
      const sortedAll = [...allNotes].sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))

      // Group by voice name (soprano / alto / tenor / bass)
      const byVoice = {}
      for (const note of allNotes) {
        const v = note.voice ?? (clef === 'treble' ? 'soprano' : 'bass')
        ;(byVoice[v] = byVoice[v] ?? []).push(note)
      }

      for (const [voice, voiceNotes] of Object.entries(byVoice)) {
        const sorted = [...voiceNotes].sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))

        for (const note of sorted) {
          // Accidental context: all clef notes with an earlier positionTick
          const precedingNotes = sortedAll.filter(n => (n.positionTick ?? 0) < (note.positionTick ?? 0))
          addEvent(voiceEvents, voice, {
            startTick:     globalOffset + (note.positionTick ?? 0),
            durationTicks: noteTicks(note),
            midiNote:      toMidi(note, precedingNotes, keyAcc),
            voice,
            clef,
            _tieAfter:     note.tieAfter ?? false,
          })
        }
      }
    }
  }
}

function buildHarmonizeModeEvents(measures, offsets, keyAcc, voiceEvents) {
  const clefVoice = { treble: 'soprano', bass: 'bass' }

  for (const clef of ['treble', 'bass']) {
    const voice = clefVoice[clef]

    for (let mIdx = 0; mIdx < measures.length; mIdx++) {
      const globalOffset = offsets[mIdx]
      // Skip isTripletPlaceholder; auto-rests (rest-auto-*) never appear in measures state
      const notes = (measures[mIdx][clef] ?? []).filter(n => !n.isTripletPlaceholder)

      let cursor = 0
      for (let nIdx = 0; nIdx < notes.length; nIdx++) {
        const note = notes[nIdx]
        // Accidental context: all earlier notes in the same measure clef
        const precedingNotes = notes.slice(0, nIdx)
        addEvent(voiceEvents, voice, {
          startTick:     globalOffset + cursor,
          durationTicks: noteTicks(note),
          midiNote:      toMidi(note, precedingNotes, keyAcc),
          voice,
          clef,
          _tieAfter:     note.tieAfter ?? false,
        })
        cursor = Math.round((cursor + noteTicks(note)) * 10000) / 10000
      }
    }
  }
}

// ─── Public API ──────────────────────────────────────────────────────────────

/**
 * Converts the current measures state into a flat, sorted list of playback events.
 *
 * @param {Array}  measures        - The measures array from Home state
 * @param {string} timeSignature   - e.g. '4/4', '3/8'
 * @param {object} tonality        - { acc: number, ... } from Home state
 * @param {number} anacruisTicks   - sixteenth-note ticks for the pickup measure (0 = none)
 * @param {string} mode            - 'harmonize' | 'check'
 * @returns {PlaybackEvent[]}
 *   PlaybackEvent = { startTick, durationTicks, midiNote, voice, clef }
 *   midiNote is null for rests; C4 = 60.
 */
export function buildPlaybackTimeline(measures, timeSignature, tonality, anacruisTicks, mode) {
  if (!measures || measures.length === 0) return []

  const normalCap = measureCapacity(timeSignature)
  const keyAcc    = getKeyAccidentals(tonality?.acc ?? 0)
  const offsets   = buildMeasureOffsets(measures, normalCap, anacruisTicks)

  const voiceEvents = {}

  if (mode === 'check') {
    buildCheckModeEvents(measures, offsets, keyAcc, voiceEvents)
  } else {
    buildHarmonizeModeEvents(measures, offsets, keyAcc, voiceEvents)
  }

  // Merge ties per voice, then flatten and sort globally
  const result = []
  for (const events of Object.values(voiceEvents)) {
    const sorted = [...events].sort((a, b) => a.startTick - b.startTick)
    result.push(...mergeTies(sorted))
  }

  return result.sort((a, b) => a.startTick - b.startTick || a.voice.localeCompare(b.voice))
}

/**
 * Total sixteenth-note ticks for the entire score (sum of all measure capacities).
 */
export function totalPlaybackTicks(measures, timeSignature, anacruisTicks) {
  if (!measures || measures.length === 0) return 0
  const normalCap = measureCapacity(timeSignature)
  return measures.reduce(
    (sum, _, i) => sum + getMeasureCap(i, measures.length, normalCap, anacruisTicks),
    0
  )
}
