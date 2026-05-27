// Order of accidentals added in key signatures (circle of fifths)
const SHARP_ORDER = ['f', 'c', 'g', 'd', 'a', 'e', 'b']
const FLAT_ORDER  = ['b', 'e', 'a', 'd', 'g', 'c', 'f']

// Semitones above C within an octave for each diatonic pitch letter
const PITCH_SEMITONES = { c: 0, d: 2, e: 4, f: 5, g: 7, a: 9, b: 11 }

// VexFlow accidental code → chromatic alteration in semitones
const ACC_OFFSET = { '#': 1, 'b': -1, 'n': 0, '##': 2, 'bb': -2 }

// VexFlow key signature string → signed accidental count
// Positive = sharps, negative = flats
export const KEY_ACC_SIGNED = {
  'C':  0,
  'G':  1, 'D':  2, 'A':  3, 'E':  4, 'B':  5, 'F#':  6, 'C#':  7,
  'F': -1, 'Bb': -2, 'Eb': -3, 'Ab': -4, 'Db': -5, 'Gb': -6, 'Cb': -7,
}

// Build a { pitchLetter -> semitoneOffset } map from a signed accidental count
// (negative = flats, positive = sharps, 0 = no accidentals)
export function getKeyAccidentals(acc) {
  if (!acc) return {}
  const result = {}
  if (acc > 0) {
    for (let i = 0; i < acc; i++) result[SHARP_ORDER[i]] = 1
  } else {
    for (let i = 0; i < -acc; i++) result[FLAT_ORDER[i]] = -1
  }
  return result
}

// Compute the effective absolute pitch of a note as a semitone value.
//
// Rules applied in order (each can override the previous):
//   1. Key signature accidental for this pitch letter (keyAcc)
//   2. Last explicit accidental on the same pitch+octave earlier in the measure
//      (an accidental carries forward to the end of the measure)
//   3. The note's own explicit accidental (highest priority)
//
// precedingNotes: array of notes that appear before `note` in the same measure.
// keyAcc: map returned by getKeyAccidentals().
export function getEffectiveSemitones(note, precedingNotes, keyAcc) {
  const { pitch, octave, accidental } = note
  const base  = octave * 12 + (PITCH_SEMITONES[pitch] ?? 0)
  let   alter = keyAcc[pitch] ?? 0

  for (const prev of precedingNotes) {
    if (prev.pitch === pitch && prev.octave === octave && prev.accidental !== undefined) {
      alter = ACC_OFFSET[prev.accidental] ?? 0
    }
  }
  if (accidental !== undefined) alter = ACC_OFFSET[accidental] ?? 0

  return base + alter
}
