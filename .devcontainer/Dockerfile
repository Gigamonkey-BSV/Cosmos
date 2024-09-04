FROM ubuntu:22.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update

RUN apt-get -y install python3-pip build-essential manpages-dev software-properties-common git cmake
RUN cmake --version
RUN apt-get -y install autoconf
RUN apt-get -y install libtool

RUN add-apt-repository ppa:ubuntu-toolchain-r/test

RUN apt-get update && apt-get -y install gcc-11 g++-11

RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-11 20

RUN update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-11 20

RUN pip install conan==2.0.2
RUN conan --version

RUN conan profile detect

#secp256k1
WORKDIR /tmp
RUN git clone https://github.com/Gigamonkey-BSV/secp256k1-conan-recipe.git
WORKDIR /tmp/secp256k1-conan-recipe
RUN conan create . --user=proofofwork --channel=stable -b missing

#pegtl
WORKDIR /tmp
RUN git clone https://github.com/taocpp/PEGTL.git
WORKDIR /tmp/PEGTL
RUN git checkout 3.2.7
RUN mkdir build
WORKDIR /tmp/PEGTL/build
RUN cmake ..
RUN make
RUN make install

#data
WORKDIR /tmp
RUN git clone --depth 1 --branch master https://github.com/DanielKrawisz/data.git
WORKDIR /tmp/data
RUN CONAN_CPU_COUNT=1 VERBOSE=1 conan create . --user=proofofwork --channel=stable -b missing

#gigamonkey
WORKDIR /tmp
RUN git clone --depth 1 --branch master https://github.com/Gigamonkey-BSV/Gigamonkey.git
WORKDIR /tmp/Gigamonkey
RUN CONAN_CPU_COUNT=1 VERBOSE=1 conan create . --user=proofofwork --channel=stable -b missing
