# Cosmos Wallet: A Bitcoin (SV) wallet

Version 0.0.1 alpha.

## Features:

* send and receive funds over BSV.
* keep utxos tiny and approximately logorithmically distributed.
* calculate capital gain.

## Known issues:

* recovery bug: the first time you recover a wallet, the account comes out incorrect. You can
  repair it by deleting all generated files except txdb.json and running the restore command again.
* Problems with recovering wallets that have unmined transactions.

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
To run the application in a Docker container, use the following command:

```bash
docker run -it -v .:/cosmos gigamonkey/cosmos <command>
```

### Explanation:

- `-it`: This runs the container in interactive mode with a terminal session.
- `-v .:/cosmos`: This mounts the current directory (.) to the `/cosmos` directory inside the container. If you want to store the wallet files elsewhere, replace the . with the path to that location.
- `gigamonkey/cosmos`: This is the Docker image name. 
- `<command>`: Replace this placeholder with the specific command you want to run.

Before running the command, make sure Docker is installed and running on your system.

## Run in Linux

You would have to figure out how to get all the dependencies and build the whole thing.

The key dependencies are
 * Gigamonkey
 * data
