#include <stdio.h>
#include <string.h>
#include "asic_info.h"

int asic_info_from_json(asic_info *asic, cJSON *item) {
    if (asic == NULL || item == NULL) {
        return -1;
    }

    cJSON *id          = cJSON_GetObjectItemCaseSensitive(item, "ID");
    cJSON *name        = cJSON_GetObjectItemCaseSensitive(item, "Name");
    cJSON *mhs_av      = cJSON_GetObjectItemCaseSensitive(item, "MHS av");
    cJSON *temperature = cJSON_GetObjectItemCaseSensitive(item, "Temperature");
    cJSON *utility     = cJSON_GetObjectItemCaseSensitive(item, "Utility");
    cJSON *accepted    = cJSON_GetObjectItemCaseSensitive(item, "Accepted");
    cJSON *rejected    = cJSON_GetObjectItemCaseSensitive(item, "Rejected");
    cJSON *hw_errors   = cJSON_GetObjectItemCaseSensitive(item, "Hardware Errors");

    if (id != NULL && cJSON_IsNumber(id)) {
        asic->id = id->valueint;
    }

    if (name != NULL && cJSON_IsString(name)) {
        strncpy(asic->name, name->valuestring, sizeof(asic->name) - 1);
        asic->name[sizeof(asic->name) - 1] = '\0';
    }

    if (mhs_av != NULL && cJSON_IsNumber(mhs_av)) {
        asic->mhs_av = mhs_av->valuedouble;
    }

    if (temperature != NULL && cJSON_IsNumber(temperature)) {
        asic->temperature = temperature->valuedouble;
    }

    if (utility != NULL && cJSON_IsNumber(utility)) {
        asic->utility = utility->valuedouble;
    }

    if (accepted != NULL && cJSON_IsNumber(accepted)) {
        asic->accepted = accepted->valueint;
    }

    if (rejected != NULL && cJSON_IsNumber(rejected)) {
        asic->rejected = rejected->valueint;
    }

    if (hw_errors != NULL && cJSON_IsNumber(hw_errors)) {
        asic->hw_errors = hw_errors->valueint;
    }

    return 0;
}



#ifdef DEBUG

void debug_asic_info_print(const asic_info *asic) {
    if (asic == NULL) {
        printf("asic_info is NULL\n");
        return;
    }

    printf("=== ASIC INFO ===\n");
    printf("ID          : %d\n", asic->id);
    printf("Name        : %s\n", asic->name);
    printf("MHS av      : %.2f\n", asic->mhs_av);
    printf("Temperature : %.2f °C\n", asic->temperature);
    printf("Utility     : %.2f\n", asic->utility);
    printf("Accepted    : %d\n", asic->accepted);
    printf("Rejected    : %d\n", asic->rejected);
    printf("HW Errors   : %d\n", asic->hw_errors);
    printf("=================\n");
}

#endif 

