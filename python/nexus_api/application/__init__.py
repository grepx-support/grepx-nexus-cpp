"""The reusable "one snapshot table, one submit() call, server-side
outbox and state machine" model for building a new application on top
of Nexus — plus RecoveringSubscriber, the subscribe/recover/drain/go-live
startup sequence every such application's consumer side needs. See
nexus_application.py and recovering_subscriber.py for the full shape.
"""
from .nexus_application import NexusApplication
from .recovering_subscriber import RecoveringSubscriber

__all__ = ["NexusApplication", "RecoveringSubscriber"]
