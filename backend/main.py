import asyncio
import json
import logging
from contextlib import asynccontextmanager
from pathlib import Path
from uuid import uuid4

import uvicorn
from fastapi import FastAPI, HTTPException, Response
from fastapi.middleware.cors import CORSMiddleware
from fastapi.responses import FileResponse, JSONResponse

from worker_models import WorkerRequest
from worker.worker_pool import WorkerPool
from worker.job_queue import JobQueue
from worker.result_store import ResultStore
from worker.job_models import JobStatus
from worker.dispatcher import dispatcher_loop, cleanup_loop

logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s  %(levelname)-8s  %(name)s  %(message)s',
)
logger = logging.getLogger(__name__)

OUTPUT_DIR = Path(__file__).parent / 'output'
OUTPUT_DIR.mkdir(exist_ok=True)

POOL_SIZE = 4
QUEUE_MAX = 50

# Module-level singletons, initialised in lifespan().
worker_pool:  WorkerPool  | None = None
job_queue:    JobQueue    | None = None
result_store: ResultStore | None = None


@asynccontextmanager
async def lifespan(app: FastAPI):
    global worker_pool, job_queue, result_store

    worker_pool  = WorkerPool(size=POOL_SIZE)
    job_queue    = JobQueue(maxsize=QUEUE_MAX)
    result_store = ResultStore()

    worker_pool.start()

    bg_tasks = [
        asyncio.create_task(dispatcher_loop(job_queue, worker_pool, result_store)),
        asyncio.create_task(cleanup_loop(result_store)),
    ]

    yield

    for t in bg_tasks:
        t.cancel()
    worker_pool.shutdown()


app = FastAPI(
    title='Harmonizer API',
    version='2.0.0',
    description='Music harmonisation backend with persistent C++ worker pool.',
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=['*'],
    allow_methods=['GET', 'POST', 'DELETE'],
    allow_headers=['*'],
)

# ── System ─────────────────────────────────────────────────────────────────────

@app.get('/api/health', tags=['system'])
async def health_check() -> dict:
    s = result_store.stats() if result_store else {}
    return {
        'status':     'ok',
        'output_dir': str(OUTPUT_DIR),
        'files':      len(list(OUTPUT_DIR.glob('*.xml'))),
        'jobs':       s,
    }

# ── Score ──────────────────────────────────────────────────────────────────────

@app.get('/api/score/download/{filename}', tags=['score'])
async def download_musicxml(filename: str) -> FileResponse:
    safe_name = Path(filename).name
    if safe_name != filename or '..' in filename:
        raise HTTPException(status_code=400, detail='Invalid filename')
    filepath = OUTPUT_DIR / safe_name
    if not filepath.exists():
        raise HTTPException(status_code=404, detail=f'File not found: {safe_name}')
    return FileResponse(
        path=str(filepath),
        media_type='application/vnd.recordare.musicxml+xml',
        filename=safe_name,
        headers={'Content-Disposition': f'attachment; filename="{safe_name}"'},
    )


# ── Legacy synchronous worker endpoint (routes through pool, not queue) ────────

@app.post('/api/worker/submit', status_code=200, tags=['worker'])
async def submit_worker_job_legacy(payload: WorkerRequest) -> dict:
    """Synchronous wrapper kept for backward compatibility.
    Routes through the persistent pool instead of spawning a new process.
    """
    logger.info('Legacy submit  jobId=%s  mode=%s', payload.jobId, payload.mode)
    raw      = payload.model_dump(exclude_none=True)
    job_json = json.dumps(raw, ensure_ascii=False, separators=(',', ':'))
    try:
        return await worker_pool.run_job(job_json, timeout=30.0)
    except asyncio.TimeoutError:
        raise HTTPException(status_code=504, detail='Worker timed out after 30s')
    except Exception as exc:
        raise HTTPException(status_code=502, detail=str(exc))

# ── Async jobs API ─────────────────────────────────────────────────────────────

@app.get('/api/jobs/stats', tags=['jobs'])
async def get_job_stats() -> dict:
    s = result_store.stats()
    return {
        'queueLength':    job_queue.qsize(),
        'poolSize':       POOL_SIZE,
        'activeWorkers':  s['processing'],
        'totalProcessed': s['totalProcessed'],
    }


@app.post('/api/jobs', status_code=202, tags=['jobs'])
async def submit_job(payload: WorkerRequest) -> dict:
    raw    = payload.model_dump(exclude_none=True)
    job_id = str(uuid4())

    await result_store.create(job_id, raw)

    if not job_queue.enqueue(job_id):
        await result_store.cancel(job_id)
        raise HTTPException(
            status_code=503,
            detail={'message': 'Queue is full, please try again later', 'retryAfter': 5},
        )

    logger.info('Job queued  jobId=%s  mode=%s', job_id, payload.mode)
    return {'jobId': job_id}


@app.get('/api/jobs/{job_id}/status', tags=['jobs'])
async def get_job_status(job_id: str) -> dict:
    record = await result_store.get(job_id)
    if record is None:
        raise HTTPException(status_code=404, detail='Job not found')
    return {'jobId': job_id, 'status': record.status}


@app.get('/api/jobs/{job_id}/result', tags=['jobs'])
async def get_job_result(job_id: str):
    record = await result_store.get(job_id)
    if record is None:
        raise HTTPException(status_code=404, detail='Job not found')
    if record.status == JobStatus.DONE:
        return JSONResponse(content=record.result)
    if record.status == JobStatus.ERROR:
        return JSONResponse(content={
            'jobId':   job_id,
            'status':  'error',
            'results': [],
            'errors':  [{'message': record.error}],
        })
    return Response(status_code=202)  # queued or processing — not ready yet


@app.delete('/api/jobs/{job_id}', tags=['jobs'])
async def cancel_job(job_id: str) -> dict:
    cancelled = await result_store.cancel(job_id)
    if not cancelled:
        record = await result_store.get(job_id)
        if record is None:
            raise HTTPException(status_code=404, detail='Job not found')
        raise HTTPException(
            status_code=409,
            detail=f'Job cannot be cancelled (status: {record.status})',
        )
    return {'jobId': job_id, 'cancelled': True}


# ── Entry point ────────────────────────────────────────────────────────────────

if __name__ == '__main__':
    uvicorn.run('main:app', host='0.0.0.0', port=8080, reload=True)
