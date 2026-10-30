#ifndef LFEPO4_BATTERY_H
#define LFEPO4_BATTERY_H

#include <stdbool.h>
#include <stdint.h>

#define BATTERY_MAX_CELLS 32
#define BATTERY_MAX_TEMPERATURES 8
#define BATTERY_MIN_PACK_ID 1
#define BATTERY_MAX_PACK_ID 15
#define BATTERY_KNOWN_FIELDS ((uint32_t)((1u << 10) - 1u))
#define BATTERY_ALARM_KNOWN_MASK UINT32_C(0x7f)

typedef enum
{
    BATTERY_SOURCE_SIMULATOR = 0,
    BATTERY_SOURCE_TCP,
    BATTERY_SOURCE_SERIAL,
    BATTERY_SOURCE_CAN
} battery_source_t;

typedef enum
{
    BATTERY_FIELD_PACK_ID = 1u << 0,
    BATTERY_FIELD_VOLTAGE = 1u << 1,
    BATTERY_FIELD_CURRENT = 1u << 2,
    BATTERY_FIELD_SOC = 1u << 3,
    BATTERY_FIELD_REMAINING_CAPACITY = 1u << 4,
    BATTERY_FIELD_NOMINAL_CAPACITY = 1u << 5,
    BATTERY_FIELD_CYCLES = 1u << 6,
    BATTERY_FIELD_CELLS = 1u << 7,
    BATTERY_FIELD_TEMPERATURES = 1u << 8,
    BATTERY_FIELD_ALARMS = 1u << 9
} battery_field_t;

typedef struct
{
    uint8_t pack_id;
    uint32_t voltage_mV;
    int32_t current_mA;
    uint8_t soc_pct;
    uint32_t remaining_mAh;
    uint32_t nominal_mAh;
    uint16_t cycles;
    uint8_t cell_count;
    uint16_t cell_mV[BATTERY_MAX_CELLS];
    uint8_t temp_count;
    int16_t temp_dC[BATTERY_MAX_TEMPERATURES];
    uint32_t alarms;
    uint8_t source;
    uint32_t valid_fields;
} battery_data_t;

typedef enum
{
    BATTERY_ALARM_NONE = 0,
    BATTERY_ALARM_VOLTAGE_LOW = 1u << 0,
    BATTERY_ALARM_VOLTAGE_HIGH = 1u << 1,
    BATTERY_ALARM_CHARGE_VOLTAGE_HIGH = 1u << 2,
    BATTERY_ALARM_CHARGE_TEMPERATURE = 1u << 3,
    BATTERY_ALARM_DISCHARGE_TEMPERATURE = 1u << 4,
    BATTERY_ALARM_CHARGE_CURRENT_HIGH = 1u << 5,
    BATTERY_ALARM_DISCHARGE_CURRENT_HIGH = 1u << 6
} battery_alarm_t;

typedef struct
{
    double stored_energy_Wh;
    double nominal_energy_Wh;
    double usable_nominal_energy_Wh;
    double power_W;
    double runtime_h;
    bool stored_energy_available;
    bool nominal_energy_available;
    bool power_available;
    bool runtime_available;
} battery_calculations_t;

typedef struct
{
    uint32_t nominal_mAh;
    double remaining_mAh;
} battery_coulomb_counter_t;

typedef struct
{
    uint8_t pack_id;
    uint32_t nominal_mAh;
    uint32_t tick;
    uint8_t current_pattern_index;
    battery_coulomb_counter_t coulomb_counter;
} battery_simulator_t;

typedef struct
{
    uint32_t nominal_mAh;
    uint32_t minimum_voltage_mV;
    uint32_t maximum_voltage_mV;
    uint32_t maximum_charge_voltage_mV;
    int16_t minimum_charge_temperature_dC;
    int16_t maximum_charge_temperature_dC;
    int16_t minimum_discharge_temperature_dC;
    int16_t maximum_discharge_temperature_dC;
    uint32_t maximum_charge_current_mA;
    uint32_t maximum_discharge_current_mA;
    bool thresholds_are_provisional;
} battery_alarm_config_t;

typedef enum
{
    BATTERY_DATA_VALID = 0,
    BATTERY_DATA_INVALID_NULL,
    BATTERY_DATA_INVALID_FIELD_MASK,
    BATTERY_DATA_INVALID_MISSING_PACK_ID,
    BATTERY_DATA_INVALID_PACK_ID,
    BATTERY_DATA_INVALID_VOLTAGE,
    BATTERY_DATA_INVALID_SOC,
    BATTERY_DATA_INVALID_CELL_COUNT,
    BATTERY_DATA_INVALID_TEMPERATURE_COUNT,
    BATTERY_DATA_INVALID_SOURCE,
    BATTERY_DATA_INVALID_CAPACITY,
    BATTERY_DATA_INVALID_CELL_VOLTAGE,
    BATTERY_DATA_INVALID_UNKNOWN_ALARM
} battery_data_validation_t;

bool battery_has_field(const battery_data_t *data, battery_field_t field);
battery_data_validation_t battery_validate_data(const battery_data_t *data);
bool battery_data_is_valid(const battery_data_t *data);
bool battery_calculate(const battery_data_t *data,
                       uint8_t depth_of_discharge_pct,
                       battery_calculations_t *calculations);
bool battery_alarm_config_for_capacity(uint32_t nominal_mAh,
                                       battery_alarm_config_t *config);
bool battery_evaluate_alarms_with_config(const battery_data_t *data,
                                         const battery_alarm_config_t *config,
                                         uint32_t *alarms);
bool battery_evaluate_alarms(const battery_data_t *data, uint32_t *alarms);
bool battery_coulomb_counter_init(battery_coulomb_counter_t *counter,
                                  uint32_t nominal_mAh,
                                  uint8_t initial_soc_pct);
bool battery_coulomb_counter_update(battery_coulomb_counter_t *counter,
                                    int32_t current_mA,
                                    uint32_t elapsed_milliseconds,
                                    uint32_t *remaining_mAh,
                                    uint8_t *estimated_soc_pct);
bool battery_simulator_init(battery_simulator_t *simulator,
                            uint8_t pack_id,
                            uint32_t nominal_mAh);
bool battery_simulator_next(battery_simulator_t *simulator,
                            battery_data_t *data);
bool battery_simulator_step(battery_simulator_t *simulator,
                            uint32_t elapsed_milliseconds,
                            battery_data_t *data);

#endif
