import { useEffect } from 'react'

// ── SVG helpers ────────────────────────────────────────────────────────────────

function svgEl(tag, attrs) {
  const el = document.createElementNS('http://www.w3.org/2000/svg', tag)
  Object.entries(attrs).forEach(([k, v]) => el.setAttribute(k, String(v)))
  return el
}

function addLine(parent, x1, y1, x2, y2, color, width = 1.5) {
  parent.appendChild(svgEl('line', {
    x1: Math.round(x1), y1: Math.round(y1),
    x2: Math.round(x2), y2: Math.round(y2),
    stroke: color, 'stroke-width': width, 'stroke-linecap': 'round',
  }))
}

function addText(parent, x, y, text, color, size = 10, bold = true) {
  const el = svgEl('text', {
    x: Math.round(x), y: Math.round(y),
    fill: color, stroke: 'none', 'font-size': size,
    'text-anchor': 'middle', 'font-family': 'Arial, sans-serif', 'font-weight': bold ? 'bold' : 'normal',
  })
  el.textContent = text
  parent.appendChild(el)
}

// Bracket constants
const BASE_BRACKET_RADIUS = 14   // horizontal bow of the slur curve
const BASE_GAP   = 8             // gap between rightmost notehead and first bracket
const STACK_GAP  = 18            // horizontal spacing between stacked brackets

// Single smooth cubic slur curve from top to bottom, bowing rightward
function createVerticalSlurPath(x, y1, y2, radius) {
  const top    = Math.min(y1, y2)
  const bottom = Math.max(y1, y2)
  return `M ${x} ${top} C ${x + radius} ${top + 4}, ${x + radius} ${bottom - 4}, ${x} ${bottom}`
}

// Cubic Bezier arc between two points; positive sag → bows downward, negative → upward
function createHorizontalBowPath(x1, y1, x2, y2, sag) {
  return `M ${x1} ${y1} C ${x1} ${y1 + sag} ${x2} ${y2 + sag} ${x2} ${y2}`
}

function vk(voice) { return voice.toLowerCase() }

// Returns the anchor point on the given side of a notehead bounding box.
// side: 'left' | 'right' | 'center'
function getNoteAnchor(box, side) {
  if (side === 'left')  return { x: box.left,   y: box.centerY }
  if (side === 'right') return { x: box.right,  y: box.centerY }
  return { x: box.centerX, y: box.centerY }
}

// Line with <title> for hover tooltip. pointer-events: visibleStroke makes the thin line hoverable.
function addLineWithTitle(parent, x1, y1, x2, y2, color, width, titleText) {
  const line = svgEl('line', {
    x1: Math.round(x1), y1: Math.round(y1),
    x2: Math.round(x2), y2: Math.round(y2),
    stroke: color, 'stroke-width': width, 'stroke-linecap': 'round',
    'pointer-events': 'visibleStroke',
  })
  if (titleText) {
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title')
    title.textContent = titleText
    line.appendChild(title)
  }
  parent.appendChild(line)
}

function addPathWithTitle(parent, d, color, width, titleText) {
  const path = svgEl('path', {
    d, stroke: color, 'stroke-width': width, fill: 'none', 'stroke-linecap': 'round',
    'pointer-events': 'visibleStroke',
  })
  if (titleText) {
    const title = document.createElementNS('http://www.w3.org/2000/svg', 'title')
    title.textContent = titleText
    path.appendChild(title)
  }
  parent.appendChild(path)
  return path
}

// Localized labels for check error codes
const CHECK_ERROR_LABELS = {
  UnknownChord:                       'невідомий акорд',
  VoiceRangeViolation:                'голос виходить за допустимий діапазон',
  MoreThanOctaveBetweenAdjacentVoices:'відстань між голосами більше октави',
  AllVoicesSameDirection:             'всі голоси в одну сторону',
  VoiceCrossing:                      'перехрещення',
  ChromaticSemitoneTransfer:          'хроматична передача напівтону',
  ParallelFifths:                     'паралельні квінти',
  ParallelOctaves:                    'паралельні октави',
  ParallelOctavesOrUnisons:           'паралельні октави або унісони',
  HiddenFifths:                       'приховані квінти між крайніми голосами',
  HiddenOctaves:                      'приховані октави між крайніми голосами',
  AugmentedIntervalInBass:            'збільшений інтервал в басу',
  VoiceLeapGreaterThanOctave:         'стрибок голосу більше допустимого',
  ConsecutiveFourthsInBass:           '2 послідовні ходи по квартам в басу',
  ConsecutiveFifthsInBass:            '2 послідовні ходи по квінтам в басу',
  FunctionalProgressionError:         'порушення функціональної послідовності',
}

