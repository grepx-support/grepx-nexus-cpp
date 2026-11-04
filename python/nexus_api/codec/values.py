# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Encode/decode a single field value using Nexus's wire tag scheme
(i:/d:/b:/s:/n: — see libs/protocol/include/nexus/protocol/codec.hpp).
"""
from __future__ import annotations


def encode_value(value) -> str:
    if value is None:
        return "n:"
    if isinstance(value, bool):
        return "b:true" if value else "b:false"
    if isinstance(value, int):
        return f"i:{value}"
    if isinstance(value, float):
        return f"d:{value}"
    if isinstance(value, str):
        if ";" in value or "=" in value:
            raise ValueError("field string values cannot contain ';' or '='")
        return f"s:{value}"
    raise TypeError(f"unsupported value type: {type(value)!r}")


def decode_value(text: str):
    tag, _, body = text.partition(":")
    if tag == "n":
        return None
    if tag == "i":
        return int(body)
    if tag == "d":
        return float(body)
    if tag == "b":
        # "true"/"false" is this module's own wire format (see
        # encode_value above); "1"/"0" is what nexus_server's C++
        # SqliteSnapshotStore writes to nexus_rows/outbox on disk (see
        # decode_storage_fields() in fields.py) — accepting both here
        # means one decode_value works for either source.
        return body in ("true", "1")
    if tag == "s":
        return body
    raise ValueError(f"unrecognised value tag: {text!r}")
