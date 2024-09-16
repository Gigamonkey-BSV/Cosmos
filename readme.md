# Cosmos Wallet: A Bitcoin (SV) wallet

Version 0.0.1 alpha.

## Features:

* send and receive funds over BSV.
* keep utxos tiny and approximately logorithmically distributed.
* calculate capital gain.

## Known issues:

* recovery bug: the first time you recover a wallet, the account comes out incorrect. You can
  repair it by deleting all generated files except txdb.json and running the restore command again.
* Random number generator: the program is slower than necessary because we use a cryptographic
  pseudo random number generator for everything even if we don't need it.
* segfault during coin splitting.

## Next features to be implemented for version 1

* config file
* pay method
* receive method
* sign method

## Plan for version 2

* p2p
* HTTP interface
* graphic interface in a web browser

## Run with Docker

The easiest way to use Cosmos Wallet is with Docker.

1. Download this repo.
2. `docker build .` to construct a docker file that can be run in any environment. This will take a long time
   but it does not need to be repeated unless you want to upgrade the program.
   If there is a problem, try `docker build . --no-cache` to build the docker container from scratch.

## Run in Linux

You would have to figure out how to get all the dependencies and build the whole thing.

The key dependencies are
 * Gigamonkey
 * data
