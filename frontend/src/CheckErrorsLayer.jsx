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

function vk(voice) { return voice.toLowerCase() }

// ── Renderers ──────────────────────────────────────────────────────────────────

// ChordMarker: red "?" above the chord at positionIndex
function renderChordMarker(g, error, map) {
  const pos = map[error.positionIndex]
  if (!pos) return
  const entries = Object.values(pos)
  if (!entries.length) return
  const x = entries.reduce((s, v) => s + v.x, 0) / entries.length
  const trebleYs = ['soprano', 'alto'].map(v => pos[v]?.y).filter(y => y != null)
  const baseY = trebleYs.length ? Math.min(...trebleYs) : entries[0].y
  addText(g, x, baseY - 18, '?', '#cc0000', 14)
}

// VerticalBracketPair: two vertical brackets (at positionIndex and nextPositionIndex)
// spanning the two involved voices, with interval number between them
function renderVerticalBracketPair(g, error, map) {
  const pos1 = map[error.positionIndex]
  const pos2 = map[error.nextPositionIndex]
  if (!pos1 || !pos2) return
  const voices = (error.voices || []).slice(0, 2).map(vk)
  if (voices.length < 2) return
  const [v1, v2] = voices
  if (!pos1[v1] || !pos1[v2] || !pos2[v1] || !pos2[v2]) return

  const x1 = (pos1[v1].x + pos1[v2].x) / 2 - 5
  const x2 = (pos2[v1].x + pos2[v2].x) / 2 + 5
  const top1 = Math.min(pos1[v1].y, pos1[v2].y) - 4
  const bot1 = Math.max(pos1[v1].y, pos1[v2].y) + 4
  const top2 = Math.min(pos2[v1].y, pos2[v2].y) - 4
  const bot2 = Math.max(pos2[v1].y, pos2[v2].y) + 4
  const c = '#cc0000'

  // Left bracket (opening)
  addLine(g, x1, top1, x1, bot1, c)
  addLine(g, x1, top1, x1 + 4, top1, c)
  addLine(g, x1, bot1, x1 + 4, bot1, c)
  // Right bracket (closing)
  addLine(g, x2, top2, x2, bot2, c)
  addLine(g, x2, top2, x2 - 4, top2, c)
  addLine(g, x2, bot2, x2 - 4, bot2, c)
  // Interval label
  if (error.interval) {
    addText(g, (x1 + x2) / 2, Math.max(bot1, bot2) + 13, String(error.interval), c, 9)
  }
}

// MotionLines: lines connecting each voice's position from positionIndex to nextPositionIndex
function renderMotionLines(g, error, map) {
  const pos1 = map[error.positionIndex]
  const pos2 = map[error.nextPositionIndex]
  if (!pos1 || !pos2) return
  ;(error.voices || []).forEach(v => {
    const voice = vk(v)
    if (!pos1[voice] || !pos2[voice]) return
    addLine(g, pos1[voice].x, pos1[voice].y, pos2[voice].x, pos2[voice].y, '#cc0000', 1.5)
  })
}

// ── Renderer dispatch ──────────────────────────────────────────────────────────

const RENDERERS = {
  ChordMarker:         renderChordMarker,
  VerticalBracketPair: renderVerticalBracketPair,
  MotionLines:         renderMotionLines,
}

// ── Component ──────────────────────────────────────────────────────────────────

export default function CheckErrorsLayer({ errors, noteLayoutMap, canvasRef }) {
  useEffect(() => {
    const svgEl = canvasRef?.current?.querySelector('svg')
    if (!svgEl) return

    svgEl.querySelector('.check-errors-overlay')?.remove()
    if (!errors?.length || !noteLayoutMap) return

    console.log('[CheckErrorsLayer] noteLayoutMap:', noteLayoutMap)
    console.log('[CheckErrorsLayer] rendering errors:', errors)

    const g = document.createElementNS('http://www.w3.org/2000/svg', 'g')
    g.setAttribute('class', 'check-errors-overlay')
    g.setAttribute('pointer-events', 'none')

    errors.forEach(error => {
      const render = RENDERERS[error.renderType]
      if (render) render(g, error, noteLayoutMap)
    })

    svgEl.appendChild(g)
    return () => { canvasRef?.current?.querySelector('svg')?.querySelector('.check-errors-overlay')?.remove() }
  }, [errors, noteLayoutMap, canvasRef])

  return null
}
