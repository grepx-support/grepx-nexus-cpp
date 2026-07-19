"""Python client library for nexus_server's wire protocol.

    from nexus_api import NexusClient, Row, NexusApplication

Layout (small, single-purpose files, mirroring the C++ side's style):
    values.py       encode/decode one field value (i:/d:/b:/s:/n: tags)
    fields.py       encode/decode a whole field set, built on values.py
    row.py          Row — key + fields
    connection.py   one line-buffered TCP connection
    subscription.py handle returned by NexusClient.subscribe()
    client.py       NexusClient — insert / select / subscribe / transition
    envelope.py     status/application/username/occurred_at, merged onto every row
    application.py  NexusApplication — the reusable stage-tables + snapshot
                    + state-machine model for building a new app on Nexus
"""
from .application import NexusApplication
from .client import NexusClient
from .row import Row
from .subscription import Subscription

__all__ = ["NexusClient", "NexusApplication", "Row", "Subscription"]
