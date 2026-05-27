"""
Converts a validated ScorePayload into a MusicXML 4.0 string.

Layout decisions:
  - divisions = 4  →  1 tick = 1 sixteenth note = 1 MusicXML duration unit
  - Single stave (treble or bass) when no bass notes exist in the score
  - Grand staff (treble + bass, staves 1 and 2) when any bass note is present
  - Auto-rest filling: each stave is padded to capacityTicks before the next
    stave begins, so every measure is rhythmically complete
  - Backup element rewinds the cursor between treble and bass voices
"""

from xml.etree.ElementTree import Element, SubElement, tostring
from xml.dom.minidom import parseString

from models import ScorePayload

# MusicXML clef definitions
_CLEF_MAP: dict[str, tuple[str, str]] = {
    'treble': ('G', '2'),
    'bass':   ('F', '4'),
}

# Rest sizes for greedy fill, largest first (ticks in sixteenth-note units)
_REST_SIZES: list[tuple[str, bool, int]] = [
    ('whole',   False, 16),
    ('half',    True,  12),
    ('half',    False,  8),
    ('quarter', True,   6),
    ('quarter', False,  4),
    ('eighth',  True,   3),
    ('eighth',  False,  2),
    ('16th',    False,  1),
]


# ── Rest filling ─────────────────────────────────────────────────────────────

def _fill_rests(notes: list[dict], capacity: float) -> list[dict]:
    """Return notes + auto-generated rests that bring the total up to capacity."""
    used = sum(n['durationTicks'] for n in notes)
    remaining = capacity - used
    if remaining <= 0:
        return list(notes)

    auto: list[dict] = []
    pos = used
    i = 0
    for dur_type, dotted, ticks in _REST_SIZES:
        while remaining >= ticks:
            auto.append({
                'id':            f'rest-fill-{i}',
                'type':          'rest',
                'durationType':  dur_type,
                'durationTicks': float(ticks),
                'dotted':        dotted,
                'tieStart':      False,
                'tieStop':       False,
                'positionTicks': pos,
            })
            remaining -= ticks
            pos      += ticks
            i        += 1

    return list(notes) + auto


# ── Single note element ───────────────────────────────────────────────────────

def _ticks_str(ticks: float) -> str:
    """Format a tick value as an integer string (safe because dotted 16th is disabled in UI)."""
    return str(int(ticks)) if ticks == int(ticks) else str(ticks)


def _add_note_el(
    parent:     Element,
    note:       dict,
    staff_num:  int,
    voice:      str,
    multistave: bool,
) -> None:
    """
    Append a <note> element to parent following the MusicXML 4.0 element order:
      pitch/rest → duration → tie → voice → type → dot → accidental → staff → notations
    """
    note_el = SubElement(parent, 'note')

    # 1. Pitch or rest
    if note['type'] == 'rest':
        SubElement(note_el, 'rest')
    else:
        pitch_el = SubElement(note_el, 'pitch')
        SubElement(pitch_el, 'step').text = note['step']
        if note.get('alter') is not None:
            alter = note['alter']
            SubElement(pitch_el, 'alter').text = (
                str(int(alter)) if alter == int(alter) else str(alter)
            )
        SubElement(pitch_el, 'octave').text = str(note['octave'])

    # 2. Duration (equals durationTicks because divisions=4 matches our tick system)
    SubElement(note_el, 'duration').text = _ticks_str(note['durationTicks'])

    # 3. Ties — stop declared before start (MusicXML convention)
    if note['type'] == 'note':
        if note.get('tieStop'):
            SubElement(note_el, 'tie', type='stop')
        if note.get('tieStart'):
            SubElement(note_el, 'tie', type='start')

    # 4. Voice
    SubElement(note_el, 'voice').text = voice

    # 5. Type (note duration name)
    SubElement(note_el, 'type').text = note['durationType']

    # 6. Dot
    if note.get('dotted'):
        SubElement(note_el, 'dot')

    # 7. Accidental mark (explicit sign shown on the staff)
    if note['type'] == 'note' and note.get('accidentalMark'):
        SubElement(note_el, 'accidental').text = note['accidentalMark']

    # 8. Staff number (grand-staff scores only)
    if multistave:
        SubElement(note_el, 'staff').text = str(staff_num)

    # 9. Notations — <tied> is required alongside <tie> for full MusicXML compliance
    if note['type'] == 'note' and (note.get('tieStart') or note.get('tieStop')):
        notations_el = SubElement(note_el, 'notations')
        if note.get('tieStop'):
            SubElement(notations_el, 'tied', type='stop')
        if note.get('tieStart'):
            SubElement(notations_el, 'tied', type='start')


