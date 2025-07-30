# use 'docker build -t cosmos .'.

FROM gigamonkey/gigamonkey-lib:v2.4 AS build

COPY . /home/cosmos
WORKDIR /home/cosmos
RUN chmod -R 777 .
RUN cmake  -G Ninja -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build -j 4

# we copy the result into a smaller version of ubuntu.
FROM ubuntu:22.04
RUN apt-get -y update &&  \
    apt-get install  --no-install-recommends \
    -y libssl-dev \
    libcrypto++-dev \
    libcrypto++-doc \
    libcrypto++-utils \
    libgmp3-dev \
        ca-certificates &&\
    rm -rf /var/lib/apt/lists/*

COPY --from=build /usr/local/lib/libsecp256k1.so.2 /usr/local/lib/libsecp256k1.so.2
COPY --from=build /home/cosmos/build/CosmosWallet /bin/CosmosWallet
WORKDIR /cosmos
ENTRYPOINT [ "/bin/CosmosWallet" ]
CMD ["help"]
