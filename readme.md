# Cosmos Wallet: A Bitcoin (SV) wallet

Version 0.0.2 alpha.

## Features:

* send and receive funds over BSV.
* keep utxos tiny and approximately log normally distributed for privacy.

## Use with Docker (linux and windows)

Make sure Docker is installed and running on your system.

```bash
docker build -t cosmos .
```

Once it is built, it can be run with. 

```bash
docker run -p <port>:<port> cosmos --port=<port> <args...>
```

where `<port>` is the port by which the program will be accessed via HTTP. 

## Use without Docker (Linux)

You would have to figure out how to get all the dependencies and build the whole thing.

The key dependencies are
 * data
 * Gigamonkey
 * BitcoinCalculator

## Running the software

Basic start: use `--port=4567` as the arguments (or whatever) and then go to `localhost:4567` to see the GUI.

### Arguments

* `--port=<port number>`
* `--db_type=<"sqlite">`: default is `sqlite`. We may support other databases in the future, so that's why this option is there.
* `--sqlite_path=<filepath>`
* `--sqlite_in_memory`: set instead of `sqlite_path` to use an in_memory db. (Testing only).

### API

Once the program runs, go to `localhost:<path>` to see the GUI. It's in JavaScript so you can read that if you want to understand the API.

## Next features to be implemented for version 2

* pay method
* receive method
* sign method
* calculate capital gain.

## Plan for version 3

* p2p
* HTTP interface
* graphic interface in a web browser
