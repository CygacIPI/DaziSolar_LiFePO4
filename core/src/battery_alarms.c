#include "battery.h"

static bool alarm_config_is_valid(const battery_alarm_config_t *config)
{
    return config != NULL &&
           config->minimum_voltage_mV < config->maximum_voltage_mV &&
           config->maximum_voltage_mV <= config->maximum_charge_voltage_mV &&
           (config->current_limits_available
                ? config->maximum_charge_current_mA > 0 &&
                      config->maximum_discharge_current_mA > 0
                : config->maximum_charge_current_mA == 0 &&
                      config->maximum_discharge_current_mA == 0) &&
           config->minimum_charge_temperature_dC <=
               config->maximum_charge_temperature_dC &&
           config->minimum_discharge_temperature_dC <=
               config->maximum_discharge_temperature_dC;
}

bool battery_alarm_config_for_capacity(uint32_t nominal_mAh,
                                       battery_alarm_config_t *config)
{
    if (config == NULL)
    {
        return false;
    }

    *config = (battery_alarm_config_t){
        .nominal_mAh = nominal_mAh,
        .minimum_voltage_mV = 43200,
        .maximum_voltage_mV = 57600,
        .maximum_charge_voltage_mV = 58400,
        .minimum_charge_temperature_dC = 0,
        .maximum_charge_temperature_dC = 450,
        .minimum_discharge_temperature_dC = -200,
        .maximum_discharge_temperature_dC = 600,
        .maximum_charge_current_mA = 0,
        .maximum_discharge_current_mA = 0,
        .current_limits_available = false,
        .thresholds_are_provisional = true};

    if (nominal_mAh == 100000)
    {
        config->maximum_charge_current_mA = 100000;
        config->maximum_discharge_current_mA = 100000;
        config->current_limits_available = true;
    }
    else if (nominal_mAh == 314000)
    {
        config->current_limits_available = false;
        config->maximum_charge_current_mA = 0;
        config->maximum_discharge_current_mA = 0;
    }
    else if (nominal_mAh == 335000)
    {
        config->maximum_charge_current_mA = 200000;
        config->maximum_discharge_current_mA = 200000;
        config->current_limits_available = true;
    }

    return true;
}

bool battery_assess_alarms_with_config(
    const battery_data_t *data,
    const battery_alarm_config_t *config,
    battery_alarm_assessment_t *assessment)
{
    if (assessment != NULL)
    {
        *assessment = (battery_alarm_assessment_t){
            .active_alarms = BATTERY_ALARM_NONE,
            .current_limit_evaluable = false};
    }
    if (!battery_data_is_valid(data) ||
        !alarm_config_is_valid(config) ||
        assessment == NULL ||
        (battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY) &&
         config->nominal_mAh != 0 &&
         config->nominal_mAh != data->nominal_mAh))
    {
        return false;
    }

    uint32_t alarms = BATTERY_ALARM_NONE;
    if (battery_has_field(data, BATTERY_FIELD_VOLTAGE))
    {
        if (data->voltage_mV < config->minimum_voltage_mV)
        {
            alarms |= BATTERY_ALARM_VOLTAGE_LOW;
        }
        const bool charging =
            battery_has_field(data, BATTERY_FIELD_CURRENT) &&
            data->current_mA > 0;
        if (charging &&
            data->voltage_mV > config->maximum_charge_voltage_mV)
        {
            alarms |= BATTERY_ALARM_CHARGE_VOLTAGE_HIGH;
        }
        else if (data->voltage_mV > config->maximum_voltage_mV)
        {
            alarms |= BATTERY_ALARM_VOLTAGE_HIGH;
        }
    }

    if (battery_has_field(data, BATTERY_FIELD_TEMPERATURES) &&
        battery_has_field(data, BATTERY_FIELD_CURRENT))
    {
        const bool charging = data->current_mA > 0;
        for (uint8_t i = 0; i < data->temp_count; ++i)
        {
            if (charging)
            {
                if (data->temp_dC[i] < config->minimum_charge_temperature_dC ||
                    data->temp_dC[i] > config->maximum_charge_temperature_dC)
                {
                    alarms |= BATTERY_ALARM_CHARGE_TEMPERATURE;
                }
            }
            else if (data->temp_dC[i] <
                         config->minimum_discharge_temperature_dC ||
                     data->temp_dC[i] >
                         config->maximum_discharge_temperature_dC)
            {
                alarms |= BATTERY_ALARM_DISCHARGE_TEMPERATURE;
            }
        }
    }

    assessment->current_limit_evaluable =
        battery_has_field(data, BATTERY_FIELD_CURRENT) &&
        config->current_limits_available;
    if (assessment->current_limit_evaluable)
    {
        const int64_t current_mA = data->current_mA;
        if (current_mA > 0 &&
            config->maximum_charge_current_mA > 0 &&
            current_mA > (int64_t)config->maximum_charge_current_mA)
        {
            alarms |= BATTERY_ALARM_CHARGE_CURRENT_HIGH;
        }
        else if (current_mA < 0 &&
                 config->maximum_discharge_current_mA > 0 &&
                 -current_mA > (int64_t)config->maximum_discharge_current_mA)
        {
            alarms |= BATTERY_ALARM_DISCHARGE_CURRENT_HIGH;
        }
    }

    assessment->active_alarms = alarms;
    return true;
}

bool battery_assess_alarms(const battery_data_t *data,
                           battery_alarm_assessment_t *assessment)
{
    if (data == NULL)
    {
        if (assessment != NULL)
        {
            *assessment = (battery_alarm_assessment_t){
                .active_alarms = BATTERY_ALARM_NONE,
                .current_limit_evaluable = false};
        }
        return false;
    }

    battery_alarm_config_t config;
    const uint32_t nominal_mAh =
        battery_has_field(data, BATTERY_FIELD_NOMINAL_CAPACITY)
            ? data->nominal_mAh
            : 0;
    return battery_alarm_config_for_capacity(nominal_mAh, &config) &&
           battery_assess_alarms_with_config(data, &config, assessment);
}

bool battery_evaluate_alarms_with_config(const battery_data_t *data,
                                         const battery_alarm_config_t *config,
                                         uint32_t *alarms)
{
    if (alarms != NULL)
    {
        *alarms = BATTERY_ALARM_NONE;
    }
    if (alarms == NULL)
    {
        return false;
    }
    battery_alarm_assessment_t assessment;
    if (!battery_assess_alarms_with_config(data, config, &assessment))
    {
        return false;
    }
    *alarms = assessment.active_alarms;
    return assessment.current_limit_evaluable;
}

bool battery_evaluate_alarms(const battery_data_t *data, uint32_t *alarms)
{
    if (alarms != NULL)
    {
        *alarms = BATTERY_ALARM_NONE;
    }
    if (alarms == NULL)
    {
        return false;
    }
    battery_alarm_assessment_t assessment;
    if (!battery_assess_alarms(data, &assessment))
    {
        return false;
    }
    *alarms = assessment.active_alarms;
    return assessment.current_limit_evaluable;
}
