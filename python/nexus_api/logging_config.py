# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Standard library `logging` setup, shared by nexus_api and every demo
entrypoint (flask_frontend.py, main.py, ...).

Timestamps carry full nanosecond precision. logging.Formatter only goes
to milliseconds out of the box (%(msecs)d comes from time.time(), a
float — its resolution is a float's precision, not the OS clock's), so
NanosecondFormatter overrides formatTime() to use time.time_ns() instead,
which is exactly what the C++ side's nexus::logging::ConsoleLogger does
(see libs/logging/include/nexus/logging/timestamp.hpp) — same width,
same idea, so C++ and Python log lines line up when read side by side.

Every entrypoint also gets a log file, not just console output — same
reasoning as the C++ side's CompositeLogger (console output alone
disappears when the terminal closes or scrolls past). Default location
is <repo_root>/logs/<script-name>.log, one file per process (frontend
and worker don't share a file, so neither has to guess which lines are
its own) — mirrors nexus_server's own logs/nexus_server.log, just next
to the repo root instead of the C++ executable since Python scripts
don't have a meaningful "next to the binary" location.
"""
from __future__ import annotations

import logging
import sys
import time
from pathlib import Path
from typing import Optional, Union

REPO_ROOT = Path(__file__).resolve().parent.parent.parent


class NanosecondFormatter(logging.Formatter):
    default_time_format = "%Y-%m-%d %H:%M:%S"

    def formatTime(self, record: logging.LogRecord, datefmt: str | None = None) -> str:
        ns = time.time_ns()
        secs, nanos = divmod(ns, 1_000_000_000)
        formatted = time.strftime(self.default_time_format, time.localtime(secs))
        return f"{formatted}.{nanos:09d}"


def _default_log_file() -> Path:
    script_name = Path(sys.argv[0]).stem or "nexus"
    return REPO_ROOT / "logs" / f"{script_name}.log"


def configure_logging(level: int = logging.INFO,
                      log_file: Optional[Union[str, Path]] = None) -> Path:
    """Call once, at the top of each entrypoint's __main__ block (or
    module scope for Flask apps run via `python flask_frontend.py`).
    Idempotent — safe to call more than once in the same process; only
    the first call actually attaches handlers.

    Returns the resolved log file path, mainly so the caller can print
    "logs at <path>" if useful — status.sh also knows this convention to
    find each demo's log without needing it passed explicitly.
    """
    root = logging.getLogger()
    if root.handlers:
        return _default_log_file() if log_file is None else Path(log_file)

    formatter = NanosecondFormatter("%(asctime)s [%(levelname)s] %(name)s: %(message)s")

    console_handler = logging.StreamHandler()
    console_handler.setFormatter(formatter)
    root.addHandler(console_handler)

    resolved_log_file = Path(log_file) if log_file is not None else _default_log_file()
    resolved_log_file.parent.mkdir(parents=True, exist_ok=True)
    file_handler = logging.FileHandler(resolved_log_file, encoding="utf-8")
    file_handler.setFormatter(formatter)
    root.addHandler(file_handler)

    root.setLevel(level)
    return resolved_log_file
