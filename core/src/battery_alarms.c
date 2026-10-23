#include "battery.h"

#define MILLIVOLTS_PER_VOLT 1000
#define DECICELSIUS_PER_CELSIUS 10
#define MILLIAMPS_PER_AMP 1000

bool battery_evaluate_alarms(const battery_data_t *data, uint32_t *result)
{
    if (!battery_data_is_valid(data) || result == NULL) {
        return false;
    }

    uint32_t alarms = BATTERY_ALARM_NONE;
    const int64_t current_mA = data->current_mA;
    const bool charging = current_mA > 0;

    if (data->voltage_mV < 43200) {
        alarms |= BATTERY_ALARM_VOLTAGE_LOW;
    }
    if (data->voltage_mV > 57600) {
        alarms |= BATTERY_ALARM_VOLTAGE_HIGH;
    }
    if (charging && data->voltage_mV > 58400) {
        alarms |= BATTERY_ALARM_CHARGE_VOLTAGE_HIGH;
    }

    for (uint8_t i = 0; i < data->temp_count; ++i) {
        if (charging) {
            if (data->temp_dC[i] < 0 || data->temp_dC[i] > 450) {
                alarms |= BATTERY_ALARM_CHARGE_TEMPERATURE;
            }
        } else if (data->temp_dC[i] < -200 || data->temp_dC[i] > 600) {
            alarms |= BATTERY_ALARM_DISCHARGE_TEMPERATURE;
        }
    }

    uint32_t continuous_current_limit_mA = 0;
    if (data->nominal_mAh == 100000) {
        continuous_current_limit_mA = 100 * MILLIAMPS_PER_AMP;
    } else if (data->nominal_mAh == 335000) {
        continuous_current_limit_mA = 200 * MILLIAMPS_PER_AMP;
    }

    if (continuous_current_limit_mA > 0) {
        if (charging && current_mA > continuous_current_limit_mA) {
            alarms |= BATTERY_ALARM_CHARGE_CURRENT_HIGH;
        } else if (!charging && -current_mA > continuous_current_limit_mA) {
            alarms |= BATTERY_ALARM_DISCHARGE_CURRENT_HIGH;
        }
    }

    *result = alarms;
    return true;
}