function getCssVar(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim()
}

// ── Renderers ──────────────────────────────────────────────────────────────────

// ChordMarker:
//   - UnknownChord (no voices): "?" above the chord
//   - VoiceRangeViolation (voices present): oval around the out-of-range notehead
function renderChordMarker(g, error, map) {
  const pos = map[error.positionIndex]
  if (!pos) return

  // VoiceRangeViolation: ellipse around each out-of-range notehead
  if (error.voices && error.voices.length > 0) {
    const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''
    error.voices.map(vk).forEach(voice => {
      const note = pos[voice]
      if (!note) return
      const el = svgEl('ellipse', {
        cx: Math.round(note.centerX),
        cy: Math.round(note.centerY),
        rx: 10, ry: 8,
        stroke: error._color, 'stroke-width': 1.5, fill: 'none',
        'pointer-events': 'visiblePainted',
      })
      if (tooltip) {
        const t = document.createElementNS('http://www.w3.org/2000/svg', 'title')
        t.textContent = tooltip
        el.appendChild(t)
      }
      g.appendChild(el)
    })
    return
  }

  // UnknownChord: "?" above the chord center
  const entries = Object.values(pos)
  if (!entries.length) return
  const x = entries.reduce((s, v) => s + v.centerX, 0) / entries.length
  const trebleYs = ['soprano', 'alto'].map(v => pos[v]?.centerY).filter(y => y != null)
  const baseY = trebleYs.length ? Math.min(...trebleYs) : entries[0].centerY
  addText(g, x, baseY - 18, '?', error._color, 22, false)
}

// Shared bracket+label renderer.
// onlySecond=true — малює дужку тільки біля nextPositionIndex (другий акорд).
function drawSlurBracketPair(g, error, map, color, { onlySecond = false } = {}) {
  const pos1 = map[error.positionIndex]
  const pos2 = map[error.nextPositionIndex]
  if (!pos1 || !pos2) return
  const voices = (error.voices || []).slice(0, 2).map(vk)
  if (voices.length < 2) return
  const [v1, v2] = voices
  if (!pos1[v1] || !pos1[v2] || !pos2[v1] || !pos2[v2]) return

  const stackIdx = error.stackIndex ?? 0
  const tooltip  = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  function withTitle(el) {
    if (tooltip) {
      const t = document.createElementNS('http://www.w3.org/2000/svg', 'title')
      t.textContent = tooltip
      el.appendChild(t)
    }
    return el
  }

  function drawBracketAt(posMap) {
    const bx  = Math.max(posMap[v1].right, posMap[v2].right) + BASE_GAP + stackIdx * STACK_GAP
    const cy1 = posMap[v1].centerY
    const cy2 = posMap[v2].centerY
    const top    = Math.min(cy1, cy2)
    const bottom = Math.max(cy1, cy2)
    const mid    = (top + bottom) / 2

    g.appendChild(withTitle(svgEl('path', {
      d: createVerticalSlurPath(bx, top, bottom, BASE_BRACKET_RADIUS),
      stroke: color, 'stroke-width': 1.5, fill: 'none', 'stroke-linecap': 'round',
      'pointer-events': 'visibleStroke',
    })))

    if (error.interval != null) {
      const labelEl = svgEl('text', {
        x: Math.round(bx + BASE_BRACKET_RADIUS - 2),
        y: Math.round(mid + 5),
        fill: color, stroke: 'none',
        'font-size': 13, 'font-weight': '700',
        'font-family': 'Arial, sans-serif', 'text-anchor': 'start',
      })
      labelEl.textContent = String(error.interval)
      g.appendChild(withTitle(labelEl))
    }
  }

  if (!onlySecond) drawBracketAt(pos1)
  drawBracketAt(pos2)
}

