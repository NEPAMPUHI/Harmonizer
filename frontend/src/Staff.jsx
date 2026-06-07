import { useEffect, useRef, useState } from 'react'
import {
  Renderer, Stave, StaveNote, GhostNote, Voice, Formatter, Beam, Tuplet,
  StaveConnector, BarlineType, Accidental, StaveTie, Dot,
} from 'vexflow'
import { measureCapacity, usedTicks, noteTicks, NOTE_TICKS, fillRests } from './capacity'
import { KEY_ACC_SIGNED, getKeyAccidentals, getEffectiveSemitones } from './pitchUtils'

// ── Layout constants ────────────────────────────────────────────
const STAVE_X  = 30
const NEXT_W   = 280
const TREBLE_Y         = 20
const BASS_Y_HARMONIZE = 162   // was 92; +70 px (2 stem lengths) vs original
const BASS_Y_CHECK     = 162   // intra-system gap treble↔bass = 102 px (3 stems); was 197 → −35 px
const ROW_H_HARMONIZE  = 200   // original row height — no two-voice stem clash in harmonize
const ROW_H_CHECK      = 305   // inter-system gap bass→treble_next = 123 px (3.5 stems); ROW_H−bassY−20

// ── Dynamic first-measure width ─────────────────────────────────
const ACC_COUNT = {
  C: 0, G: 1, D: 2, A: 3, E: 4, B: 5, 'F#': 6, 'C#': 7,
  F: 1, Bb: 2, Eb: 3, Ab: 4, Db: 5, Gb: 6, Cb: 7,
}

function firstMeasureWidth(vexKey) {
  const acc = ACC_COUNT[vexKey] ?? 0
  return 50 + acc * 16 + 35 + NEXT_W   // clef + key sig + time sig + note area
}

// ── VexFlow stave geometry (empirically verified) ───────────────
//   getYForLine(0)=top → staveY+40,  getYForLine(4)=bottom → staveY+80
const BOTTOM_LINE_OFFSET = 80   // staveY + 80 = bottom line Y in SVG
const HALF_STEP_PX       = 5    // px per diatonic step

// ── Pitch math ──────────────────────────────────────────────────
const DIATONIC   = ['c', 'd', 'e', 'f', 'g', 'a', 'b']
const BASE_TOTAL = { treble: 4 * 7 + 2, bass: 2 * 7 + 4 }  // E4, G2
const NOTE_LABELS  = { c: 'До', d: 'Ре', e: 'Мі', f: 'Фа', g: 'Соль', a: 'Ля', b: 'Сі' }
const VOICE_BADGE  = { soprano: 'S', alto: 'A', tenor: 'T', bass: 'B' }

// Allowed input range per clef (diatonic totals = octave×7 + pitch_index)
// treble: G3 (3×7+4=25) – C6 (6×7+0=42)
// bass:   C2 (2×7+0=14) – F4 (4×7+3=31)
const NOTE_RANGE = {
  treble: { min: 25, max: 42 },
  bass:   { min: 14, max: 31 },
}

function intPosToNote(intPos, clef) {
  const total    = BASE_TOTAL[clef] + intPos
  const octave   = Math.floor(total / 7)
  const pitchIdx = ((total % 7) + 7) % 7
  return { pitch: DIATONIC[pitchIdx], octave }
}

// ── Rest positioning ─────────────────────────────────────────────
function getRestKey(duration, clef) {
  if (clef === 'bass') return duration === 'w' ? 'f/3' : 'd/3'
  return duration === 'w' ? 'd/5' : 'b/4'
}

function diatonicTotalToKey(dt) {
  const pitchIdx = ((dt % 7) + 7) % 7
  const octave   = Math.floor(dt / 7)
  return `${DIATONIC[pitchIdx]}/${octave}`
}

function keyToDiatonicTotal(key) {
  const [pitch, octave] = key.split('/')
  return parseInt(octave) * 7 + DIATONIC.indexOf(pitch)
}

// Returns the VexFlow key string for lower-voice rests in check mode.
// Positions them 5 diatonic steps below the lowest pitched note in the upper voice.
// The caller clamps this against the standard position so the rest never rises above standard.
// Returns null when the upper voice has no pitched notes (use default positioning).
function computeLowerVoiceRestKey(upperNotes, clef = 'treble') {
  if (!upperNotes.length) return null
  // Include rests: use their standard rest-key position as the reference DT
  const dts = upperNotes.map(n =>
    n.isRest
      ? keyToDiatonicTotal(getRestKey(n.duration, clef))
      : noteDiatonicTotal(n.pitch, n.octave)
  )
  const minDT = Math.min(...dts)
  return diatonicTotalToKey(minDT - 5)
}

// Returns the VexFlow key string for upper-voice rests in check mode.
// Positions them 5 diatonic steps above the highest pitched note in the lower voice.
// The caller clamps this against the standard position so the rest never falls below standard.
// Returns null when the lower voice has no pitched notes (use default positioning).
function computeUpperVoiceRestKey(lowerNotes) {
  const pitched = lowerNotes.filter(n => !n.isRest)
  if (!pitched.length) return null
  const maxDT = Math.max(...pitched.map(n => noteDiatonicTotal(n.pitch, n.octave)))
  return diatonicTotalToKey(maxDT + 5)
}

// ── Adaptive measure-width estimation ───────────────────────────
// Per-duration minimum widths (note head + internal VexFlow spacing)
const DUR_MIN_W = { w: 32, h: 22, q: 16, '8': 13, '16': 11 }
const ACC_EXTRA = 14   // pixels a sharp/flat/natural occupies before the note head
const DOT_EXTRA = 7    // pixels an augmentation dot occupies after the note head
const NOTE_GAP  = 8    // minimum gap between adjacent note visual bounds

function estimateNoteAreaW(notes) {
  if (!notes.length) return 0
  let w = 0
  for (let i = 0; i < notes.length; i++) {
    const n    = notes[i]
    const base = DUR_MIN_W[n.duration] ?? 13
    w += (n.accidental ? ACC_EXTRA : 0) + base + (n.dotted ? DOT_EXTRA : 0)
    if (i < notes.length - 1) w += NOTE_GAP
  }
  return w + 8   // right padding before barline
}

// ── Beam grouping ────────────────────────────────────────────────
const BEAMABLE = new Set(['8', '16'])

function computeBeams(notes, vexNotes, timeSig, isPickup = false, totalTicks = 0) {
  const [, d] = timeSig.split('/').map(Number)
  const beatGroupTicks = d === 4 ? 4 : 6

  const beams = []
  let tick = 0
  let groupBeat = -1
  let groupIdxs = []

  const flush = () => {
    if (groupIdxs.length >= 2) beams.push(new Beam(groupIdxs.map(i => vexNotes[i])))
    groupIdxs = []
    groupBeat = -1
  }

  for (let i = 0; i < notes.length; i++) {
    const ticks = noteTicks(notes[i])
    // Round accumulated tick to avoid floating-point drift from triplet fractions
    const rt   = Math.round(tick * 10000) / 10000
    const beat = isPickup
      ? Math.floor((totalTicks - rt - 1) / beatGroupTicks)
      : Math.floor(rt / beatGroupTicks)
    if (BEAMABLE.has(notes[i].duration) && !notes[i].isRest) {
      if (beat !== groupBeat) { flush(); groupBeat = beat }
      groupIdxs.push(i)
    } else {
      flush()
    }
    tick += ticks
  }
  flush()
  return beams
}

// ── Stem direction ───────────────────────────────────────────────
// Boundary: B4 = treble 3rd line (total 34), D3 = bass 3rd line (total 22)
const STEM_BOUNDARY = { treble: 4 * 7 + 6, bass: 3 * 7 + 1 }

function noteDiatonicTotal(pitch, octave) {
  return octave * 7 + DIATONIC.indexOf(pitch)
}

function singleStemDir(pitch, octave, clef) {
  const boundary = STEM_BOUNDARY[clef] ?? STEM_BOUNDARY.treble
  return noteDiatonicTotal(pitch, octave) >= boundary ? -1 : 1
}

// Weighted displacement from boundary: notes far above outweigh notes near below
function groupStemDir(groupNotes, clef) {
  const boundary = STEM_BOUNDARY[clef] ?? STEM_BOUNDARY.treble
  const sum = groupNotes.reduce((s, n) => s + (noteDiatonicTotal(n.pitch, n.octave) - boundary), 0)
  return sum > 0 ? -1 : 1
}

function precomputeStemDirs(notes, timeSig, clef, isPickup = false, totalTicks = 0) {
  const [, d] = timeSig.split('/').map(Number)
  const beatGroupTicks = d === 4 ? 4 : 6
  const dirs = new Array(notes.length).fill(null)
  let tick = 0
  let groupBeat = -1
  let groupIdxs = []

  const flush = () => {
    if (groupIdxs.length >= 2) {
      const pitched = groupIdxs.map(i => notes[i]).filter(n => !n.isRest)
      const dir = pitched.length ? groupStemDir(pitched, clef) : 1
      groupIdxs.forEach(i => { dirs[i] = dir })
    }
    groupIdxs = []
    groupBeat = -1
  }

  for (let i = 0; i < notes.length; i++) {
    const ticks = noteTicks(notes[i])
    const rt   = Math.round(tick * 10000) / 10000
    const beat = isPickup
      ? Math.floor((totalTicks - rt - 1) / beatGroupTicks)
      : Math.floor(rt / beatGroupTicks)
    if (BEAMABLE.has(notes[i].duration) && !notes[i].isRest) {
      if (beat !== groupBeat) { flush(); groupBeat = beat }
      groupIdxs.push(i)
    } else {
      flush()
    }
    tick += ticks
  }
  flush()

  for (let i = 0; i < notes.length; i++) {
    if (dirs[i] === null && !notes[i].isRest) {
      dirs[i] = singleStemDir(notes[i].pitch, notes[i].octave, clef)
    }
  }

  return dirs
}

// ── Check-mode voice gap filler ─────────────────────────────────
// Inserts auto-rests before, between, and after user notes so the
// returned array covers exactly [0, capacity) ticks.  Each input note
// must carry a positionTick field.
const CHECK_REST_SIZES = [
  { duration: 'w',  dotted: false, ticks: 16 },
  { duration: 'h',  dotted: true,  ticks: 12 },
  { duration: 'h',  dotted: false, ticks:  8 },
  { duration: 'q',  dotted: true,  ticks:  6 },
  { duration: 'q',  dotted: false, ticks:  4 },
  { duration: '8',  dotted: true,  ticks:  3 },
  { duration: '8',  dotted: false, ticks:  2 },
  { duration: '16', dotted: false, ticks:  1 },
]

