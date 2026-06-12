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

function addText(parent, x, y, text, color, size = 10) {
  const el = svgEl('text', {
    x: Math.round(x), y: Math.round(y),
    fill: color, 'font-size': size,
    'text-anchor': 'middle', 'font-family': 'Arial, sans-serif', 'font-weight': 'bold',
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

// Localized labels for check error codes
const CHECK_ERROR_LABELS = {
  AllVoicesSameDirection:   'всі голоси в одну сторону',
  VoiceCrossing:            'перехрещення',
  ParallelFifths:           'паралельні квінти',
  ParallelOctaves:          'паралельні октави',
  ParallelOctavesOrUnisons: 'паралельні октави або унісони',
  HiddenFifths:             'приховані квінти між крайніми голосами',
  HiddenOctaves:            'приховані октави між крайніми голосами',
}

function getCssVar(name) {
  return getComputedStyle(document.documentElement).getPropertyValue(name).trim()
}

// ── Renderers ──────────────────────────────────────────────────────────────────

// ChordMarker: "?" above the chord at positionIndex
function renderChordMarker(g, error, map) {
  const pos = map[error.positionIndex]
  if (!pos) return
  const entries = Object.values(pos)
  if (!entries.length) return
  const x = entries.reduce((s, v) => s + v.centerX, 0) / entries.length
  const trebleYs = ['soprano', 'alto'].map(v => pos[v]?.centerY).filter(y => y != null)
  const baseY = trebleYs.length ? Math.min(...trebleYs) : entries[0].centerY
  console.log('[CheckErrorsLayer] ChordMarker at posIdx', error.positionIndex, { x, baseY })
  addText(g, x, baseY - 18, '?', error._color, 14)
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
  const pairKey  = `${error.positionIndex}:${error.nextPositionIndex}`
  const tooltip  = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  console.log('[SlurBracketPair]', { pairKey, stackIdx, interval: error.interval, color, onlySecond })

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

    console.log('[SlurBracketPair] bracket', { bx, top, bottom, mid, interval: error.interval })

    g.appendChild(withTitle(svgEl('path', {
      d: createVerticalSlurPath(bx, top, bottom, BASE_BRACKET_RADIUS),
      stroke: color, 'stroke-width': 1.5, fill: 'none', 'stroke-linecap': 'round',
      'pointer-events': 'visibleStroke',
    })))

    if (error.interval != null) {
      const labelEl = svgEl('text', {
        x: Math.round(bx + BASE_BRACKET_RADIUS - 2),
        y: Math.round(mid + 5),
        fill: color,
        stroke: 'none',
        'font-size': 16,
        'font-weight': '700',
        'font-family': 'Arial, sans-serif',
        'text-anchor': 'start',
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

// MotionLines: connect nearest sides of same-voice noteheads across two chords
function renderMotionLines(g, error, map) {
  console.log('[MotionLines error]', error)

  const fromPosition = map[error.positionIndex]
  const toPosition   = map[error.nextPositionIndex]

  console.log('[MotionLines fromPosition]', error.positionIndex, fromPosition)
  console.log('[MotionLines toPosition]',   error.nextPositionIndex, toPosition)

  if (!fromPosition || !toPosition) return

  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  ;(error.voices || []).forEach(v => {
    const voice    = vk(v)
    const fromNote = fromPosition[voice]
    const toNote   = toPosition[voice]

    if (!fromNote || !toNote) {
      console.warn('[MotionLines] missing voice', voice, { fromNote, toNote })
      return
    }

    const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)

    console.log('[MotionLines]', voice, { start: { x: startX, y: startY }, end: { x: endX, y: endY } })

    addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
  })
}

// CrossingLines: two crossing lines connecting the swapped voices between two chords,
// e.g. voices=["soprano","alto"] draws soprano→alto and alto→soprano lines.
function renderCrossingLines(g, error, map) {
  console.log('[CrossingLines error]', error)

  const fromPosition = map[error.positionIndex]
  const toPosition   = map[error.nextPositionIndex]

  if (!fromPosition || !toPosition) return

  const voices = (error.voices || []).map(vk)
  if (voices.length !== 2) {
    console.warn('[CrossingLines] expected 2 voices, got', voices.length, error)
    return
  }

  const [v1, v2] = voices
  const tooltip = CHECK_ERROR_LABELS[error.code] || error.message || error.code || ''

  // Line 1: v1 from chord A → v2 in chord B  (e.g. soprano → alto)
  // Line 2: v2 from chord A → v1 in chord B  (e.g. alto → soprano)
  const pairs = [[v1, v2], [v2, v1]]

  pairs.forEach(([fromVoice, toVoice]) => {
    const fromNote = fromPosition[fromVoice]
    const toNote   = toPosition[toVoice]

    if (!fromNote || !toNote) {
      console.warn('[CrossingLines] missing note', { fromVoice, toVoice, fromNote, toNote })
      return
    }

    const { startX, startY, endX, endY } = getMotionLineAnchors(fromNote, toNote)

    console.log('[CrossingLines]', fromVoice, '→', toVoice, { start: { x: startX, y: startY }, end: { x: endX, y: endY } })

    addLineWithTitle(g, startX, startY, endX, endY, error._color, 1.5, tooltip)
  })
}

// ── Renderer dispatch ──────────────────────────────────────────────────────────

const RENDERERS = {
  ChordMarker:         renderChordMarker,
  VerticalBracketPair: renderVerticalBracketPair,
  HiddenInterval:      renderHiddenInterval,
  MotionLines:         renderMotionLines,
  CrossingLines:       renderCrossingLines,
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
