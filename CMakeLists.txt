cmake_minimum_required (VERSION 3.16)

# Back compatibility for VERSION range
if (${CMAKE_VERSION} VERSION_LESS 3.12)
  cmake_policy (VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif ()

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
  message(FATAL_ERROR "In-source builds are not allowed")
endif ()

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE STRING "Modules for CMake" FORCE)

project (cosmos_lib CXX)

set (CMAKE_CXX_STANDARD 20)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

find_package (Boost 1.86 COMPONENTS thread chrono log_setup log REQUIRED)
# if we do not have boost
#   * linux:
#     * go to the boost website, download and extract the latest version.
#     * `./bootstrap.sh`
#     * `./b2`
#     * `sudo ./b2 install`

add_definitions ("-DHAS_BOOST")

find_package (OpenSSL REQUIRED Crypto SSL)
# if we do not have openssl
#   * on linux, `sudo apt-get install libssl-dev`

find_package (SECP256K1 REQUIRED)
# if we do not have SECP256K1
#   * on linux
#     * go to `https://github.com/bitcoin-core/secp256k1`, download the repo, and follow the instructions.
#     * you mighht need autoconf: `sudo apt install autoconf`
#     * and libtool: `sudo apt-get install libtool-bin`

find_package (pegtl REQUIRED)
# If we do not have PegTL
#   * on linux, `sudo apt-get install tao-pegtl-dev`

find_package (PkgConfig REQUIRED)
# If we do not have PkgConfig
#   * on linux, `sudo apt-get install pkg-config`

IF (WIN32)
  pkg_check_modules (cryptopp REQUIRED IMPORTED_TARGET cryptopp)
ELSE ()
  pkg_check_modules (cryptopp REQUIRED IMPORTED_TARGET libcrypto++)
  # If we do not have crypto++, `sudo apt install libcrypto++-dev`
ENDIF ()

## Enable testing
include (FetchContent)
FetchContent_Declare (json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable (json)

FetchContent_Declare (argh URL https://github.com/adishavit/argh/archive/refs/tags/v1.3.2.zip)
FetchContent_MakeAvailable (argh)

FetchContent_Declare (ctre URL https://github.com/hanickadot/compile-time-regular-expressions/archive/refs/tags/v3.9.0.zip)
FetchContent_MakeAvailable (ctre)

find_package (data CONFIG REQUIRED)

find_package (gigamonkey CONFIG REQUIRED)

add_library (cosmos_lib STATIC
    source/Cosmos/database/write.cpp
    source/Cosmos/database/price_data.cpp
    source/Cosmos/database/txdb.cpp
    source/Cosmos/database/memory/txdb.cpp
    source/Cosmos/database/json/txdb.cpp
    source/Cosmos/database/json/price_data.cpp
    source/Cosmos/network/whatsonchain.cpp
    source/Cosmos/network.cpp
    source/Cosmos/files.cpp
    source/Cosmos/wallet/keys/derivation.cpp
    source/Cosmos/wallet/keys/sequence.cpp
    source/Cosmos/wallet/keys/pubkeys.cpp
    source/Cosmos/wallet/keys/secret.cpp
    source/Cosmos/wallet/account.cpp
    source/Cosmos/wallet/restore.cpp
    source/Cosmos/wallet/select.cpp
    source/Cosmos/wallet/change.cpp
    source/Cosmos/wallet/wallet.cpp
    source/Cosmos/wallet/split.cpp
    source/Cosmos/history.cpp
    source/Cosmos/tax.cpp
    source/Cosmos/boost/miner_options.cpp
    source/Cosmos/pay.cpp
)

target_include_directories (cosmos_lib PUBLIC include)

target_link_libraries (cosmos_lib PUBLIC nlohmann_json argh gigamonkey Data::data)

target_compile_features (cosmos_lib PUBLIC cxx_std_20)
set_target_properties (cosmos_lib PROPERTIES CXX_EXTENSIONS OFF)

add_executable (CosmosWallet
    source/random.cpp
    source/interface.cpp
    source/generate.cpp
    source/restore.cpp
    source/request.cpp
    source/accept.cpp
    source/split.cpp
    source/import.cpp
    source/Cosmos.cpp)

target_link_libraries (CosmosWallet PUBLIC
    cosmos_lib
)

target_include_directories (CosmosWallet PUBLIC include)

target_compile_features (CosmosWallet PUBLIC cxx_std_20)
set_target_properties (CosmosWallet PROPERTIES CXX_EXTENSIONS OFF)

# add_definitions ("-DHAS_BOOST")

# option (PACKAGE_TESTS "Build the tests" ON)

#if (PACKAGE_TESTS)
#    include (CTest)
#    find_package (GTest REQUIRED)
#    enable_testing ()
#    add_subdirectory (test)
#endif ()

install (DIRECTORY include/ DESTINATION include)
install (TARGETS cosmos_lib)

