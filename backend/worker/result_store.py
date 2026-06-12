"""
In-memory result store with TTL-based cleanup.

Thread safety: all mutations go through an asyncio.Lock so concurrent
endpoint handlers never see partially-written records.
"""
import asyncio
import logging
import time
from typing import Any

from .job_models import JobRecord, JobStatus

logger = logging.getLogger(__name__)

TTL_SECONDS = 600.0  # completed jobs are kept for 10 minutes


class ResultStore:
    def __init__(self, ttl: float = TTL_SECONDS) -> None:
        self._records: dict[str, JobRecord] = {}
        self._lock = asyncio.Lock()
        self._ttl  = ttl
        self._total_processed = 0

    # ── Write operations ───────────────────────────────────────────────

    async def create(self, job_id: str, payload: dict[str, Any]) -> JobRecord:
        now    = time.time()
        record = JobRecord(
            job_id=job_id,
            status=JobStatus.QUEUED,
            created_at=now,
            updated_at=now,
            payload=payload,
        )
        async with self._lock:
            self._records[job_id] = record
        return record

    async def mark_processing(self, job_id: str) -> None:
        async with self._lock:
            if r := self._records.get(job_id):
                r.status     = JobStatus.PROCESSING
                r.updated_at = time.time()

    async def mark_done(self, job_id: str, result: dict[str, Any]) -> None:
        async with self._lock:
            if r := self._records.get(job_id):
                r.status     = JobStatus.DONE
                r.result     = result
                r.updated_at = time.time()
                self._total_processed += 1

    async def mark_error(self, job_id: str, error: str) -> None:
        async with self._lock:
            if r := self._records.get(job_id):
                r.status     = JobStatus.ERROR
                r.error      = error
                r.updated_at = time.time()
                self._total_processed += 1

    async def cancel(self, job_id: str) -> bool:
        """Remove a queued job record. Returns True only if the job was QUEUED."""
        async with self._lock:
            r = self._records.get(job_id)
            if r and r.status == JobStatus.QUEUED:
                del self._records[job_id]
                return True
        return False

    # ── Read operations ────────────────────────────────────────────────

    async def get(self, job_id: str) -> JobRecord | None:
        async with self._lock:
            return self._records.get(job_id)

    # ── Maintenance ────────────────────────────────────────────────────

    async def cleanup(self) -> int:
        cutoff = time.time() - self._ttl
        async with self._lock:
            expired = [
                jid for jid, r in self._records.items()
                if r.updated_at < cutoff
                and r.status in (JobStatus.DONE, JobStatus.ERROR)
            ]
            for jid in expired:
                del self._records[jid]
        if expired:
            logger.debug('Cleaned up %d expired job records', len(expired))
        return len(expired)

    def stats(self) -> dict[str, int]:
        statuses = [r.status for r in self._records.values()]
        return {
            'queued':         statuses.count(JobStatus.QUEUED),
            'processing':     statuses.count(JobStatus.PROCESSING),
            'done':           statuses.count(JobStatus.DONE),
            'error':          statuses.count(JobStatus.ERROR),
            'totalProcessed': self._total_processed,
        }
