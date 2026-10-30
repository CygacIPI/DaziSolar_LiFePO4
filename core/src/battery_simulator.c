#include "battery.h"

#include <math.h>
#include <string.h>

#define SIMULATOR_INITIAL_SOC_PCT 68
#define SIMULATOR_TEMPERATURE_COUNT 2
#define SIMULATOR_CELL_COUNT 16
#define SIMULATOR_NOMINAL_VOLTAGE_MV 51200

static const int32_t CURRENT_PATTERN_MA[] = {
    -12000, -8000, 0, 15000, 25000, 0};
static const int16_t TEMPERATURE_PATTERN_DC[] = {
    245, 252, 248, 255, 251, 247};

#define CURRENT_PATTERN_LENGTH \
    (sizeof(CURRENT_PATTERN_MA) / sizeof(CURRENT_PATTERN_MA[0]))
#define SIMULATOR_CYCLE_MS \
    (BATTERY_SIMULATOR_STATE_DURATION_MS * CURRENT_PATTERN_LENGTH)
#define SIMULATOR_CYCLE_CURRENT_MA_MS INT64_C(20000000)
#define SIMULATOR_CYCLE_MINIMUM_MA_MS INT64_C(-20000000)

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

static bool apply_segment(battery_simulator_t *simulator,
                          uint32_t elapsed_milliseconds,
                          int64_t *current_integral_mA_ms)
{
    const int32_t current_mA =
        CURRENT_PATTERN_MA[simulator->current_pattern_index];
    uint32_t remaining_mAh = 0;
    uint8_t estimated_soc_pct = 0;
    if (!battery_coulomb_counter_update(&simulator->coulomb_counter,
                                        current_mA,
                                        elapsed_milliseconds,
                                        &remaining_mAh,
                                        &estimated_soc_pct))
    {
        return false;
    }

    *current_integral_mA_ms +=
        (int64_t)current_mA * (int64_t)elapsed_milliseconds;
    simulator->pattern_elapsed_ms =
        (uint16_t)(simulator->pattern_elapsed_ms + elapsed_milliseconds);
    if (simulator->pattern_elapsed_ms == BATTERY_SIMULATOR_STATE_DURATION_MS)
    {
        simulator->pattern_elapsed_ms = 0;
        simulator->current_pattern_index =
            (uint8_t)((simulator->current_pattern_index + 1u) %
                      CURRENT_PATTERN_LENGTH);
        simulator->tick++;
    }
    return true;
}

static bool apply_pattern_cycle(battery_simulator_t *simulator,
                                int64_t *current_integral_mA_ms)
{
    for (size_t i = 0; i < CURRENT_PATTERN_LENGTH; ++i)
    {
        if (!apply_segment(simulator, BATTERY_SIMULATOR_STATE_DURATION_MS,
                           current_integral_mA_ms))
        {
            return false;
        }
    }
    return true;
}

static bool skip_pattern_cycles(battery_simulator_t *simulator,
                                uint32_t *cycle_count,
                                int64_t *current_integral_mA_ms)
{
    const double cycle_charge_mAh =
        (double)SIMULATOR_CYCLE_CURRENT_MA_MS / 3600000.0;
    const double minimum_charge_mAh =
        (double)(-SIMULATOR_CYCLE_MINIMUM_MA_MS) / 3600000.0;

    if ((double)simulator->coulomb_counter.remaining_mAh <
        minimum_charge_mAh)
    {
        if (!apply_pattern_cycle(simulator, current_integral_mA_ms))
        {
            return false;
        }
        --(*cycle_count);
    }

    if (*cycle_count == 0)
    {
        return true;
    }
    if (simulator->coulomb_counter.remaining_mAh >=
        (double)simulator->nominal_mAh)
    {
        *current_integral_mA_ms +=
            SIMULATOR_CYCLE_CURRENT_MA_MS * (int64_t)*cycle_count;
        simulator->tick +=
            (uint32_t)(*cycle_count * CURRENT_PATTERN_LENGTH);
        *cycle_count = 0;
        return true;
    }

    const double headroom =
        (double)simulator->nominal_mAh -
        simulator->coulomb_counter.remaining_mAh;
    uint32_t safe_cycles = (uint32_t)(headroom / cycle_charge_mAh);
    if (safe_cycles > *cycle_count)
    {
        safe_cycles = *cycle_count;
    }
    while (safe_cycles > 0 &&
           simulator->coulomb_counter.remaining_mAh +
                   (double)safe_cycles * cycle_charge_mAh >
               (double)simulator->nominal_mAh)
    {
        --safe_cycles;
    }

    if (safe_cycles > 0)
    {
        uint32_t remaining_mAh = 0;
        uint8_t estimated_soc_pct = 0;
        const double charge_delta_mAh =
            (double)SIMULATOR_CYCLE_CURRENT_MA_MS *
            (double)safe_cycles / 3600000.0;
        if (!battery_coulomb_counter_apply_charge_delta_mAh(
                &simulator->coulomb_counter,
                charge_delta_mAh,
                &remaining_mAh,
                &estimated_soc_pct))
        {
            return false;
        }
        *current_integral_mA_ms +=
            SIMULATOR_CYCLE_CURRENT_MA_MS * (int64_t)safe_cycles;
        simulator->tick +=
            (uint32_t)(safe_cycles * CURRENT_PATTERN_LENGTH);
        *cycle_count -= safe_cycles;
    }

    if (*cycle_count > 0)
    {
        if (!apply_pattern_cycle(simulator, current_integral_mA_ms))
        {
            return false;
        }
        --(*cycle_count);
    }

    if (*cycle_count > 0)
    {
        if (simulator->coulomb_counter.remaining_mAh <
            (double)simulator->nominal_mAh)
        {
            return false;
        }
        *current_integral_mA_ms +=
            SIMULATOR_CYCLE_CURRENT_MA_MS * (int64_t)*cycle_count;
        simulator->tick +=
            (uint32_t)(*cycle_count * CURRENT_PATTERN_LENGTH);
        *cycle_count = 0;
    }
    return true;
}

