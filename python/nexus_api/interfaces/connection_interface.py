"""ConnectionInterface — the contract one line-buffered connection to
nexus_server must satisfy. impl/tcp_connection.py is the only concrete
implementation today; the point of the split is that NexusClient (and
Subscription) depend on this abstraction, not on sockets directly, so a
test double or an alternate transport can stand in without touching
anything upstream — same reasoning as the C++ side's ports::* concepts.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Optional


class ConnectionInterface(ABC):
    @abstractmethod
    def send_line(self, line: str) -> None:
        """Sends one line (newline-delimited) to nexus_server."""

    @abstractmethod
    def recv_line(self) -> Optional[str]:
        """Blocks for the next line from nexus_server; None means the
        peer closed the connection."""

    @abstractmethod
    def close(self) -> None:
        """Closes the underlying connection."""
