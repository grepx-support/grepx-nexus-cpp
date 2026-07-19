"""Handle returned by NexusClient.subscribe() — closes the background
listener thread and its connection."""
from __future__ import annotations

import threading
from dataclasses import dataclass

from .connection import Connection


@dataclass
class Subscription:
    _conn: Connection
    _thread: threading.Thread
    _stop: threading.Event

    def close(self) -> None:
        self._stop.set()
        self._conn.close()
