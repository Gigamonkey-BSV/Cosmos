FROM gigamonkey/gigamonkey-lib:v1.0.0 AS build

COPY . /home/cosmos
WORKDIR /home/cosmos
RUN chmod -R 777 .
RUN cmake  -B build -S . -DCMAKE_BUILD_TYPE=Release
RUN cmake --build build

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
CMD ["/bin/CosmosWallet"]