// VerticalBracketPair: parallel intervals — дужки біля обох акордів
function renderVerticalBracketPair(g, error, map) {
  drawSlurBracketPair(g, error, map, error._color)
}

// VerticalBracket: single-chord — дужка між двома голосами в одному акорді + ">8"
function renderVerticalBracket(g, error, map) {
  const pos = map[error.positionIndex]
  if (!pos) return
  const voices = (error.voices || []).slice(0, 2).map(vk)
  if (voices.length < 2) return
  const [v1, v2] = voices
  if (!pos[v1] || !pos[v2]) return

  const color   = error._color
  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  const bx     = Math.max(pos[v1].right, pos[v2].right) + BASE_GAP
  const cy1    = pos[v1].centerY
  const cy2    = pos[v2].centerY
  const top    = Math.min(cy1, cy2)
  const bottom = Math.max(cy1, cy2)
  const mid    = (top + bottom) / 2

  const pathEl = svgEl('path', {
    d: createVerticalSlurPath(bx, top, bottom, BASE_BRACKET_RADIUS),
    stroke: color, 'stroke-width': 1.5, fill: 'none', 'stroke-linecap': 'round',
    'pointer-events': 'visibleStroke',
  })
  if (tooltip) {
    const t = document.createElementNS('http://www.w3.org/2000/svg', 'title')
    t.textContent = tooltip
    pathEl.appendChild(t)
  }
  g.appendChild(pathEl)

  const labelEl = svgEl('text', {
    x: Math.round(bx + BASE_BRACKET_RADIUS - 2),
    y: Math.round(mid + 5),
    fill: color, stroke: 'none',
    'font-size': 13, 'font-weight': '700',
    'font-family': 'Arial, sans-serif', 'text-anchor': 'start',
  })
  labelEl.textContent = '>8'
  g.appendChild(labelEl)
}

// HiddenInterval: hidden fifths/octaves
// — дужка тільки біля другого акорду + motion lines для soprano і bass
function renderHiddenInterval(g, error, map) {
  const pos1 = map[error.positionIndex]
  const pos2 = map[error.nextPositionIndex]
  if (!pos1 || !pos2) return

  drawSlurBracketPair(g, error, map, error._color, { onlySecond: true })

  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''
  const voices  = (error.voices || []).map(vk)

  voices.forEach(voice => {
    const fromNote = pos1[voice]
    const toNote   = pos2[voice]
    if (!fromNote || !toNote) return
    const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)
    addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
  })
}

// Extra px pushed past bbox right edge at line start
const NOTEHEAD_RIGHT_OFFSET = 6

// Computes start/end anchor coords for a line from fromNote to toNote,
// connecting the nearest sides of the two noteheads.
function getMotionLineAnchors(fromNote, toNote) {
  const fromCenterX = fromNote.centerX ?? fromNote.x
  const toCenterX   = toNote.centerX   ?? toNote.x
  const goRight     = toCenterX >= fromCenterX
  const startX = (goRight ? (fromNote.right ?? fromNote.centerX) : (fromNote.left ?? fromNote.centerX))
               + (goRight ? NOTEHEAD_RIGHT_OFFSET : -NOTEHEAD_RIGHT_OFFSET)
  const startY = fromNote.centerY ?? fromNote.y
  const endX   = (toNote.centerX ?? toNote.x) + (goRight ? -2 : 2)
  const endY   = toNote.centerY ?? toNote.y
  return { startX, startY, endX, endY }
}

// MotionLines: AllVoicesSameDirection (4 voices) → connecting lines;
//             VoiceLeapGreaterThanOctave (1 voice) → arc with label
const VOICE_LEAP_SAG = 20

