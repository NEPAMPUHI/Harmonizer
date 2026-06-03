// German/Ukrainian notation → VexFlow key signature
// acc: negative = flats, positive = sharps, 0 = no accidentals
export const TONALITIES = [
  { key: 'C',   label: 'до мажор',      vexKey: 'C',  acc:  0, major: true  },
  { key: 'c',   label: 'до мінор',      vexKey: 'Eb', acc: -3, major: false },
  { key: 'cis', label: 'до# мінор',     vexKey: 'E',  acc:  4, major: false },
  { key: 'Des', label: 'ре♭ мажор',    vexKey: 'Db', acc: -5, major: true  },
  { key: 'D',   label: 'ре мажор',      vexKey: 'D',  acc:  2, major: true  },
  { key: 'd',   label: 'ре мінор',      vexKey: 'F',  acc: -1, major: false },
  { key: 'Dis', label: 'ре# мінор',     vexKey: 'F#', acc:  6, major: false },
  { key: 'Es',  label: 'мі♭ мажор',    vexKey: 'Eb', acc: -3, major: true  },
  { key: 'es',  label: 'мі♭ мінор',    vexKey: 'Gb', acc: -6, major: false },
  { key: 'F',   label: 'фа мажор',      vexKey: 'F',  acc: -1, major: true  },
  { key: 'f',   label: 'фа мінор',      vexKey: 'Ab', acc: -4, major: false },
  { key: 'Fis', label: 'фа# мажор',     vexKey: 'F#', acc:  6, major: true  },
  { key: 'fis', label: 'фа# мінор',     vexKey: 'A',  acc:  3, major: false },
  { key: 'Ges', label: 'соль♭ мажор',  vexKey: 'Gb', acc: -6, major: true  },
  { key: 'G',   label: 'соль мажор',    vexKey: 'G',  acc:  1, major: true  },
  { key: 'g',   label: 'соль мінор',    vexKey: 'Bb', acc: -2, major: false },
  { key: 'gis', label: 'соль# мінор',   vexKey: 'B',  acc:  5, major: false },
  { key: 'As',  label: 'ля♭ мажор',    vexKey: 'Ab', acc: -4, major: true  },
  { key: 'A',   label: 'ля мажор',      vexKey: 'A',  acc:  3, major: true  },
  { key: 'a',   label: 'ля мінор',      vexKey: 'C',  acc:  0, major: false },
  { key: 'B',   label: 'сі♭ мажор',    vexKey: 'Bb', acc: -2, major: true  },
  { key: 'b',   label: 'сі♭ мінор',    vexKey: 'Db', acc: -5, major: false },
  { key: 'H',   label: 'сі мажор',      vexKey: 'B',  acc:  5, major: true  },
  { key: 'h',   label: 'сі мінор',      vexKey: 'D',  acc:  2, major: false },
]

export const DEFAULT_TONALITY = TONALITIES.find(t => t.key === 'C')

export function accLabel(acc) {
  if (acc === 0) return '—'
  return acc > 0 ? `${acc}♯` : `${Math.abs(acc)}♭`
}
