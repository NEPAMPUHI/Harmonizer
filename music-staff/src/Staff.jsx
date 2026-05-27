import { useEffect, useRef, useState } from 'react'
import {
  Renderer, Stave, StaveNote, Voice, Formatter, Beam, Tuplet,
  StaveConnector, BarlineType, Accidental, StaveTie, Dot,
} from 'vexflow'
import { measureCapacity, usedTicks, noteTicks, NOTE_TICKS, fillRests } from './capacity'
import { KEY_ACC_SIGNED, getKeyAccidentals, getEffectiveSemitones } from './pitchUtils'

// ── Layout constants ────────────────────────────────────────────
const STAVE_X  = 30
const NEXT_W   = 280
const TREBLE_Y = 20
const BASS_Y   = 92
const ROW_H    = 200

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
const NOTE_LABELS = { c: 'До', d: 'Ре', e: 'Мі', f: 'Фа', g: 'Соль', a: 'Ля', b: 'Сі' }

function intPosToNote(intPos, clef) {
  const total    = BASE_TOTAL[clef] + intPos
  const octave   = Math.floor(total / 7)
  const pitchIdx = ((total % 7) + 7) % 7
  return { pitch: DIATONIC[pitchIdx], octave }
}

// ── Rest positioning ─────────────────────────────────────────────
function getRestKey(duration, clef) {
  if (clef === 'bass') return duration === 'w' ? 'd/3' : 'b/2'
  return duration === 'w' ? 'd/5' : 'b/4'
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

// ── VexFlow helpers ─────────────────────────────────────────────
function parseTimeSig(ts) {
  const [n, d] = ts.split('/').map(Number)
  return { num_beats: n, beat_value: d }
}

function createVoice(notes, timeSig, clef = 'treble', stemDirs = null) {
  if (!notes.length) return null
  const { num_beats, beat_value } = parseTimeSig(timeSig)
  const vexNotes = notes.map((n, i) => {
    const baseDur = n.duration + (n.dotted ? 'd' : '')
    if (n.isRest) {
      const rest = new StaveNote({ keys: [getRestKey(n.duration, clef)], duration: baseDur + 'r', clef })
      if (n.dotted) rest.addModifier(new Dot(), 0)
      return rest
    }
    const dir = stemDirs?.[i] ?? 1
    const opts = { keys: [`${n.pitch}/${n.octave}`], duration: baseDur, clef, stemDirection: dir }
    const sn = new StaveNote(opts)
    sn.setStemDirection(dir)
    if (n.accidental) sn.addModifier(new Accidental(n.accidental), 0)
    if (n.dotted) sn.addModifier(new Dot(), 0)
    return sn
  })
  const voice = new Voice({ num_beats, beat_value })
  voice.setStrict(false)
  voice.addTickables(vexNotes)
  return { voice, vexNotes }
}

// ── Hit detection threshold (screen px) ─────────────────────────
const HIT_PX = 20

// ── Component ───────────────────────────────────────────────────
export default function Staff({
  measures, timeSignature, keySignature, drag, onDrop, anacruisTicks = 0,
  selectedNoteId, onSelectNote, onSetNotePitch, showBass = true, singleClef = 'treble',
  isTriplet = false, tripletCount = 0, pendingTriplet = null,
  onNoteDragStart,
}) {
  const canvasRef  = useRef(null)
  const wrapperRef = useRef(null)
  const stavesRef  = useRef([])
  const canvasWRef = useRef(0)
  const notePositionsRef   = useRef([])   // { id, svgX, svgY, pitch, octave } after each render
  const noteDragRef        = useRef(null) // { id, startClientY, origPitchIdx, origOctave }
  const noteDragStartedRef = useRef(false)

  const [preview,         setPreview]         = useState(null)
  const [highlightCoords, setHighlightCoords] = useState(null)
  const [vexRenderCount,  setVexRenderCount]  = useState(0)
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

    const rowH = showBass ? ROW_H : 120
    const renderer = new Renderer(el, Renderer.Backends.SVG)
    renderer.resize(CANVAS_W, rows.length * rowH + 30)
    const ctx = renderer.getContext()
    ctx.setFont('Arial', 10)

    const tieNotes = { treble: [], bass: [] }

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
          bass = new Stave(curX, rowY + BASS_Y, mW)
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
            { measureIdx: globalIdx, clef: 'treble', staveX: curX, staveW: mW, staveY: rowY + TREBLE_Y },
            { measureIdx: globalIdx, clef: 'bass',   staveX: curX, staveW: mW, staveY: rowY + BASS_Y   },
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
            { measureIdx: globalIdx, clef: singleClef, staveX: curX, staveW: mW, staveY: rowY + TREBLE_Y },
          )
        }

        const mCap = anacruisTicks > 0 && globalIdx === 0
          ? anacruisTicks
          : anacruisTicks > 0 && globalIdx === measures.length - 1 && measures.length > 1
            ? normalCap - anacruisTicks
            : normalCap

        const dTreble = fillRests(showBass ? measure.treble : measure[singleClef], mCap, timeSignature)
        const dBass   = showBass ? fillRests(measure.bass, mCap, timeSignature) : []

        const isPickupMeasure = anacruisTicks > 0 && globalIdx === 0
        const tClef      = showBass ? 'treble' : singleClef
        const trebleDirs = precomputeStemDirs(dTreble, timeSignature, tClef, isPickupMeasure, mCap)
        const bassDirs   = showBass ? precomputeStemDirs(dBass, timeSignature, 'bass', isPickupMeasure, mCap) : null

        const tv = createVoice(dTreble, timeSignature, tClef, trebleDirs)
        const bv = showBass ? createVoice(dBass, timeSignature, 'bass', bassDirs) : null

        // ── Tuplets — must be created BEFORE Formatter.format() so VexFlow
        //    adjusts tick contexts for the 3-in-2 ratio during spacing.
        const mTuplets = []
        const collectTuplets = (notes, vexNotes) => {
          if (!notes || !vexNotes) return
          const byGroup = {}
          notes.forEach((n, i) => {
            if (n.triplet && n.tripletGroup != null) {
              ;(byGroup[n.tripletGroup] ??= []).push(vexNotes[i])
            }
          })
          // Accept groups of 2 or 3 (mixed-duration triplets have 2 VexFlow notes).
          // Always pass num_notes:3 so VexFlow shows "3" and applies the 3:2 tick ratio.
          Object.values(byGroup).forEach(grp => {
            if (grp.length >= 2) mTuplets.push(new Tuplet(grp, { num_notes: 3 }))
          })
        }
        collectTuplets(dTreble, tv?.vexNotes)
        collectTuplets(dBass,   bv?.vexNotes)

        const allVoices = [tv, bv].filter(Boolean).map(x => x.voice)
        if (allVoices.length) {
          // Reserve RIGHT_PAD so the last note's head never touches the barline.
          // getNoteEndX() doesn't account for the manually-drawn StaveConnector,
          // and VexFlow's last TickContext.getWidth() can be only 2–3 px for dense
          // 16th-note passages — the note head itself adds ~4–5 px more to the right.
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
              const hasRealNote = [...dTreble, ...dBass].some(n => !n.isRest)
              if (!hasRealNote) ctx.setX(availW / 2 - ctx.getWidth() / 2)
            } else {
              const leftPad = map[list[0]].getX()
              const shift = Math.floor(leftPad / 2)
              list.forEach(tick => map[tick].setX(map[tick].getX() - shift))
            }
          }
        }

        const tvBeams = tv ? computeBeams(dTreble, tv.vexNotes, timeSignature, isPickupMeasure, mCap) : []
        const bvBeams = bv ? computeBeams(dBass,   bv.vexNotes, timeSignature, isPickupMeasure, mCap) : []

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
          mainNotes.forEach((n, i) => {
            const vn = tv.vexNotes[i]
            const ys = vn.getYs()
            if (!ys?.length) return
            if (!n.isRest) {
              notePositionsRef.current.push({
                id: n.id, svgX: vn.getAbsoluteX(), svgY: ys[0],
                pitch: n.pitch, octave: n.octave,
              })
            } else if (!n.isTripletPlaceholder) {
              notePositionsRef.current.push({
                id: n.id, svgX: vn.getAbsoluteX(), svgY: ys[0],
                isRest: true,
              })
            }
          })
        }
        if (bv) {
          measure.bass.forEach((n, i) => tieNotes.bass.push({
            vexNote: bv.vexNotes[i], note: n,
            measureIdx: globalIdx, noteIdxInMeasure: i, measureNotes: measure.bass,
            rowIdx, stave: bass,
          }))
          bv.voice.draw(ctx, bass)
          bvBeams.forEach(b => b.setContext(ctx).draw())
          measure.bass.forEach((n, i) => {
            const vn = bv.vexNotes[i]
            const ys = vn.getYs()
            if (!ys?.length) return
            if (!n.isRest) {
              notePositionsRef.current.push({
                id: n.id, svgX: vn.getAbsoluteX(), svgY: ys[0],
                pitch: n.pitch, octave: n.octave,
              })
            } else if (!n.isTripletPlaceholder) {
              notePositionsRef.current.push({
                id: n.id, svgX: vn.getAbsoluteX(), svgY: ys[0],
                isRest: true,
              })
            }
          })
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

    setVexRenderCount(c => c + 1)
  }, [measures, timeSignature, keySignature, anacruisTicks, showBass, singleClef, containerW])

  // ── Update highlight position after render or selection change ──
  useEffect(() => {
    if (!selectedNoteId) { setHighlightCoords(null); return }
    const pos = notePositionsRef.current.find(p => p.id === selectedNoteId)
    if (!pos) { setHighlightCoords(null); return }
    const svgEl  = canvasRef.current?.querySelector('svg')
    const wrapEl = wrapperRef.current
    if (!svgEl || !wrapEl) { setHighlightCoords(null); return }
    const svgRect  = svgEl.getBoundingClientRect()
    const wrapRect = wrapEl.getBoundingClientRect()
    const scale    = svgRect.width / canvasWRef.current
    setHighlightCoords({
      x: svgRect.left - wrapRect.left + pos.svgX * scale,
      y: svgRect.top  - wrapRect.top  + pos.svgY * scale,
    })
  }, [selectedNoteId, vexRenderCount])

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
    if (!nearest || minDist > 75) return null

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
      // Starting a new group: the full group (2×base) must fit
      if (used + base * 2 > cap) return null
    } else {
      if (used + base * (drag.dotted ? 1.5 : 1) > cap) return null
    }

    const bottomY    = nearest.staveY + BOTTOM_LINE_OFFSET
    const intPos     = Math.round((bottomY - svgY) / HALF_STEP_PX)
    const clampedPos = Math.max(-4, Math.min(12, intPos))

    const { pitch, octave } = intPosToNote(clampedPos, nearest.clef)

    const headSvgY    = bottomY - clampedPos * HALF_STEP_PX
    const headScreenY = svgRect.top + headSvgY * scale

    return {
      measureIdx: nearest.measureIdx,
      clef: nearest.clef,
      pitch, octave,
      localX: clientX    - wrapRect.left,
      localY: headScreenY - wrapRect.top,
    }
  }

  // ── Find nearest note to a screen coordinate ─────────────────────
  function findNearestNote(clientX, clientY) {
    const svgEl = canvasRef.current?.querySelector('svg')
    if (!svgEl || !notePositionsRef.current.length) return null
    const svgRect = svgEl.getBoundingClientRect()
    const scale   = svgRect.width / canvasWRef.current

    let nearest = null, minDist = Infinity
    for (const p of notePositionsRef.current) {
      const sx = svgRect.left + p.svgX * scale
      const sy = svgRect.top  + p.svgY * scale
      const dist = Math.sqrt((clientX - sx) ** 2 + (clientY - sy) ** 2)
      if (dist < minDist) { minDist = dist; nearest = p }
    }
    return minDist <= HIT_PX ? nearest : null
  }

  // ── Mouse handlers ──────────────────────────────────────────────
  function onMouseMove(e) {
    if (drag) setPreview(computePreview(e.clientX, e.clientY))
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

  return (
    <div
      ref={wrapperRef}
      className={[
        'staff-wrapper',
        drag          ? 'staff-drop-target' : '',
        isDraggingNote ? 'staff-note-dragging' : '',
      ].filter(Boolean).join(' ')}
      onMouseMove={onMouseMove}
      onMouseLeave={onMouseLeave}
      onMouseDown={onMouseDown}
      onClick={onClick}
    >
      <div ref={canvasRef} className="staff-canvas" />

      {highlightCoords && (
        <div
          className="note-selected-highlight"
          style={{ left: highlightCoords.x, top: highlightCoords.y }}
        />
      )}

      {preview && (
        <div className="note-preview" style={{ left: preview.localX, top: preview.localY }}>
          <div className="note-preview-head" />
          <div className="note-preview-label">
            {drag?.isRest ? 'Пауза' : NOTE_LABELS[preview.pitch]}
          </div>
        </div>
      )}
    </div>
  )
}
