# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""RecoveringSubscriber — the reusable "start up without missing or
double-guessing anything" sequence for a service that reacts to Nexus
row changes.

The problem this solves: if you just call client.subscribe() and start
acting on events as they arrive, you have no idea what happened to
transaction_id "abc" *before* you connected — every row that was
already sitting in the snapshot table (submitted while you were down,
or before you ever started) is invisible to a plain subscription, which
only pushes *future* changes. React only to live events and you silently
ignore an unbounded backlog of already-pending work every time the
process restarts.

The fix is a fixed four-step startup order, not two independent steps
run in either order:

  1. Subscribe FIRST, before reading anything — but buffer every event
     instead of acting on it. If step 1 ran after step 2 instead, a row
     that changed in the gap between "recovery finished reading" and
     "subscription started" would be missed entirely; buffering removes
     that gap by making sure nothing landing from this point on is ever
     lost, before we've even looked at what already exists.
  2. Recover: SELECT the table for whatever "pending" means for this
     application (a criteria string, e.g. "status != 'ACCEPTED'"),
     and run on_event for each row found — this is the "build a local
     cache of pending work" step.
  3. Drain the buffer collected in step 1, in the same on_event, in
     order. Some of these may be redundant with what recovery just
     loaded (a row that both existed before *and* changed again during
     recovery) — on_event needs to be safe to call more than once for
     the same transaction_id; that's a property of the business logic,
     not something this class can enforce.
  4. Go live: every event from here on is handled the moment it
     arrives, no buffering.

None of this is specific to any one demo's business logic — on_event is
just a callback — which is why it lives here in the SDK rather than
being copy-pasted into order_demo and trade_demo separately.
"""
from __future__ import annotations

import logging
import threading
from collections import deque
from typing import Callable, Deque

from ..interfaces.client_interface import ClientInterface
from ..models.row import Row
from ..models.subscription import Subscription

logger = logging.getLogger(__name__)

OnEvent = Callable[[Row], None]


class RecoveringSubscriber:
    def __init__(self, client: ClientInterface, table: str, recovery_criteria: str,
                 on_event: OnEvent, live_criteria: str = ""):
        self._client = client
        self._table = table
        self._recovery_criteria = recovery_criteria
        self._live_criteria = live_criteria
        self._on_event = on_event
        self._buffer: Deque[Row] = deque()
        self._lock = threading.Lock()
        self._live = False
        self._subscription: Subscription | None = None
        self._started = False

    def start(self) -> "RecoveringSubscriber":
        if self._started:
            raise RuntimeError("RecoveringSubscriber.start() called more than once")
        self._started = True

        # Step 1 — subscribe before reading anything, buffer-only.
        self._subscription = self._client.subscribe(self._table, self._live_criteria, self._on_live_event)
        logger.info("recovery: subscribed table=%s (buffering until recovery completes)", self._table)

        # Step 2 — recover: load whatever's currently pending and seed
        # the caller's cache with it.
        recovered = self._client.select(self._table, self._recovery_criteria)
        logger.info("recovery: found %d pending row(s) matching %r", len(recovered), self._recovery_criteria)
        for row in recovered:
            self._on_event(row)

        # Step 3 & 4 — drain whatever arrived while we were recovering,
        # then flip live, atomically with respect to _on_live_event (see
        # its docstring for why this can't be two separate steps without
        # a race that drops an event).
        self._go_live()
        logger.info("recovery: complete, table=%s now live", self._table)
        return self

    def _on_live_event(self, row: Row) -> None:
        # Runs on the subscription's background thread (see
        # NexusClient.subscribe) — same as any other Nexus subscriber,
        # a slow on_event here delays reading the next EVENT line off
        # the wire. Not a new constraint this class introduces.
        with self._lock:
            if not self._live:
                self._buffer.append(row)
                return
        self._on_event(row)

    def _go_live(self) -> None:
        # Snapshotting the buffer, clearing it, and flipping _live all
        # happen under one lock acquisition — if this were three
        # separate steps, an event arriving between "clear" and "flip
        # live" would be silently dropped (too late to land in the
        # snapshot just drained, too early to be treated as live).
        with self._lock:
            pending = list(self._buffer)
            self._buffer.clear()
            self._live = True
        for row in pending:
            self._on_event(row)

    def close(self) -> None:
        if self._subscription is not None:
            self._subscription.close()
