"""
Background dispatcher and TTL cleanup coroutines.

dispatcher_loop() pulls job IDs from the queue one at a time and spawns
an asyncio Task for each.  The WorkerPool's internal asyncio.Queue<Slot>
acts as the concurrency gate: tasks that cannot acquire a slot wait there,
so the OS is never flooded with parallel subprocess calls.
"""
import asyncio
import json
import logging

from .job_models import JobStatus
from .job_queue import JobQueue
from .result_store import ResultStore
from .worker_pool import WorkerPool

logger = logging.getLogger(__name__)

WORKER_TIMEOUT = 60.0  # seconds


async def dispatcher_loop(
    queue: JobQueue,
    pool:  WorkerPool,
    store: ResultStore,
) -> None:
    logger.info('Job dispatcher started')
    while True:
        job_id = await queue.dequeue()
        asyncio.create_task(_run_job(job_id, pool, store))


async def _run_job(job_id: str, pool: WorkerPool, store: ResultStore) -> None:
    record = await store.get(job_id)
    if record is None or record.status != JobStatus.QUEUED:
        return  # job was cancelled between enqueue and dispatch

    await store.mark_processing(job_id)
    logger.info('Job %s  processing', job_id)

    try:
        job_json = json.dumps(record.payload, ensure_ascii=False, separators=(',', ':'))
        result   = await pool.run_job(job_json, timeout=WORKER_TIMEOUT)
        await store.mark_done(job_id, result)
        logger.info('Job %s  done', job_id)
    except asyncio.TimeoutError:
        msg = f'Worker timed out after {WORKER_TIMEOUT:.0f}s'
        await store.mark_error(job_id, msg)
        logger.error('Job %s  timed out', job_id)
    except Exception as exc:
        await store.mark_error(job_id, str(exc))
        logger.error('Job %s  error: %s', job_id, exc)


async def cleanup_loop(store: ResultStore, interval: float = 60.0) -> None:
    while True:
        await asyncio.sleep(interval)
        await store.cleanup()
