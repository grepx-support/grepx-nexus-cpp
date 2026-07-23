# Nexus — quick start

All commands run from `scripts/`.

## Server

```
./setup.sh                  # one-time: create DB schema, seed demo user
./start_nexus_server.sh     # start (leave running in its own terminal)
./stop_nexus_server.sh      # stop
./status_nexus_server.sh    # is it up, recent log lines
```

## Client

```
./start_client.sh order_demo    # or: trade_demo
./stop_client.sh order_demo     # or: trade_demo
```

order_demo: http://localhost:5000/orders
trade_demo: http://localhost:5001/trades

## Check the data

```
./inspect_db.sh
```
