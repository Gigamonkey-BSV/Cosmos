FROM gigamonkey/gigamonkey-base-dev:v1.1.4 AS build

#data
WORKDIR /tmp
ADD https://api.github.com/repos/DanielKrawisz/data/git/refs/heads/master /root/data_version.json
RUN git clone --depth 1 --branch master https://github.com/DanielKrawisz/data.git
WORKDIR /tmp/data
RUN cmake -G Ninja -B build -S . -DPACKAGE_TESTS=OFF 
RUN cmake --build build -j 10
RUN cmake --install build

#gigamonkey
WORKDIR /tmp
ADD https://api.github.com/repos/Gigamonkey-BSV/Gigamonkey/git/refs/heads/master /root/gigamonkey_version.json
RUN git clone --depth 1 --branch master https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /tmp/Gigamonkey
RUN cmake -G Ninja -B build -S . -DPACKAGE_TESTS=OFF
RUN cmake --build build -j 10
RUN cmake --install build
