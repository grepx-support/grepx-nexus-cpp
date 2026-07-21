"""The SDK's public contracts — what nexus_api promises callers, with
none of the socket/threading detail underneath. impl/ holds the concrete
classes that satisfy these; callers depending on ClientInterface or
ConnectionInterface (type hints, mocks in tests, a future alternate
transport) never need to know impl/ exists.
"""
from .client_interface import ClientInterface
from .connection_interface import ConnectionInterface

__all__ = ["ClientInterface", "ConnectionInterface"]
