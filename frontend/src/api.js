/**
 * API client for the FastAPI backend.
 *
 * All functions return plain JS objects in the frontend's internal format
 * (VexFlow duration codes, lowercase pitch letters, etc.) so callers
 * never need to know about the backend's MusicXML-flavoured representation.
 */

import { getKeyAccidentals } from './pitchUtils'

const API_BASE = import.meta.env.VITE_API_BASE ?? ''

// ── Format converters (backend → frontend) ────────────────────────────────────

const DUR_TO_VEXFLOW = {
  whole:   'w',
  half:    'h',
  quarter: 'q',
  eighth:  '8',
  '16th':  '16',
}

// Numeric alter → VexFlow accidental symbol
const ALTER_TO_ACC = { '-2': 'bb', '-1': 'b', 0: 'n', 1: '#', 2: '##' }

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

// ── Async jobs API ────────────────────────────────────────────────────────────

/**
 * Enqueue a job and return { jobId }.
 */
export async function submitJob(workerRequest) {
  return post('/api/jobs', workerRequest)
}

async function _get(path) {
  const resp = await fetch(`${API_BASE}${path}`)
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({}))
    throw new Error(err.detail ?? `HTTP ${resp.status} — ${resp.statusText}`)
  }
  return resp.json()
}

export async function getJobStatus(jobId) {
  return _get(`/api/jobs/${jobId}/status`)
}

/**
 * Fetch the job result.
 * Returns null when the job is not yet ready (HTTP 202).
 */
export async function getJobResult(jobId) {
  const resp = await fetch(`${API_BASE}/api/jobs/${jobId}/result`)
  if (resp.status === 202) return null
  if (!resp.ok) {
    const err = await resp.json().catch(() => ({}))
    throw new Error(err.detail ?? `HTTP ${resp.status} — ${resp.statusText}`)
  }
  return resp.json()
}

/**
 * Poll /status every 1 000 ms until done or error, then fetch the result.
 * Throws if the job does not complete within 65 seconds.
 */
export async function pollJobResult(jobId) {
  const INTERVAL_MS = 1000
  const MAX_WAIT_MS = 65_000
  const deadline    = Date.now() + MAX_WAIT_MS

  while (Date.now() < deadline) {
    await new Promise(r => setTimeout(r, INTERVAL_MS))
    const { status } = await getJobStatus(jobId)
    if (status === 'done' || status === 'error') {
      return getJobResult(jobId)
    }
  }
  throw new Error('Час очікування результату вичерпано')
}

// ── Worker result → frontend variant converter ────────────────────────────────

/**
 * Convert a single worker FrontendNote to a Staff-compatible note object.
 * keyAcc: map { pitchLetter → alter } from getKeyAccidentals().
 */
function workerNoteToStaff(wn, voice, stemDir, positionTick, keyAcc, id) {
  const pitch    = wn.step.toLowerCase()
  const keyAlter = keyAcc[pitch] ?? 0
  const note = {
    id,
    pitch,
    octave:       wn.octave,
    duration:     DUR_TO_VEXFLOW[wn.durationType] ?? 'q',
    dotted:       wn.dotted ?? false,
    isRest:       false,
    voice,
    stemDir,
    positionTick,
  }
  // Show an explicit accidental only when it deviates from the key signature.
  if (wn.alter !== keyAlter) note.accidental = ALTER_TO_ACC[wn.alter]
  return note
}

/**
 * Convert a worker measure (with voices.soprano/alto/tenor/bass arrays)
 * into the Staff-compatible { treble, bass, isPickup? } shape.
 */
function workerMeasureToStaff(wMeasure, variantId, keyAcc) {
  const convertVoice = (notes, voice, stemDir, prefix) => {
    let cursor = 0
    return notes.map((wn, idx) => {
      const tick = cursor
      cursor += wn.durationSixteenths
      return workerNoteToStaff(wn, voice, stemDir, tick, keyAcc,
        `${variantId}-m${wMeasure.number}-${prefix}${idx}`)
    })
  }

  const soprano = convertVoice(wMeasure.voices.soprano, 'soprano',  1, 's')
  const alto    = convertVoice(wMeasure.voices.alto,    'alto',    -1, 'a')
  const tenor   = convertVoice(wMeasure.voices.tenor,  'tenor',    1, 't')
  const bassV   = convertVoice(wMeasure.voices.bass,    'bass',    -1, 'b')

  // Sort treble and bass by positionTick; within same tick soprano before alto,
  // tenor before bass (stem-up before stem-down for consistent VexFlow layout).
  const byTick = (a, b) => a.positionTick - b.positionTick || b.stemDir - a.stemDir

  const measure = {
    treble: [...soprano, ...alto].sort(byTick),
    bass:   [...tenor,   ...bassV].sort(byTick),
  }
  if (wMeasure.number === 0) measure.isPickup = true
  if (Array.isArray(wMeasure.chordNames) && wMeasure.chordNames.length)
    measure.chordNames = wMeasure.chordNames
  if (Array.isArray(wMeasure.chordTicks) && wMeasure.chordTicks.length)
    measure.chordTicks = wMeasure.chordTicks
  if (Array.isArray(wMeasure.chordLabelVisible))
    measure.chordLabelVisible = wMeasure.chordLabelVisible
  return measure
}

/**
 * Convert a complete worker job result into the harmonizeVariants format
 * expected by Home.jsx / Staff.jsx.
 *
 * @param {object} data     — raw JSON from /api/worker/submit
 * @param {object} tonality — frontend tonality object (needs .acc for key sig)
 * @returns {Array} variants, each: { id, score, musicXml, measures }
 */
export function workerResultToVariants(data, tonality) {
  const keyAcc = getKeyAccidentals(tonality?.acc)
  return (data.results ?? []).map(r => ({
    id:       r.variantId,
    score:    r.score,
    musicXml: r.musicXml,
    measures: (r.measures ?? []).map(m => workerMeasureToStaff(m, r.variantId, keyAcc)),
  }))
}
