"""Python SDK for nexus_server's wire protocol.

    from nexus_api import NexusClient, Row, NexusApplication

Layout — interface separated from implementation, one class per file:
    interfaces/     the SDK's public contracts (ClientInterface,
                     ConnectionInterface) — what callers, tests, and
                     NexusApplication actually depend on
    impl/           concrete implementations (NexusClient, TcpConnection)
                     of those contracts — internal; only this file
                     imports from here
    models/         plain data types (Row, Subscription)
    codec/          wire encode/decode, pure functions
    application/    NexusApplication — the reusable stage-tables +
                     snapshot + state-machine model for building a new
                     app on Nexus
    logging_config.py   nanosecond-precision logging setup, shared by
                         every entrypoint

NexusClient is exported as the concrete class (callers need something to
instantiate) but implements ClientInterface — code that only needs to
*use* a client, not construct one, should type-hint against
ClientInterface instead, the same way application/nexus_application.py
does.
"""
from .application import NexusApplication
from .impl.nexus_client import NexusClient
from .interfaces import ClientInterface, ConnectionInterface
from .models import Row, Subscription

__all__ = [
    "NexusClient",
    "NexusApplication",
    "Row",
    "Subscription",
    "ClientInterface",
    "ConnectionInterface",
]
