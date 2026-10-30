#include "battery.h"
#include "battery_csv.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(condition)                                                        \
    do                                                                          \
    {                                                                           \
        if (!(condition))                                                       \
        {                                                                       \
            fprintf(stderr, "FALLO %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            failures++;                                                         \
        }                                                                       \
    } while (0)

static int nearly_equal(double left, double right, double tolerance)
{
    return fabs(left - right) <= tolerance;
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
    data.cycles = 12;
    data.cell_count = 16;
    data.temp_count = 1;
    data.temp_dC[0] = 250;
    data.source = BATTERY_SOURCE_SIMULATOR;
    data.valid_fields = BATTERY_KNOWN_FIELDS;
    for (uint8_t i = 0; i < data.cell_count; ++i)
    {
        data.cell_mV[i] = (uint16_t)(data.voltage_mV / data.cell_count);
    }
    data.cell_mV[0] = (uint16_t)(data.cell_mV[0] +
                                 data.voltage_mV % data.cell_count);
    return data;
}

static uint32_t evaluate_alarms(const battery_data_t *data)
{
    uint32_t alarms = UINT32_MAX;
    CHECK(battery_evaluate_alarms(data, &alarms));
    return alarms;
}

static void test_data_validation(void)
{
    battery_data_t data = reference_battery();

    CHECK(battery_data_is_valid(&data));
    CHECK(battery_validate_data(NULL) == BATTERY_DATA_INVALID_NULL);
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);
    CHECK(battery_has_field(&data, BATTERY_FIELD_CURRENT));
    CHECK(!battery_has_field(&data, (battery_field_t)0));
    CHECK(!battery_has_field(
        &data, (battery_field_t)(BATTERY_FIELD_VOLTAGE | BATTERY_FIELD_CURRENT)));
    CHECK(!battery_has_field(&data, (battery_field_t)(1u << 20)));

    data.valid_fields |= 1u << 20;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_FIELD_MASK);
    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_PACK_ID;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_MISSING_PACK_ID);
    data = reference_battery();
    data.pack_id = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_PACK_ID);
    data.pack_id = 16;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_PACK_ID);

    data = reference_battery();
    data.voltage_mV = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_VOLTAGE);
    data = reference_battery();
    data.soc_pct = 101;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_SOC);

    data = reference_battery();
    data.cell_count = BATTERY_MAX_CELLS + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CELL_COUNT);
    data = reference_battery();
    data.cell_mV[3] = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CELL_VOLTAGE);
    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_CELLS;
    data.cell_count = 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CELL_COUNT);
    data.cell_count = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);

    data = reference_battery();
    data.temp_count = BATTERY_MAX_TEMPERATURES + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_TEMPERATURE_COUNT);
    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_TEMPERATURES;
    data.temp_count = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);

    data = reference_battery();
    data.source = BATTERY_SOURCE_CAN + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_SOURCE);
    data = reference_battery();
    data.remaining_mAh = data.nominal_mAh + 1;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_CAPACITY);
    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_NOMINAL_CAPACITY;
    data.nominal_mAh = 0;
    data.remaining_mAh = 1000;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_REMAINING_CAPACITY;
    data.remaining_mAh = 0;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_VALID);

    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_CURRENT;
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_ALARMS;
    data.current_mA = 0;
    data.alarms = 1u << 31;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_FIELD_MASK);
    data.alarms = 0;
    data.valid_fields |= BATTERY_FIELD_ALARMS;
    data.alarms = 1u << 31;
    CHECK(battery_validate_data(&data) == BATTERY_DATA_INVALID_UNKNOWN_ALARM);
}

static void test_calculations_and_unknown_fields(void)
{
    const battery_data_t data = reference_battery();
    battery_calculations_t calculations;

    CHECK(battery_calculate(&data, 80, &calculations));
    CHECK(calculations.stored_energy_available);
    CHECK(nearly_equal(calculations.stored_energy_Wh, 3671.94179, 0.01));
    CHECK(calculations.nominal_energy_available);
    CHECK(nearly_equal(calculations.nominal_energy_Wh, 5120.0, 0.001));
    CHECK(nearly_equal(calculations.usable_nominal_energy_Wh, 4096.0, 0.001));
    CHECK(calculations.power_available);
    CHECK(nearly_equal(calculations.power_W, 533.17, 0.001));
    CHECK(calculations.runtime_available);
    CHECK(nearly_equal(calculations.runtime_h,
                       calculations.stored_energy_Wh / calculations.power_W,
                       0.001));

    battery_data_t partial = reference_battery();
    partial.valid_fields = BATTERY_FIELD_PACK_ID | BATTERY_FIELD_VOLTAGE;
    partial.current_mA = 0;
    partial.soc_pct = 0;
    partial.remaining_mAh = 0;
    partial.nominal_mAh = 0;
    partial.cycles = 0;
    partial.cell_count = 0;
    partial.temp_count = 0;
    partial.alarms = 0;
    CHECK(battery_data_is_valid(&partial));
    CHECK(battery_calculate(&partial, 80, &calculations));
    CHECK(!calculations.stored_energy_available);
    CHECK(!calculations.nominal_energy_available);
    CHECK(!calculations.power_available);
    CHECK(!calculations.runtime_available);
    CHECK(calculations.stored_energy_Wh == 0.0);
    CHECK(calculations.runtime_h == 0.0);

    CHECK(!battery_calculate(&data, 101, &calculations));
    battery_data_t invalid = data;
    invalid.soc_pct = 101;
    CHECK(!battery_calculate(&invalid, 80, &calculations));
    invalid = data;
    invalid.remaining_mAh = invalid.nominal_mAh + 1;
    CHECK(!battery_calculate(&invalid, 80, &calculations));
}

