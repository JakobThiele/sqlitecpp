cmake_minimum_required(VERSION 3.18)
project(SqliteCPP)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

set(SOURCE_FILES
    src/SqliteConnection.cpp
    src/SqliteRow.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sqlite/sqlite3.c
)

set(INCLUDE_DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}/sqlite
)

add_library(SqliteCPP SHARED ${SOURCE_FILES})
target_include_directories(SqliteCPP PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)