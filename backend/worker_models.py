from pydantic import BaseModel, field_validator
from typing import Optional


class WorkerTimeSignature(BaseModel):
    beats:    int
    beatType: int

    @field_validator('beatType')
    @classmethod
    def beat_type_is_power_of_two(cls, v: int) -> int:
        if v not in (1, 2, 4, 8, 16, 32):
            raise ValueError(f'beatType must be a power of 2 (1–32), got {v}')
        return v


class WorkerSettings(BaseModel):
    key:                 str
    measureCount:        int
    timeSignature:       WorkerTimeSignature
    anacrusisSixteenths: int

    # present for harmonize_melody / harmonize_bass; absent for check_solution
    scaleMode:      Optional[list[str]] = None
    forbiddenRules: Optional[list[str]] = None
    allowedChords:  Optional[list[str]] = None

    @field_validator('key')
    @classmethod
    def key_not_empty(cls, v: str) -> str:
        if not v.strip():
            raise ValueError('key must not be empty')
        return v

    @field_validator('measureCount')
    @classmethod
    def measure_count_positive(cls, v: int) -> int:
        if v < 1:
            raise ValueError('measureCount must be at least 1')
        return v


class WorkerRequest(BaseModel):
    """
    Worker-format request forwarded (unchanged) to the C++ harmonisation engine.

    Field names are intentionally camelCase to match the engine's expected JSON schema.
    Do NOT rename fields — the payload is passed through verbatim.
    """
    jobId:    str
    mode:     str    # 'harmonize_melody' | 'check_solution'
    settings: WorkerSettings
    input:    dict   # voice/melody data — structure varies by mode
