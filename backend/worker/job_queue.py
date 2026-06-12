import asyncio

QUEUE_MAX_SIZE = 50


class JobQueue:
    """Thin wrapper around asyncio.Queue with a bounded maxsize.

    enqueue() is non-blocking and returns False when the queue is full,
    so the caller can immediately respond with HTTP 503 instead of waiting.
    """

    def __init__(self, maxsize: int = QUEUE_MAX_SIZE) -> None:
        self._q: asyncio.Queue[str] = asyncio.Queue(maxsize=maxsize)

    def enqueue(self, job_id: str) -> bool:
        try:
            self._q.put_nowait(job_id)
            return True
        except asyncio.QueueFull:
            return False

    async def dequeue(self) -> str:
        return await self._q.get()

    def qsize(self) -> int:
        return self._q.qsize()