function fillCheckVoice(notes, capacity) {
  const result = []
  let cursor = 0
  let autoId = 0

  function addGap(ticks) {
    let rem = Math.round(ticks * 10000) / 10000
    for (const { duration, dotted, ticks: t } of CHECK_REST_SIZES) {
      while (rem >= t - 0.0001) {
        result.push({ duration, dotted: dotted || undefined,
          isRest: true, pitch: 'b', octave: 4, id: `cr-auto-${autoId++}` })
        rem = Math.round((rem - t) * 10000) / 10000
      }
    }
  }

  for (const note of notes) {
    const tick = note.positionTick ?? cursor
    const gap  = Math.round((tick - cursor) * 10000) / 10000
    if (gap > 0.0001) addGap(gap)
    result.push(note)
    cursor = Math.round((tick + noteTicks(note)) * 10000) / 10000
  }

  const tail = Math.round((capacity - cursor) * 10000) / 10000
  if (tail > 0.0001) addGap(tail)

  return result
}

// ── Check-mode paired voice filler ──────────────────────────────
// For a clef's two voices, distributes rests so that:
//   • The exclusive region (one voice has user content, the other doesn't)
//     gets auto-rests only in the voice that's absent.  Lower-voice rests
//     are tagged { autoRest: true } and shifted down by createVoice.
//   • The shared region (neither voice has user content yet) renders as a
//     single visible rest in the UPPER voice { sharedRest: true } at the
//     standard staff position, while the LOWER voice gets invisible
//     GhostNotes { ghostNote: true } so the formatter can still align them.
// fillCheckVoice is kept for computeFirstNoteSvgX (preview positioning).
function fillCheckVoicePair(upperRawNotes, lowerRawNotes, capacity) {
  let autoId = 0

  const calcEnd = notes => notes.reduce((max, n) => {
    const end = Math.round(((n.positionTick ?? 0) + noteTicks(n)) * 10000) / 10000
    return Math.max(max, end)
  }, 0)

  const upperEnd    = calcEnd(upperRawNotes)
  const lowerEnd    = calcEnd(lowerRawNotes)
  const sharedStart = Math.max(upperEnd, lowerEnd)
  const sharedDur   = Math.round((capacity - sharedStart) * 10000) / 10000

  function makeItems(ticks, flags) {
    const arr = []
    let rem = Math.round(ticks * 10000) / 10000
    for (const { duration, dotted, ticks: t } of CHECK_REST_SIZES) {
      while (rem >= t - 0.0001) {
        arr.push({
          duration, dotted: dotted || undefined,
          isRest: true, pitch: 'b', octave: 4,
          id: `cr-auto-${autoId++}`, ...flags,
        })
        rem = Math.round((rem - t) * 10000) / 10000
      }
    }
    return arr
  }

  function fillUserSection(notes) {
    const result = []
    let cursor = 0
    for (const note of notes) {
      const tick = note.positionTick ?? cursor
      const gap  = Math.round((tick - cursor) * 10000) / 10000
      if (gap > 0.0001) result.push(...makeItems(gap, { autoRest: true }))
      result.push(note)
      cursor = Math.round((tick + noteTicks(note)) * 10000) / 10000
    }
    return result
  }

  const dUpper = [
    ...fillUserSection(upperRawNotes),
    ...(lowerEnd > upperEnd ? makeItems(lowerEnd - upperEnd, { autoRest: true }) : []),
    ...(sharedDur > 0.0001  ? makeItems(sharedDur,          { sharedRest: true }) : []),
  ]

  const dLower = [
    ...fillUserSection(lowerRawNotes),
    ...(upperEnd > lowerEnd ? makeItems(upperEnd - lowerEnd, { autoRest: true }) : []),
    ...(sharedDur > 0.0001  ? makeItems(sharedDur,          { ghostNote: true }) : []),
  ]

  return { dUpper, dLower }
}

// Returns the SVG X where a note with the given duration would land at tick=0
// of an empty check-mode measure. Mirrors the full 4-voice format the main
// render uses (all other voices filled with auto-rests) so the returned X
// matches getAbsoluteX() pixel-exactly.
function computeFirstNoteSvgX(noteStartX, noteEndX, duration, dotted, timeSig, mCap, clef) {
  const dummy = {
    id: '__preview__', pitch: 'c', octave: 4,
    duration, dotted: dotted || undefined,
    stemDir: 1, positionTick: 0, isRest: false,
  }
  // Put the dummy note in the target clef's upper voice; all others are auto-rest.
  const tSop  = fillCheckVoice(clef === 'treble' ? [dummy] : [], mCap)
  const tAlt  = fillCheckVoice([], mCap)
  const bTen  = fillCheckVoice([], mCap)
  const bBas  = fillCheckVoice(clef === 'bass'   ? [dummy] : [], mCap)

  const tv  = createVoice(tSop, timeSig, 'treble', tSop.map(() =>  1))
  const tv2 = createVoice(tAlt, timeSig, 'treble', tAlt.map(() => -1))
  const bv  = createVoice(bTen, timeSig, 'bass',   bTen.map(() =>  1))
  const bv2 = createVoice(bBas, timeSig, 'bass',   bBas.map(() => -1))

  const allVoices = [tv, tv2, bv, bv2].filter(Boolean).map(x => x.voice)
  if (!allVoices.length) return null

  const availW = Math.max(10, noteEndX - noteStartX - 8)
  const fmt    = new Formatter()
  allVoices.forEach(v => fmt.joinVoices([v]))
  fmt.format(allVoices, availW)

  const { list, map } = fmt.getTickContexts()
  if (!list?.length) return null

  const lastCtx  = map[list[list.length - 1]]
  const maxRight = lastCtx.getX() + lastCtx.getWidth()
  if (maxRight > availW) {
    const scale = availW / maxRight
    list.forEach(t => map[t].setX(map[t].getX() * scale))
  }

  // Stave.padding is a VexFlow font metric (12px for Bravura) added by getAbsoluteX()
  // as: tc.getX() + stave.getNoteStartX() + Stave.padding.
  // Derive it from Stave's public statics to avoid hardcoding.
  const stavePadding = Stave.defaultPadding - Stave.rightPadding

  const tc0 = map[list[0]]
  if (list.length === 1) {
    return noteStartX + tc0.getX() + stavePadding
  }
  const leftPad = tc0.getX()
  const shift   = Math.floor(leftPad / 2)
  return noteStartX + tc0.getX() - shift + stavePadding
}

// ── Check-mode voice-pair overlap helpers ───────────────────────
const NOTE_HEAD_W = 10          // px to shift upper-voice note for side-by-side display
const SHARED_HEAD_DURS_SET = new Set(['q', '8', '16'])

// Returns:
//   suppressAccIds    — lower-voice note IDs whose accidental is hidden (unison: upper already shows it)
//   xShiftUpperIds    — upper-voice note ID → px to shift the notehead right (always NOTE_HEAD_W)
//   xShiftLowerIds    — lower-voice note ID → px to shift the notehead right
//   accPushLeftUpper  — upper-voice note IDs whose accidental needs an extra ACC_EXTRA leftward push
//   accPushLeftLower  — lower-voice note IDs whose accidental needs an extra ACC_EXTRA leftward push
//
// Key insight: note.setXShift() moves the notehead only. Accidentals are positioned relative to
// getAbsoluteX() which excludes the note's xShift, so accidentals stay at the tick's X position
// regardless of notehead shift. Single-accidental cases need no adjustment (the acc naturally
// sits left of both noteheads). Only both-accidentals cases need one acc pushed further left.
//
// Standard order: lower LEFT (tick X), upper RIGHT (shifted by NOTE_HEAD_W).
// Inverted order (lo has dot, hi doesn't): upper LEFT (tick X), lower RIGHT (shifted).
function computeVoicePairOverlaps(upperRaw, lowerRaw) {
  const suppressAccIds   = new Set()
  const xShiftUpperIds   = new Map()
  const xShiftLowerIds   = new Map()
  const accPushLeftUpper = new Set()
  const accPushLeftLower = new Set()

  for (const hi of upperRaw) {
    if (hi.isRest || hi.positionTick == null) continue
    for (const lo of lowerRaw) {
      if (lo.isRest || lo.positionTick == null) continue
      if (hi.positionTick !== lo.positionTick) continue
      const hiDT = noteDiatonicTotal(hi.pitch, hi.octave)
      const loDT = noteDiatonicTotal(lo.pitch, lo.octave)
      const diff  = hiDT - loDT

      if (diff === 0) {
        // Unison – suppress duplicate accidental on the lower-voice note
        if (lo.accidental) suppressAccIds.add(lo.id)
        // Decide shared-head vs. side-by-side for different durations
        if (hi.duration !== lo.duration) {
          const bothInSet = SHARED_HEAD_DURS_SET.has(hi.duration) && SHARED_HEAD_DURS_SET.has(lo.duration)
          // Side-by-side when: not both in {q,8,16}, OR both in set but exactly one is dotted
          const needSideBySide = !bothInSet || (!!hi.dotted !== !!lo.dotted)
          if (needSideBySide) {
            if (lo.dotted && !hi.dotted) {
              // Inverted: upper LEFT (tick X), lower RIGHT — lo_acc is suppressed, no overlap issue
              xShiftLowerIds.set(lo.id, NOTE_HEAD_W)
            } else {
              // Standard: lower LEFT (tick X), upper RIGHT — lo_acc is suppressed, no overlap issue
              xShiftUpperIds.set(hi.id, NOTE_HEAD_W)
            }
          }
        }
      } else if (diff === 1) {
        if (lo.dotted && !hi.dotted) {
          // Inverted: upper LEFT (tick X), lower RIGHT
          xShiftLowerIds.set(lo.id, NOTE_HEAD_W)
          // Both at tick X: only need separation when BOTH have accidentals
          if (hi.accidental && lo.accidental) accPushLeftUpper.add(hi.id)
        } else {
          // Standard: lower LEFT (tick X), upper RIGHT
          xShiftUpperIds.set(hi.id, NOTE_HEAD_W)
          // Both at tick X: only need separation when BOTH have accidentals
          if (hi.accidental && lo.accidental) accPushLeftLower.add(lo.id)
        }
      } else if (diff > 1 && diff < 7) {
        // Within octave but not adjacent: noteheads are on different lines so no head shift
        // needed, but both accidentals land at the same tick X and overlap each other.
        // Standard order: lo_acc further left, hi_acc closer to noteheads.
        if (hi.accidental && lo.accidental) accPushLeftLower.add(lo.id)
      }
    }
  }

  return { suppressAccIds, xShiftUpperIds, xShiftLowerIds, accPushLeftUpper, accPushLeftLower }
}

// ── VexFlow helpers ─────────────────────────────────────────────
function parseTimeSig(ts) {
  const [n, d] = ts.split('/').map(Number)
  return { num_beats: n, beat_value: d }
}

