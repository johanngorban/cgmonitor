#include "parsers.h"

#include <cjson/cJSON.h>

int parse_json_stats(miner_info *miner, const char *stats_json) {
    if (miner == NULL || stats_json == NULL) {
        return -1;
    }

    cJSON *root = cJSON_Parse(stats_json);
    if (root == NULL) {
        return -1;
    }

    cJSON *stats_array = cJSON_GetObjectItemCaseSensitive(root, "STATS");
    if (!cJSON_IsArray(stats_array)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item = cJSON_GetArrayItem(stats_array, 0);
    if (item == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *power   = cJSON_GetObjectItemCaseSensitive(item, "Power");
    cJSON *voltage = cJSON_GetObjectItemCaseSensitive(item, "Voltage");

    if (cJSON_IsNumber(power)) {
        miner->power = power->valuedouble;
    }

    if (cJSON_IsNumber(voltage)) {
        miner->voltage = voltage->valuedouble;
    }

    cJSON_Delete(root);
    return 0;
}

int parse_json_summary(miner_info *miner, const char *summary_json) {
    if (miner == NULL || summary_json == NULL) {
        return -1;
    }

    cJSON *root = cJSON_Parse(summary_json);
    if (root == NULL) {
        return -1;
    }

    cJSON *summary_array = cJSON_GetObjectItemCaseSensitive(root, "SUMMARY");
    if (!cJSON_IsArray(summary_array)) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *item = cJSON_GetArrayItem(summary_array, 0);
    if (item == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *mhs_av  = cJSON_GetObjectItemCaseSensitive(item, "MHS av");
    cJSON *elapsed = cJSON_GetObjectItemCaseSensitive(item, "Elapsed");
    cJSON *utility = cJSON_GetObjectItemCaseSensitive(item, "Utility");

    if (cJSON_IsNumber(mhs_av)) {
        miner->hashrate = mhs_av->valuedouble;
    }

    if (cJSON_IsNumber(elapsed)) {
        miner->uptime = elapsed->valueint;
    }

    if (cJSON_IsNumber(utility)) {
        miner->utility = utility->valuedouble;
    }

    cJSON_Delete(root);
    return 0;
}
