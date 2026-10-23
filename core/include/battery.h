#ifndef LFEPO4_BATTERY_H
#define LFEPO4_BATTERY_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BATTERY_MAX_CELLS 32
#define BATTERY_MAX_TEMPERATURES 8
#define BATTERY_MIN_PACK_ID 1
#define BATTERY_MAX_PACK_ID 15

typedef enum {
    BATTERY_SOURCE_SIMULATOR = 0,
    BATTERY_SOURCE_TCP,
    BATTERY_SOURCE_SERIAL,
    BATTERY_SOURCE_CAN
} battery_source_t;

typedef struct {
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
} battery_data_t;

typedef enum {
    BATTERY_ALARM_NONE = 0,
    BATTERY_ALARM_VOLTAGE_LOW = 1u << 0,
    BATTERY_ALARM_VOLTAGE_HIGH = 1u << 1,
    BATTERY_ALARM_CHARGE_VOLTAGE_HIGH = 1u << 2,
    BATTERY_ALARM_CHARGE_TEMPERATURE = 1u << 3,
    BATTERY_ALARM_DISCHARGE_TEMPERATURE = 1u << 4,
    BATTERY_ALARM_CHARGE_CURRENT_HIGH = 1u << 5,
    BATTERY_ALARM_DISCHARGE_CURRENT_HIGH = 1u << 6
} battery_alarm_t;

typedef struct {
    double stored_energy_Wh;
    double nominal_energy_Wh;
    double usable_nominal_energy_Wh;
    double power_W;
    double runtime_h;
    bool runtime_available;
} battery_calculations_t;

typedef struct {
    uint8_t pack_id;
    uint32_t nominal_mAh;
    uint32_t tick;
} battery_simulator_t;

typedef struct {
    uint32_t nominal_mAh;
    double soc_pct;
} battery_coulomb_counter_t;

bool battery_data_is_valid(const battery_data_t *data);
bool battery_calculate(const battery_data_t *data,
                       uint8_t depth_of_discharge_pct,
                       battery_calculations_t *calculations);
bool battery_evaluate_alarms(const battery_data_t *data, uint32_t *alarms);
bool battery_coulomb_counter_init(battery_coulomb_counter_t *counter,
                                  uint32_t nominal_mAh,
                                  uint8_t initial_soc_pct);
bool battery_coulomb_counter_update(battery_coulomb_counter_t *counter,
                                    int32_t current_mA,
                                    uint32_t elapsed_seconds,
                                    uint8_t *estimated_soc_pct);

bool battery_simulator_init(battery_simulator_t *simulator,
                            uint8_t pack_id,
                            uint32_t nominal_mAh);
bool battery_simulator_next(battery_simulator_t *simulator,
                            battery_data_t *data);

#endif
