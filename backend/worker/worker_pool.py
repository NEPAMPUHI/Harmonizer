"""
Persistent pool of C++ harmonizer_worker.exe processes.

Each WorkerSlot keeps one Popen alive for the lifetime of the server.
Jobs are submitted by writing one JSON line to stdin and reading one JSON
line back from stdout.  stderr is drained in a daemon thread to prevent
the OS pipe-buffer from filling up and deadlocking the worker.

Protocol (unchanged from the original per-process model):
  stdin  → one JSON line per job
  stdout ← one JSON line per result
  "shutdown\n" sent to stdin on graceful shutdown
"""
import asyncio
import json
import logging
import subprocess
import threading
from pathlib import Path
from typing import Any

logger = logging.getLogger(__name__)

_REPO_ROOT = Path(__file__).parent.parent.parent
_BUILD_DIR = _REPO_ROOT / 'worker-cpp' / 'build'
_CANDIDATES = [
    _BUILD_DIR / 'Release' / 'harmonizer_worker.exe',
    _BUILD_DIR / 'Debug'   / 'harmonizer_worker.exe',
    _BUILD_DIR / 'harmonizer_worker',
    _BUILD_DIR / 'harmonizer_worker.exe',
]
WORKER_EXE: Path = next((p for p in _CANDIDATES if p.is_file()), _CANDIDATES[-1])


class WorkerSlot:
    """One persistent harmonizer_worker.exe process."""

    def __init__(self, executable: Path) -> None:
        self._exe  = executable
        self._proc: subprocess.Popen | None = None

    # ── Lifecycle ─────────────────────────────────────────────────────

    def start(self) -> None:
        if not self._exe.is_file():
            logger.error('Worker executable not found: %s', self._exe)
            return
        self._spawn()

    def _spawn(self) -> None:
        proc = subprocess.Popen(
            [str(self._exe)],
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        self._proc = proc
        # Drain stderr in a daemon thread so the worker's pipe buffer never fills.
        threading.Thread(target=self._drain_stderr, args=(proc,), daemon=True).start()
        logger.info('Worker slot spawned  pid=%d', proc.pid)

    @staticmethod
    def _drain_stderr(proc: subprocess.Popen) -> None:
        for raw in proc.stderr:
            text = raw.decode('utf-8', errors='replace').rstrip()
            if text:
                logger.debug('[worker stderr pid=%d] %s', proc.pid, text)

    def _is_alive(self) -> bool:
        return self._proc is not None and self._proc.poll() is None

    def _kill_and_respawn(self) -> None:
        if self._proc is not None:
            try:
                self._proc.kill()
                self._proc.wait(timeout=5)
            except Exception:
                pass
        self._spawn()

    def shutdown(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            try:
                self._proc.stdin.write(b'shutdown\n')
                self._proc.stdin.flush()
                self._proc.wait(timeout=5)
            except Exception:
                try:
                    self._proc.kill()
                except Exception:
                    pass
        logger.info('Worker slot shut down  pid=%s',
                    self._proc.pid if self._proc else '?')

    # ── Job execution ─────────────────────────────────────────────────

    async def run(self, job_json: str, timeout: float = 30.0) -> dict[str, Any]:
        if not self._is_alive():
            logger.warning('Worker slot dead before job — respawning')
            self._kill_and_respawn()

        proc = self._proc  # capture; may change on respawn

        def _write_and_read() -> bytes:
            proc.stdin.write((job_json + '\n').encode('utf-8'))
            proc.stdin.flush()
            return proc.stdout.readline()

        try:
            raw = await asyncio.wait_for(
                asyncio.to_thread(_write_and_read),
                timeout=timeout,
            )
        except asyncio.TimeoutError:
            logger.error('Worker slot timeout (%.0fs) — respawning', timeout)
            self._kill_and_respawn()
            raise
        except Exception as exc:
            logger.error('Worker slot I/O error: %s — respawning', exc)
            self._kill_and_respawn()
            raise

        if not raw:
            self._kill_and_respawn()
            raise RuntimeError('Worker returned empty stdout — process died mid-job')

        return json.loads(raw.decode('utf-8', errors='replace'))


class WorkerPool:
    """Fixed-size pool of persistent WorkerSlots.

    Callers await run_job(); the internal asyncio.Queue<WorkerSlot> guarantees
    that at most `size` jobs run concurrently without any explicit semaphore.
    """

    def __init__(self, size: int = 4) -> None:
        self.size  = size
        self.slots = [WorkerSlot(WORKER_EXE) for _ in range(size)]
        self._available: asyncio.Queue[WorkerSlot] = asyncio.Queue()

    def start(self) -> None:
        for slot in self.slots:
            slot.start()
            self._available.put_nowait(slot)
        logger.info('WorkerPool started  size=%d  exe=%s', self.size, WORKER_EXE)

    async def run_job(self, job_json: str, timeout: float = 30.0) -> dict[str, Any]:
        slot = await self._available.get()
        try:
            return await slot.run(job_json, timeout=timeout)
        finally:
            await self._available.put(slot)

    def shutdown(self) -> None:
        for slot in self.slots:
            slot.shutdown()
        logger.info('WorkerPool shut down')