function renderMotionLines(g, error, map) {
  const voices = error.voices || []

  // VoiceLeapGreaterThanOctave: single voice → arc
  if (voices.length === 1) {
    const fromPos = map[error.positionIndex]
    const toPos   = map[error.nextPositionIndex]
    if (!fromPos || !toPos) return

    const voice    = vk(voices[0])
    const fromNote = fromPos[voice]
    const toNote   = toPos[voice]
    if (!fromNote || !toNote) return

    const tooltip  = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''
    const color    = error._color

    // Alto and Bass bow downward; Soprano and Tenor bow upward
    const bowsDown = voice === 'alto' || voice === 'bass'
    const sag      = bowsDown ? VOICE_LEAP_SAG : -VOICE_LEAP_SAG
    const label    = (voice === 'soprano' || voice === 'bass') ? '>8' : '>4'

    const x1 = Math.round(fromNote.centerX)
    const y1 = Math.round(fromNote.centerY)
    const x2 = Math.round(toNote.centerX)
    const y2 = Math.round(toNote.centerY)

    addPathWithTitle(g, createHorizontalBowPath(x1, y1, x2, y2, sag), color, 1.5, tooltip)

    const labelX = (x1 + x2) / 2
    const labelY = 0.5 * (y1 + y2) + 0.75 * sag + (bowsDown ? 12 : -4)
    addText(g, labelX, labelY, label, color, 13, true)
    return
  }

  // AllVoicesSameDirection: 4 voices → motion lines
  const fromPosition = map[error.positionIndex]
  const toPosition   = map[error.nextPositionIndex]
  if (!fromPosition || !toPosition) return

  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  voices.forEach(v => {
    const voice    = vk(v)
    const fromNote = fromPosition[voice]
    const toNote   = toPosition[voice]
    if (!fromNote || !toNote) return
    const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)
    addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
  })
}

// CrossingLines: two crossing lines connecting the swapped voices between two chords,
// e.g. voices=["soprano","alto"] draws soprano→alto and alto→soprano lines.
function renderCrossingLines(g, error, map) {
  const fromPosition = map[error.positionIndex]
  const toPosition   = map[error.nextPositionIndex]
  if (!fromPosition || !toPosition) return

  const voices = (error.voices || []).map(vk)
  if (voices.length !== 2) return

  const [v1, v2] = voices
  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  const pairs = [[v1, v2], [v2, v1]]
  pairs.forEach(([fromVoice, toVoice]) => {
    const fromNote = fromPosition[fromVoice]
    const toNote   = toPosition[toVoice]
    if (!fromNote || !toNote) return
    const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)
    addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
  })
}

// ChromaticTransfer: single line from voices[0] at positionIndex to voices[1] at nextPositionIndex
function renderChromaticTransfer(g, error, map) {
  const fromPosition = map[error.positionIndex]
  const toPosition   = map[error.nextPositionIndex]
  if (!fromPosition || !toPosition) return

  const voices = (error.voices || []).map(vk)
  if (voices.length < 2) return

  const fromNote = fromPosition[voices[0]]
  const toNote   = toPosition[voices[1]]
  if (!fromNote || !toNote) return

  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''
  const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)
  addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
}

// BassLineMarker:
//   - ConsecutiveFourths/FifthsInBass: 2 downward arcs across 3 chords, label "4"/"5"
//   - AugmentedIntervalInBass: 1 downward arc, label "зб."
const BASS_ARC_SAG = 20

function renderBassLineMarker(g, error, map) {
  const isConsecutive = error.code === 'ConsecutiveFourthsInBass'
                     || error.code === 'ConsecutiveFifthsInBass'
  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''
  const color   = error._color

  const pairs = isConsecutive
    ? [[error.positionIndex, error.positionIndex + 1], [error.positionIndex + 1, error.nextPositionIndex]]
    : [[error.positionIndex, error.nextPositionIndex]]

  const getLabel = () => {
    if (error.code === 'ConsecutiveFourthsInBass') return '4'
    if (error.code === 'ConsecutiveFifthsInBass')  return '5'
    return 'зб.'
  }

  pairs.forEach(([fromIdx, toIdx]) => {
    const fromBass = map[fromIdx]?.['bass']
    const toBass   = map[toIdx]?.['bass']
    if (!fromBass || !toBass) return

    const x1 = Math.round(fromBass.centerX)
    const y1 = Math.round(fromBass.centerY)
    const x2 = Math.round(toBass.centerX)
    const y2 = Math.round(toBass.centerY)

    addPathWithTitle(g, createHorizontalBowPath(x1, y1, x2, y2, BASS_ARC_SAG), color, 1.5, tooltip)

    const labelX = (x1 + x2) / 2
    const labelY = 0.5 * (y1 + y2) + 0.75 * BASS_ARC_SAG + 12
    addText(g, labelX, labelY, getLabel(), color, 13, true)
  })
}