# ── Main builder ─────────────────────────────────────────────────────────────

def build_musicxml(score: ScorePayload) -> str:
    """Convert a validated ScorePayload to a MusicXML 4.0 string."""

    # Detect whether any measure has bass notes → grand staff
    has_bass = any(
        any(s.clef == 'bass' and s.notes for s in m.staves)
        for m in score.measures
    )
    multistave = has_bass

    root = Element('score-partwise', version='4.0')

    # ── Part list ───
    # ─────────────────────────────────────────────────
    part_list  = SubElement(root, 'part-list')
    score_part = SubElement(part_list, 'score-part', id='P1')
    SubElement(score_part, 'part-name').text = 'Piano' if multistave else 'Melody'

    # ── Part ─────────────────────────────────────────────────────────
    part = SubElement(root, 'part', id='P1')

    stave_number: dict[str, str] = {'treble': '1', 'bass': '2'}

    for measure in score.measures:
        m_el = SubElement(part, 'measure', number=str(measure.number))

        # ── Attributes block (first measure only) ────────────────────
        if measure.number == 1:
            attrs = SubElement(m_el, 'attributes')
            SubElement(attrs, 'divisions').text = str(score.divisions)

            key_el = SubElement(attrs, 'key')
            SubElement(key_el, 'fifths').text = str(score.keySignature.fifths)
            SubElement(key_el, 'mode').text    = score.keySignature.mode

            time_el = SubElement(attrs, 'time')
            SubElement(time_el, 'beats').text     = str(score.timeSignature.beats)
            SubElement(time_el, 'beat-type').text = str(score.timeSignature.beatType)

            if multistave:
                SubElement(attrs, 'staves').text = '2'

            for stave in measure.staves:
                if not multistave and stave.clef == 'bass':
                    continue
                clef_el = SubElement(attrs, 'clef')
                if multistave:
                    clef_el.set('number', stave_number[stave.clef])
                sign, line = _CLEF_MAP[stave.clef]
                SubElement(clef_el, 'sign').text = sign
                SubElement(clef_el, 'line').text = line

        # ── Treble (voice 1, staff 1) ────────────────────────────────
        treble = next((s for s in measure.staves if s.clef == 'treble'), None)
        treble_notes = _fill_rests(
            [n.model_dump() for n in (treble.notes if treble else [])],
            measure.capacityTicks,
        )
        for note in treble_notes:
            _add_note_el(m_el, note, staff_num=1, voice='1', multistave=multistave)

        # ── Bass (voice 2, staff 2) — grand staff only ───────────────
        if multistave:
            bass = next((s for s in measure.staves if s.clef == 'bass'), None)
            bass_notes = _fill_rests(
                [n.model_dump() for n in (bass.notes if bass else [])],
                measure.capacityTicks,
            )

            # Backup rewinds the cursor to the start of the measure
            treble_total = int(sum(n['durationTicks'] for n in treble_notes))
            backup_el = SubElement(m_el, 'backup')
            SubElement(backup_el, 'duration').text = str(treble_total)

            for note in bass_notes:
                _add_note_el(m_el, note, staff_num=2, voice='2', multistave=True)

    # ── Serialize to pretty-printed XML ──────────────────────────────
    raw    = tostring(root, encoding='unicode')
    pretty = parseString(raw).toprettyxml(indent='  ', encoding=None)

    # Strip the auto-generated declaration and prepend the proper MusicXML header
    lines = pretty.splitlines()
    if lines and lines[0].startswith('<?xml'):
        lines = lines[1:]

    header = (
        '<?xml version="1.0" encoding="UTF-8"?>\n'
        '<!DOCTYPE score-partwise PUBLIC\n'
        '  "-//Recordare//DTD MusicXML 4.0 Partwise//EN"\n'
        '  "http://www.musicxml.org/dtds/partwise.dtd">'
    )
    return header + '\n' + '\n'.join(lines)
