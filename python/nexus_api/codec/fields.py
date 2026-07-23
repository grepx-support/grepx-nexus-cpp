"""Encode/decode a row's whole field set: "k1=type:v1;k2=type:v2;..."."""
from __future__ import annotations

from .values import decode_value, encode_value


def encode_fields(fields: dict) -> str:
    return ";".join(f"{k}={encode_value(v)}" for k, v in fields.items())


def decode_fields(text: str) -> dict:
    if not text:
        return {}
    result = {}
    for pair in text.split(";"):
        key, _, value = pair.partition("=")
        result[key] = decode_value(value)
    return result


def decode_storage_fields(text: str) -> dict:
    """Decodes the format nexus_server's C++ SqliteSnapshotStore actually
    writes to nexus_rows.fields / outbox.fields on disk — NOT the same
    wire format as decode_fields() above (which is what NexusClient sends/
    receives over the TCP connection). The on-disk format uses ASCII
    control characters as delimiters — '\\x1e' (record separator) between
    a key and its type:value, '\\x1f' (unit separator) between pairs —
    instead of '='/';', specifically so a field value that happens to
    contain a literal '=' or ';' can never be misparsed as a delimiter.
    See libs/persistence/include/nexus/persistence/sqlite_snapshot_store.hpp's
    detail::encode_fields/decode_fields for the canonical C++ version this
    mirrors; if one side changes delimiters, the other must too.
    """
    if not text:
        return {}
    result = {}
    for pair in text.split("\x1f"):
        key, sep, value = pair.partition("\x1e")
        if not sep:
            continue
        result[key] = decode_value(value)
    return result
