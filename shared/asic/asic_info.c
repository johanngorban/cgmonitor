#include <stdio.h>
#include <string.h>
#include "cjson/cJSON.h"
#include "asic_info.h"

int asic_info_from_json(asic_info *asic, const char *json) {
    if (asic == NULL || json == NULL) {
        return -1;
    }

    cJSON *root = cJSON_Parse(json);
    if (root == NULL) {
        return -1;
    }

    memset(asic, 0, sizeof(*asic));

    cJSON *id = cJSON_GetObjectItemCaseSensitive(root, "ID");
    cJSON *name = cJSON_GetObjectItemCaseSensitive(root, "Name");
    cJSON *mhs_av = cJSON_GetObjectItemCaseSensitive(root, "MHS av");
    cJSON *temperature = cJSON_GetObjectItemCaseSensitive(root, "Temperature");
    cJSON *utility = cJSON_GetObjectItemCaseSensitive(root, "Utility");
    cJSON *accepted = cJSON_GetObjectItemCaseSensitive(root, "Accepted");
    cJSON *rejected = cJSON_GetObjectItemCaseSensitive(root, "Rejected");
    cJSON *hw_errors = cJSON_GetObjectItemCaseSensitive(root, "Hardware Errors");

    if (cJSON_IsNumber(id)) {
        asic->id = id->valueint;
    }
    if (cJSON_IsString(name)) {
        strncpy(asic->name, name->valuestring, sizeof(asic->name) - 1);
        asic->name[sizeof(asic->name) - 1] = '\0';
    }
    if (cJSON_IsNumber(mhs_av)) {
        asic->mhs_av = mhs_av->valuedouble;
    }
    if (cJSON_IsNumber(temperature)) {
        asic->temperature = temperature->valuedouble;
    }
    if (cJSON_IsNumber(utility)) {
        asic->utility = utility->valuedouble;
    }
    if (cJSON_IsNumber(accepted)) {
        asic->accepted = accepted->valueint;
    }
    if (cJSON_IsNumber(rejected)) {
        asic->rejected = rejected->valueint;
    }
    if (cJSON_IsNumber(hw_errors)) {
        asic->hw_errors = hw_errors->valueint;
    }

    cJSON_Delete(root);
    return 0;
}

