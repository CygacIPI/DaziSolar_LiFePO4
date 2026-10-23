#include "battery.h"

#include <string.h>

bool battery_simulator_init(battery_simulator_t *simulator,
                            uint8_t pack_id,
                            uint32_t nominal_mAh)
{
    if (simulator == NULL ||
        pack_id < BATTERY_MIN_PACK_ID ||
        pack_id > BATTERY_MAX_PACK_ID ||
        nominal_mAh == 0) {
        return false;
    }

    simulator->pack_id = pack_id;
    simulator->nominal_mAh = nominal_mAh;
    simulator->tick = 0;
    return true;
}

bool battery_simulator_next(battery_simulator_t *simulator,
                            battery_data_t *data)
{
    if (simulator == NULL || data == NULL || simulator->nominal_mAh == 0) {
        return false;
    }

    static const int32_t current_pattern_mA[] = {
        -12000, -8000, 0, 15000, 25000, 0
    };
    static const int16_t temperature_pattern_dC[] = {
        245, 252, 248, 255, 251, 247
    };
    const size_t pattern_length =
        sizeof(current_pattern_mA) / sizeof(current_pattern_mA[0]);
    const size_t pattern_index = simulator->tick % pattern_length;

    memset(data, 0, sizeof(*data));
    data->pack_id = simulator->pack_id;
    data->voltage_mV = 53317;
    data->current_mA = current_pattern_mA[pattern_index];
    data->soc_pct = 68;
    data->nominal_mAh = simulator->nominal_mAh;
    data->remaining_mAh = (uint32_t)(((uint64_t)simulator->nominal_mAh * 6887u) /
                                     10000u);
    data->cycles = 124;
    data->cell_count = 16;
    for (uint8_t i = 0; i < data->cell_count; ++i) {
        data->cell_mV[i] = (i < 11) ? 3332 : 3333;
    }
    data->temp_count = 2;
    data->temp_dC[0] = temperature_pattern_dC[pattern_index];
    data->temp_dC[1] = (int16_t)(temperature_pattern_dC[pattern_index] - 15);
    data->source = BATTERY_SOURCE_SIMULATOR;
    if (!battery_evaluate_alarms(data, &data->alarms)) {
        return false;
    }
    simulator->tick++;

    return battery_data_is_valid(data);
}
