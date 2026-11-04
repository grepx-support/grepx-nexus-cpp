# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""ClientInterface — the operations a service needs against nexus_server:
INSERT, SELECT, SUBSCRIBE, SUBMIT (and TRANSITION, kept for lower-level
callers that already have an expected_version in hand). impl/nexus_client.py
is the concrete implementation; NexusApplication (application/) depends
on this abstraction rather than the concrete class, so it never needs to
know how a request actually reaches the server.
"""
from __future__ import annotations

from abc import ABC, abstractmethod
from typing import Callable, Optional

from ..models.row import Row
from ..models.subscription import Subscription


class ClientInterface(ABC):
    @abstractmethod
    def insert(self, table: str, key: str, fields: dict) -> None:
        """Inserts/upserts one row."""

    @abstractmethod
    def select(self, table: str, criteria: str = "") -> list[Row]:
        """Returns every row in `table` matching `criteria` (empty = all)."""

    @abstractmethod
    def transition(self, table: str, key: str, from_status: str, to_status: str,
                   expected_version: int, fields: dict) -> int:
        """Compare-and-swap status transition. Returns the new version;
        raises on conflict or an illegal transition."""

    @abstractmethod
    def submit(self, table: str, key: str, fields: dict) -> int:
        """One business transaction, submitted once. The server reads the
        row's current status itself and validates the transition
        server-side (see libs/application/submit_service.hpp) — unlike
        transition(), callers never supply from_status or
        expected_version, because there's nothing for them to have read
        first that could go stale. `fields` must include a "status" key;
        that's both the row's new status and, along with whatever
        row currently exists (if any), what the server's state machine
        validates against. Returns the new version; raises on an illegal
        transition."""

    @abstractmethod
    def subscribe(self, table: str, criteria: str, on_event: Callable[[Row], None],
                  subscription_id: Optional[str] = None) -> Subscription:
        """Streams every insert/update matching criteria to on_event,
        called on a background thread, until the returned Subscription
        is closed."""

    @abstractmethod
    def close(self) -> None:
        """Closes the persistent request/response connection, if open."""
