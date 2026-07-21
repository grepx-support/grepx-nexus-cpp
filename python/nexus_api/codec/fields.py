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