static bool populate_data(battery_simulator_t *simulator,
                          int32_t current_mA,
                          battery_data_t *data)
{
    uint32_t remaining_mAh = 0;
    uint8_t soc_pct = 0;
    if (!battery_coulomb_counter_apply_charge_delta_mAh(
            &simulator->coulomb_counter,
            0.0,
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
    data->valid_fields = BATTERY_KNOWN_FIELDS;

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

    const size_t temperature_index = simulator->tick % CURRENT_PATTERN_LENGTH;
    data->temp_dC[0] = TEMPERATURE_PATTERN_DC[temperature_index];
    data->temp_dC[1] =
        (int16_t)(TEMPERATURE_PATTERN_DC[temperature_index] - 15);

    return populate_cell_voltages(data) &&
           battery_evaluate_alarms(data, &data->alarms) &&
           battery_data_is_valid(data);
}

bool battery_simulator_step(battery_simulator_t *simulator,
                            uint32_t elapsed_milliseconds,
                            battery_data_t *data)
{
    if (simulator == NULL || data == NULL ||
        simulator->pack_id < BATTERY_MIN_PACK_ID ||
        simulator->pack_id > BATTERY_MAX_PACK_ID ||
        simulator->nominal_mAh == 0 ||
        simulator->coulomb_counter.nominal_mAh != simulator->nominal_mAh ||
        !isfinite(simulator->coulomb_counter.remaining_mAh) ||
        simulator->coulomb_counter.remaining_mAh < 0.0 ||
        simulator->coulomb_counter.remaining_mAh >
            (double)simulator->nominal_mAh ||
        simulator->current_pattern_index >= CURRENT_PATTERN_LENGTH ||
        simulator->pattern_elapsed_ms >= BATTERY_SIMULATOR_STATE_DURATION_MS)
    {
        return false;
    }

    uint32_t pending_milliseconds = elapsed_milliseconds;
    int64_t current_integral_mA_ms = 0;
    while (pending_milliseconds > 0 &&
           (simulator->current_pattern_index != 0 ||
            simulator->pattern_elapsed_ms != 0))
    {
        const uint32_t to_boundary =
            BATTERY_SIMULATOR_STATE_DURATION_MS -
            simulator->pattern_elapsed_ms;
        const uint32_t segment =
            pending_milliseconds < to_boundary
                ? pending_milliseconds
                : to_boundary;
        if (!apply_segment(simulator, segment, &current_integral_mA_ms))
        {
            return false;
        }
        pending_milliseconds -= segment;
    }

    if (pending_milliseconds >= SIMULATOR_CYCLE_MS)
    {
        uint32_t cycle_count = pending_milliseconds / SIMULATOR_CYCLE_MS;
        if (!skip_pattern_cycles(simulator, &cycle_count,
                                 &current_integral_mA_ms))
        {
            return false;
        }
        pending_milliseconds %= SIMULATOR_CYCLE_MS;
    }

    while (pending_milliseconds > 0)
    {
        const uint32_t to_boundary =
            BATTERY_SIMULATOR_STATE_DURATION_MS -
            simulator->pattern_elapsed_ms;
        const uint32_t segment =
            pending_milliseconds < to_boundary
                ? pending_milliseconds
                : to_boundary;
        if (!apply_segment(simulator, segment, &current_integral_mA_ms))
        {
            return false;
        }
        pending_milliseconds -= segment;
    }

    int32_t output_current_mA =
        CURRENT_PATTERN_MA[simulator->current_pattern_index];
    if (elapsed_milliseconds > 0)
    {
        const int64_t elapsed = (int64_t)elapsed_milliseconds;
        const int64_t rounded_integral =
            current_integral_mA_ms >= 0
                ? current_integral_mA_ms + elapsed / 2
                : current_integral_mA_ms - elapsed / 2;
        output_current_mA = (int32_t)(rounded_integral / elapsed);
    }

    return populate_data(simulator, output_current_mA, data);
}

bool battery_simulator_next(battery_simulator_t *simulator,
                            battery_data_t *data)
{
    return battery_simulator_step(simulator,
                                  BATTERY_SIMULATOR_STATE_DURATION_MS,
                                  data);
}
