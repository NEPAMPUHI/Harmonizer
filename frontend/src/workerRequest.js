import { noteTicks } from './capacity'
import { getKeyAccidentals } from './pitchUtils'

// VexFlow accidental code → integer chromatic alteration (as C++ Note.alter)
const ACC_TO_ALTER = {
  '#': 1, 'b': -1, 'n': 0, '##': 2, 'bb': -2,
}

// Frontend German/mixed key notation → C++ engine key notation
const GERMAN_TO_CPP_KEY = {
  // Major keys
  'C':  'C',  'Des': 'Db', 'D':  'D',  'Es':  'Eb', 'F':  'F',
  'Fis':'F#', 'Ges': 'Gb', 'G':  'G',  'As':  'Ab', 'A':  'A',
  'B':  'Bb', 'H':  'B',
  // Minor keys
  'c':  'c',  'cis': 'c#', 'd':  'd',  'Dis': 'd#', 'es': 'eb',
  'f':  'f',  'fis': 'f#', 'g':  'g',  'gis': 'g#', 'a':  'a',
  'b':  'bb', 'h':  'b',
}

// Compute the effective chromatic alteration for a note:
// key signature applies first, then the note's own explicit accidental overrides.
function computeEffectiveAlter(note, keyAcc) {
  let alter = keyAcc[note.pitch] ?? 0
  if (note.accidental != null) alter = ACC_TO_ALTER[note.accidental] ?? 0
  return alter
}

// Serialize a single note to the format expected by the C++ JobParser::parseNote.
// Returns null for rests and triplet placeholders (not sent to the engine).
function serializeCppNote(note, keyAcc) {
  if (note.isRest || note.isTripletPlaceholder) return null
  return {
    step:               note.pitch.toUpperCase(),
    octave:             note.octave,
    alter:              computeEffectiveAlter(note, keyAcc),
    durationSixteenths: Math.round(noteTicks(note)),
  }
}

// harmonize_melody / harmonize_bass: flat array of non-rest notes from one staff.
function buildHarmonizeInput(measures, keyAcc, mode) {
  const clef  = mode === 'harmonize_bass' ? 'bass' : 'treble'
  const notes = measures
    .flatMap(m => m[clef] ?? [])
    .map(n => serializeCppNote(n, keyAcc))
    .filter(Boolean)
  return { notes }
}

// check_solution: flat array of all non-rest, non-placeholder notes from both staves,
// ordered by cumulative beat position across measures.
function buildCheckInput(measures, keyAcc) {
  const notes = []
  for (const m of measures) {
    for (const clef of ['treble', 'bass']) {
      for (const note of m[clef] ?? []) {
        if (note.deletionRest) continue
        const serialized = serializeCppNote(note, keyAcc)
        if (serialized) notes.push(serialized)
      }
    }
  }
  return { notes }
}

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * Build a worker-format request JSON for the C++ harmonization/verification engine.
 *
 * @param {'harmonize_melody'|'harmonize_bass'|'check_solution'} workerMode
 * @param {object}   opts
 * @param {Array}    opts.measures               — frontend measure array
 * @param {string}   opts.timeSignature           — '4/4', '6/8', …
 * @param {object}   opts.tonality                — { key, major, acc, … }
 * @param {number}   opts.anacruisTicks           — pickup length in 16th-note units (0 if none)
 * @param {string[]} opts.selectedModes           — active scale modes ('natural'|'harmonic'|'melodic')
 * @param {string[]} opts.selectedForbiddenRules  — forbidden rule IDs
 * @param {string[]} opts.selectedAllowedChords   — allowed chord IDs
 * @returns {object} ready for JSON serialisation and POST to /api/worker/submit
 */
export function buildWorkerRequest(workerMode, {
  measures,
  timeSignature,
  tonality,
  anacruisTicks,
  selectedModes          = [],
  selectedForbiddenRules = [],
  selectedAllowedChords  = [],
}) {
  const [beats, beatType] = timeSignature.split('/').map(Number)

  // Map German-notation key to C++ expected key string
  const cppKey = GERMAN_TO_CPP_KEY[tonality.key] ?? tonality.key

  // measureCount excludes the pickup (anacrusis) measure
  const pickupOffset = measures[0]?.isPickup ? 1 : 0
  const measureCount = measures.length - pickupOffset

  const settings = {
    key:                 cppKey,
    measureCount,
    timeSignature:       { beats, beatType },
    anacrusisSixteenths: anacruisTicks,
    forbiddenRules:      selectedForbiddenRules,
    allowedChords:       selectedAllowedChords,
  }

  // scaleMode is an array sent only for harmonization modes.
  // For major keys the only valid mode is 'natural'; for minor use the UI selection.
  if (workerMode !== 'check_solution') {
    settings.scaleMode = tonality.major ? ['natural'] : (selectedModes.length ? selectedModes : ['natural'])
  }

  const keyAcc = getKeyAccidentals(tonality.acc)
  const input  = workerMode === 'harmonize_melody' || workerMode === 'harmonize_bass'
    ? buildHarmonizeInput(measures, keyAcc, workerMode)
    : buildCheckInput(measures, keyAcc)

  const request = {
    jobId:    `job_${Date.now()}`,
    mode:     workerMode,
    settings,
    input,
  }

  // Debug: log the full worker request so every note's alter field can be inspected.
  console.log('[worker] request JSON:', JSON.stringify(request, null, 2))
  if (request.input.notes.length > 0) {
    console.log('[worker] first note sample:', request.input.notes[0])
    const missing = request.input.notes.filter(n => !Object.prototype.hasOwnProperty.call(n, 'alter'))
    if (missing.length > 0) {
      console.error('[worker] BUG: notes missing alter field:', missing)
    } else {
      console.log(`[worker] all ${request.input.notes.length} note(s) have alter ✓`)
    }
  } else {
    console.warn('[worker] input.notes is empty — no notes to harmonize')
  }

  return request
}
