#include "battery.h"

#include <math.h>
#include <string.h>

#define NOMINAL_PACK_VOLTAGE_V 51.2

bool battery_calculate(const battery_data_t *data,
                       uint8_t depth_of_discharge_pct,
                       battery_calculations_t *calculations)
{
    if (!battery_data_is_valid(data) ||
        calculations == NULL ||
        depth_of_discharge_pct > 100)
    {
        return false;
    }

    memset(calculations, 0, sizeof(*calculations));

    const bool has_voltage = battery_has_field(data, BATTERY_FIELD_VOLTAGE);
    const bool has_remaining =
        battery_has_field(data, BATTERY_FIELD_REMAINING_CAPACITY);
    const bool has_nominal =
        battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY);
    const bool has_current = battery_has_field(data, BATTERY_FIELD_CURRENT);

    if (has_voltage && has_remaining)
    {
        const double voltage_V = (double)data->voltage_mV / 1000.0;
        const double remaining_Ah = (double)data->remaining_mAh / 1000.0;
        calculations->stored_energy_Wh = voltage_V * remaining_Ah;
        calculations->stored_energy_available =
            isfinite(calculations->stored_energy_Wh);
    }
    if (has_nominal)
    {
        const double nominal_Ah = (double)data->nominal_mAh / 1000.0;
        calculations->nominal_energy_Wh = NOMINAL_PACK_VOLTAGE_V * nominal_Ah;
        calculations->usable_nominal_energy_Wh =
            calculations->nominal_energy_Wh *
            ((double)depth_of_discharge_pct / 100.0);
        calculations->nominal_energy_available =
            isfinite(calculations->nominal_energy_Wh) &&
            isfinite(calculations->usable_nominal_energy_Wh);
    }
    if (has_voltage && has_current)
    {
        const double voltage_V = (double)data->voltage_mV / 1000.0;
        const double current_A = (double)data->current_mA / 1000.0;
        calculations->power_W = voltage_V * fabs(current_A);
        calculations->power_available = isfinite(calculations->power_W);
        calculations->runtime_available =
            calculations->stored_energy_available &&
            data->current_mA < 0 &&
            calculations->power_available &&
            calculations->power_W > 0.0;
        if (calculations->runtime_available)
        {
            calculations->runtime_h =
                calculations->stored_energy_Wh / calculations->power_W;
            if (!isfinite(calculations->runtime_h))
            {
                calculations->runtime_h = 0.0;
                calculations->runtime_available = false;
            }
        }
    }

    return true;
}
