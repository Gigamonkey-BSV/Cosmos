cmake_minimum_required (VERSION 3.16)
project (CosmosWallet CXX)

add_library (CosmosLib STATIC
    source/Cosmos/wallet/write.cpp
    source/Cosmos/wallet/keys.cpp
    source/Cosmos/wallet/txdb.cpp
    source/Cosmos/wallet/account.cpp
    source/Cosmos/wallet/select.cpp
    source/Cosmos/wallet/change.cpp
    source/Cosmos/wallet/wallet.cpp
    source/Cosmos/wallet/split.cpp
    source/Cosmos/network/whatsonchain.cpp
    source/Cosmos/network.cpp
    source/Cosmos/context.cpp)

find_package (gigamonkey CONFIG REQUIRED)

target_include_directories (CosmosLib PUBLIC include)
target_link_libraries (CosmosLib PUBLIC gigamonkey::gigamonkey data::data)
target_compile_features (CosmosLib PUBLIC cxx_std_20)
set_target_properties (CosmosLib PROPERTIES CXX_EXTENSIONS OFF)

add_executable (CosmosWallet Cosmos.cpp)

target_link_libraries (CosmosWallet PUBLIC CosmosLib)
target_include_directories (CosmosWallet PUBLIC include)

target_compile_features (CosmosWallet PUBLIC cxx_std_20)
set_target_properties (CosmosWallet PROPERTIES CXX_EXTENSIONS OFF)

add_definitions ("-DHAS_BOOST")

# option (PACKAGE_TESTS "Build the tests" ON)

#if (PACKAGE_TESTS)
#    include (CTest)
#    find_package (GTest REQUIRED)
#    enable_testing ()
#    add_subdirectory (test)
#endif ()

install (TARGETS CosmosWallet)

