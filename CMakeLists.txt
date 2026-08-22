cmake_minimum_required(VERSION 4.2)
project(liblogger)

set(CMAKE_CXX_STANDARD 17)

add_library(liblogger_static STATIC
        src/file_logger.cpp
        src/socket_logger.cpp
)

target_include_directories(liblogger_static PUBLIC
${CMAKE_SOURCE_DIR}/include
)

add_library(liblogger_shared SHARED
        src/file_logger.cpp
        src/socket_logger.cpp
)

target_include_directories(liblogger_shared PUBLIC
        ${CMAKE_SOURCE_DIR}/include
)

add_executable(example_app
        app/app.cpp)

target_link_libraries(example_app PRIVATE liblogger_static)

add_executable(server
    server/server.cpp
        server/statistics.cpp
        server/log_parser.cpp)

add_executable(server_test
    tests/server_test.cpp)

target_link_libraries(server_test PRIVATE liblogger_static)

enable_testing()

add_executable(library_tests
    tests/library_tests.cpp)

target_link_libraries(library_tests PRIVATE liblogger_static)

add_test(
        NAME LibraryTests
        COMMAND library_tests
)