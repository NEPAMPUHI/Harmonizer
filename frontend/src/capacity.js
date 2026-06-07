// Sixteenth-note ticks per note duration (base unit = 1 sixteenth)
export const NOTE_TICKS = { w: 16, h: 8, q: 4, '8': 2, '16': 1 }

// Total sixteenth-note ticks a measure can hold
export function measureCapacity(timeSig) {
  const [n, d] = timeSig.split('/').map(Number)
  return (n * 16) / d
}

export function noteTicks(note) {
  const base = NOTE_TICKS[note.duration] ?? 4
  // Triplet: 3 notes occupy the rhythmic space of 2 notes of the same duration
  if (note.triplet) return (base * 2) / 3
  return note.dotted ? base * 1.5 : base
}

export function usedTicks(notes) {
  const raw = notes.reduce((s, n) => s + noteTicks(n), 0)
  // Round to 4 decimal places to absorb floating-point drift from triplet fractions
  // (e.g. 3 × 4/3 = 3.9999… → 4)
  return Math.round(raw * 10000) / 10000
}

// capacity is a number (ticks), not a time-sig string
export function canAdd(notes, duration, capacity, dotted = false) {
  const base = NOTE_TICKS[duration] ?? 4
  const ticks = dotted ? base * 1.5 : base
  return usedTicks(notes) + ticks <= capacity
}

// Check whether a complete triplet group (3 notes, total = 2 × base ticks) fits
export function canAddTriplet(notes, duration, capacity) {
  const base = NOTE_TICKS[duration] ?? 4
  return usedTicks(notes) + base * 2 <= capacity
}

// ── Auto-rest filling ────────────────────────────────────────────
const REST_SIZES = [
  { duration: 'w',   dotted: false, ticks: 16 },
  { duration: 'h',   dotted: true,  ticks: 12 },
  { duration: 'h',   dotted: false, ticks: 8  },
  { duration: 'q',   dotted: true,  ticks: 6  },
  { duration: 'q',   dotted: false, ticks: 4  },
  { duration: '8',   dotted: true,  ticks: 3  },
  { duration: '8',   dotted: false, ticks: 2  },
  { duration: '16',  dotted: false, ticks: 1  },
]

function greedyRests(remaining) {
  const out = []
  for (const { duration, dotted, ticks } of REST_SIZES) {
    while (remaining >= ticks) {
      out.push({ duration, dotted: dotted || undefined, isRest: true, pitch: 'b', octave: 4 })
      remaining -= ticks
    }
  }
  return out.reverse()
}

// Returns a new array: user notes followed by auto-generated rests that fill
// the remaining capacity. Compound meters (6/8, 9/8, 12/8) fill beat-by-beat
// using dotted-quarter beats; simple meters use greedy fill.
export function fillRests(notes, capacity, timeSig) {
  const used = usedTicks(notes)
  if (used >= capacity) return notes

  if (notes.length === 0) {
    return [{ duration: 'w', isRest: true, pitch: 'b', octave: 4, id: 'rest-whole' }]
  }

  const [n, d] = timeSig.split('/').map(Number)
  const isCompound = d === 8 && n % 3 === 0

  const autoRests = []
  let id = 0
  const stamp = r => ({ ...r, id: `rest-auto-${id++}` })

  if (isCompound) {
    const BEAT = 6  // dotted quarter
    let pos = used
    let rem = capacity - used
    while (rem > 0) {
      const offset = pos % BEAT
      const chunk  = Math.min(offset === 0 ? BEAT : BEAT - offset, rem)
      greedyRests(chunk).forEach(r => autoRests.push(stamp(r)))
      pos += chunk
      rem -= chunk
    }
  } else {
    greedyRests(capacity - used).forEach(r => autoRests.push(stamp(r)))
  }

  return [...notes, ...autoRests]
}
