#!/usr/bin/env python3
# Copyright (c) 2026 GrepX Private Limited (grepx.sg). All rights reserved.
# This file is part of the GrepX platform and is proprietary and confidential.
# Unauthorised copying, use, modification or distribution is strictly prohibited.

"""Adds/updates one row in the "users" table nexus_server's DbAuthenticator
checks against (see libs/authentication/include/nexus/authentication/db_authenticator.hpp).
Without this, every AUTH attempt fails — there's nothing to match against.

Password is hashed the same way DbAuthenticator verifies it: plain
SHA-256, no salt. Fine for local demo use; swap for something salted
(bcrypt/argon2) on both sides before this goes anywhere real — see the
caveat in db_authenticator.hpp.

Normally you don't run this directly — scripts/setup.sh calls it (along
with create_schema.py) as step one of bringing the whole stack up.

Usage:
    python python/seed_user.py <username> <password> [db_path]

If db_path is omitted, applies to every DB create_schema.py would also
apply to (repo root, NEXUS_DB_PATH, any built nexus_server's directory).
"""
import hashlib
import sqlite3
import sys
from pathlib import Path

from create_schema import resolve_db_paths


def hash_password(password: str) -> str:
    return hashlib.sha256(password.encode()).hexdigest()


def main() -> None:
    if len(sys.argv) < 3:
        print("usage: python seed_user.py <username> <password> [db_path]", file=sys.stderr)
        sys.exit(1)

    username, password = sys.argv[1], sys.argv[2]
    explicit_path = sys.argv[3] if len(sys.argv) > 3 else None
    db_paths = resolve_db_paths(explicit_path)

    for db_path in db_paths:
        conn = sqlite3.connect(db_path)
        try:
            conn.execute(
                "INSERT INTO users(username, password_hash) VALUES (?, ?) "
                "ON CONFLICT(username) DO UPDATE SET password_hash = excluded.password_hash",
                (username, hash_password(password)),
            )
            conn.commit()
            print(f"user '{username}' set in {db_path.resolve()}")
        finally:
            conn.close()


if __name__ == "__main__":
    main()