function createVoice(notes, timeSig, clef = 'treble', stemDirs = null, suppressAccIds = null, restKeyOverride = null, restKeyUpperOverride = null) {
  if (!notes.length) return null
  const { num_beats, beat_value } = parseTimeSig(timeSig)
  const vexNotes = notes.map((n, i) => {
    const baseDur = n.duration + (n.dotted ? 'd' : '')
    if (n.ghostNote) {
      return new GhostNote({ duration: baseDur })
    }
    if (n.isRest) {
      let restKey = getRestKey(n.duration, clef)
      if (!n.sharedRest && restKeyOverride) {
        const standardDT = keyToDiatonicTotal(restKey)
        let overrideDT   = keyToDiatonicTotal(restKeyOverride)
        // Whole and half rests look correct only on staff lines (even DT in treble/bass).
        // If the shifted position lands on a space, snap up one step to the nearest line
        // so the rest retains its correct visual appearance (hanging/sitting on a line).
        if ((n.duration === 'w' || n.duration === 'h') && overrideDT % 2 !== 0) overrideDT += 1
        if (overrideDT < standardDT) restKey = diatonicTotalToKey(overrideDT)
      }
      if (!n.sharedRest && restKeyUpperOverride) {
        const standardDT = keyToDiatonicTotal(restKey)
        let overrideDT   = keyToDiatonicTotal(restKeyUpperOverride)
        if ((n.duration === 'w' || n.duration === 'h') && overrideDT % 2 !== 0) overrideDT += 1
        if (overrideDT > standardDT) restKey = diatonicTotalToKey(overrideDT)
      }
      const rest = new StaveNote({ keys: [restKey], duration: baseDur + 'r', clef })
      if (n.dotted) rest.addModifier(new Dot(), 0)
      return rest
    }
    const dir = stemDirs?.[i] ?? 1
    const opts = { keys: [`${n.pitch}/${n.octave}`], duration: baseDur, clef, stemDirection: dir }
    const sn = new StaveNote(opts)
    sn.setStemDirection(dir)
    if (n.accidental && !suppressAccIds?.has(n.id)) sn.addModifier(new Accidental(n.accidental), 0)
    if (n.dotted) sn.addModifier(new Dot(), 0)
    return sn
  })
  const voice = new Voice({ num_beats, beat_value })
  voice.setStrict(false)
  voice.addTickables(vexNotes)
  return { voice, vexNotes }
}

// ── Two-step voice pipeline ─────────────────────────────────────
// Tuplets must be attached to StaveNotes BEFORE voice.addTickables so that
// voice.resolutionMultiplier captures the fractional denominator (3 for
// triplets). Formatter.getResolutionMultiplier then returns 3 instead of 1,
// making createContexts use integer keys that correctly align beat boundaries
// across all voices.
function buildVexNotes(notes, clef = 'treble', stemDirs = null, suppressAccIds = null, restKeyOverride = null, restKeyUpperOverride = null) {
  if (!notes || !notes.length) return null
  return notes.map((n, i) => {
    const baseDur = n.duration + (n.dotted ? 'd' : '')
    if (n.ghostNote) {
      return new GhostNote({ duration: baseDur })
    }
    if (n.isRest) {
      let restKey = getRestKey(n.duration, clef)
      if (!n.sharedRest && restKeyOverride) {
        const standardDT = keyToDiatonicTotal(restKey)
        let overrideDT   = keyToDiatonicTotal(restKeyOverride)
        if ((n.duration === 'w' || n.duration === 'h') && overrideDT % 2 !== 0) overrideDT += 1
        if (overrideDT < standardDT) restKey = diatonicTotalToKey(overrideDT)
      }
      if (!n.sharedRest && restKeyUpperOverride) {
        const standardDT = keyToDiatonicTotal(restKey)
        let overrideDT   = keyToDiatonicTotal(restKeyUpperOverride)
        if ((n.duration === 'w' || n.duration === 'h') && overrideDT % 2 !== 0) overrideDT += 1
        if (overrideDT > standardDT) restKey = diatonicTotalToKey(overrideDT)
      }
      const rest = new StaveNote({ keys: [restKey], duration: baseDur + 'r', clef })
      if (n.dotted) rest.addModifier(new Dot(), 0)
      return rest
    }
    const dir = stemDirs?.[i] ?? 1
    const opts = { keys: [`${n.pitch}/${n.octave}`], duration: baseDur, clef, stemDirection: dir }
    const sn = new StaveNote(opts)
    sn.setStemDirection(dir)
    if (n.accidental && !suppressAccIds?.has(n.id)) sn.addModifier(new Accidental(n.accidental), 0)
    if (n.dotted) sn.addModifier(new Dot(), 0)
    return sn
  })
}

function buildVoice(vexNotes, timeSig) {
  if (!vexNotes || !vexNotes.length) return null
  const { num_beats, beat_value } = parseTimeSig(timeSig)
  const voice = new Voice({ num_beats, beat_value })
  voice.setStrict(false)
  voice.addTickables(vexNotes)
  return { voice, vexNotes }
}

// ── Hit detection threshold (screen px) ─────────────────────────
const HIT_PX = 20

// ── Playback cursor ──────────────────────────────────────────────
// Returns { svgX, topSvgY, bottomSvgY } in VexFlow SVG coordinates,
// or null if the tick falls outside the rendered staves.
function computeCursorPosition(currentTick, stavesArr, measures, timeSignature, anacruisTicks) {
  if (!stavesArr.length) return null
  const normalCap = measureCapacity(timeSignature)
  let offset = 0
  for (let i = 0; i < measures.length; i++) {
    const cap = anacruisTicks > 0 && i === 0
      ? anacruisTicks
      : anacruisTicks > 0 && i === measures.length - 1 && measures.length > 1
        ? normalCap - anacruisTicks
        : normalCap
    if (currentTick <= offset + cap + 0.0001) {
      const localTick  = Math.max(0, currentTick - offset)
      const progress   = Math.min(1, localTick / cap)
      // Any stave for this measure gives the same noteStartX / noteEndX
      const refStave   = stavesArr.find(s => s.measureIdx === i)
      if (!refStave) return null
      const noteAreaW  = Math.max(1, refStave.noteEndX - refStave.noteStartX)
      const svgX       = refStave.noteStartX + progress * noteAreaW
      // Span from top-of-first-stave to bottom-of-last-stave in this measure
      const mStaves    = stavesArr.filter(s => s.measureIdx === i)
      const minY       = Math.min(...mStaves.map(s => s.staveY))
      const maxY       = Math.max(...mStaves.map(s => s.staveY))
      return { svgX, topSvgY: minY + 20, bottomSvgY: maxY + 90 }
    }
    offset += cap
  }
  return null
}

