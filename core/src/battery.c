#include "battery.h"

battery_data_validation_t battery_validate_data(const battery_data_t *data)
{
    if (data == NULL) {
        return BATTERY_DATA_INVALID_NULL;
    }

    if (data->pack_id < BATTERY_MIN_PACK_ID ||
        data->pack_id > BATTERY_MAX_PACK_ID) {
        return BATTERY_DATA_INVALID_PACK_ID;
    }
    if (data->soc_pct > 100) {
        return BATTERY_DATA_INVALID_SOC;
    }
    if (data->cell_count > BATTERY_MAX_CELLS) {
        return BATTERY_DATA_INVALID_CELL_COUNT;
    }
    if (data->temp_count > BATTERY_MAX_TEMPERATURES) {
        return BATTERY_DATA_INVALID_TEMPERATURE_COUNT;
    }
    if (data->source > BATTERY_SOURCE_CAN) {
        return BATTERY_DATA_INVALID_SOURCE;
    }
    if (data->nominal_mAh > 0 &&
        data->remaining_mAh > data->nominal_mAh) {
        return BATTERY_DATA_INVALID_CAPACITY;
    }

    for (uint8_t i = 0; i < data->cell_count; ++i) {
        if (data->cell_mV[i] == 0) {
            return BATTERY_DATA_INVALID_CELL_VOLTAGE;
        }
    }

    return BATTERY_DATA_VALID;
}

bool battery_data_is_valid(const battery_data_t *data)
{
    return battery_validate_data(data) == BATTERY_DATA_VALID;
}
