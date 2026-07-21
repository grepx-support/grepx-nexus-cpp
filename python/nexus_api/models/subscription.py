"""Handle returned by NexusClient.subscribe() — closes the background
listener thread and its connection.

Depends on ConnectionInterface, not any concrete connection class — this
model has no idea (and shouldn't need to) whether it's holding a real
TCP socket or a test double."""
from __future__ import annotations

import threading
from dataclasses import dataclass

from ..interfaces.connection_interface import ConnectionInterface


@dataclass
class Subscription:
    _conn: ConnectionInterface
    _thread: threading.Thread
    _stop: threading.Event

    def close(self) -> None:
        self._stop.set()
        self._conn.close()
