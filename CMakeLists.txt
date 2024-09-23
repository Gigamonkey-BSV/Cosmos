cmake_minimum_required (VERSION 3.16)
project (cosmos_lib CXX)

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

find_package (Gigamonkey CONFIG REQUIRED)

target_include_directories (cosmos_lib PUBLIC include)

target_link_libraries (cosmos_lib PUBLIC gigamonkey::gigamonkey data::data)

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

