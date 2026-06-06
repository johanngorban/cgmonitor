cmake_minimum_required(VERSION 3.16)
project(cgmonitor C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(
    -Wall -Wextra -Wshadow
    -Wno-unused-parameter
    -fno-strict-aliasing
)

find_package(Threads REQUIRED)

include(FindPkgConfig)

pkg_check_modules(MHD REQUIRED libmicrohttpd)

find_path(CJSON_INCLUDE_DIR cjson/cJSON.h)
find_library(CJSON_LIB NAMES cjson)
if(NOT CJSON_INCLUDE_DIR OR NOT CJSON_LIB)
    pkg_check_modules(CJSON REQUIRED libcjson)
    set(CJSON_INCLUDE_DIR ${CJSON_INCLUDE_DIRS})
    set(CJSON_LIB ${CJSON_LIBRARIES})
endif()

find_package(SQLite3)
if(NOT SQLite3_FOUND)
    pkg_check_modules(SQLITE3 REQUIRED sqlite3)
    set(SQLite3_INCLUDE_DIRS ${SQLITE3_INCLUDE_DIRS})
    set(SQLite3_LIBRARIES    ${SQLITE3_LIBRARIES})
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_subdirectory(shared/asic)

set(WEB_STATIC_HEADER "${CMAKE_BINARY_DIR}/generated/web_static.h")
file(GLOB_RECURSE WEB_FILES CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/web/*")

add_custom_command(
    OUTPUT  "${WEB_STATIC_HEADER}"
    COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/embed_files.py"
            "${CMAKE_SOURCE_DIR}/web"
            "${WEB_STATIC_HEADER}"
    DEPENDS ${WEB_FILES}
            "${CMAKE_SOURCE_DIR}/tools/embed_files.py"
    COMMENT "Embedding web/ → web_static.h"
    VERBATIM
)
add_custom_target(web_embed DEPENDS "${WEB_STATIC_HEADER}")

# --- main executable -----------------------------------------------------
set(SRC
    main.c
    config/config.c
    protocol/protocol.c
    protocol/cgminer/cgminer.c
    protocol/cgminer/cgminer_parse.c
    collector/collector.c
    storage/cache.c
    storage/database.c
    storage/storage.c
    api/server.c
    api/json_out.c
    api/static_files.c
)

add_executable(cgmonitor ${SRC})
add_dependencies(cgmonitor web_embed)

target_include_directories(cgmonitor PRIVATE
    "${CMAKE_SOURCE_DIR}/shared/asic"
    "${CMAKE_SOURCE_DIR}/config"
    "${CMAKE_SOURCE_DIR}/protocol"
    "${CMAKE_SOURCE_DIR}/protocol/cgminer"
    "${CMAKE_SOURCE_DIR}/collector"
    "${CMAKE_SOURCE_DIR}/storage"
    "${CMAKE_SOURCE_DIR}/api"
    "${CMAKE_BINARY_DIR}/generated"
    "${CJSON_INCLUDE_DIR}"
    "${MHD_INCLUDE_DIRS}"
    "${SQLite3_INCLUDE_DIRS}"
)

target_link_libraries(cgmonitor PRIVATE
    asic
    ${CJSON_LIB}
    ${MHD_LIBRARIES}
    ${SQLite3_LIBRARIES}
    Threads::Threads
)

target_link_directories(cgmonitor PRIVATE ${MHD_LIBRARY_DIRS})

# --- install rules -------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS cgmonitor RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES   config/cgmonitor.conf.example
        DESTINATION ${CMAKE_INSTALL_SYSCONFDIR}/cgmonitor
        RENAME      cgmonitor.conf.example)
install(FILES   systemd/cgmonitor.service
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/systemd/system
        OPTIONAL)
