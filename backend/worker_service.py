"""
Thin wrapper that spawns the C++ harmoniser worker as a subprocess,
feeds it one JSON job via stdin, and returns the parsed JSON response.

I/O contract with harmonizer_worker.exe:
  - stdin:  one JSON line, then "shutdown\n" to make the worker's loop exit cleanly
  - stdout: one JSON line (the result)
  - stderr: diagnostic/error text (never mixed into stdout)
"""
import json
import logging
import subprocess
from pathlib import Path
from typing import Any

from fastapi import HTTPException

logger = logging.getLogger(__name__)

# Path resolved relative to this file so it works regardless of cwd.
_REPO_ROOT        = Path(__file__).parent.parent
WORKER_EXECUTABLE = _REPO_ROOT / 'worker-cpp' / 'build' / 'Release' / 'harmonizer_worker.exe'
WORKER_TIMEOUT    = 30  # seconds


def call_worker(payload: dict[str, Any]) -> dict[str, Any]:
    """
    Send *payload* to the C++ worker and return its JSON response.

    Raises HTTPException on every failure path so the FastAPI endpoint
    does not need any extra try/except.
    """
    if not WORKER_EXECUTABLE.is_file():
        raise HTTPException(
            status_code=503,
            detail=f'C++ worker executable not found: {WORKER_EXECUTABLE}',
        )

    # One JSON line + "shutdown" so the worker's readline loop exits cleanly.
    stdin_data = json.dumps(payload, ensure_ascii=False) + '\nshutdown\n'

    logger.debug('Calling worker: %s', WORKER_EXECUTABLE.name)

    try:
        proc = subprocess.run(
            [str(WORKER_EXECUTABLE)],
            input=stdin_data,
            text=True,
            capture_output=True,
            timeout=WORKER_TIMEOUT,
        )
    except subprocess.TimeoutExpired:
        logger.error('Worker timed out after %ds', WORKER_TIMEOUT)
        raise HTTPException(
            status_code=504,
            detail=f'C++ worker timed out after {WORKER_TIMEOUT}s',
        )

    if proc.stderr.strip():
        logger.warning('Worker stderr: %s', proc.stderr.strip())

    if proc.returncode != 0:
        logger.error('Worker exited with code %d', proc.returncode)
        raise HTTPException(
            status_code=502,
            detail={
                'message':    'C++ worker exited with non-zero code',
                'returncode': proc.returncode,
                'stderr':     proc.stderr.strip(),
            },
        )

    stdout = proc.stdout.strip()
    if not stdout:
        logger.error('Worker produced empty stdout')
        raise HTTPException(
            status_code=502,
            detail={
                'message': 'C++ worker returned empty stdout',
                'stderr':  proc.stderr.strip(),
            },
        )

    try:
        response = json.loads(stdout)
    except json.JSONDecodeError as exc:
        logger.error('Worker stdout is not valid JSON: %s', exc)
        raise HTTPException(
            status_code=502,
            detail={
                'message': 'C++ worker returned non-JSON stdout',
                'stdout':  stdout[:500],
                'stderr':  proc.stderr.strip(),
            },
        )

    logger.info('Worker response: jobId=%s status=%s',
                response.get('jobId', '?'), response.get('status', '?'))
    return response
