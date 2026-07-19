"""NexusClient — the four operations a service needs against nexus_server:
INSERT, SELECT, SUBSCRIBE, TRANSITION. See
libs/protocol/include/nexus/protocol/codec.hpp for the wire format this
implements.
"""
from __future__ import annotations

import threading
from typing import Callable, Optional

from .connection import Connection
from .fields import decode_fields, encode_fields
from .row import Row
from .subscription import Subscription


class NexusClient:
    """INSERT/SELECT/TRANSITION each open a short-lived connection — fine
    at demo volume; a long-running service would keep a connection pool
    instead."""

    def __init__(self, host: str = "127.0.0.1", port: int = 7878):
        self.host = host
        self.port = port

    def insert(self, table: str, key: str, fields: dict) -> None:
        conn = Connection(self.host, self.port)
        try:
            conn.send_line(f"INSERT {table} {key} {encode_fields(fields)}")
            response = conn.recv_line()
            if response is None or not response.startswith("OK"):
                raise RuntimeError(f"insert failed: {response}")
        finally:
            conn.close()

    def select(self, table: str, criteria: str = "") -> list[Row]:
        conn = Connection(self.host, self.port)
        rows: list[Row] = []
        try:
            conn.send_line(f"SELECT {table} {criteria}")
            while True:
                line = conn.recv_line()
                if line is None or line == "END":
                    break
                if line.startswith("ROW "):
                    _, key, fields_text = line.split(" ", 2)
                    rows.append(Row(key, decode_fields(fields_text)))
        finally:
            conn.close()
        return rows

    def transition(self, table: str, key: str, from_status: str, to_status: str,
                    expected_version: int, fields: dict) -> int:
        """Returns the new version on success; raises on conflict or an
        illegal transition (see libs/statemachine/.../transaction_transitions.hpp
        for the allowed from->to table)."""
        conn = Connection(self.host, self.port)
        try:
            conn.send_line(
                f"TRANSITION {table} {key} {from_status} {to_status} "
                f"{expected_version} {encode_fields(fields)}"
            )
            response = conn.recv_line()
            if response is None:
                raise RuntimeError("no response from Nexus")
            if response.startswith("OK"):
                return int(response.split(" ", 1)[1])
            raise RuntimeError(f"transition failed: {response}")
        finally:
            conn.close()

    def subscribe(self, table: str, criteria: str, on_event: Callable[[Row], None],
                   subscription_id: Optional[str] = None) -> Subscription:
        """Opens a dedicated long-lived connection and calls on_event(row)
        on a background thread for every insert/update matching criteria.
        The server itself filters by predicate before sending EVENT lines
        (see DispatchService) — on_event only ever sees matching rows."""
        conn = Connection(self.host, self.port)
        sub_id = subscription_id or f"py-{id(conn)}"
        conn.send_line(f"SUBSCRIBE {sub_id} {table} {criteria}")
        ack = conn.recv_line()
        if ack is None or not ack.startswith("OK"):
            conn.close()
            raise RuntimeError(f"subscribe failed: {ack}")

        stop = threading.Event()

        def _loop():
            while not stop.is_set():
                line = conn.recv_line()
                if line is None:
                    return
                if line.startswith("EVENT "):
                    _, _table, key, fields_text = line.split(" ", 3)
                    on_event(Row(key, decode_fields(fields_text)))

        thread = threading.Thread(target=_loop, daemon=True)
        thread.start()
        return Subscription(conn, thread, stop)
