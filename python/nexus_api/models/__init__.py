# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Plain data types passed across the SDK boundary — no behavior beyond
what a dataclass gives for free, so these don't need an interface split
of their own (there's nothing to swap an implementation of)."""
from .row import Row
from .subscription import Subscription

__all__ = ["Row", "Subscription"]
