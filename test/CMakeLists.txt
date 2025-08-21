
cmake_minimum_required (VERSION 3.1...3.14)

# Back compatibility for VERSION range
if (${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy (VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif ()

include (GoogleTest)

add_executable (unit_tests
    ../source/random.cpp
    ../source/Cosmos/database/SQLite.cpp
    ../source/server/db.cpp
    ../source/server/options.cpp
    ../source/server/problem.cpp
    ../source/server/method.cpp
    ../source/server/server.cpp
    ../source/server/key.cpp
    ../source/server/invert_hash.cpp
    ../source/server/generate.cpp
    server.cpp
)

target_include_directories (
    unit_tests PUBLIC
    ${CMAKE_BINARY_DIR}/_deps/sqliteorm-src/include
    ../include
)

target_link_libraries (
    unit_tests
    PRIVATE
    SQLite::SQLite3
    cosmos_lib
    gtest_main
)

gtest_discover_tests (unit_tests)
