# Cosmos Wallet: A Bitcoin (SV) wallet

Version 0.0.2 alpha.

## Features:

* send and receive funds over BSV.
* keep utxos tiny and approximately logorithmically distributed.

## Use with Docker (linux and windows)

Make sure Docker is installed and running on your system.

```bash
docker build -t cosmos .
```

Once it is built, it can be run with. 

```bash
docker run cosmos <args...>
```

## Use without Docker (Linux)

You would have to figure out how to get all the dependencies and build the whole thing.

The key dependencies are
 * data
 * Gigamonkey
 * BitcoinCalculator

## Next features to be implemented for version 2

* pay method
* receive method
* sign method
* calculate capital gain.

## Plan for version 3

* p2p
* HTTP interface
* graphic interface in a web browser
