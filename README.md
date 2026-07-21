# Nexus — quick start

## 1. Compile

Open the repo in CLion (`CMakeLists.txt` at the root) and build the
`nexus_server` target — CLion's Build button, or from the top menu:
Build | Build Project.

Command-line equivalent, from the repo root:

```
cmake -B cmake-build-debug
cmake --build cmake-build-debug --target nexus_server
```

This produces `cmake-build-debug/apps/nexus_server/nexus_server.exe`.

## 2. Set up the database

From `scripts/`:

```
./setup.sh
```

Creates the SQLite schema and seeds a demo user (`demo`/`demo`). Only
needs to be run once — safe to re-run any time, it won't wipe existing
data.

## 3. Start the server

From `scripts/`:

```
./run.sh
```

Finds the built `nexus_server` executable and starts it. Leave this
running in its own terminal — it logs every request/response to the
console and to `logs/nexus_server.log` next to the executable.

To stop it from a different terminal instead of Ctrl+C:

```
./stop_server.sh
```

## 4. Start a client

From `scripts/`, in a second terminal:

```
./start_client.sh order_demo
```

or

```
./start_client.sh trade_demo
```

Sets up that demo's Python virtual environment (first run only, then
reuses it), waits for the server to be reachable, and starts it. Each
demo runs a Flask dashboard (order_demo: http://localhost:5000/orders,
trade_demo: http://localhost:5001/trades) plus a background worker.

To stop it from a different terminal instead of Ctrl+C:

```
./stop_client.sh order_demo
```

## Checking things are working

```
./status.sh        # is nexus_server up, recent log lines
./inspect_db.sh     # dump every table's actual contents
```

## Troubleshooting

- **`run.sh` says "no built nexus_server found"** — build the target
  first (step 1).
- **`run.sh` warns the binary is stale** — rebuild; the running exe
  predates a source change.
- **A client fails to log in with "unauthorized"** — run `./setup.sh`
  (step 2) if you haven't yet, or check `NEXUS_USERNAME`/`NEXUS_PASSWORD`
  match what was seeded.
- **Anything else** — `./status.sh` and `./inspect_db.sh` are the first
  two things to check; they show whether the server is up and whether
  data is actually landing in the database.
