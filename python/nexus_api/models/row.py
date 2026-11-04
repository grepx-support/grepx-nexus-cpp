# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""One row as returned by SELECT or delivered by an EVENT."""
from __future__ import annotations

from dataclasses import dataclass


@dataclass
class Row:
    key: str
    fields: dict
