add_library(asic STATIC
    snapshot.c
    logging.c
)

target_include_directories(asic PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

set_target_properties(asic PROPERTIES C_STANDARD 11)
