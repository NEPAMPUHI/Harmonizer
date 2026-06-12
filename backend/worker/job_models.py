from dataclasses import dataclass
from typing import Any


class JobStatus:
    QUEUED     = 'queued'
    PROCESSING = 'processing'
    DONE       = 'done'
    ERROR      = 'error'


@dataclass
class JobRecord:
    job_id:     str
    status:     str
    created_at: float
    updated_at: float
    payload:    dict[str, Any]
    result:     dict[str, Any] | None = None
    error:      str | None            = None
