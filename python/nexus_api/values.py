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
        return body == "true"
    if tag == "s":
        return body
    raise ValueError(f"unrecognised value tag: {text!r}")
