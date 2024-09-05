# Cosmos Wallet: A Bitcoin (SV) wallet

Version 0.0.1 alpha.

## Features:

* send and receive funds over BSV.
* keep utxos tiny and approximately logorithmically distributed.
* calculate capital gain.

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
