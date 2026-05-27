from pydantic import BaseModel, field_validator, model_validator
from typing import Literal, Optional

# Ticks per duration (1 tick = 1 sixteenth note, divisions = 4)
DURATION_TICKS: dict[str, int] = {
    'whole':   16,
    'half':     8,
    'quarter':  4,
    'eighth':   2,
    '16th':     1,
}


class TimeSignature(BaseModel):
    beats: int
    beatType: int

    @field_validator('beats')
    @classmethod
    def beats_in_range(cls, v: int) -> int:
        if not 1 <= v <= 32:
            raise ValueError(f'beats must be 1–32, got {v}')
        return v

    @field_validator('beatType')
    @classmethod
    def beat_type_is_power_of_two(cls, v: int) -> int:
        if v not in (1, 2, 4, 8, 16, 32):
            raise ValueError(f'beatType must be a power of 2 (1–32), got {v}')
        return v


class KeySignature(BaseModel):
    fifths: int   # negative = flats, positive = sharps (MusicXML convention)
    mode: Literal['major', 'minor']

    @field_validator('fifths')
    @classmethod
    def fifths_in_range(cls, v: int) -> int:
        if not -7 <= v <= 7:
            raise ValueError(f'fifths must be –7 to 7, got {v}')
        return v


class NoteItem(BaseModel):
    id: str
    type: Literal['note', 'rest']

    # Pitch — required for notes, absent for rests
    step: Optional[Literal['C', 'D', 'E', 'F', 'G', 'A', 'B']] = None
    octave: Optional[int] = None          # MusicXML octave range: 0–9
    alter: Optional[float] = None         # None → key sig applies; 0=natural, ±1=sharp/flat, ±2=double
    accidentalMark: Optional[Literal[
        'sharp', 'flat', 'natural', 'double-sharp', 'double-flat'
    ]] = None

    # Rhythm
    durationType: Literal['whole', 'half', 'quarter', 'eighth', '16th']
    durationTicks: float                  # sixteenth-note units, must match durationType+dotted
    dotted: bool

    # Articulation
    tieStart: bool = False
    tieStop: bool = False

    # Onset position within the measure — informational, not used for XML generation
    positionTicks: float

    @model_validator(mode='after')
    def validate_note(self) -> 'NoteItem':
        # Note-specific required fields
        if self.type == 'note':
            if self.step is None:
                raise ValueError('step is required for type "note"')
            if self.octave is None:
                raise ValueError('octave is required for type "note"')
            if not 0 <= self.octave <= 9:
                raise ValueError(f'octave must be 0–9, got {self.octave}')

        # Duration consistency: durationTicks must match durationType × dotted factor
        base = DURATION_TICKS.get(self.durationType)
        if base is not None:
            expected = base * 1.5 if self.dotted else float(base)
            if abs(self.durationTicks - expected) > 0.01:
                raise ValueError(
                    f'durationTicks {self.durationTicks} is inconsistent with '
                    f'durationType={self.durationType!r}, dotted={self.dotted} '
                    f'(expected {expected})'
                )

        return self


class Stave(BaseModel):
    clef: Literal['treble', 'bass']
    notes: list[NoteItem]


class Measure(BaseModel):
    number: int
    isPickup: bool
    capacityTicks: float
    staves: list[Stave]

    @field_validator('staves')
    @classmethod
    def treble_stave_required(cls, v: list[Stave]) -> list[Stave]:
        if not any(s.clef == 'treble' for s in v):
            raise ValueError('Each measure must contain a treble stave')
        return v

    @model_validator(mode='after')
    def validate_stave_capacities(self) -> 'Measure':
        for stave in self.staves:
            used = sum(n.durationTicks for n in stave.notes)
            if used > self.capacityTicks + 0.01:
                raise ValueError(
                    f'Stave "{stave.clef}" in measure {self.number} '
                    f'overfills capacity: {used:.1f} > {self.capacityTicks:.1f} ticks'
                )
        return self


class ScorePayload(BaseModel):
    divisions: int                   # must be 4 (sixteenth notes per quarter beat)
    timeSignature: TimeSignature
    keySignature: KeySignature
    anacruisTicks: float             # 0 if no pickup measure
    measures: list[Measure]

    @field_validator('divisions')
    @classmethod
    def divisions_must_be_4(cls, v: int) -> int:
        if v != 4:
            raise ValueError(
                f'divisions must be 4 (sixteenth notes per quarter note), got {v}'
            )
        return v

    @field_validator('measures')
    @classmethod
    def at_least_one_measure(cls, v: list[Measure]) -> list[Measure]:
        if not v:
            raise ValueError('Score must contain at least one measure')
        return v
