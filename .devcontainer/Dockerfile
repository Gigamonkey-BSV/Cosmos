FROM gigamonkey/gigamonkey-base-dev:v1.1.3 AS build

#data
WORKDIR /tmp
ADD https://api.github.com/repos/DanielKrawisz/data/git/refs/heads/redo_cmake /root/data_version.json
RUN git clone --depth 1 --branch redo_cmake https://github.com/DanielKrawisz/data.git
WORKDIR /tmp/data
RUN cmake -B build -S . -DPACKAGE_TESTS=OFF 
RUN cmake --build build
RUN cmake --install build

#gigamonkey
WORKDIR /tmp
ADD https://api.github.com/repos/Gigamonkey-BSV/Gigamonkey/git/refs/heads/redo_cmake /root/gigamonkey_version.json
RUN git clone --depth 1 --branch redo_cmake https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /tmp/Gigamonkey
RUN cmake -B build -S . -DPACKAGE_TESTS=OFF
RUN cmake --build build
RUN cmake --install build