static void test_alarm_thresholds_and_profiles(void)
{
    battery_data_t data = reference_battery();
    battery_alarm_config_t config;
    uint32_t alarms = 0;

    CHECK(battery_alarm_config_for_capacity(100000, &config));
    CHECK(config.thresholds_are_provisional);

    data.voltage_mV = 43200;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_LOW) == 0);
    data.voltage_mV = 43199;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_LOW) != 0);
    data.voltage_mV = 57600;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_HIGH) == 0);
    data.voltage_mV = 57601;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_VOLTAGE_HIGH) != 0);

    data.voltage_mV = 58400;
    data.current_mA = 1;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_VOLTAGE_HIGH) == 0);
    data.voltage_mV = 58401;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_VOLTAGE_HIGH) != 0);

    data.voltage_mV = 53317;
    data.current_mA = 100000;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_CURRENT_HIGH) == 0);
    data.current_mA = 100001;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_CURRENT_HIGH) != 0);
    data.current_mA = -100000;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_CURRENT_HIGH) == 0);
    data.current_mA = -100001;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_CURRENT_HIGH) != 0);

    data.current_mA = 1;
    data.temp_dC[0] = 0;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_TEMPERATURE) == 0);
    data.temp_dC[0] = 450;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_TEMPERATURE) == 0);
    data.temp_dC[0] = -1;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_TEMPERATURE) != 0);
    data.temp_dC[0] = 451;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_CHARGE_TEMPERATURE) != 0);

    data.current_mA = 0;
    data.temp_dC[0] = -200;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_TEMPERATURE) == 0);
    data.temp_dC[0] = 600;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_TEMPERATURE) == 0);
    data.temp_dC[0] = -201;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_TEMPERATURE) != 0);
    data.temp_dC[0] = 601;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_TEMPERATURE) != 0);

    data = reference_battery();
    data.valid_fields &= ~(uint32_t)BATTERY_FIELD_CURRENT;
    data.current_mA = 0;
    data.temp_dC[0] = -201;
    CHECK((evaluate_alarms(&data) & BATTERY_ALARM_DISCHARGE_TEMPERATURE) == 0);

    CHECK(battery_alarm_config_for_capacity(335000, &config));
    data = reference_battery();
    data.nominal_mAh = 335000;
    data.remaining_mAh = 200000;
    data.current_mA = 200000;
    CHECK(battery_evaluate_alarms_with_config(&data, &config, &alarms));
    CHECK((alarms & BATTERY_ALARM_CHARGE_CURRENT_HIGH) == 0);
    data.current_mA = 200001;
    CHECK(battery_evaluate_alarms_with_config(&data, &config, &alarms));
    CHECK((alarms & BATTERY_ALARM_CHARGE_CURRENT_HIGH) != 0);
    config.maximum_voltage_mV = config.minimum_voltage_mV;
    CHECK(!battery_evaluate_alarms_with_config(&data, &config, &alarms));
    CHECK(alarms == BATTERY_ALARM_NONE);
}

static void test_coulomb_counter(void)
{
    battery_coulomb_counter_t counter;
    uint32_t remaining_mAh = 0;
    uint8_t soc = 0;

    CHECK(!battery_coulomb_counter_init(&counter, 0, 50));
    CHECK(!battery_coulomb_counter_init(&counter, 100000, 101));
    CHECK(battery_coulomb_counter_init(&counter, 100000, 50));
    CHECK(battery_coulomb_counter_update(&counter, 10000, 3600000,
                                         &remaining_mAh, &soc));
    CHECK(remaining_mAh == 60000);
    CHECK(soc == 60);
    CHECK(battery_coulomb_counter_update(&counter, -5000, 3600000,
                                         &remaining_mAh, &soc));
    CHECK(remaining_mAh == 55000);
    CHECK(soc == 55);
    CHECK(battery_coulomb_counter_update(&counter, INT32_MIN, UINT32_MAX,
                                         &remaining_mAh, &soc));
    CHECK(remaining_mAh == 0);
    CHECK(soc == 0);
    CHECK(battery_coulomb_counter_update(&counter, INT32_MAX, UINT32_MAX,
                                         &remaining_mAh, &soc));
    CHECK(remaining_mAh == 100000);
    CHECK(soc == 100);
    CHECK(!battery_coulomb_counter_update(&counter, 1, 1, NULL, &soc));
    CHECK(!battery_coulomb_counter_update(&counter, 1, 1,
                                          &remaining_mAh, NULL));
    counter.remaining_mAh = NAN;
    CHECK(!battery_coulomb_counter_update(&counter, 1, 1,
                                          &remaining_mAh, &soc));
    CHECK(battery_coulomb_counter_init(&counter, UINT32_MAX, 100));
    CHECK(battery_coulomb_counter_update(&counter, 0, 0,
                                          &remaining_mAh, &soc));
    CHECK(remaining_mAh == UINT32_MAX);
    CHECK(soc == 100);
}

