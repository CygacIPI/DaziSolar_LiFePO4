#include "battery.h"

bool battery_data_is_valid(const battery_data_t *data)
{
    if (data == NULL ||
        data->pack_id < BATTERY_MIN_PACK_ID ||
        data->pack_id > BATTERY_MAX_PACK_ID ||
        data->soc_pct > 100 ||
        data->cell_count > BATTERY_MAX_CELLS ||
        data->temp_count > BATTERY_MAX_TEMPERATURES ||
        data->source > BATTERY_SOURCE_CAN) {
        return false;
    }

    return true;
}
