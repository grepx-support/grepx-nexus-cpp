"""NexusClient — the concrete ClientInterface implementation. See
libs/protocol/include/nexus/protocol/codec.hpp for the wire format this
implements.

Depends on ConnectionInterface, not TcpConnection directly — the
connection_factory constructor parameter is where that's injected
(defaulting to TcpConnection so callers don't have to think about it for
normal use). Swapping in a test double means passing a different
factory; NexusClient's own logic never changes.
"""
from __future__ import annotations

import logging
import threading
from typing import Callable, Optional

from ..codec.fields import decode_fields, encode_fields
from ..interfaces.client_interface import ClientInterface
from ..interfaces.connection_interface import ConnectionInterface
from ..models.row import Row
from ..models.subscription import Subscription
from .tcp_connection import TcpConnection

logger = logging.getLogger(__name__)

ConnectionFactory = Callable[[str, int], ConnectionInterface]


class NexusClient(ClientInterface):
    """One persistent, authenticated connection per NexusClient instance,
    reused for every INSERT/SELECT/TRANSITION call — nexus_server treats
    application_name as a stable per-client identity (see libs/session)
    and reconnecting for every single call would mean re-logging-in every
    single call too. subscribe() opens ONE additional dedicated
    connection instead, since it blocks on incoming EVENT pushes and
    can't share a socket with synchronous request/response calls.

    application_name + access_mode together must be unique among
    currently-active sessions — nexus_server rejects a second concurrent
    login using the same (application_name, access_mode) pair (see
    InMemorySessionRegistry). This client always uses access_mode
    "SUBSCRIBE" for the dedicated subscribe connection and whatever
    access_mode was passed to the constructor (default "ALL") for
    everything else, so the two never collide with each other — but two
    NexusClient instances sharing one application_name for the same role
    still will, by design.
    """

    def __init__(self, username: str, password: str, application_name: str,
                 access_mode: str = "ALL", host: str = "127.0.0.1", port: int = 7878,
                 connection_factory: ConnectionFactory = TcpConnection):
        self.host = host
        self.port = port
        self.username = username
        self.password = password
        self.application_name = application_name
        self.access_mode = access_mode
        self._connection_factory = connection_factory
        self._lock = threading.Lock()
        self._conn: Optional[ConnectionInterface] = None

    def _authenticate(self, conn: ConnectionInterface, access_mode: str) -> None:
        logger.info("connecting: application=%s access_mode=%s host=%s:%s",
                    self.application_name, access_mode, self.host, self.port)
        conn.send_line(f"AUTH {self.username} {self.password} {self.application_name} {access_mode}")
        response = conn.recv_line()
        if response is None or not response.startswith("OK"):
            conn.close()
            logger.warning("login failed: application=%s access_mode=%s response=%s",
                           self.application_name, access_mode, response)
            raise RuntimeError(
                f"login failed for application '{self.application_name}' ({access_mode}): {response}")
        logger.info("login OK: application=%s access_mode=%s", self.application_name, access_mode)

    def _get_conn(self) -> ConnectionInterface:
        """Returns the persistent request/response connection, (re)connecting
        and re-authenticating on first use or after a drop."""
        if self._conn is None:
            conn = self._connection_factory(self.host, self.port)
            self._authenticate(conn, self.access_mode)
            self._conn = conn
        return self._conn

    def insert(self, table: str, key: str, fields: dict) -> None:
        with self._lock:
            conn = self._get_conn()
            conn.send_line(f"INSERT {table} {key} {encode_fields(fields)}")
            response = conn.recv_line()
            if response is None or not response.startswith("OK"):
                logger.error("insert failed: table=%s key=%s response=%s", table, key, response)
                raise RuntimeError(f"insert failed: {response}")
            logger.debug("insert OK: table=%s key=%s", table, key)

    def select(self, table: str, criteria: str = "") -> list[Row]:
        with self._lock:
            conn = self._get_conn()
            rows: list[Row] = []
            conn.send_line(f"SELECT {table} {criteria}")
            while True:
                line = conn.recv_line()
                if line is None or line == "END":
                    break
                if line.startswith("ROW "):
                    _, key, fields_text = line.split(" ", 2)
                    rows.append(Row(key, decode_fields(fields_text)))
            logger.debug("select: table=%s criteria=%r rows=%d", table, criteria, len(rows))
            return rows

    def transition(self, table: str, key: str, from_status: str, to_status: str,
                   expected_version: int, fields: dict) -> int:
        """Returns the new version on success; raises on conflict or an
        illegal transition (see libs/statemachine/.../transaction_transitions.hpp
        for the allowed from->to table)."""
        with self._lock:
            conn = self._get_conn()
            conn.send_line(
                f"TRANSITION {table} {key} {from_status} {to_status} "
                f"{expected_version} {encode_fields(fields)}"
            )
            response = conn.recv_line()
            if response is None:
                logger.error("transition failed: no response, table=%s key=%s", table, key)
                raise RuntimeError("no response from Nexus")
            if response.startswith("OK"):
                new_version = int(response.split(" ", 1)[1])
                logger.debug("transition OK: table=%s key=%s %s->%s version=%d",
                            table, key, from_status, to_status, new_version)
                return new_version
            logger.warning("transition rejected: table=%s key=%s %s->%s response=%s",
                           table, key, from_status, to_status, response)
            raise RuntimeError(f"transition failed: {response}")

    def subscribe(self, table: str, criteria: str, on_event: Callable[[Row], None],
                  subscription_id: Optional[str] = None) -> Subscription:
        """Opens the dedicated long-lived SUBSCRIBE connection and calls
        on_event(row) on a background thread for every insert/update
        matching criteria. The server itself filters by predicate before
        sending EVENT lines (see DispatchService) — on_event only ever
        sees matching rows."""
        conn = self._connection_factory(self.host, self.port)
        self._authenticate(conn, "SUBSCRIBE")

        sub_id = subscription_id or f"py-{id(conn)}"
        conn.send_line(f"SUBSCRIBE {sub_id} {table} {criteria}")
        ack = conn.recv_line()
        if ack is None or not ack.startswith("OK"):
            conn.close()
            logger.error("subscribe failed: table=%s criteria=%r response=%s", table, criteria, ack)
            raise RuntimeError(f"subscribe failed: {ack}")
        logger.info("subscribed: table=%s criteria=%r subscription_id=%s", table, criteria, sub_id)

        stop = threading.Event()

        def _loop():
            while not stop.is_set():
                line = conn.recv_line()
                if line is None:
                    logger.info("subscription connection closed: subscription_id=%s", sub_id)
                    return
                if line.startswith("EVENT "):
                    _, _table, key, fields_text = line.split(" ", 3)
                    logger.debug("event: table=%s key=%s", _table, key)
                    on_event(Row(key, decode_fields(fields_text)))

        thread = threading.Thread(target=_loop, daemon=True)
        thread.start()
        return Subscription(conn, thread, stop)

    def close(self) -> None:
        """Closes the persistent request/response connection, if open.
        subscribe() connections are independent — close those via the
        returned Subscription instead."""
        with self._lock:
            if self._conn is not None:
                self._conn.close()
                self._conn = None
                logger.info("closed persistent connection: application=%s", self.application_name)
