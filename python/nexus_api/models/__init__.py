"""Plain data types passed across the SDK boundary — no behavior beyond
what a dataclass gives for free, so these don't need an interface split
of their own (there's nothing to swap an implementation of)."""
from .row import Row
from .subscription import Subscription

__all__ = ["Row", "Subscription"]