// FunctionalRelationMarker: horizontal arrow below both chords (below C2 level)
// with a diagonal strike-through at the midpoint
const FUNC_MARKER_OFFSET = 90   // px below bass note to approximate C2 level

function renderFunctionalMarker(g, error, map) {
  const pos1 = map[error.positionIndex]
  const pos2 = map[error.nextPositionIndex]
  if (!pos1 || !pos2) return

  const avgX = (pos) => {
    const entries = Object.values(pos)
    return entries.reduce((s, v) => s + v.centerX, 0) / entries.length
  }
  const bassY = (pos) => pos['bass']?.centerY ?? Object.values(pos)[0]?.centerY ?? 0

  const x1 = Math.round(avgX(pos1))
  const x2 = Math.round(avgX(pos2))
  const y  = Math.round(Math.max(bassY(pos1), bassY(pos2)) + FUNC_MARKER_OFFSET)

  const color   = error._color
  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  // Main horizontal shaft
  addLineWithTitle(g, x1, y, x2, y, color, 1.5, tooltip)

  // Arrowhead pointing right: two short lines at ±35°
  const AH = 7
  addLine(g, x2, y, x2 - AH, y - Math.round(AH * 0.6), color, 1.5)
  addLine(g, x2, y, x2 - AH, y + Math.round(AH * 0.6), color, 1.5)

  // Diagonal strike-through at midpoint, 45 degrees
  const mx = (x1 + x2) / 2
  const D  = 7
  addLine(g, mx - D, y + D, mx + D, y - D, color, 1.5)
}

// ── Renderer dispatch ──────────────────────────────────────────────────────────

const RENDERERS = {
  ChordMarker:              renderChordMarker,
  VerticalBracket:          renderVerticalBracket,
  VerticalBracketPair:      renderVerticalBracketPair,
  HiddenInterval:           renderHiddenInterval,
  MotionLines:              renderMotionLines,
  CrossingLines:            renderCrossingLines,
  ChromaticTransfer:        renderChromaticTransfer,
  BassLineMarker:           renderBassLineMarker,
  FunctionalRelationMarker: renderFunctionalMarker,
}

// ── Component ──────────────────────────────────────────────────────────────────

export default function CheckErrorsLayer({ errors, noteLayoutMap, canvasRef, highlightedCheckErrorIndex = null }) {
  useEffect(() => {
    const svgEl = canvasRef?.current?.querySelector('svg')
    if (!svgEl) return

    svgEl.querySelector('.check-errors-overlay')?.remove()
    if (!errors?.length || !noteLayoutMap) return

    const systemRed  = getCssVar('--danger')      || '#B54855'
    const systemBlue = getCssVar('--system-blue') || 'rgb(50,100,200)'

    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g')
    g.setAttribute('class', 'check-errors-overlay')
    g.setAttribute('pointer-events', 'none')

    const pairCounters = {}
    const enrichedErrors = errors.map((error) => {
      const isHighlighted = error.__index != null && error.__index === highlightedCheckErrorIndex
      const _color = isHighlighted ? systemRed : systemBlue
      let result = { ...error, _color }
      if (error.renderType === 'VerticalBracketPair') {
        const key = `${error.positionIndex}:${error.nextPositionIndex}`
        const idx = pairCounters[key] ?? 0
        pairCounters[key] = idx + 1
        result = { ...result, stackIndex: idx }
      }
      return result
    })

    enrichedErrors.forEach(error => {
      const render = RENDERERS[error.renderType]
      if (render) render(g, error, noteLayoutMap)
    })

    svgEl.appendChild(g)
    return () => { canvasRef?.current?.querySelector('svg')?.querySelector('.check-errors-overlay')?.remove() }
  }, [errors, noteLayoutMap, canvasRef, highlightedCheckErrorIndex])

  return null
}
