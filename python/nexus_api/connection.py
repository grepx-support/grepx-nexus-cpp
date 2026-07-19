"""One TCP connection to nexus_server, line-buffered exactly like
nexus::net::Socket (libs/net/include/nexus/net/socket.hpp)."""
from __future__ import annotations

import socket
from typing import Optional


class Connection:
    def __init__(self, host: str, port: int):
        self._sock = socket.create_connection((host, port))
        self._buf = b""

    def send_line(self, line: str) -> None:
        self._sock.sendall((line + "\n").encode())

    def recv_line(self) -> Optional[str]:
        while b"\n" not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                return None
            self._buf += chunk
        line, _, self._buf = self._buf.partition(b"\n")
        return line.decode().rstrip("\r")

    def close(self) -> None:
        self._sock.close()
