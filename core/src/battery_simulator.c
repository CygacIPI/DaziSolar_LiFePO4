#include "battery.h"

#include <string.h>

#define SIMULATOR_INITIAL_SOC_PCT 68
#define SIMULATOR_TEMPERATURE_COUNT 2
#define SIMULATOR_CELL_COUNT 16
#define SIMULATOR_NOMINAL_VOLTAGE_MV 51200

static const int32_t CURRENT_PATTERN_MA[] = {
    -12000, -8000, 0, 15000, 25000, 0};
static const int16_t TEMPERATURE_PATTERN_DC[] = {
    245, 252, 248, 255, 251, 247};

static bool populate_cell_voltages(battery_data_t *data)
{
    if (data->cell_count == 0)
    {
        return false;
    }

    const uint32_t base_cell_mV = data->voltage_mV / data->cell_count;
    const uint32_t remainder_mV = data->voltage_mV % data->cell_count;
    for (uint8_t i = 0; i < data->cell_count; ++i)
    {
        data->cell_mV[i] =
            (uint16_t)(base_cell_mV + (i < remainder_mV ? 1u : 0u));
    }
    return true;
}

bool battery_simulator_init(battery_simulator_t *simulator,
                            uint8_t pack_id,
                            uint32_t nominal_mAh)
{
    if (simulator == NULL ||
        pack_id < BATTERY_MIN_PACK_ID ||
        pack_id > BATTERY_MAX_PACK_ID ||
        nominal_mAh == 0)
    {
        return false;
    }

    memset(simulator, 0, sizeof(*simulator));
    simulator->pack_id = pack_id;
    simulator->nominal_mAh = nominal_mAh;
    return battery_coulomb_counter_init(&simulator->coulomb_counter,
                                        nominal_mAh,
                                        SIMULATOR_INITIAL_SOC_PCT);
}

bool battery_simulator_step(battery_simulator_t *simulator,
                            uint32_t elapsed_milliseconds,
                            battery_data_t *data)
{
    if (simulator == NULL || data == NULL ||
        simulator->nominal_mAh == 0 ||
        simulator->current_pattern_index >=
            sizeof(CURRENT_PATTERN_MA) / sizeof(CURRENT_PATTERN_MA[0]))
    {
        return false;
    }

    const size_t pattern_length =
        sizeof(CURRENT_PATTERN_MA) / sizeof(CURRENT_PATTERN_MA[0]);
    const int32_t current_mA =
        CURRENT_PATTERN_MA[simulator->current_pattern_index];
    uint32_t remaining_mAh = 0;
    uint8_t soc_pct = 0;
    if (!battery_coulomb_counter_update(&simulator->coulomb_counter,
                                        current_mA,
                                        elapsed_milliseconds,
                                        &remaining_mAh,
                                        &soc_pct))
    {
        return false;
    }

    memset(data, 0, sizeof(*data));
    data->pack_id = simulator->pack_id;
    data->current_mA = current_mA;
    data->soc_pct = soc_pct;
    data->remaining_mAh = remaining_mAh;
    data->nominal_mAh = simulator->nominal_mAh;
    data->cycles = 124;
    data->cell_count = SIMULATOR_CELL_COUNT;
    data->temp_count = SIMULATOR_TEMPERATURE_COUNT;
    data->source = BATTERY_SOURCE_SIMULATOR;
    data->valid_fields =
        BATTERY_FIELD_PACK_ID |
        BATTERY_FIELD_VOLTAGE |
        BATTERY_FIELD_CURRENT |
        BATTERY_FIELD_SOC |
        BATTERY_FIELD_REMAINING_CAPACITY |
        BATTERY_FIELD_NOMINAL_CAPACITY |
        BATTERY_FIELD_CYCLES |
        BATTERY_FIELD_CELLS |
        BATTERY_FIELD_TEMPERATURES |
        BATTERY_FIELD_ALARMS;

    const int32_t soc_offset_mV =
        ((int32_t)soc_pct - SIMULATOR_INITIAL_SOC_PCT) * 12;
    const int32_t current_offset_mV = current_mA > 0 ? 20 :
                                      (current_mA < 0 ? -20 : 0);
    int32_t voltage_mV = (int32_t)SIMULATOR_NOMINAL_VOLTAGE_MV +
                         2137 + soc_offset_mV + current_offset_mV;
    if (voltage_mV < (int32_t)SIMULATOR_NOMINAL_VOLTAGE_MV)
    {
        voltage_mV = (int32_t)SIMULATOR_NOMINAL_VOLTAGE_MV;
    }
    data->voltage_mV = (uint32_t)voltage_mV;

    const size_t temperature_index = simulator->tick % pattern_length;
    data->temp_dC[0] = TEMPERATURE_PATTERN_DC[temperature_index];
    data->temp_dC[1] =
        (int16_t)(TEMPERATURE_PATTERN_DC[temperature_index] - 15);

    if (!populate_cell_voltages(data) ||
        !battery_evaluate_alarms(data, &data->alarms) ||
        !battery_data_is_valid(data))
    {
        return false;
    }

    simulator->tick++;
    simulator->current_pattern_index =
        (uint8_t)((simulator->current_pattern_index + 1u) % pattern_length);
    return true;
}

bool battery_simulator_next(battery_simulator_t *simulator,
                            battery_data_t *data)
{
    return battery_simulator_step(simulator, 1000, data);
}
