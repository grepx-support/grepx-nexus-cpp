# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""The SDK's public contracts — what nexus_api promises callers, with
none of the socket/threading detail underneath. impl/ holds the concrete
classes that satisfy these; callers depending on ClientInterface or
ConnectionInterface (type hints, mocks in tests, a future alternate
transport) never need to know impl/ exists.
"""
from .client_interface import ClientInterface
from .connection_interface import ConnectionInterface

__all__ = ["ClientInterface", "ConnectionInterface"]
