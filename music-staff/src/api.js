/**
 * API client for the FastAPI backend.
 *
 * All functions return plain JS objects in the frontend's internal format
 * (VexFlow duration codes, lowercase pitch letters, etc.) so callers
 * never need to know about the backend's MusicXML-flavoured representation.
 */

const API_BASE = import.meta.env.VITE_API_BASE ?? 'http://localhost:8080'

// ── Format converters (backend → frontend) ────────────────────────────────────

const DUR_TO_VEXFLOW = {
  whole:   'w',
  half:    'h',
  quarter: 'q',
  eighth:  '8',
  '16th':  '16',
}

const ACC_TO_VEXFLOW = {
  sharp:         '#',
  flat:          'b',
  natural:       'n',
  'double-sharp': '##',
  'double-flat':  'bb',
}

/**
 * Convert a single NoteItem (backend format) to a frontend note object
 * compatible with the VexFlow Staff component.
 */
function apiNoteToFrontend(note) {
  if (note.type === 'rest') {
    return {
      id:       note.id,
      pitch:    'b',
      octave:   4,
      duration: DUR_TO_VEXFLOW[note.durationType] ?? 'q',
      dotted:   note.dotted ?? false,
      isRest:   true,
    }
  }

  const n = {
    id:       note.id,
    pitch:    note.step.toLowerCase(),
    octave:   note.octave,
    duration: DUR_TO_VEXFLOW[note.durationType] ?? 'q',
    dotted:   note.dotted ?? false,
    isRest:   false,
  }
  if (note.accidentalMark) n.accidental = ACC_TO_VEXFLOW[note.accidentalMark]
  if (note.tieStart)       n.tieAfter   = true
  return n
}

/**
 * Convert a backend Measure (with staves array) to the frontend's
 * { treble: [], bass: [], isPickup? } shape expected by Staff.jsx.
 */
function apiMeasureToFrontend(m) {
  const treble = m.staves.find(s => s.clef === 'treble')?.notes ?? []
  const bass   = m.staves.find(s => s.clef === 'bass')?.notes   ?? []
  const result = {
    treble: treble.map(apiNoteToFrontend),
    bass:   bass.map(apiNoteToFrontend),
  }
  if (m.isPickup) result.isPickup = true
  return result
}

// ── API calls ─────────────────────────────────────────────────────────────────

async function post(path, body) {
  const resp = await fetch(`${API_BASE}${path}`, {
    method:  'POST',
    headers: { 'Content-Type': 'application/json' },
    body:    JSON.stringify(body),
  })
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({}))
    throw new Error(err.detail ?? `HTTP ${resp.status} — ${resp.statusText}`)
  }
  return resp.json()
}

/**
 * Send the score to the backend and return a list of harmonization variants.
 *
 * Each variant:
 *   { id, name, description, filename, downloadUrl, measures }
 *
 * where `measures` is already in the frontend Staff-compatible format.
 */
export async function harmonizeScore(scoreJson) {
  const data = await post('/api/score/harmonize', scoreJson)

  return data.variants.map(v => ({
    id:          v.id,
    name:        v.name,
    description: v.description,
    filename:    v.filename,
    downloadUrl: `${API_BASE}${v.download_url}`,
    measures:    v.measures.map(apiMeasureToFrontend),
  }))
}

/**
 * Export the score as a MusicXML file and trigger a browser download.
 * Returns the download URL.
 */
export async function exportMusicxml(scoreJson) {
  const data = await post('/api/score/musicxml', scoreJson)
  return `${API_BASE}${data.download_url}`
}
