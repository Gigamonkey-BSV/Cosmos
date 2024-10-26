cmake_minimum_required (VERSION 3.16)
project (cosmos_lib CXX)

set (CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake" CACHE STRING "Modules for CMake" FORCE)

include (FetchContent)
FetchContent_Declare (json URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz)
FetchContent_MakeAvailable (json)

FetchContent_Declare (argh URL https://github.com/adishavit/argh/archive/refs/tags/v1.3.2.zip)
FetchContent_MakeAvailable (argh)

FetchContent_Declare (ctre URL https://github.com/hanickadot/compile-time-regular-expressions/archive/refs/tags/v3.9.0.zip)
FetchContent_MakeAvailable (ctre)


find_package (PkgConfig REQUIRED)
pkg_check_modules(Cryptopp REQUIRED IMPORTED_TARGET libcrypto++)


add_library (cosmos_lib STATIC
    source/Cosmos/database/write.cpp
    source/Cosmos/database/price_data.cpp
    source/Cosmos/database/txdb.cpp
    source/Cosmos/database/memory/txdb.cpp
    source/Cosmos/database/json/txdb.cpp
    source/Cosmos/database/json/price_data.cpp
    source/Cosmos/network/whatsonchain.cpp
    source/Cosmos/network.cpp
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

find_package (gigamonkey CONFIG REQUIRED)

target_include_directories (cosmos_lib PUBLIC include)

target_link_libraries (cosmos_lib PUBLIC nlohmann_json argh Gigamonkey::gigamonkey Data::data)

target_compile_features (cosmos_lib PUBLIC cxx_std_20)
set_target_properties (cosmos_lib PROPERTIES CXX_EXTENSIONS OFF)

add_executable (CosmosWallet
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

