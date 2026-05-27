"""
Simple rule-based harmonizer: generates bass lines for a given melody.

Strategy:
  - Determine the tonic semitone from the key signature
  - For each measure, assign a chord degree (I, IV, or V) based on the pattern
  - Produce a single bass note per measure — the root of the chosen chord
  - Auto-rest filling in xml_builder covers any remaining ticks

Three patterns produce three variants, giving the frontend three staff previews.
"""

from __future__ import annotations
from datetime import datetime
from pathlib import Path

from models import ScorePayload, Measure, Stave, NoteItem
from xml_builder import build_musicxml

# ── Key helpers ───────────────────────────────────────────────────────────────

# Circle of fifths → major tonic semitone (C=0, C#=1, …, B=11)
_MAJOR_TONIC: dict[int, int] = {
     0: 0,   # C
     1: 7,   # G
     2: 2,   # D
     3: 9,   # A
     4: 4,   # E
     5: 11,  # B
     6: 6,   # F#
     7: 1,   # C#
    -1: 5,   # F
    -2: 10,  # Bb
    -3: 3,   # Eb
    -4: 8,   # Ab
    -5: 1,   # Db
    -6: 6,   # Gb
    -7: 11,  # Cb
}

# Chromatic note spelling: sharp-preferred and flat-preferred tables
_SHARP_SPELL: list[tuple[str, int]] = [
    ('C', 0), ('C', 1), ('D', 0), ('D', 1), ('E', 0),
    ('F', 0), ('F', 1), ('G', 0), ('G', 1), ('A', 0), ('A', 1), ('B', 0),
]
_FLAT_SPELL: list[tuple[str, int]] = [
    ('C', 0),  ('D', -1), ('D', 0), ('E', -1), ('E', 0),
    ('F', 0),  ('G', -1), ('G', 0), ('A', -1), ('A', 0), ('B', -1), ('B', 0),
]

# Duration code → base ticks (sixteenth-note units)
_DUR_TICKS: dict[str, int] = {
    'whole': 16, 'half': 8, 'quarter': 4, 'eighth': 2, '16th': 1,
}

# (min_ticks, durationType, dotted) — largest fitting duration, sorted descending
_TICK_TABLE: list[tuple[int, str, bool]] = sorted([
    (24, 'whole',   True),
    (16, 'whole',   False),
    (12, 'half',    True),
    (8,  'half',    False),
    (6,  'quarter', True),
    (4,  'quarter', False),
    (3,  'eighth',  True),
    (2,  'eighth',  False),
    (1,  '16th',    False),
], reverse=True)

# ── Harmonic patterns ─────────────────────────────────────────────────────────
# degrees[i] = semitone interval above tonic for the chord root in measure i
# (cycling when the score has more measures than degrees)

PATTERNS: list[dict] = [
    {
        'name':        'Варіант 1',
        'description': 'Тоніка у басі',
        'degrees':     [0],              # I I I I …
    },
    {
        'name':        'Варіант 2',
        'description': 'Тоніка — Домінанта',
        'degrees':     [0, 7, 7, 0],     # I V V I (cycling)
    },
    {
        'name':        'Варіант 3',
        'description': 'Тоніка — Субдомінанта — Домінанта',
        'degrees':     [0, 5, 7, 0],     # I IV V I (cycling)
    },
]


# ── Helpers ───────────────────────────────────────────────────────────────────

def _tonic_semitone(fifths: int, mode: str) -> int:
    """Return the semitone of the tonic note (0 = C)."""
    major = _MAJOR_TONIC.get(fifths, 0)
    return (major - 3) % 12 if mode == 'minor' else major


def _spell(semitone: int, fifths: int) -> tuple[str, int | None]:
    """
    Return (step, alter) for a given semitone.
    alter is None for natural notes so <alter> is omitted from MusicXML,
    letting the key signature handle the pitch.
    """
    tbl = _FLAT_SPELL if fifths < 0 else _SHARP_SPELL
    step, alter = tbl[semitone % 12]
    return step, (alter if alter != 0 else None)


def _bass_note(semitone: int, cap: float, fifths: int, note_id: str) -> NoteItem:
    """
    Build a single bass NoteItem occupying the largest duration that fits cap ticks.
    Any remaining ticks are filled by xml_builder._fill_rests automatically.
    """
    step, alter = _spell(semitone, fifths)

    t = int(cap)
    dur_type, dotted = next(
        (d, dt) for tick, d, dt in _TICK_TABLE if tick <= t
    )
    actual_ticks = float(_DUR_TICKS[dur_type] * (1.5 if dotted else 1))

    return NoteItem(
        id=note_id,
        type='note',
        step=step,
        octave=2,
        alter=float(alter) if alter is not None else None,
        accidentalMark=None,
        durationType=dur_type,
        durationTicks=actual_ticks,
        dotted=dotted,
        tieStart=False,
        tieStop=False,
        positionTicks=0.0,
    )


# ── Public API ────────────────────────────────────────────────────────────────

def generate_variants(score: ScorePayload, output_dir: Path) -> list[dict]:
    """
    Produce one harmonization variant per PATTERNS entry.

    Each variant:
      - clones the input treble melody unchanged
      - replaces the bass stave with a pattern-driven chord root per measure
      - saves the result as a MusicXML file in output_dir
      - returns a dict with metadata + serialized measures for frontend rendering
    """
    tonic = _tonic_semitone(score.keySignature.fifths, score.keySignature.mode)
    fifths = score.keySignature.fifths
    results: list[dict] = []

    for v_idx, pattern in enumerate(PATTERNS, start=1):
        degrees = pattern['degrees']
        new_measures: list[Measure] = []

        for m_idx, measure in enumerate(score.measures):
            degree = degrees[m_idx % len(degrees)]
            semi   = (tonic + degree) % 12
            bass   = _bass_note(semi, measure.capacityTicks, fifths,
                                f'bv{v_idx}m{m_idx}')

            new_staves = [
                Stave(clef='bass', notes=[bass]) if s.clef == 'bass' else s
                for s in measure.staves
            ]
            new_measures.append(Measure(
                number=measure.number,
                isPickup=measure.isPickup,
                capacityTicks=measure.capacityTicks,
                staves=new_staves,
            ))

        modified = ScorePayload(
            divisions=score.divisions,
            timeSignature=score.timeSignature,
            keySignature=score.keySignature,
            anacruisTicks=score.anacruisTicks,
            measures=new_measures,
        )

        xml = build_musicxml(modified)
        ts  = datetime.now().strftime('%Y%m%d_%H%M%S_%f')
        fn  = f'harmonize_{v_idx}_{ts}.xml'
        (output_dir / fn).write_text(xml, encoding='utf-8')

        results.append({
            'id':          v_idx,
            'name':        pattern['name'],
            'description': pattern['description'],
            'filename':    fn,
            'download_url': f'/api/score/download/{fn}',
            # Serialized measures let the frontend render each variant with Staff
            'measures':    [m.model_dump() for m in new_measures],
        })

    return results
