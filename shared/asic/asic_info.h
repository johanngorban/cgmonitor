#pragma once

typedef struct {
    int         id;
    char        name[256];
    double      mhs_av;
    double      temperature;
    double      utility;
    int         accepted;
    int         rejected;
    int         hw_errors;
} asic_info;

/// @brief convert json to struct asic_info
/// @return 0 on success, -1 on error
int asic_info_from_json(asic_info *asic, const char *json); 