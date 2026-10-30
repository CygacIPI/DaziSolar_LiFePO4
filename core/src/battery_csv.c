#include "battery_csv.h"

static bool write_header(FILE *file)
{
    if (fprintf(file,
                "timestamp_unix_s,pack_id,source,valid_fields,voltage_mV,"
                "current_mA,soc_pct,remaining_mAh,nominal_mAh,cycles,alarms,"
                "stored_energy_Wh,cell_count") < 0)
    {
        return false;
    }

    for (uint8_t i = 0; i < BATTERY_MAX_CELLS; ++i)
    {
        if (fprintf(file, ",cell_%u_mV", (unsigned)(i + 1)) < 0)
        {
            return false;
        }
    }
    if (fprintf(file, ",temperature_count") < 0)
    {
        return false;
    }
    for (uint8_t i = 0; i < BATTERY_MAX_TEMPERATURES; ++i)
    {
        if (fprintf(file, ",temperature_%u_dC", (unsigned)(i + 1)) < 0)
        {
            return false;
        }
    }
    return fputc('\n', file) != EOF;
}

static bool write_optional_unsigned(FILE *file, bool available, uint32_t value)
{
    if (!available)
    {
        return fputc(',', file) != EOF;
    }
    return fprintf(file, ",%lu", (unsigned long)value) >= 0;
}

static bool write_optional_signed(FILE *file, bool available, int32_t value)
{
    if (!available)
    {
        return fputc(',', file) != EOF;
    }
    return fprintf(file, ",%ld", (long)value) >= 0;
}

bool battery_csv_write(FILE *file,
                       const battery_csv_sample_t *samples,
                       size_t sample_count)
{
    if (file == NULL || (samples == NULL && sample_count != 0))
    {
        return false;
    }
    if (!write_header(file))
    {
        return false;
    }

    for (size_t i = 0; i < sample_count; ++i)
    {
        const battery_csv_sample_t *sample = &samples[i];
        const battery_data_t *data = &sample->data;
        battery_calculations_t calculations;
        if (!battery_data_is_valid(data) ||
            !battery_calculate(data, 100, &calculations))
        {
            return false;
        }

        if (fprintf(file, "%lld,%u,%u,0x%08lX",
                    (long long)sample->timestamp_unix_s,
                    battery_has_field(data, BATTERY_FIELD_PACK_ID)
                        ? (unsigned)data->pack_id
                        : 0u,
                    (unsigned)data->source,
                    (unsigned long)data->valid_fields) < 0 ||
            !write_optional_unsigned(file,
                                     battery_has_field(data, BATTERY_FIELD_VOLTAGE),
                                     data->voltage_mV) ||
            !write_optional_signed(file,
                                   battery_has_field(data, BATTERY_FIELD_CURRENT),
                                   data->current_mA) ||
            !write_optional_unsigned(file,
                                     battery_has_field(data, BATTERY_FIELD_SOC),
                                     data->soc_pct) ||
            !write_optional_unsigned(
                file, battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY),
                data->remaining_mAh) ||
            !write_optional_unsigned(
                file, battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY),
                data->nominal_mAh) ||
            !write_optional_unsigned(file,
                                     battery_has_field(data, BATTERY_FIELD_CYCLES),
                                     data->cycles) ||
            !write_optional_unsigned(file,
                                     battery_has_field(data, BATTERY_FIELD_ALARMS),
                                     data->alarms))
        {
            return false;
        }

        if (calculations.stored_energy_available)
        {
            if (fprintf(file, ",%.3f", calculations.stored_energy_Wh) < 0)
            {
                return false;
            }
        }
        else if (fputc(',', file) == EOF)
        {
            return false;
        }

        if (!write_optional_unsigned(file,
                                     battery_has_field(data, BATTERY_FIELD_CELLS),
                                     data->cell_count))
        {
            return false;
        }
        for (uint8_t cell = 0; cell < BATTERY_MAX_CELLS; ++cell)
        {
            if (cell < data->cell_count)
            {
                if (fprintf(file, ",%u", (unsigned)data->cell_mV[cell]) < 0)
                {
                    return false;
                }
            }
            else if (fputc(',', file) == EOF)
            {
                return false;
            }
        }

        if (!write_optional_unsigned(
                file, battery_has_field(data, BATTERY_FIELD_TEMPERATURES),
                data->temp_count))
        {
            return false;
        }
        for (uint8_t temperature = 0;
             temperature < BATTERY_MAX_TEMPERATURES;
             ++temperature)
        {
            if (temperature < data->temp_count)
            {
                if (fprintf(file, ",%d", (int)data->temp_dC[temperature]) < 0)
                {
                    return false;
                }
            }
            else if (fputc(',', file) == EOF)
            {
                return false;
            }
        }
        if (fputc('\n', file) == EOF)
        {
            return false;
        }
    }

    return !ferror(file);
}
