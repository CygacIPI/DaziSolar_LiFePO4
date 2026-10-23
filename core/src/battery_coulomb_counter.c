#include "battery.h"

bool battery_coulomb_counter_init(battery_coulomb_counter_t *counter,
                                  uint32_t nominal_mAh,
                                  uint8_t initial_soc_pct)
{
    if (counter == NULL || nominal_mAh == 0 || initial_soc_pct > 100) {
        return false;
    }

    counter->nominal_mAh = nominal_mAh;
    counter->soc_pct = (double)initial_soc_pct;
    return true;
}

bool battery_coulomb_counter_update(battery_coulomb_counter_t *counter,
                                    int32_t current_mA,
                                    uint32_t elapsed_seconds,
                                    uint8_t *estimated_soc_pct)
{
    if (counter == NULL ||
        estimated_soc_pct == NULL ||
        counter->nominal_mAh == 0 ||
        counter->soc_pct < 0.0 ||
        counter->soc_pct > 100.0) {
        return false;
    }

    const double delta_mAh =
        ((double)current_mA * (double)elapsed_seconds) / 3600.0;
    counter->soc_pct += delta_mAh * 100.0 / (double)counter->nominal_mAh;

    if (counter->soc_pct < 0.0) {
        counter->soc_pct = 0.0;
    } else if (counter->soc_pct > 100.0) {
        counter->soc_pct = 100.0;
    }

    *estimated_soc_pct = (uint8_t)(counter->soc_pct + 0.5);
    return true;
}
