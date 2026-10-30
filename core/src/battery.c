#include "battery.h"

bool battery_has_field(const battery_data_t *data, battery_field_t field)
{
    const uint32_t field_mask = (uint32_t)field;
    return data != NULL &&
           field_mask != 0 &&
           (field_mask & (field_mask - 1u)) == 0 &&
           (field_mask & BATTERY_KNOWN_FIELDS) != 0 &&
           (data->valid_fields & field_mask) != 0;
}

static bool field_is_absent_and_zero(const battery_data_t *data,
                                     battery_field_t field,
                                     uint32_t value)
{
    return battery_has_field(data, field) || value == 0;
}

battery_data_validation_t battery_validate_data(const battery_data_t *data)
{
    if (data == NULL)
    {
        return BATTERY_DATA_INVALID_NULL;
    }

    if ((data->valid_fields & ~BATTERY_KNOWN_FIELDS) != 0)
    {
        return BATTERY_DATA_INVALID_FIELD_MASK;
    }
    if (!battery_has_field(data, BATTERY_FIELD_PACK_ID))
    {
        return BATTERY_DATA_INVALID_MISSING_PACK_ID;
    }
    if (data->pack_id < BATTERY_MIN_PACK_ID ||
        data->pack_id > BATTERY_MAX_PACK_ID)
    {
        return BATTERY_DATA_INVALID_PACK_ID;
    }
    if (data->source > BATTERY_SOURCE_CAN)
    {
        return BATTERY_DATA_INVALID_SOURCE;
    }
    if (battery_has_field(data, BATTERY_FIELD_VOLTAGE) &&
        data->voltage_mV == 0)
    {
        return BATTERY_DATA_INVALID_VOLTAGE;
    }
    if (battery_has_field(data, BATTERY_FIELD_SOC) &&
        data->soc_pct > 100)
    {
        return BATTERY_DATA_INVALID_SOC;
    }
    if (!battery_has_field(data, BATTERY_FIELD_CELLS))
    {
        if (data->cell_count != 0)
        {
            return BATTERY_DATA_INVALID_CELL_COUNT;
        }
    }
    else
    {
        if (data->cell_count == 0 || data->cell_count > BATTERY_MAX_CELLS)
        {
            return BATTERY_DATA_INVALID_CELL_COUNT;
        }
        for (uint8_t i = 0; i < data->cell_count; ++i)
        {
            if (data->cell_mV[i] == 0)
            {
                return BATTERY_DATA_INVALID_CELL_VOLTAGE;
            }
        }
    }
    if (!battery_has_field(data, BATTERY_FIELD_TEMPERATURES))
    {
        if (data->temp_count != 0)
        {
            return BATTERY_DATA_INVALID_TEMPERATURE_COUNT;
        }
    }
    else if (data->temp_count == 0 ||
             data->temp_count > BATTERY_MAX_TEMPERATURES)
    {
        return BATTERY_DATA_INVALID_TEMPERATURE_COUNT;
    }
    if (battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY) &&
        data->nominal_mAh == 0)
    {
        return BATTERY_DATA_INVALID_CAPACITY;
    }
    if (battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY) &&
        battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY) &&
        data->remaining_mAh > data->nominal_mAh)
    {
        return BATTERY_DATA_INVALID_CAPACITY;
    }
    if (battery_has_field(data, BATTERY_FIELD_SOC) &&
        battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY) &&
        battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY))
    {
        const uint64_t soc_scaled =
            (uint64_t)data->soc_pct * data->nominal_mAh;
        const uint64_t capacity_scaled =
            (uint64_t)data->remaining_mAh * 100u;
        const uint64_t difference = soc_scaled > capacity_scaled
                                        ? soc_scaled - capacity_scaled
                                        : capacity_scaled - soc_scaled;
        const uint64_t tolerance =
            (uint64_t)data->nominal_mAh *
            BATTERY_SOC_CAPACITY_TOLERANCE_PCT;
        if (difference > tolerance)
        {
            return BATTERY_DATA_INVALID_SOC_CAPACITY_MISMATCH;
        }
    }
    if ((!battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY) &&
         data->remaining_mAh != 0) ||
        (!battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY) &&
         data->nominal_mAh != 0) ||
        (!battery_has_field(data, BATTERY_FIELD_SOC) && data->soc_pct != 0) ||
        (!battery_has_field(data, BATTERY_FIELD_CYCLES) && data->cycles != 0))
    {
        return BATTERY_DATA_INVALID_FIELD_MASK;
    }
    if (!battery_has_field(data, BATTERY_FIELD_ALARMS))
    {
        if (data->alarms != 0)
        {
            return BATTERY_DATA_INVALID_FIELD_MASK;
        }
    }
    else if ((data->alarms & ~BATTERY_ALARM_KNOWN_MASK) != 0)
    {
        return BATTERY_DATA_INVALID_UNKNOWN_ALARM;
    }
    if (!field_is_absent_and_zero(data, BATTERY_FIELD_VOLTAGE, data->voltage_mV) ||
        !field_is_absent_and_zero(data, BATTERY_FIELD_CURRENT,
                                  (uint32_t)data->current_mA))
    {
        return BATTERY_DATA_INVALID_FIELD_MASK;
    }

    return BATTERY_DATA_VALID;
}

bool battery_data_is_valid(const battery_data_t *data)
{
    return battery_validate_data(data) == BATTERY_DATA_VALID;
}
