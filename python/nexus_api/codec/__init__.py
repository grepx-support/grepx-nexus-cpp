"""Wire encoding/decoding — pure functions, no state, nothing to swap an
implementation of, so these are plain modules rather than interface +
impl. See libs/protocol/include/nexus/protocol/codec.hpp for the C++
side of the same wire format.
"""
from .fields import decode_fields, decode_storage_fields, encode_fields
from .values import decode_value, encode_value

__all__ = ["encode_value", "decode_value", "encode_fields", "decode_fields", "decode_storage_fields"]
