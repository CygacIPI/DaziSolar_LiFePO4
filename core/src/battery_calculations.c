#include "battery.h"

#include <math.h>

bool battery_calculate(const battery_data_t *data,
                       uint8_t depth_of_discharge_pct,
                       battery_calculations_t *calculations)
{
    if (!battery_data_is_valid(data) ||
        calculations == NULL ||
        depth_of_discharge_pct > 100) {
        return false;
    }

    const double voltage_V = (double)data->voltage_mV / 1000.0;
    const double remaining_Ah = (double)data->remaining_mAh / 1000.0;
    const double nominal_Ah = (double)data->nominal_mAh / 1000.0;
    const double current_A = (double)data->current_mA / 1000.0;

    calculations->stored_energy_Wh = voltage_V * remaining_Ah;
    calculations->nominal_energy_Wh = 51.2 * nominal_Ah;
    calculations->usable_nominal_energy_Wh =
        calculations->nominal_energy_Wh *
        ((double)depth_of_discharge_pct / 100.0);
    calculations->power_W = voltage_V * fabs(current_A);
    calculations->runtime_available = data->current_mA < 0 &&
                                       calculations->power_W > 0.0;
    calculations->runtime_h = calculations->runtime_available
                                  ? calculations->stored_energy_Wh / calculations->power_W
                                  : 0.0;

    return true;
}
