import { getKeyAccidentals, getEffectiveSemitones } from './pitchUtils'

// Converts VexFlow duration codes to MusicXML type names
const DURATION_TYPE = {
  w:    'whole',
  h:    'half',
  q:    'quarter',
  '8':  'eighth',
  '16': '16th',
}

// Maps VexFlow accidental codes to MusicXML mark + chromatic alteration in semitones
const ACCIDENTAL_INFO = {
  '#':  { mark: 'sharp',        alter:  1 },
  'b':  { mark: 'flat',         alter: -1 },
  'n':  { mark: 'natural',      alter:  0 },
  '##': { mark: 'double-sharp', alter:  2 },
  'bb': { mark: 'double-flat',  alter: -2 },
}

const NOTE_TICKS_MAP = { w: 16, h: 8, q: 4, '8': 2, '16': 1 }

function noteTicks(note) {
  const base = NOTE_TICKS_MAP[note.duration] ?? 4
  if (note.triplet) return (base * 2) / 3
  return note.dotted ? base * 1.5 : base
}

/**
 * Converts the in-memory score state to a MusicXML-ready JSON object.
 *
 * Coordinate system:
 *   divisions     = 4  (sixteenth notes per quarter-note beat, matches MusicXML <divisions>)
 *   durationTicks = note length in sixteenth-note units
 *   positionTicks = onset tick from the start of the measure (0-based)
 *
 * tieStop is derived: a note receives tieStop:true when the previous note
 * in the same clef stream had tieAfter:true and the same pitch+octave.
 *
 * alter is null when there is no explicit accidental — the backend must
 * apply key-signature context to determine the actual chromatic pitch.
 *
 * @param {{ measures, timeSignature, tonality, anacruisTicks }} score
 * @returns {object} MusicXML-ready JSON
 */
export function scoreToJson({ measures, timeSignature, tonality, anacruisTicks, selectedModes }) {
  const [beats, beatType] = timeSignature.split('/').map(Number)
  const normalCap = (beats * 16) / beatType

  // Pass 1 — resolve tieStop ids across measure boundaries per clef
  // Uses effective pitch (key sig + in-measure accidentals) rather than written accidentals.
  const keyAcc     = getKeyAccidentals(tonality?.acc ?? 0)
  const tieStopIds = new Set()
  for (const clef of ['treble', 'bass']) {
    let prev      = null
    let prevNotes = null
    let prevIdx   = -1
    for (const measure of measures) {
      const notes = measure[clef] ?? []
      for (let nIdx = 0; nIdx < notes.length; nIdx++) {
        const note = notes[nIdx]
        if (prev?.tieAfter && !prev.isRest && !note.isRest) {
          const semPrev = getEffectiveSemitones(prev, prevNotes.slice(0, prevIdx), keyAcc)
          const semNote = getEffectiveSemitones(note, notes.slice(0, nIdx), keyAcc)
          if (semPrev === semNote) tieStopIds.add(note.id)
        }
        prev      = note
        prevNotes = notes
        prevIdx   = nIdx
      }
    }
  }

  // Pass 2 — build the structured measure list
  const jsonMeasures = measures.map((measure, mIdx) => {
    const isPickup = measure.isPickup === true

    const capacityTicks =
      anacruisTicks > 0 && mIdx === 0
        ? anacruisTicks
        : anacruisTicks > 0 && mIdx === measures.length - 1 && measures.length > 1
          ? normalCap - anacruisTicks
          : normalCap

    const staves = ['treble', 'bass'].map(clef => {
      let posTicks = 0

      const notes = (measure[clef] ?? []).map(note => {
        const ticks = noteTicks(note)
        const acc   = note.accidental ? ACCIDENTAL_INFO[note.accidental] : null
        const onset = posTicks
        posTicks += ticks

        if (note.isRest) {
          return {
            id:            String(note.id),
            type:          'rest',
            durationType:  DURATION_TYPE[note.duration] ?? note.duration,
            durationTicks: ticks,
            dotted:        note.dotted ?? false,
            positionTicks: onset,
          }
        }

        return {
          id:             String(note.id),
          type:           'note',
          step:           note.pitch.toUpperCase(),
          octave:         note.octave,
          alter:          acc?.alter ?? null,
          accidentalMark: acc?.mark  ?? null,
          durationType:   DURATION_TYPE[note.duration] ?? note.duration,
          durationTicks:  ticks,
          dotted:         note.dotted        ?? false,
          triplet:        note.triplet       ?? false,
          tripletGroup:   note.tripletGroup  ?? null,
          tieStart:       note.tieAfter ?? false,
          tieStop:        tieStopIds.has(note.id),
          positionTicks:  onset,
        }
      })

      return { clef, notes }
    })

    return {
      number: mIdx + 1,
      isPickup,
      capacityTicks,
      staves,
    }
  })

  return {
    divisions:     4,
    timeSignature: { beats, beatType },
    keySignature: {
      fifths: tonality.acc,
      mode:   tonality.major ? 'major' : 'minor',
      modes:  selectedModes ?? ['natural'],
    },
    anacruisTicks,
    measures: jsonMeasures,
  }
}

/**
 * Serializes the score to JSON and triggers a browser file download.
 * @param {object} score — same shape as scoreToJson's argument
 * @param {string} [filename]
 */
export function downloadScoreJson(score, filename = 'score.json') {
  const data = scoreToJson(score)
  const blob = new Blob([JSON.stringify(data, null, 2)], { type: 'application/json' })
  const url  = URL.createObjectURL(blob)
  const a    = document.createElement('a')
  a.href     = url
  a.download = filename
  a.click()
  URL.revokeObjectURL(url)
}
