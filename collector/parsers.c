#include "parsers.h"
#include "logging.h"

#include <cjson/cJSON.h>
#include <math.h>
#include <stdio.h>

static unsigned int get_miner_temperature(cJSON *stats_item) {
    unsigned int total_temp = 0;

    cJSON *temp1 = cJSON_GetObjectItemCaseSensitive(stats_item, "temp1");
    if (cJSON_IsNumber(temp1)) {
        total_temp += temp1->valueint;
    }

    cJSON *temp2 = cJSON_GetObjectItemCaseSensitive(stats_item, "temp2");
    if (cJSON_IsNumber(temp2)) {
        total_temp += temp2->valueint;
    }

    cJSON *temp3 = cJSON_GetObjectItemCaseSensitive(stats_item, "temp3");
    if (cJSON_IsNumber(temp3)) {
        total_temp += temp3->valueint;
    }

    return total_temp / 3;
}

static unsigned int get_miner_consumption(cJSON *stats_item) {
    unsigned int consumption = 0;

    cJSON *consumption1 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_consumption1");
    if (cJSON_IsNumber(consumption1)) {
        consumption += consumption1->valuedouble;
    }

    cJSON *consumption2 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_consumption2");
    if (cJSON_IsNumber(consumption2)) {
        consumption += consumption2->valuedouble;
    }

    cJSON *consumption3 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_consumption3");
    if (cJSON_IsNumber(consumption3)) {
        consumption += consumption3->valuedouble;
    }

    return consumption / 3;
}

static unsigned int get_miner_voltage(cJSON *stats_item) {
    unsigned int voltage = 0;

    cJSON *chain_vol1 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_vol1");
    if (cJSON_IsNumber(chain_vol1)) {
        voltage += chain_vol1->valueint;
    }

    cJSON *chain_vol2 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_vol2");
    if (cJSON_IsNumber(chain_vol2)) {
        voltage += chain_vol2->valueint;
    }

    cJSON *chain_vol3 = cJSON_GetObjectItemCaseSensitive(stats_item, "chain_vol3");
    if (cJSON_IsNumber(chain_vol3)) {
        voltage += chain_vol3->valueint;
    }

    return voltage / 3;
}

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

    cJSON *item = cJSON_GetArrayItem(stats_array, 1);
    if (item == NULL) {
        cJSON_Delete(root);
        return -1;
    }

    cJSON *hashrate_avg = cJSON_GetObjectItemCaseSensitive(item, "GHS av");
    if (cJSON_IsNumber(hashrate_avg)) {
        miner->hashrate = hashrate_avg->valuedouble;
        if (fabs(miner->hashrate) < 0.1) {
            miner->hashrate = 0;
        }
    }

    miner->temp = get_miner_temperature(item);
    miner->power = get_miner_consumption(item);
    miner->voltage = get_miner_voltage(item);

    cJSON_Delete(root);
    return 0;
}