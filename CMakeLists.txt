project(cgmonitor)
cmake_minimum_required(VERSION 3.16)

find_package(cJSON REQUIRED)
find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(MHD REQUIRED libmicrohttpd)

add_subdirectory(shared/asic)
add_subdirectory(shared/clog)
add_subdirectory(shared/cstruct)

file(GLOB SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/api/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/collector/*.c
    ${CMAKE_CURRENT_SOURCE_DIR}/storage/source/*.c
)

add_executable(cgmonitor ${SOURCES})

target_include_directories(cgmonitor
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/api
        ${CMAKE_CURRENT_SOURCE_DIR}/collector
        ${CMAKE_CURRENT_SOURCE_DIR}/storage/include
        ${MHD_INCLUDE_DIRS}
)

target_link_libraries(cgmonitor
    PUBLIC
        asic
        cjson
        clog
        cstruct
        SQLite::SQLite3
        ${MHD_LIBRARIES}
        pthread
)