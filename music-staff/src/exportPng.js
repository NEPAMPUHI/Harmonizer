const SCALE = 2

// VexFlow loads these fonts from jsDelivr CDN via FontFace API.
// They must be embedded into the serialized SVG blob so the browser
// can render SMuFL unicode glyphs (noteheads, clefs, etc.) correctly.
const VEXFLOW_CDN  = 'https://cdn.jsdelivr.net/npm/@vexflow-fonts/'
const VEXFLOW_FONTS = {
  'Bravura':         'bravura/bravura.woff2',
  'Bravura Text':    'bravuratext/bravuratext.woff2',
  'Academico':       'academico/academico.woff2',
  'Edwin':           'edwin/edwin-roman.woff2',
  'Gonville':        'gonville/gonville.woff2',
  'Gootville':       'gootville/gootville.woff2',
  'Leipzig':         'leipzig/leipzig.woff2',
  'Leland':          'leland/leland.woff2',
  'Leland Text':     'lelandtext/lelandtext.woff2',
  'MuseJazz':        'musejazz/musejazz.woff2',
  'MuseJazz Text':   'musejazztext/musejazztext.woff2',
  'Petaluma':        'petaluma/petaluma.woff2',
  'Petaluma Script': 'petalumascript/petalumascript.woff2',
  'Sebastian':       'sebastian/sebastian.woff2',
}

// Module-level cache so repeated exports don't re-fetch.
const fontB64Cache = {}

function bufToBase64(buffer) {
  const bytes = new Uint8Array(buffer)
  let bin = ''
  const CHUNK = 8192
  for (let i = 0; i < bytes.length; i += CHUNK) {
    bin += String.fromCharCode.apply(null, bytes.subarray(i, i + CHUNK))
  }
  return btoa(bin)
}

// Collects every unique font-family name referenced anywhere in the SVG tree.
function collectFontFamilies(svgEl) {
  const found = new Set()
  const walk = (el) => {
    if (el.getAttribute) {
      const ff = el.getAttribute('font-family')
      if (ff) ff.split(',').forEach(f => found.add(f.trim().replace(/['"]/g, '')))
    }
    for (const child of el.children ?? []) walk(child)
  }
  walk(svgEl)
  return found
}

// Fetches any VexFlow music fonts referenced by the SVG and returns a CSS
// string of @font-face rules with base64-encoded woff2 data URIs.
async function buildFontCSS(svgEl) {
  const families = collectFontFamilies(svgEl)
  const rules = []

  for (const family of families) {
    const path = VEXFLOW_FONTS[family]
    if (!path) continue

    if (!fontB64Cache[family]) {
      try {
        const resp = await fetch(VEXFLOW_CDN + path)
        if (!resp.ok) continue
        fontB64Cache[family] = bufToBase64(await resp.arrayBuffer())
      } catch {
        continue
      }
    }

    rules.push(
      `@font-face { font-family: "${family}"; src: url("data:font/woff2;base64,${fontB64Cache[family]}"); }`
    )
  }

  return rules.join('\n')
}

// Injects a <style> with @font-face rules into the SVG clone's <defs>.
function injectFontStyle(svgClone, css) {
  if (!css) return
  let defs = svgClone.querySelector('defs')
  if (!defs) {
    defs = document.createElementNS('http://www.w3.org/2000/svg', 'defs')
    svgClone.prepend(defs)
  }
  const style = document.createElementNS('http://www.w3.org/2000/svg', 'style')
  style.textContent = css
  defs.prepend(style)
}

export async function exportSvgToPng(svgElement, filename = 'score.png') {
  if (!svgElement) return

  // Wait for all document fonts to be ready (VexFlow loads them async).
  await document.fonts.ready

  const fontCSS  = await buildFontCSS(svgElement)
  const svgClone = svgElement.cloneNode(true)
  injectFontStyle(svgClone, fontCSS)

  const vb   = svgElement.getAttribute('viewBox')?.split(/[\s,]+/).map(Number)
  const bbox = svgElement.getBoundingClientRect()
  const srcW = vb?.[2] || svgElement.width?.baseVal?.value  || bbox.width
  const srcH = vb?.[3] || svgElement.height?.baseVal?.value || bbox.height

  const svgString = new XMLSerializer().serializeToString(svgClone)
  const blob      = new Blob([svgString], { type: 'image/svg+xml;charset=utf-8' })
  const blobUrl   = URL.createObjectURL(blob)

  return new Promise((resolve) => {
    const img = new Image()
    img.onload = () => {
      const canvas  = document.createElement('canvas')
      canvas.width  = Math.round(srcW * SCALE)
      canvas.height = Math.round(srcH * SCALE)
      const ctx = canvas.getContext('2d')
      ctx.fillStyle = '#ffffff'
      ctx.fillRect(0, 0, canvas.width, canvas.height)
      ctx.scale(SCALE, SCALE)
      ctx.drawImage(img, 0, 0, srcW, srcH)
      URL.revokeObjectURL(blobUrl)

      canvas.toBlob(pngBlob => {
        const a    = document.createElement('a')
        a.href     = URL.createObjectURL(pngBlob)
        a.download = filename
        a.click()
        URL.revokeObjectURL(a.href)
        resolve()
      }, 'image/png')
    }
    img.onerror = () => { URL.revokeObjectURL(blobUrl); resolve() }
    img.src = blobUrl
  })
}
