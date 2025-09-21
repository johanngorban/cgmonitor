cmake_minimum_required(VERSION 3.16)
project(asic)

set(CMAKE_C_STANDARD, 11)

file(GLOB_RECURSE SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/*.c
)

add_library(asic STATIC
    ${SOURCES}
)

target_include_directories(asic
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(asic
    PUBLIC
        cjson
)