// ── Component ───────────────────────────────────────────────────
export default function Staff({
  measures, timeSignature, keySignature, drag, onDrop, anacruisTicks = 0,
  selectedNoteId, onSelectNote, onSetNotePitch, showBass = true, singleClef = 'treble',
  isTriplet = false, tripletCount = 0, pendingTriplet = null,
  onNoteDragStart, isCheckMode = false,
  currentTick = 0, totalTicks = 0, playbackState = 'idle',
}) {
  const canvasRef  = useRef(null)
  const wrapperRef = useRef(null)
  const stavesRef  = useRef([])
  const canvasWRef = useRef(0)
  const notePositionsRef   = useRef([])   // { id, svgX, svgY, pitch, octave } after each render
  const tickPositionsRef   = useRef({})   // "mIdx.clef.tick" → actual VexFlow svgX (check mode)
  const lastMousePosRef    = useRef(null) // last known mouse {x,y} for post-drop preview refresh
  const noteDragRef        = useRef(null) // { id, startClientY, origPitchIdx, origOctave }
  const cursorRef          = useRef(null)
  const noteDragStartedRef = useRef(false)

  const [preview,         setPreview]         = useState(null)
  const [isDraggingNote,  setIsDraggingNote]  = useState(false)
  const [containerW,      setContainerW]      = useState(0)

  // ── Container-width tracking (drives adaptive row layout) ──────
  useEffect(() => {
    const el = wrapperRef.current
    if (!el) return
    const notify = (w) => {
      const cw = Math.max(200, Math.floor(w))
      setContainerW(prev => prev === cw ? prev : cw)
    }
    const obs = new ResizeObserver(entries => notify(entries[0].contentRect.width))
    obs.observe(el)
    notify(el.getBoundingClientRect().width - 40)   // subtract 2×20 px padding
    return () => obs.disconnect()
  }, [])

  // ── VexFlow render ─────────────────────────────────────────────
  useEffect(() => {
    const el = canvasRef.current
    if (!el || !containerW) return
    el.innerHTML = ''
    stavesRef.current = []
    notePositionsRef.current = []
    tickPositionsRef.current = {}

    const normalCap  = measureCapacity(timeSignature)
    const hasPickup  = anacruisTicks > 0 && anacruisTicks < normalCap
    const DECO_W     = firstMeasureWidth(keySignature) - NEXT_W
    const PICKUP_NOTE_AREA = hasPickup
      ? Math.max(60, Math.round(NEXT_W * anacruisTicks / normalCap))
      : NEXT_W
    const FIRST_W    = hasPickup ? DECO_W + PICKUP_NOTE_AREA : firstMeasureWidth(keySignature)

    // ── Adaptive row packing ──────────────────────────────────────
    // Decoration width for measures that start a new row (clef + key sig, no time sig)
    const accCount   = ACC_COUNT[keySignature] ?? 0
    const DECO_W_ROW = 50 + accCount * 15
    // Available stave width per row: container content area minus left/right margins.
    // containerW is the inner content width of .staff-wrapper (no padding included).
    const AVAIL_W = Math.max(200, containerW - STAVE_X - 20)

    // Minimum note-area needed per measure (max of treble and bass)
    const noteAreaNeeded = measures.map((m, i) => {
      const cap = anacruisTicks > 0 && i === 0 ? anacruisTicks
        : anacruisTicks > 0 && i === measures.length - 1 && measures.length > 1 ? normalCap - anacruisTicks
        : normalCap
      const tNotes = fillRests(showBass ? m.treble : m[singleClef], cap, timeSignature)
      const bNotes = showBass ? fillRests(m.bass, cap, timeSignature) : []
      return Math.max(estimateNoteAreaW(tNotes), showBass ? estimateNoteAreaW(bNotes) : 0)
    })

    // Minimum stave width for measure i given its row position, capped at AVAIL_W
    const measureStaveW = (i, isFirstGlobal, isRowFirst) => {
      const na = noteAreaNeeded[i]
      let w
      if (isFirstGlobal)  w = Math.max(FIRST_W, DECO_W + na)
      else if (isRowFirst) w = DECO_W_ROW + Math.max(NEXT_W, na)
      else                 w = Math.max(NEXT_W, na)
      return Math.min(w, AVAIL_W)
    }

    // Greedy pack: add each measure to current row while it fits
    const rowPacks = []
    let rPackCur = [], rPackCurW = 0
    measures.forEach((_, i) => {
      const sw = measureStaveW(i, i === 0, rPackCur.length === 0)
      if (rPackCur.length === 0 || rPackCurW + sw <= AVAIL_W) {
        rPackCur.push({ measureIdx: i, staveW: sw })
        rPackCurW += sw
      } else {
        rowPacks.push(rPackCur)
        const sw2 = measureStaveW(i, false, true)   // recompute as row-first (already capped)
        rPackCur  = [{ measureIdx: i, staveW: sw2 }]
        rPackCurW = sw2
      }
    })
    if (rPackCur.length) rowPacks.push(rPackCur)

    // Stretch non-last rows to fill AVAIL_W proportionally
    rowPacks.forEach((rp, ri) => {
      if (ri === rowPacks.length - 1) return   // last row: don't stretch
      const used  = rp.reduce((s, e) => s + e.staveW, 0)
      const extra = AVAIL_W - used
      if (extra <= 0) return
      let given = 0
      rp.forEach((e, j) => {
        const share = j < rp.length - 1 ? Math.round(extra * e.staveW / used) : extra - given
        e.staveW += share
        given    += share
      })
    })

    // SVG canvas exactly matches the container content width — no overflow possible
    const CANVAS_W = containerW
    canvasWRef.current = CANVAS_W

    // Build rows: each entry carries the measure object, its global index, and computed width
    const rows = rowPacks.map(rp => rp.map(({ measureIdx, staveW }) => ({
      measure: measures[measureIdx], measureIdx, staveW,
    })))

    const bassY = isCheckMode ? BASS_Y_CHECK : BASS_Y_HARMONIZE
    const rowH  = showBass ? ROW_H_CHECK : 120
    const renderer = new Renderer(el, Renderer.Backends.SVG)
    renderer.resize(CANVAS_W, rows.length * rowH + 30)
    const ctx = renderer.getContext()
    ctx.setFont('Arial', 10)

    const tieNotes      = { treble: [], bass: [] }
    const checkTieNotes = { soprano: [], alto: [], tenor: [], bass: [] }

    rows.forEach((rowMeasures, rowIdx) => {
      const rowY    = rowIdx * rowH
      const tStaves = []
      const bStaves = []
      let curX = STAVE_X

      const isLastRow = rowIdx === rows.length - 1

      rowMeasures.forEach(({ measure, measureIdx: globalIdx, staveW: mW }, mIdx) => {
        const isFirstGlobal = globalIdx === 0
        const isFirstInRow  = mIdx === 0

        const isLastInRow  = mIdx === rowMeasures.length - 1
        const isLastGlobal = isLastRow && isLastInRow

        const treble = new Stave(curX, rowY + TREBLE_Y, mW)
        let bass = null

        if (showBass) {
          treble.clef = 'treble'
          bass = new Stave(curX, rowY + bassY, mW)
          bass.clef = 'bass'
          treble.setBegBarType(BarlineType.NONE)
          treble.setEndBarType(BarlineType.NONE)
          bass.setBegBarType(BarlineType.NONE)
          bass.setEndBarType(BarlineType.NONE)
          if (isFirstInRow) {
            treble.addClef('treble')
            bass.addClef('bass')
            if (keySignature !== 'C') {
              treble.addKeySignature(keySignature)
              bass.addKeySignature(keySignature)
            }
          }
          if (isFirstGlobal) {
            treble.addTimeSignature(timeSignature)
            bass.addTimeSignature(timeSignature)
          }
          treble.setContext(ctx).draw()
          bass.setContext(ctx).draw()
          tStaves.push(treble)
          bStaves.push(bass)
          stavesRef.current.push(
            { measureIdx: globalIdx, clef: 'treble', staveX: curX, staveW: mW, staveY: rowY + TREBLE_Y,
              noteStartX: treble.getNoteStartX(), noteEndX: treble.getNoteEndX() },
            { measureIdx: globalIdx, clef: 'bass',   staveX: curX, staveW: mW, staveY: rowY + bassY,
              noteStartX: bass.getNoteStartX(), noteEndX: bass.getNoteEndX() },
          )
        } else {
          treble.clef = singleClef
          treble.setBegBarType(BarlineType.NONE)
          treble.setEndBarType(isLastGlobal ? BarlineType.END : BarlineType.SINGLE)
          if (isFirstInRow) {
            treble.addClef(singleClef)
            if (keySignature !== 'C') treble.addKeySignature(keySignature)
          }
          if (isFirstGlobal) treble.addTimeSignature(timeSignature)
          treble.setContext(ctx).draw()
          tStaves.push(treble)
          stavesRef.current.push(
            { measureIdx: globalIdx, clef: singleClef, staveX: curX, staveW: mW, staveY: rowY + TREBLE_Y,
              noteStartX: treble.getNoteStartX(), noteEndX: treble.getNoteEndX() },
          )
        }

        const mCap = anacruisTicks > 0 && globalIdx === 0
          ? anacruisTicks
          : anacruisTicks > 0 && globalIdx === measures.length - 1 && measures.length > 1
            ? normalCap - anacruisTicks
            : normalCap

        const isPickupMeasure  = anacruisTicks > 0 && globalIdx === 0
        const tClef            = showBass ? 'treble' : singleClef
        const useCheckVoices   = isCheckMode && showBass

        // ── Prepare note arrays and Voice objects ─────────────────
        // Pipeline: buildVexNotes → collectTuplets → buildVoice
        // Tuplets must exist before addTickables so voice.resolutionMultiplier
        // captures denominator 3, giving Formatter the correct TickContext keys.
        let tv = null, tv2 = null, bv = null, bv2 = null
        let dSoprano, dAlto, dTenor, dBassV, dTreble, dBass
        const emptyOvl = () => ({ suppressAccIds: new Set(), xShiftUpperIds: new Map(), xShiftLowerIds: new Map(), accPushLeftUpper: new Set(), accPushLeftLower: new Set() })
        let tOvl = emptyOvl()
        let bOvl = emptyOvl()

        const mTuplets = []
        const collectTuplets = (notes, vexNotes, location) => {
          if (!notes || !vexNotes) return
          const byGroup = {}
          notes.forEach((n, i) => {
            if (n.triplet && n.tripletGroup != null) {
              ;(byGroup[n.tripletGroup] ??= []).push(vexNotes[i])
            }
          })
          Object.values(byGroup).forEach(grp => {
            if (grp.length >= 2) {
              const opts = { num_notes: 3 }
              if (location != null) opts.location = location
              mTuplets.push(new Tuplet(grp, opts))
            }
          })
        }

        if (useCheckVoices) {
          const sopranoRaw = measure.treble.filter(n => n.stemDir !== -1)
          const altoRaw    = measure.treble.filter(n => n.stemDir === -1)
          const tenorRaw   = measure.bass.filter(n => n.stemDir !== -1)
          const bassVRaw   = measure.bass.filter(n => n.stemDir === -1)

          // When both voices in a clef are fully empty (no user notes, only deletion-rests
          // or nothing at all), render a single whole rest centered in the measure —
          // identical to the harmonize-mode empty-measure look regardless of time signature.
          const isAllDR     = arr => arr.every(n => n.deletionRest)
          const trebleEmpty = isAllDR(sopranoRaw) && isAllDR(altoRaw)
          const bassEmpty   = isAllDR(tenorRaw)   && isAllDR(bassVRaw)
          const wholeRestPair = () => ({
            dUpper: [{ duration: 'w', isRest: true, pitch: 'b', octave: 4, id: 'cr-shared', sharedRest: true }],
            dLower: [{ duration: 'w', isRest: true, pitch: 'b', octave: 4, id: 'cr-ghost',  ghostNote:  true }],
          })

          const treblePair = trebleEmpty ? wholeRestPair() : fillCheckVoicePair(sopranoRaw, altoRaw, mCap)
          const bassPair   = bassEmpty   ? wholeRestPair() : fillCheckVoicePair(tenorRaw,   bassVRaw, mCap)
          dSoprano = treblePair.dUpper
          dAlto    = treblePair.dLower
          dTenor   = bassPair.dUpper
          dBassV   = bassPair.dLower

          tOvl = computeVoicePairOverlaps(sopranoRaw, altoRaw)
          bOvl = computeVoicePairOverlaps(tenorRaw,   bassVRaw)

          const altoRestKey     = computeLowerVoiceRestKey(sopranoRaw, 'treble')
          const bassVRestKey    = computeLowerVoiceRestKey(tenorRaw,   'bass')
          const sopranoRestKey  = computeUpperVoiceRestKey(altoRaw)
          const tenorRestKey    = computeUpperVoiceRestKey(bassVRaw)

          // Step 1: create VexFlow notes (no Voice yet)
          const sopVex = buildVexNotes(dSoprano, 'treble', dSoprano.map(() =>  1), null, null, sopranoRestKey)
          const altVex = buildVexNotes(dAlto,    'treble', dAlto.map(() => -1), tOvl.suppressAccIds, altoRestKey)
          const tenVex = buildVexNotes(dTenor,   'bass',   dTenor.map(() =>  1), null, null, tenorRestKey)
          const basVex = buildVexNotes(dBassV,   'bass',   dBassV.map(() => -1), bOvl.suppressAccIds, bassVRestKey)

          // Step 2: attach Tuplets — sets note.ticks = Fraction(…, 3) before addTickables
          // Upper voices: bracket above; lower voices: bracket below
          collectTuplets(dSoprano, sopVex,  Tuplet.LOCATION_TOP)
          collectTuplets(dAlto,    altVex,  Tuplet.LOCATION_BOTTOM)
          collectTuplets(dTenor,   tenVex,  Tuplet.LOCATION_TOP)
          collectTuplets(dBassV,   basVex,  Tuplet.LOCATION_BOTTOM)

          // Step 3: build Voices — addTickables now sees fractional ticks
          tv  = buildVoice(sopVex, timeSignature)
          tv2 = buildVoice(altVex, timeSignature)
          bv  = buildVoice(tenVex, timeSignature)
          bv2 = buildVoice(basVex, timeSignature)
        } else {
          dTreble = fillRests(showBass ? measure.treble : measure[singleClef], mCap, timeSignature)
          dBass   = showBass ? fillRests(measure.bass, mCap, timeSignature) : []

          const trebleDirs = precomputeStemDirs(dTreble, timeSignature, tClef, isPickupMeasure, mCap)
          const bassDirs   = showBass ? precomputeStemDirs(dBass, timeSignature, 'bass', isPickupMeasure, mCap) : null

          // Step 1: create VexFlow notes
          const trebleVex = buildVexNotes(dTreble, tClef, trebleDirs)
          const bassVex   = showBass ? buildVexNotes(dBass, 'bass', bassDirs) : null

          // Step 2: attach Tuplets
          collectTuplets(dTreble, trebleVex)
          collectTuplets(dBass,   bassVex)

          // Step 3: build Voices
          tv = buildVoice(trebleVex, timeSignature)
          bv = showBass ? buildVoice(bassVex, timeSignature) : null
        }

        // ── Format all voices together ────────────────────────────
        const allVoices = [tv, tv2, bv, bv2].filter(Boolean).map(x => x.voice)
        if (allVoices.length) {
          const RIGHT_PAD = 8
          const rawAvailW = Math.max(20, treble.getNoteEndX() - treble.getNoteStartX())
          const availW    = Math.max(10, rawAvailW - RIGHT_PAD)

          const fmt = new Formatter()
          allVoices.forEach(v => fmt.joinVoices([v]))
          fmt.format(allVoices, availW)

          const { list, map } = fmt.getTickContexts()
          if (list && list.length > 0) {
            const lastCtx = map[list[list.length - 1]]
            const maxRight = lastCtx.getX() + lastCtx.getWidth()
            if (maxRight > availW) {
              const scale = availW / maxRight
              list.forEach(tick => map[tick].setX(map[tick].getX() * scale))
            }

            if (list.length === 1) {
              const ctx = map[list[0]]
              const allNotes = useCheckVoices
                ? [...dSoprano, ...dAlto, ...dTenor, ...dBassV]
                : [...(dTreble ?? []), ...(dBass ?? [])]
              const hasRealNote = allNotes.some(n => !n.isRest)
              if (!hasRealNote) ctx.setX(availW / 2 - ctx.getWidth() / 2)
            } else {
              const leftPad = map[list[0]].getX()
              const shift = Math.floor(leftPad / 2)
              list.forEach(tick => map[tick].setX(map[tick].getX() - shift))
            }

            // ── Beat equalization ──────────────────────────────────────────
            // VexFlow Formatter distributes space by glyph complexity: a beat with
            // 4 sixteenths gets more pixels than a beat with 1 quarter. The playback
            // cursor moves linearly by tick, so this visual non-uniformity causes
            // desync. Fix: rescale every beat to the same pixel width while keeping
            // internal note proportions within each beat.
            const [, beqSigD] = timeSignature.split('/').map(Number)
            const beqBeatTicks = beqSigD === 4 ? 4 : 6
            const beqNumBeats  = Math.round(mCap / beqBeatTicks)

            if (list.length > 1 && beqNumBeats >= 2) {
              // Build ourTick (sixteenth-note ticks) → TickContext, walking all voices.
              const ourTickToTC = new Map()
              const recordVoiceTicks = (noteArr, vexArr) => {
                if (!noteArr || !vexArr) return
                let cur = 0
                for (let j = 0; j < noteArr.length; j++) {
                  const rt = Math.round(cur * 10000) / 10000
                  const tc = vexArr[j]?.tickContext
                  if (tc && !ourTickToTC.has(rt)) ourTickToTC.set(rt, tc)
                  cur = Math.round((cur + noteTicks(noteArr[j])) * 10000) / 10000
                }
              }
              if (useCheckVoices) {
                recordVoiceTicks(dSoprano, tv?.vexNotes)
                recordVoiceTicks(dAlto,    tv2?.vexNotes)
                recordVoiceTicks(dTenor,   bv?.vexNotes)
                recordVoiceTicks(dBassV,   bv2?.vexNotes)
              } else {
                recordVoiceTicks(dTreble, tv?.vexNotes)
                recordVoiceTicks(dBass,   bv?.vexNotes)
              }

              if (ourTickToTC.size > 0) {
                // Natural start X per beat = leftmost TC X in that beat's tick range.
                const beatNatStartX = new Array(beqNumBeats).fill(null)
                for (const [tick, tc] of ourTickToTC) {
                  const b = Math.min(beqNumBeats - 1, Math.floor(tick / beqBeatTicks + 1e-9))
                  const x = tc.getX()
                  if (beatNatStartX[b] === null || x < beatNatStartX[b]) beatNatStartX[b] = x
                }

                // Natural end X per beat = start of the next non-null beat, or availW.
                const beatNatEndX = beatNatStartX.map((_, b) => {
                  for (let nb = b + 1; nb < beqNumBeats; nb++) {
                    if (beatNatStartX[nb] !== null) return beatNatStartX[nb]
                  }
                  return availW
                })

                // Natural width per beat; max sets the target equalized width.
                const beatNatW = beatNatStartX.map((sx, b) =>
                  sx !== null ? Math.max(1, beatNatEndX[b] - sx) : 0
                )
                const maxNatW  = Math.max(...beatNatW)
                const anchorX  = beatNatStartX[0] ?? beatNatStartX.find(x => x !== null) ?? 0
                // Cap so beats collectively never exceed the available note area.
                const effectiveBeatW = Math.min(maxNatW, (availW - anchorX) / beqNumBeats)

                if (effectiveBeatW > 0) {
                  for (const [tick, tc] of ourTickToTC) {
                    const b        = Math.min(beqNumBeats - 1, Math.floor(tick / beqBeatTicks + 1e-9))
                    const natStart = beatNatStartX[b]
                    const natW     = beatNatW[b]
                    if (natStart === null || natW <= 0) continue
                    // Linear rescale within beat: preserve internal spacing ratios.
                    const posInBeat    = (tc.getX() - natStart) / natW
                    const newBeatStart = anchorX + b * effectiveBeatW
                    tc.setX(newBeatStart + posInBeat * effectiveBeatW)
                  }
                }
              }
            }
          }
        }

        // ── Apply x-shift for adjacent / unison note pairs ───────
        const hasAnyShift = (ovl) =>
          ovl.xShiftUpperIds.size > 0 || ovl.xShiftLowerIds.size > 0 ||
          ovl.accPushLeftUpper.size > 0 || ovl.accPushLeftLower.size > 0
        if (useCheckVoices && (hasAnyShift(tOvl) || hasAnyShift(bOvl))) {
          const idToVex = new Map()
          if (tv)  dSoprano.forEach((n, i) => { if (n.id) idToVex.set(n.id, tv.vexNotes[i]) })
          if (tv2) dAlto.forEach((n, i)    => { if (n.id) idToVex.set(n.id, tv2.vexNotes[i]) })
          if (bv)  dTenor.forEach((n, i)   => { if (n.id) idToVex.set(n.id, bv.vexNotes[i]) })
          if (bv2) dBassV.forEach((n, i)   => { if (n.id) idToVex.set(n.id, bv2.vexNotes[i]) })

          const setNoteXShift = (id, shift) => { const vn = idToVex.get(id); if (vn) vn.setXShift(shift) }
          // Adds ACC_EXTRA to the existing leftward push on the note's accidental(s).
          // Modifier.setXShift(x) stores this.xShift = -x for LEFT-position modifiers,
          // so getXShift() = -currentPush and the additive call is setXShift(-getXShift() + ACC_EXTRA).
          const pushAccLeft = (id) => {
            const vn = idToVex.get(id)
            if (!vn) return
            for (const mod of vn.getModifiers()) {
              if (mod instanceof Accidental) mod.setXShift(-mod.getXShift() + ACC_EXTRA)
            }
          }

          for (const [id, shift] of tOvl.xShiftUpperIds) setNoteXShift(id, shift)
          for (const [id, shift] of tOvl.xShiftLowerIds) setNoteXShift(id, shift)
          for (const [id, shift] of bOvl.xShiftUpperIds) setNoteXShift(id, shift)
          for (const [id, shift] of bOvl.xShiftLowerIds) setNoteXShift(id, shift)
          for (const id of tOvl.accPushLeftUpper) pushAccLeft(id)
          for (const id of tOvl.accPushLeftLower) pushAccLeft(id)
          for (const id of bOvl.accPushLeftUpper) pushAccLeft(id)
          for (const id of bOvl.accPushLeftLower) pushAccLeft(id)
          // Shift tick contexts right by ACC_EXTRA for every tick that had an accidental
          // pushed further left. This cancels the leftward overflow of the pushed accidental
          // back to its VexFlow-assigned position and moves the other accidental + noteheads
          // right by ACC_EXTRA, ensuring nothing overlaps the barline or time signature.
          const shiftedTCs = new Set()
          const addTCShift = (id) => {
            const vn = idToVex.get(id)
            if (!vn) return
            const tc = vn.tickContext
            if (tc && !shiftedTCs.has(tc)) { shiftedTCs.add(tc); tc.setX(tc.getX() + ACC_EXTRA) }
          }
          for (const id of tOvl.accPushLeftUpper) addTCShift(id)
          for (const id of tOvl.accPushLeftLower) addTCShift(id)
          for (const id of bOvl.accPushLeftUpper) addTCShift(id)
          for (const id of bOvl.accPushLeftLower) addTCShift(id)
        }

        // ── Color selected notehead + stem ───────────────────────
        if (selectedNoteId) {
          const SELECTED_COLOR = '#A63A46'
          const applyNoteStyle = (vn) => {
            if (!vn?.setKeyStyle) return
            vn.setKeyStyle(0, { fillStyle: SELECTED_COLOR, strokeStyle: SELECTED_COLOR })
            if (vn.setStemStyle) vn.setStemStyle({ fillStyle: SELECTED_COLOR, strokeStyle: SELECTED_COLOR })
            if (vn.setFlagStyle) vn.setFlagStyle({ fillStyle: SELECTED_COLOR, strokeStyle: SELECTED_COLOR })
          }
          const colorPairs = useCheckVoices
            ? [[dSoprano, tv], [dAlto, tv2], [dTenor, bv], [dBassV, bv2]]
            : [[dTreble, tv], [dBass, bv]]
          // Map each check-mode voice array to its same-stave partner for unison head coloring
          const unisonPartner = useCheckVoices ? new Map([
            [dSoprano, [dAlto, tv2]], [dAlto, [dSoprano, tv]],
            [dTenor,   [dBassV, bv2]], [dBassV, [dTenor, bv]],
          ]) : null
          for (const [arr, voice] of colorPairs) {
            if (!voice || !arr) continue
            const idx = arr.findIndex(n => n.id === selectedNoteId)
            if (idx === -1) continue
            applyNoteStyle(voice.vexNotes[idx])
            // When two voices share one notehead, color the partner's vexNote too so
            // the last-drawn voice doesn't overwrite our red color with black.
            if (unisonPartner) {
              const sel = arr[idx]
              const companion = unisonPartner.get(arr)
              if (companion && sel.positionTick != null && !sel.isRest) {
                const [cArr, cVoice] = companion
                if (cVoice) {
                  const cIdx = cArr.findIndex(n =>
                    !n.isRest && !n.ghostNote &&
                    n.positionTick === sel.positionTick &&
                    n.pitch === sel.pitch && n.octave === sel.octave
                  )
                  if (cIdx !== -1) applyNoteStyle(cVoice.vexNotes[cIdx])
                }
              }
            }
            break
          }
        }

        // ── Beams ─────────────────────────────────────────────────
        const tvBeams  = tv  ? computeBeams(useCheckVoices ? dSoprano : dTreble, tv.vexNotes,  timeSignature, isPickupMeasure, mCap) : []
        const tv2Beams = tv2 ? computeBeams(dAlto,  tv2.vexNotes, timeSignature, isPickupMeasure, mCap) : []
        const bvBeams  = bv  ? computeBeams(useCheckVoices ? dTenor : dBass, bv.vexNotes,  timeSignature, isPickupMeasure, mCap) : []
        const bv2Beams = bv2 ? computeBeams(dBassV, bv2.vexNotes, timeSignature, isPickupMeasure, mCap) : []

        // ── Draw & record note positions ─────────────────────────
        const recordNotePos = (note, vexNote, clef) => {
          if (!vexNote) return
          const ys = vexNote.getYs()
          if (!ys?.length) return
          if (!note.isRest) {
            let stemTopY = null, stemBottomY = null, stemX = null
            if (vexNote.hasStem?.()) {
              try {
                const ext = vexNote.getStemExtents()
                stemTopY    = ext.topY
                stemBottomY = ext.baseY
                stemX       = vexNote.getStemX()
              } catch (_) {}
            }
            notePositionsRef.current.push({
              id: note.id, svgX: vexNote.getAbsoluteX(), svgY: ys[0],
              pitch: note.pitch, octave: note.octave, clef,
              stemTopY, stemBottomY, stemX,
            })
          } else if (!note.isTripletPlaceholder) {
            notePositionsRef.current.push({
              id: note.id, svgX: vexNote.getAbsoluteX(), svgY: ys[0],
              isRest: true, clef,
            })
          }
        }

        if (useCheckVoices) {
          // Check mode: 4 voices — draw first, then record positions (getYs needs draw)
          const trackAndDraw = (rawNotes, dFilled, voice, beams, staveEl, clef, voiceName) => {
            if (!voice) return
            voice.voice.draw(ctx, staveEl)
            beams.forEach(b => b.setContext(ctx).draw())

            // Record user-note positions for selection hit-testing; collect for tie drawing
            rawNotes.forEach(n => {
              const idx = dFilled.findIndex(d => d.id === n.id)
              if (idx !== -1) {
                recordNotePos(n, voice.vexNotes[idx], clef)
                checkTieNotes[voiceName].push({
                  vexNote: voice.vexNotes[idx], note: n,
                  measureIdx: globalIdx, noteIdxInMeasure: idx, measureNotes: dFilled,
                  rowIdx, stave: staveEl,
                })
              }
            })

            // Record user note positions: tick → actual VexFlow X
            rawNotes.forEach(n => {
              if (n.positionTick !== undefined) {
                const key = `${globalIdx}.${clef}.${n.positionTick}`
                if (tickPositionsRef.current[key] === undefined) {
                  const idx = dFilled.findIndex(d => d.id === n.id)
                  if (idx !== -1) {
                    const ax = voice.vexNotes[idx].getAbsoluteX()
                  tickPositionsRef.current[key] = ax
                  }
                }
              }
            })

            // Also record auto-rest tick positions when the voice already has user
            // notes. Those auto-rests are laid out by the same formatter at the
            // correct tick slots, so they give the true X for the next note.
            // We skip empty voices (whole-rest placeholder is centered = wrong X).
            if (rawNotes.length > 0) {
              let cur = 0
              dFilled.forEach((n, i) => {
                const tick = Math.round(cur * 10000) / 10000
                const key  = `${globalIdx}.${clef}.${tick}`
                if (tickPositionsRef.current[key] === undefined) {
                  tickPositionsRef.current[key] = voice.vexNotes[i].getAbsoluteX()
                }
                cur = Math.round((cur + noteTicks(n)) * 10000) / 10000
              })
            }
          }
          trackAndDraw(measure.treble.filter(n => n.stemDir !== -1), dSoprano, tv,  tvBeams,  treble, 'treble', 'soprano')
          trackAndDraw(measure.treble.filter(n => n.stemDir === -1), dAlto,    tv2, tv2Beams, treble, 'treble', 'alto')
          trackAndDraw(measure.bass.filter(n => n.stemDir !== -1),   dTenor,   bv,  bvBeams,  bass,   'bass',   'tenor')
          trackAndDraw(measure.bass.filter(n => n.stemDir === -1),   dBassV,   bv2, bv2Beams, bass,   'bass',   'bass')
        } else {
          // Harmonize mode: single voice per stave, tie tracking active
          if (tv) {
            const mainNotes = showBass ? measure.treble : measure[singleClef]
            const mainClef  = showBass ? 'treble' : singleClef
            mainNotes.forEach((n, i) => tieNotes[mainClef].push({
              vexNote: tv.vexNotes[i], note: n,
              measureIdx: globalIdx, noteIdxInMeasure: i, measureNotes: mainNotes,
              rowIdx, stave: treble,
            }))
            tv.voice.draw(ctx, treble)
            tvBeams.forEach(b => b.setContext(ctx).draw())
            mainNotes.forEach((n, i) => recordNotePos(n, tv.vexNotes[i], tClef))
            // Record tick positions for preview snapping (only when there are user notes,
            // otherwise the whole-rest is centered and its X is not the insertion point)
            if (mainNotes.length > 0) {
              let tCur = 0
              ;(dTreble ?? []).forEach((n, i) => {
                const tick = Math.round(tCur * 10000) / 10000
                const key  = `${globalIdx}.${tClef}.${tick}`
                if (tickPositionsRef.current[key] === undefined) {
                  tickPositionsRef.current[key] = tv.vexNotes[i].getAbsoluteX()
                }
                tCur = Math.round((tCur + noteTicks(n)) * 10000) / 10000
              })
            }
          }
          if (bv) {
            measure.bass.forEach((n, i) => tieNotes.bass.push({
              vexNote: bv.vexNotes[i], note: n,
              measureIdx: globalIdx, noteIdxInMeasure: i, measureNotes: measure.bass,
              rowIdx, stave: bass,
            }))
            bv.voice.draw(ctx, bass)
            bvBeams.forEach(b => b.setContext(ctx).draw())
            measure.bass.forEach((n, i) => recordNotePos(n, bv.vexNotes[i], 'bass'))
          }
        }

        // Draw tuplet brackets and numerals ("3")
        mTuplets.forEach(t => t.setContext(ctx).draw())

        curX += mW
      })

      if (showBass) {
        const brace = new StaveConnector(tStaves[0], bStaves[0])
        brace.setType(StaveConnector.type.BRACE)
        brace.setContext(ctx).draw()

        const leftLine = new StaveConnector(tStaves[0], bStaves[0])
        leftLine.setType(StaveConnector.type.SINGLE_LEFT)
        leftLine.setContext(ctx).draw()

        for (let i = 0; i < tStaves.length - 1; i++) {
          const midBar = new StaveConnector(tStaves[i], bStaves[i])
          midBar.setType(StaveConnector.type.SINGLE_RIGHT)
          midBar.setContext(ctx).draw()
        }

        const last     = tStaves.length - 1
        const rightBar = new StaveConnector(tStaves[last], bStaves[last])
        rightBar.setType(
          isLastRow ? StaveConnector.type.BOLD_DOUBLE_RIGHT : StaveConnector.type.SINGLE_RIGHT
        )
        rightBar.setContext(ctx).draw()
      }
    })

    const keyAcc = getKeyAccidentals(KEY_ACC_SIGNED[keySignature] ?? 0)
    for (const clef of ['treble', 'bass']) {
      const items = tieNotes[clef]
      for (let i = 0; i < items.length - 1; i++) {
        const a = items[i]
        const b = items[i + 1]
        if (!a.note.tieAfter || a.note.isRest || b.note.isRest) continue
        const semA = getEffectiveSemitones(
          a.note, a.measureNotes.slice(0, a.noteIdxInMeasure), keyAcc)
        const semB = getEffectiveSemitones(
          b.note, b.measureNotes.slice(0, b.noteIdxInMeasure), keyAcc)
        if (semA !== semB) continue
        if (a.rowIdx === b.rowIdx) {
          new StaveTie({
            firstNote: a.vexNote, lastNote: b.vexNote,
            firstIndexes: [0], lastIndexes: [0],
          }).setContext(ctx).draw()
        } else {
          new StaveTie({
            firstNote: a.vexNote, lastNote: null,
            firstIndexes: [0], lastIndexes: [0],
          }).setContext(ctx).draw()
          new StaveTie({
            firstNote: null, lastNote: b.vexNote,
            firstIndexes: [0], lastIndexes: [0],
          }).setContext(ctx).draw()
        }
      }
    }

    // Check mode: draw ties per voice so soprano→soprano and alto→alto are handled
    // separately, and a tie never crosses to a different voice.
    // Upper voices (soprano, tenor) curve above (direction=1); lower voices curve below (-1).
    for (const [voiceName, voiceItems] of Object.entries(checkTieNotes)) {
      const tieDir = (voiceName === 'soprano' || voiceName === 'tenor') ? -1 : 1
      for (let i = 0; i < voiceItems.length - 1; i++) {
        const a = voiceItems[i]
        const b = voiceItems[i + 1]
        if (!a.note.tieAfter || a.note.isRest || b.note.isRest) continue
        const semA = getEffectiveSemitones(
          a.note, a.measureNotes.slice(0, a.noteIdxInMeasure), keyAcc)
        const semB = getEffectiveSemitones(
          b.note, b.measureNotes.slice(0, b.noteIdxInMeasure), keyAcc)
        if (semA !== semB) continue
        if (a.rowIdx === b.rowIdx) {
          new StaveTie({
            firstNote: a.vexNote, lastNote: b.vexNote,
            firstIndexes: [0], lastIndexes: [0],
          }).setDirection(tieDir).setContext(ctx).draw()
        } else {
          new StaveTie({
            firstNote: a.vexNote, lastNote: null,
            firstIndexes: [0], lastIndexes: [0],
          }).setDirection(tieDir).setContext(ctx).draw()
          new StaveTie({
            firstNote: null, lastNote: b.vexNote,
            firstIndexes: [0], lastIndexes: [0],
          }).setDirection(tieDir).setContext(ctx).draw()
        }
      }
    }

  }, [measures, timeSignature, keySignature, anacruisTicks, showBass, singleClef, containerW, selectedNoteId])

  // ── Global mouse handlers for note drag (works outside wrapper) ─
  useEffect(() => {
    function handleDocMouseMove(e) {
      const nd = noteDragRef.current
      if (!nd) return
      e.preventDefault()
      const svgEl = canvasRef.current?.querySelector('svg')
      if (!svgEl) return
      const svgRect = svgEl.getBoundingClientRect()
      const scale   = svgRect.width / canvasWRef.current
      const deltaY  = e.clientY - nd.startClientY
      const steps   = Math.round(-deltaY / (HALF_STEP_PX * scale))
      const totalIdx  = nd.origPitchIdx + nd.origOctave * 7 + steps
      const newOctave = Math.floor(totalIdx / 7)
      if (newOctave < 1 || newOctave > 8) return
      const range = NOTE_RANGE[nd.clef] ?? NOTE_RANGE.treble
      if (totalIdx < range.min || totalIdx > range.max) return
      const newPitchIdx = ((totalIdx % 7) + 7) % 7
      if (!noteDragStartedRef.current) {
        noteDragStartedRef.current = true
        onNoteDragStart?.()
      }
      onSetNotePitch(nd.id, DIATONIC[newPitchIdx], newOctave)
    }

    function handleDocMouseUp() {
      if (noteDragRef.current) {
        noteDragRef.current = null
        noteDragStartedRef.current = false
        setIsDraggingNote(false)
      }
    }

    document.addEventListener('mousemove', handleDocMouseMove)
    document.addEventListener('mouseup',   handleDocMouseUp)
    return () => {
      document.removeEventListener('mousemove', handleDocMouseMove)
      document.removeEventListener('mouseup',   handleDocMouseUp)
    }
  }, [onSetNotePitch])

  // ── Preview calculation ─────────────────────────────────────────
  function computePreview(clientX, clientY) {
    const svgEl  = canvasRef.current?.querySelector('svg')
    const wrapEl = wrapperRef.current
    if (!svgEl || !wrapEl || !drag || !stavesRef.current.length) return null

    const svgRect  = svgEl.getBoundingClientRect()
    const wrapRect = wrapEl.getBoundingClientRect()
    const scale    = svgRect.width / canvasWRef.current

    const svgX = (clientX - svgRect.left) / scale
    const svgY = (clientY - svgRect.top)  / scale

    const candidates = stavesRef.current.filter(
      s => svgX >= s.staveX && svgX <= s.staveX + s.staveW
    )
    if (!candidates.length) return null

    let nearest = null, minDist = Infinity
    for (const s of candidates) {
      const dist = Math.abs(svgY - (s.staveY + 60))
      if (dist < minDist) { minDist = dist; nearest = s }
    }
    if (!nearest || minDist > (isCheckMode ? 120 : 100)) return null

    const targetMeasure = measures[nearest.measureIdx]
    if (!targetMeasure) return null
    const used = usedTicks(targetMeasure[nearest.clef])
    const normalCap = measureCapacity(timeSignature)
    const mIdx = nearest.measureIdx
    const cap = anacruisTicks > 0 && mIdx === 0
      ? anacruisTicks
      : anacruisTicks > 0 && mIdx === measures.length - 1 && measures.length > 1
        ? normalCap - anacruisTicks
        : normalCap
    const base = NOTE_TICKS[drag.duration] ?? 2

    let targetTick   = null
    let previewVoice = null, previewStemDir = null
    let snappedLocalX = null

    if (isTriplet && pendingTriplet) {
      // Mid-group: must target the exact same measure+clef as the pending group
      if (nearest.measureIdx !== pendingTriplet.measureIdx || nearest.clef !== pendingTriplet.clef) return null
      // Duration must be a whole multiple of the reference and fit remaining units
      const noteDurTicks = NOTE_TICKS[drag.duration] ?? 2
      const refTicks     = NOTE_TICKS[pendingTriplet.referenceN] ?? 2
      if (noteDurTicks < refTicks || noteDurTicks % refTicks !== 0) return null
      if (noteDurTicks / refTicks > pendingTriplet.remainingUnits) return null
      // Space is already reserved by placeholder rests — no capacity check needed
    } else if (isTriplet) {
      // Starting a new group: the full group (2×base) must fit.
      // In check mode each voice has its own timeline, so per-voice capacity
      // is validated in the check-mode block below — skip the total-clef check.
      if (!isCheckMode && used + base * 2 > cap) return null
    } else if (!isCheckMode) {
      const effectiveDotted  = drag.duration !== '16' && drag.dotted
      const noteActualTicks  = base * (effectiveDotted ? 1.5 : 1)

      // Find the first measure (from hovered onward) that has room for this note
      let harmMeasureIdx = nearest.measureIdx
      while (harmMeasureIdx < measures.length) {
        const hmIdx = harmMeasureIdx
        const hCap  = anacruisTicks > 0 && hmIdx === 0
          ? anacruisTicks
          : anacruisTicks > 0 && hmIdx === measures.length - 1 && measures.length > 1
            ? normalCap - anacruisTicks
            : normalCap
        const hUsed = usedTicks(measures[hmIdx]?.[nearest.clef] ?? [])
        if (Math.round((hUsed + noteActualTicks) * 10000) / 10000 <= hCap) break
        harmMeasureIdx++
      }
      if (harmMeasureIdx >= measures.length) return null

      const hStave = harmMeasureIdx === nearest.measureIdx
        ? nearest
        : stavesRef.current.find(s => s.measureIdx === harmMeasureIdx && s.clef === nearest.clef)
      if (!hStave) return null

      const hCap = anacruisTicks > 0 && harmMeasureIdx === 0
        ? anacruisTicks
        : anacruisTicks > 0 && harmMeasureIdx === measures.length - 1 && measures.length > 1
          ? normalCap - anacruisTicks
          : normalCap
      const hTargetTick = Math.round(usedTicks(measures[harmMeasureIdx]?.[nearest.clef] ?? []) * 10000) / 10000
      const harmNoteAreaStart = hStave.noteStartX
      const harmNoteAreaEnd   = hStave.noteEndX

      const harmTickKey = `${harmMeasureIdx}.${nearest.clef}.${hTargetTick}`
      let harmSvgX = tickPositionsRef.current[harmTickKey]
      if (harmSvgX === undefined) {
        if (hTargetTick === 0) {
          harmSvgX = computeFirstNoteSvgX(
            harmNoteAreaStart, harmNoteAreaEnd,
            drag.duration, drag.dotted, timeSignature, hCap, nearest.clef,
          ) ?? harmNoteAreaStart
        } else {
          harmSvgX = harmNoteAreaStart + (hTargetTick / hCap) * Math.max(1, harmNoteAreaEnd - harmNoteAreaStart)
        }
      }

      snappedLocalX = svgRect.left + harmSvgX * scale - wrapRect.left
      if (harmMeasureIdx !== nearest.measureIdx) {
        nearest = { ...nearest, measureIdx: harmMeasureIdx }
      }
    }

    // ── Pitch from Y ─────────────────────────────────────────────
    const bottomY    = nearest.staveY + BOTTOM_LINE_OFFSET
    const intPos     = Math.round((bottomY - svgY) / HALF_STEP_PX)
    const minPos     = NOTE_RANGE[nearest.clef].min - BASE_TOTAL[nearest.clef]
    const maxPos     = NOTE_RANGE[nearest.clef].max - BASE_TOTAL[nearest.clef]
    const clampedPos = Math.max(minPos, Math.min(maxPos, intPos))
    let { pitch, octave } = intPosToNote(clampedPos, nearest.clef)
    let headSvgY    = bottomY - clampedPos * HALF_STEP_PX
    let headScreenY = svgRect.top + headSvgY * scale

    // ── Check mode: sequential-voice target tick + voice prediction ─
    if (isCheckMode) {
      const noteDurTicks    = NOTE_TICKS[drag.duration] ?? 4
      const effectiveDotted = drag.duration !== '16' && drag.dotted
      const noteActualTicks = effectiveDotted ? noteDurTicks * 1.5 : noteDurTicks
      const noteAreaStart   = nearest.noteStartX ?? nearest.staveX
      const noteAreaEnd     = nearest.noteEndX   ?? (nearest.staveX + nearest.staveW)

      const clefNotes       = measures[nearest.measureIdx]?.[nearest.clef] ?? []
      const upperVoiceNotes = clefNotes.filter(n => n.stemDir !== -1)
      const lowerVoiceNotes = clefNotes.filter(n => n.stemDir === -1)
      const upperV = nearest.clef === 'treble' ? 'soprano' : 'tenor'
      const lowerV = nearest.clef === 'treble' ? 'alto'    : 'bass'

      if (isTriplet && pendingTriplet) {
        // ── Mid-group triplet: snap to the next unfilled placeholder ──
        // The placeholder already has the correct positionTick; use it
        // directly so preview and actual insertion always agree.
        const firstPH = clefNotes
          .filter(n => n.tripletGroup === pendingTriplet.groupId && n.isTripletPlaceholder)
          .sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))[0]
        if (!firstPH || firstPH.positionTick == null) return null
        targetTick     = firstPH.positionTick
        previewVoice   = pendingTriplet.voice
        previewStemDir = pendingTriplet.stemDir
      } else {
        // ── Normal: find the next available slot in each voice ────────
        // Returns the first tick position where the voice has a gap (i.e. where
        // the next note should go).  Scanning from tick=0 is essential: after a
        // higher-pitched note displaces a lower one to the opposite voice, the
        // displaced notes at ticks 4/8/12 remain in this voice and the old
        // "last-note end" formula would wrongly report tick=16, skipping the
        // real gap at tick=2 (right after the newly placed eighth).
        const nextTick = voiceNotes => {
          // Deletion-rests are overwritable placeholders — treat their span as empty.
          const effective = voiceNotes.filter(n => !n.deletionRest)
          if (effective.length === 0) return 0
          const sorted = [...effective].sort((a, b) => (a.positionTick ?? 0) - (b.positionTick ?? 0))
          let cursor = 0
          for (const note of sorted) {
            const tick = Math.round((note.positionTick ?? 0) * 10000) / 10000
            if (tick > cursor + 0.0001) return cursor   // gap found
            cursor = Math.round((tick + noteTicks(note)) * 10000) / 10000
          }
          return cursor   // no gap — return end of last note
        }
        const upperNext = nextTick(upperVoiceNotes)
        const lowerNext = nextTick(lowerVoiceNotes)

        // For a new triplet group the full span (2×base) must fit in each voice;
        // for regular notes use the note's actual duration.
        const spanCheck = (isTriplet && !pendingTriplet) ? base * 2 : noteActualTicks
        const upperFull = Math.round((upperNext + spanCheck) * 10000) / 10000 > cap
        const lowerFull = Math.round((lowerNext + spanCheck) * 10000) / 10000 > cap
        if (upperFull && lowerFull) return null

        const dtFn  = (p, o) => o * 7 + DIATONIC.indexOf(p)
        const newDT = drag.isRest ? -Infinity : dtFn(pitch, octave)

        // SVG X for any tick in this measure/clef (formatter output or linear fallback)
        const getTickX = tick => {
          const key = `${nearest.measureIdx}.${nearest.clef}.${tick}`
          const x   = tickPositionsRef.current[key]
          if (x !== undefined) return x
          if (tick === 0) {
            return computeFirstNoteSvgX(
              noteAreaStart, noteAreaEnd, drag.duration, drag.dotted, timeSignature, cap, nearest.clef,
            ) ?? noteAreaStart
          }
          return noteAreaStart + (tick / cap) * Math.max(1, noteAreaEnd - noteAreaStart)
        }

        // Voice assignment for a new note at `tick` given existing notes there.
        // Returns null when the slot is empty (caller supplies the default).
        const resolveVoice = tick => {
          const atTick = clefNotes.filter(n => n.positionTick === tick && !n.isRest)
          if (atTick.length === 0) return null
          if (atTick.length === 1) {
            const existDT = dtFn(atTick[0].pitch, atTick[0].octave)
            return newDT > existDT
              ? { voice: upperV, stemDir: 1 }
              : { voice: lowerV, stemDir: -1 }
          }
          const sorted = [...atTick].sort((a, b) => dtFn(b.pitch, b.octave) - dtFn(a.pitch, a.octave))
          const hiDT = dtFn(sorted[0].pitch, sorted[0].octave)
          const loDT = dtFn(sorted[1].pitch, sorted[1].octave)
          if      (newDT >= hiDT)                 return { voice: upperV, stemDir: 1 }
          else if (newDT <= loDT)                 return { voice: lowerV, stemDir: -1 }
          else if (hiDT - newDT <= newDT - loDT) return { voice: upperV, stemDir: 1 }
          else                                    return { voice: lowerV, stemDir: -1 }
        }

        if (upperNext !== lowerNext && !upperFull && !lowerFull) {
          // ── Asynchronous voices ────────────────────────────────────
          // Fill the behind voice for any cursor position left of the ahead
          // voice's tick X.  This lets the user fill the behind voice
          // sequentially without fighting a narrow zone boundary — the behind
          // voice keeps its independent timeline regardless of the other voice's
          // note positions.  Only when the cursor moves past aheadX do we
          // switch to advancing the ahead voice to its next tick.
          //
          // Exception: when behindTick and aheadTick fall within the same beat
          // (e.g. quarter→sixteenth edit leaves aheadTick=1), aheadX is too
          // close to the measure start to serve as a reliable boundary.  Skip
          // the X-based switch and always continue the ahead voice so sub-beat
          // sequences are not interrupted by an unreachable zone.
          const behindTick   = Math.min(upperNext, lowerNext)
          const aheadTick    = Math.max(upperNext, lowerNext)
          const aheadIsUpper = upperNext > lowerNext

          const [, sigD]   = timeSignature.split('/').map(Number)
          const beatTicks  = sigD === 4 ? 4 : 6
          const inSameBeat = Math.floor(behindTick / beatTicks) === Math.floor(aheadTick / beatTicks)

          const aheadX = getTickX(aheadTick)

          if (!inSameBeat && svgX <= aheadX) {
            const vr = resolveVoice(behindTick)
            if (vr) {
              previewVoice = vr.voice; previewStemDir = vr.stemDir
            } else {
              previewVoice   = aheadIsUpper ? lowerV : upperV
              previewStemDir = aheadIsUpper ? -1 : 1
            }
            targetTick = behindTick
          } else {
            previewVoice   = aheadIsUpper ? upperV : lowerV
            previewStemDir = aheadIsUpper ? 1 : -1
            targetTick     = aheadTick
          }
        } else {
          // ── Synchronized voices (or one voice full) ────────────────
          const earlierTick = Math.min(
            upperFull ? Infinity : upperNext,
            lowerFull ? Infinity : lowerNext,
          )
          const vr = resolveVoice(earlierTick)
          if (vr) {
            previewVoice = vr.voice; previewStemDir = vr.stemDir; targetTick = earlierTick
          } else if (!upperFull && upperNext === earlierTick) {
            previewVoice = upperV; previewStemDir = 1; targetTick = upperNext
          } else {
            previewVoice = lowerV; previewStemDir = -1; targetTick = lowerNext
          }
        }
      }

      if (targetTick == null) return null

      const tickKey    = `${nearest.measureIdx}.${nearest.clef}.${targetTick}`
      const actualSvgX = tickPositionsRef.current[tickKey]
      let ovalSvgX
      if (actualSvgX !== undefined) {
        ovalSvgX = actualSvgX
      } else if (targetTick === 0) {
        ovalSvgX = computeFirstNoteSvgX(
          noteAreaStart, noteAreaEnd, drag.duration, drag.dotted, timeSignature, cap, nearest.clef,
        ) ?? noteAreaStart
      } else {
        const noteAreaW = Math.max(1, noteAreaEnd - noteAreaStart)
        ovalSvgX = noteAreaStart + (targetTick / cap) * noteAreaW
      }
      snappedLocalX = svgRect.left + ovalSvgX * scale - wrapRect.left

      // Clamp preview pitch to avoid voice crossing with concurrent opposite-voice notes.
      // Upper voice (soprano/tenor) must be >= max concurrent lower; lower must be <= min concurrent upper.
      if (previewVoice != null && targetTick != null && !drag.isRest) {
        const clefNotesAll   = measures[nearest.measureIdx]?.[nearest.clef] ?? []
        const isUpperPreview = previewVoice === upperV
        const oppDirClamp    = isUpperPreview ? -1 : 1
        const dtFnC = (p, o) => o * 7 + DIATONIC.indexOf(p)
        const concurOpp = clefNotesAll.filter(n => {
          if (n.isRest || n.stemDir !== oppDirClamp || n.positionTick == null) return false
          const nStart = n.positionTick
          const nEnd   = Math.round((nStart + noteTicks(n)) * 10000) / 10000
          return nStart < targetTick + noteActualTicks && nEnd > targetTick
        })
        if (concurOpp.length > 0) {
          const cDTs   = concurOpp.map(n => dtFnC(n.pitch, n.octave))
          let newDT    = dtFnC(pitch, octave)
          let changed  = false
          if  (isUpperPreview && newDT < Math.max(...cDTs)) { newDT = Math.max(...cDTs); changed = true }
          else if (!isUpperPreview && newDT > Math.min(...cDTs)) { newDT = Math.min(...cDTs); changed = true }
          if (changed) {
            octave   = Math.floor(newDT / 7)
            pitch    = DIATONIC[((newDT % 7) + 7) % 7]
            const newIntPos = newDT - BASE_TOTAL[nearest.clef]
            headSvgY    = bottomY - newIntPos * HALF_STEP_PX
            headScreenY = svgRect.top + headSvgY * scale
          }
        }
      }
    }

    return {
      measureIdx: nearest.measureIdx,
      clef: nearest.clef,
      pitch, octave,
      localX: clientX - wrapRect.left,
      localY: headScreenY - wrapRect.top + wrapEl.scrollTop,
      targetTick,
      snappedLocalX,
      previewVoice,
      previewStemDir,
    }
  }

  // ── Find nearest note to a screen coordinate ─────────────────────
  function findNearestNote(clientX, clientY) {
    const svgEl = canvasRef.current?.querySelector('svg')
    if (!svgEl || !notePositionsRef.current.length) return null
    const svgRect = svgEl.getBoundingClientRect()
    const scale   = svgRect.width / canvasWRef.current

    // First pass: stem proximity. Clicks within STEM_HIT_W px of a stem centerline
    // and between its endpoints select that note. This is the primary disambiguation
    // path when two voices share a notehead — click the stem to pick the right voice.
    const STEM_HIT_W = 5
    let stemNearest = null, stemMinDx = Infinity
    for (const p of notePositionsRef.current) {
      if (p.isRest || p.stemX == null) continue
      const stemSx = svgRect.left + p.stemX * scale
      const yLo    = svgRect.top  + Math.min(p.stemTopY, p.stemBottomY) * scale
      const yHi    = svgRect.top  + Math.max(p.stemTopY, p.stemBottomY) * scale
      const dx     = Math.abs(clientX - stemSx)
      if (dx <= STEM_HIT_W && clientY >= yLo - 3 && clientY <= yHi + 3) {
        if (dx < stemMinDx) { stemMinDx = dx; stemNearest = p }
      }
    }
    if (stemNearest) return stemNearest

    // Second pass: notehead proximity
    let nearest = null, minDist = Infinity
    for (const p of notePositionsRef.current) {
      const sx = svgRect.left + p.svgX * scale
      const sy = svgRect.top  + p.svgY * scale
      const dist = Math.sqrt((clientX - sx) ** 2 + (clientY - sy) ** 2)
      if (dist < minDist) { minDist = dist; nearest = p }
    }
    return minDist <= HIT_PX ? nearest : null
  }

  // ── Playback cursor ──────────────────────────────────────────────
  useEffect(() => {
    const cursorEl = cursorRef.current
    if (!cursorEl) return
    if (playbackState === 'idle' || !stavesRef.current.length) {
      cursorEl.style.display = 'none'
      return
    }
    const pos = computeCursorPosition(currentTick, stavesRef.current, measures, timeSignature, anacruisTicks)
    if (!pos) { cursorEl.style.display = 'none'; return }
    const svgEl  = canvasRef.current?.querySelector('svg')
    const wrapEl = wrapperRef.current
    if (!svgEl || !wrapEl) { cursorEl.style.display = 'none'; return }
    const svgRect  = svgEl.getBoundingClientRect()
    const wrapRect = wrapEl.getBoundingClientRect()
    const scale    = canvasWRef.current > 0 ? svgRect.width / canvasWRef.current : 1
    cursorEl.style.display = 'block'
    cursorEl.style.left    = `${Math.round(svgRect.left + pos.svgX * scale - wrapRect.left)}px`
    cursorEl.style.top     = `${Math.round(svgRect.top  + pos.topSvgY * scale - wrapRect.top + wrapEl.scrollTop)}px`
    cursorEl.style.height  = `${Math.round((pos.bottomSvgY - pos.topSvgY) * scale)}px`
  })

  // ── Mouse handlers ──────────────────────────────────────────────
  function onMouseMove(e) {
    lastMousePosRef.current = { x: e.clientX, y: e.clientY }
    if (drag) setPreview(computePreview(e.clientX, e.clientY))
  }

  function onScroll() {
    const last = lastMousePosRef.current
    if (!last || !drag) return
    setPreview(computePreview(last.x, last.y))
  }

  function onMouseLeave() { setPreview(null) }

  function onMouseDown(e) {
    if (drag) return
    const found = findNearestNote(e.clientX, e.clientY)
    if (found) {
      onSelectNote(found.id)
      if (!found.isRest) {
        noteDragRef.current = {
          id: found.id,
          clef: found.clef,
          startClientY: e.clientY,
          origPitchIdx: DIATONIC.indexOf(found.pitch),
          origOctave:   found.octave,
        }
        setIsDraggingNote(true)
      }
      e.preventDefault()
    }
  }

  function onClick(e) {
    if (drag) {
      const p = computePreview(e.clientX, e.clientY)
      if (p) onDrop(p)
      return
    }
    const found = findNearestNote(e.clientX, e.clientY)
    if (!found) onSelectNote(null)
  }

  useEffect(() => { if (!drag) setPreview(null) }, [drag])

  // After a note is placed (measures prop changes), re-compute preview at the same
  // cursor position so the indicator jumps to the next available slot immediately.
  useEffect(() => {
    if (drag && lastMousePosRef.current) {
      setPreview(computePreview(lastMousePosRef.current.x, lastMousePosRef.current.y))
    }
  }, [measures])

  return (
    <div
      ref={wrapperRef}
      className={[
        'staff-wrapper',
        drag          ? 'staff-drop-target' : '',
        isDraggingNote ? 'staff-note-dragging' : '',
      ].filter(Boolean).join(' ')}
      onMouseMove={onMouseMove}
      onScroll={onScroll}
      onMouseLeave={onMouseLeave}
      onMouseDown={onMouseDown}
      onClick={onClick}
    >
      <div ref={canvasRef} className="staff-canvas" />

      <div ref={cursorRef} className="playback-cursor" style={{ display: 'none' }} />

      {preview && (
        <div
          className={[
            'note-preview',
            preview.previewVoice != null ? 'check-preview' : '',
            preview.previewStemDir === -1 ? 'voice-lower' : '',
          ].filter(Boolean).join(' ')}
          style={{
            left: preview.snappedLocalX ?? preview.localX,
            top:  preview.localY,
          }}
        >
          <div className="note-preview-head" />
          <div className="note-preview-label">
            {drag?.isRest ? 'Пауза' : NOTE_LABELS[preview.pitch]}
            {preview.previewVoice && (
              <span className="preview-voice-badge">{VOICE_BADGE[preview.previewVoice]}</span>
            )}
          </div>
        </div>
      )}
    </div>
  )
}