static void test_simulator_sequences(void)
{
    battery_simulator_t simulator;
    battery_data_t data;
    uint32_t previous_remaining;
    uint8_t previous_soc;

    CHECK(!battery_simulator_init(&simulator, 0, 100000));
    CHECK(!battery_simulator_init(&simulator, 1, 0));
    CHECK(battery_simulator_init(&simulator, 1, 100000));
    CHECK(battery_simulator_next(&simulator, &data));
    CHECK(battery_data_is_valid(&data));
    CHECK(data.valid_fields == BATTERY_KNOWN_FIELDS);
    CHECK(data.source == BATTERY_SOURCE_SIMULATOR);
    CHECK(data.voltage_mV == 53317);
    CHECK(data.remaining_mAh == 67997);
    CHECK(data.soc_pct == 68);
    previous_remaining = data.remaining_mAh;
    previous_soc = data.soc_pct;

    uint32_t cell_sum = 0;
    for (uint8_t i = 0; i < data.cell_count; ++i)
    {
        cell_sum += data.cell_mV[i];
    }
    CHECK(cell_sum == data.voltage_mV);

    CHECK(battery_simulator_step(&simulator, 3600000, &data));
    CHECK(data.current_mA == -8000);
    CHECK(data.remaining_mAh < previous_remaining);
    CHECK(data.soc_pct <= previous_soc);

    CHECK(battery_simulator_step(&simulator, 3600000, &data));
    CHECK(data.current_mA == 0);
    previous_remaining = data.remaining_mAh;
    previous_soc = data.soc_pct;
    CHECK(battery_simulator_step(&simulator, 3600000, &data));
    CHECK(data.current_mA == 15000);
    CHECK(data.remaining_mAh > previous_remaining);
    CHECK(data.soc_pct >= previous_soc);
    CHECK(data.voltage_mV != 53317);

    for (size_t i = 0; i < 10000; ++i)
    {
        CHECK(battery_simulator_step(&simulator, 1000, &data));
        CHECK(battery_data_is_valid(&data));
        CHECK(data.remaining_mAh <= data.nominal_mAh);
        CHECK(data.soc_pct <= 100);
        uint32_t sum = 0;
        for (uint8_t cell = 0; cell < data.cell_count; ++cell)
        {
            sum += data.cell_mV[cell];
        }
        CHECK(sum == data.voltage_mV);
    }

    CHECK(battery_simulator_init(&simulator, 2, 335000));
    CHECK(battery_simulator_next(&simulator, &data));
    CHECK(data.nominal_mAh == 335000);
    CHECK(data.remaining_mAh == 227797);
    CHECK(data.pack_id == 2);
}

static void test_csv_export(void)
{
    battery_csv_sample_t samples[2];
    memset(samples, 0, sizeof(samples));
    samples[0].timestamp_unix_s = 123456789;
    samples[0].data = reference_battery();
    samples[1].timestamp_unix_s = 123456790;
    samples[1].data.pack_id = 2;
    samples[1].data.source = BATTERY_SOURCE_SIMULATOR;
    samples[1].data.valid_fields = BATTERY_FIELD_PACK_ID;

    FILE *file = tmpfile();
    CHECK(file != NULL);
    if (file == NULL)
    {
        return;
    }
    CHECK(!battery_csv_write(NULL, samples, 2));
    CHECK(!battery_csv_write(file, NULL, 1));
    CHECK(battery_csv_write(file, samples, 2));
    CHECK(fflush(file) == 0);
    CHECK(fseek(file, 0, SEEK_SET) == 0);

    char content[16384];
    const size_t length = fread(content, 1, sizeof(content) - 1, file);
    content[length] = '\0';
    CHECK(strstr(content, "timestamp_unix_s") != NULL);
    CHECK(strstr(content, "voltage_mV") != NULL);
    CHECK(strstr(content, "temperature_8_dC") != NULL);
    CHECK(strstr(content, "123456789,1,0,0x000003FF,53317,-10000,68,68870") != NULL);
    CHECK(strstr(content, "123456790,2,0,0x00000001,,,,,,,") != NULL);

    battery_csv_sample_t invalid = samples[0];
    invalid.data.voltage_mV = 0;
    CHECK(!battery_csv_write(file, &invalid, 1));
    CHECK(fclose(file) == 0);
}

int main(void)
{
    test_data_validation();
    test_calculations_and_unknown_fields();
    test_alarm_thresholds_and_profiles();
    test_coulomb_counter();
    test_simulator_sequences();
    test_csv_export();

    if (failures > 0)
    {
        fprintf(stderr, "%d prueba(s) fallaron.\n", failures);
        return 1;
    }

    puts("Todas las pruebas del nucleo pasaron.");
    return 0;
}
