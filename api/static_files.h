/**
 * static_files.h — interface to embedded HTML/JS/CSS/etc.
 *
 * The actual data array (static_files[]) is generated at build time by
 * tools/embed_files.py reading the contents of the web/ directory.
 * Generated header is web_static.h (see api/CMakeLists rules).
 *
 * A consumer asks `static_files_get("/index.html")` and gets back a pointer
 * + length + content-type. NULL if not found.
 */
#pragma once

#include <stddef.h>

typedef struct {
    const char *path;          /* e.g. "/index.html" */
    const char *content_type;  /* e.g. "text/html; charset=utf-8" */
    const unsigned char *data;
    size_t      size;
} static_file_t;

const static_file_t *static_files_get(const char *path);
