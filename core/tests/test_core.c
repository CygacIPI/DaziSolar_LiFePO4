#include "battery.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "FALLO %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static int nearly_equal(double left, double right, double tolerance)
{
    return fabs(left - right) <= tolerance;
}

static uint32_t evaluate_alarms(const battery_data_t *data)
{
    uint32_t alarms = UINT32_MAX;
    CHECK(battery_evaluate_alarms(data, &alarms));
    return alarms;
}

static battery_data_t reference_battery(void)
{
    battery_data_t data;
    memset(&data, 0, sizeof(data));
    data.pack_id = 1;
    data.voltage_mV = 53317;
    data.current_mA = -10000;
    data.soc_pct = 68;
    data.remaining_mAh = 68870;
    data.nominal_mAh = 100000;
    data.cell_count = 16;
    for (uint8_t i = 0; i < data.cell_count; ++i) {
        data.cell_mV[i] = (i < 11) ? 3332 : 3333;
    }
    data.temp_count = 1;
    data.temp_dC[0] = 250;
    data.source = BATTERY_SOURCE_SIMULATOR;
    return data;
}

static void test_reference_calculations(void)
{
    const battery_data_t data = reference_battery();
    battery_calculations_t calculations;

    CHECK(battery_calculate(&data, 80, &calculations));
    CHECK(nearly_equal(calculations.stored_energy_Wh, 3671.94179, 0.01));
    CHECK(nearly_equal(calculations.nominal_energy_Wh, 5120.0, 0.001));
    CHECK(nearly_equal(calculations.usable_nominal_energy_Wh, 4096.0, 0.001));
    CHECK(nearly_equal(calculations.power_W, 533.17, 0.001));
    CHECK(calculations.runtime_available);
    CHECK(nearly_equal(calculations.runtime_h,
                       calculations.stored_energy_Wh / 533.17,
                       0.001));
}

static void test_calculation_guards(void)
{
    battery_data_t data = reference_battery();
    battery_calculations_t calculations;

    CHECK(!battery_calculate(&data, 101, &calculations));
    data.soc_pct = 101;
    CHECK(!battery_calculate(&data, 80, &calculations));
    data = reference_battery();
    data.current_mA = 0;
    CHECK(battery_calculate(&data, 80, &calculations));
    CHECK(!calculations.runtime_available);
    CHECK(calculations.runtime_h == 0.0);
}

static void test_alarm_thresholds(void)
{
    battery_data_t data = reference_battery();

    data.voltage_mV = 43199;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_LOW) != 0);
    data.voltage_mV = 57601;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_HIGH) != 0);

    data.current_mA = 1;
    data.voltage_mV = 58401;
    CHECK((evaluate_alarms(&data) &
           BATTERY_ALARM_CHARGE_VOLTAGE_HIGH) != 0);

    data.voltage_mV = 53317;
    data.current_mA = 100001;
    CHECK((evaluate_alarms(&data) &
           BATTERY_ALARM_CHARGE_CURRENT_HIGH) != 0);
    data.current_mA = -100001;
    CHECK((evaluate_alarms(&data) &
           BATTERY_ALARM_DISCHARGE_CURRENT_HIGH) != 0);

    data.current_mA = 0;
    data.temp_dC[0] = -201;
    CHECK((evaluate_alarms(&data) &
           BATTERY_ALARM_DISCHARGE_TEMPERATURE) != 0);
    data.current_mA = 1;
    CHECK((evaluate_alarms(&data) &
           BATTERY_ALARM_CHARGE_TEMPERATURE) != 0);

    data.pack_id = 0;
    uint32_t alarms = 0;
    CHECK(!battery_evaluate_alarms(&data, &alarms));
}

static void test_data_validation(void)
{
    battery_data_t data = reference_battery();

    CHECK(battery_data_is_valid(&data));
    CHECK(battery_validate_data(NULL) == BATTERY_DATA_INVALID_NULL);
    data.pack_id = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_PACK_ID);
    CHECK(!battery_data_is_valid(&data));
    data = reference_battery();
    data.cell_count = BATTERY_MAX_CELLS + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CELL_COUNT);
    CHECK(!battery_data_is_valid(&data));
    data = reference_battery();
    data.temp_count = BATTERY_MAX_TEMPERATURES + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_TEMPERATURE_COUNT);
    data = reference_battery();
    data.source = BATTERY_SOURCE_CAN + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_SOURCE);
    data = reference_battery();
    data.remaining_mAh = data.nominal_mAh + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CAPACITY);
    data = reference_battery();
    data.cell_mV[3] = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CELL_VOLTAGE);
    data = reference_battery();
    data.nominal_mAh = 0;
    data.remaining_mAh = 1000;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);
}

static void test_simulator(void)
{
    battery_simulator_t simulator;
    battery_data_t data;
    uint32_t cell_voltage_sum = 0;

    CHECK(!battery_simulator_init(&simulator, 0, 100000));
    CHECK(battery_simulator_init(&simulator, 1, 100000));
    CHECK(battery_simulator_next(&simulator, &data));
    CHECK(battery_data_is_valid(&data));
    CHECK(data.source == BATTERY_SOURCE_SIMULATOR);
    CHECK(data.remaining_mAh == 68870);
    CHECK(data.alarms == BATTERY_ALARM_NONE);
    for (uint8_t i = 0; i < data.cell_count; ++i) {
        cell_voltage_sum += data.cell_mV[i];
    }
    CHECK(cell_voltage_sum == data.voltage_mV);
    CHECK(battery_simulator_next(&simulator, &data));
    CHECK(data.current_mA == -8000);
    CHECK(battery_simulator_init(&simulator, 2, 335000));
    CHECK(battery_simulator_next(&simulator, &data));
    CHECK(data.remaining_mAh == 230714);
    CHECK(data.pack_id == 2);
}

static void test_coulomb_counter(void)
{
    battery_coulomb_counter_t counter;
    uint8_t soc = 0;

    CHECK(!battery_coulomb_counter_init(&counter, 0, 50));
    CHECK(!battery_coulomb_counter_init(&counter, 100000, 101));
    CHECK(battery_coulomb_counter_init(&counter, 100000, 50));
    CHECK(battery_coulomb_counter_update(&counter, 10000, 3600, &soc));
    CHECK(soc == 60);
    CHECK(battery_coulomb_counter_update(&counter, -5000, 3600, &soc));
    CHECK(soc == 55);
    CHECK(battery_coulomb_counter_update(&counter, -100000, 3600, &soc));
    CHECK(soc == 0);
    CHECK(battery_coulomb_counter_update(&counter, 100000, 3600, &soc));
    CHECK(soc == 100);
    CHECK(!battery_coulomb_counter_update(&counter, 1000, 1, NULL));
}

int main(void)
{
    test_reference_calculations();
    test_calculation_guards();
    test_alarm_thresholds();
    test_data_validation();
    test_simulator();
    test_coulomb_counter();

    if (failures > 0) {
        fprintf(stderr, "%d prueba(s) fallaron.\n", failures);
        return 1;
    }

    puts("Todas las pruebas del nucleo pasaron.");
    return 0;
}
