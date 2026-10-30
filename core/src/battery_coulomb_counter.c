#include "battery.h"

#include <math.h>

bool battery_coulomb_counter_init(battery_coulomb_counter_t *counter,
                                  uint32_t nominal_mAh,
                                  uint8_t initial_soc_pct)
{
    if (counter == NULL || nominal_mAh == 0 || initial_soc_pct > 100)
    {
        return false;
    }

    counter->nominal_mAh = nominal_mAh;
    counter->remaining_mAh =
        (double)nominal_mAh * (double)initial_soc_pct / 100.0;
    return true;
}

bool battery_coulomb_counter_update(battery_coulomb_counter_t *counter,
                                    int32_t current_mA,
                                    uint32_t elapsed_milliseconds,
                                    uint32_t *remaining_mAh,
                                    uint8_t *estimated_soc_pct)
{
    if (counter == NULL ||
        remaining_mAh == NULL ||
        estimated_soc_pct == NULL)
    {
        return false;
    }

    const double delta_mAh =
        (double)current_mA * (double)elapsed_milliseconds / 3600000.0;
    return battery_coulomb_counter_apply_charge_delta_mAh(
        counter, delta_mAh, remaining_mAh, estimated_soc_pct);
}

bool battery_coulomb_counter_apply_charge_delta_mAh(
    battery_coulomb_counter_t *counter,
    double charge_delta_mAh,
    uint32_t *remaining_mAh,
    uint8_t *estimated_soc_pct)
{
    if (counter == NULL ||
        remaining_mAh == NULL ||
        estimated_soc_pct == NULL ||
        counter->nominal_mAh == 0 ||
        !isfinite(counter->remaining_mAh) ||
        counter->remaining_mAh < 0.0 ||
        counter->remaining_mAh > (double)counter->nominal_mAh ||
        !isfinite(charge_delta_mAh))
    {
        return false;
    }

    const double next_remaining_mAh =
        counter->remaining_mAh + charge_delta_mAh;
    if (!isfinite(next_remaining_mAh))
    {
        return false;
    }

    counter->remaining_mAh = next_remaining_mAh;
    if (counter->remaining_mAh < 0.0)
    {
        counter->remaining_mAh = 0.0;
    }
    else if (counter->remaining_mAh > (double)counter->nominal_mAh)
    {
        counter->remaining_mAh = (double)counter->nominal_mAh;
    }

    const double rounded_remaining_mAh = floor(counter->remaining_mAh + 0.5);
    *remaining_mAh = rounded_remaining_mAh >= (double)UINT32_MAX
                        ? UINT32_MAX
                        : (uint32_t)rounded_remaining_mAh;
    const double soc =
        counter->remaining_mAh * 100.0 / (double)counter->nominal_mAh;
    *estimated_soc_pct = (uint8_t)(soc + 0.5);
    return true;
}
