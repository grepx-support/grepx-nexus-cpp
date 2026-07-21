"""TcpConnection — the concrete, socket-backed ConnectionInterface.
Line-buffered exactly like nexus::net::Socket
(libs/net/include/nexus/net/socket.hpp).

Every line sent and received is logged in full here — the single choke
point every NexusClient call funnels through — mirroring nexus_server's
own REQUEST/RESPONSE logging in apps/nexus_server/src/main.cpp (see the
`send` lambda there).
"""
from __future__ import annotations

import itertools
import logging
import socket
from typing import Optional

from ..interfaces.connection_interface import ConnectionInterface

logger = logging.getLogger(__name__)

_next_conn_id = itertools.count(1)


class TcpConnection(ConnectionInterface):
    def __init__(self, host: str, port: int):
        self._sock = socket.create_connection((host, port))
        self._buf = b""
        # Purely for log correlation, same idea as nexus_server's conn_id —
        # one NexusClient can hold two connections at once (the persistent
        # one plus a dedicated subscribe one), so tagging lines by id keeps
        # their REQUEST/RESPONSE pairs distinguishable in an interleaved log.
        self.conn_id = next(_next_conn_id)
        logger.debug("conn=%d connected to %s:%d", self.conn_id, host, port)

    def send_line(self, line: str) -> None:
        logger.info("conn=%d REQUEST: %s", self.conn_id, line)
        self._sock.sendall((line + "\n").encode())

    def recv_line(self) -> Optional[str]:
        while b"\n" not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                logger.debug("conn=%d connection closed by peer", self.conn_id)
                return None
            self._buf += chunk
        line, _, self._buf = self._buf.partition(b"\n")
        text = line.decode().rstrip("\r")
        logger.info("conn=%d RESPONSE: %s", self.conn_id, text)
        return text

    def close(self) -> None:
        logger.debug("conn=%d closing", self.conn_id)
        self._sock.close()
