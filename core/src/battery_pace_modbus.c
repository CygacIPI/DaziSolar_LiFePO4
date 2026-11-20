#include "battery_pace_modbus.h"

#include <string.h>

bool battery_pace_modbus_crc16(const uint8_t *data,
                               size_t length,
                               uint16_t *crc)
{
    if (data == NULL || crc == NULL)
    {
        return false;
    }

    uint16_t value = 0xffffu;
    for (size_t i = 0; i < length; ++i)
    {
        value ^= (uint16_t)data[i];
        for (uint8_t bit = 0; bit < 8u; ++bit)
        {
            value = (value & 1u) != 0u
                        ? (uint16_t)((value >> 1u) ^ 0xa001u)
                        : (uint16_t)(value >> 1u);
        }
    }
    *crc = value;
    return true;
}

static bool crc_matches(const uint8_t *frame, size_t length)
{
    if (frame == NULL || length < 2u)
    {
        return false;
    }

    uint16_t calculated = 0u;
    if (!battery_pace_modbus_crc16(frame, length - 2u, &calculated))
    {
        return false;
    }

    const uint16_t received =
        (uint16_t)frame[length - 2u] | ((uint16_t)frame[length - 1u] << 8u);
    return calculated == received;
}

static void append_crc(uint8_t *frame, size_t payload_length)
{
    uint16_t crc = 0u;
    if (battery_pace_modbus_crc16(frame, payload_length, &crc))
    {
        frame[payload_length] = (uint8_t)(crc & 0xffu);
        frame[payload_length + 1u] = (uint8_t)((crc >> 8u) & 0xffu);
    }
}

bool battery_pace_modbus_build_read_request(uint8_t slave_address,
                                            uint16_t starting_register,
                                            uint16_t register_count,
                                            uint8_t *frame,
                                            size_t frame_capacity,
                                            size_t *frame_length)
{
    if (frame == NULL || frame_length == NULL ||
        frame_capacity < BATTERY_PACE_MODBUS_REQUEST_LENGTH)
    {
        return false;
    }
    if (slave_address == 0u || slave_address > 247u ||
        register_count == 0u || register_count > 125u)
    {
        return false;
    }

    frame[0] = slave_address;
    frame[1] = 0x03u;
    frame[2] = (uint8_t)((starting_register >> 8u) & 0xffu);
    frame[3] = (uint8_t)(starting_register & 0xffu);
    frame[4] = (uint8_t)((register_count >> 8u) & 0xffu);
    frame[5] = (uint8_t)(register_count & 0xffu);
    append_crc(frame, 6u);
    *frame_length = BATTERY_PACE_MODBUS_REQUEST_LENGTH;
    return true;
}

bool battery_pace_modbus_parse_read_response(
    const uint8_t *frame,
    size_t frame_length,
    uint8_t slave_address,
    uint8_t pack_id,
    battery_data_t *data)
{
    if (frame == NULL || data == NULL ||
        slave_address == 0u || pack_id < BATTERY_MIN_PACK_ID ||
        pack_id > BATTERY_MAX_PACK_ID)
    {
        return false;
    }
    if (frame_length < 5u || frame[0] != slave_address ||
        frame[1] != 0x03u)
    {
        return false;
    }
    if (!crc_matches(frame, frame_length))
    {
        return false;
    }

    const uint8_t byte_count = frame[2];
    if (byte_count == 0u || (byte_count & 1u) != 0u ||
        frame_length != (size_t)byte_count + 5u)
    {
        return false;
    }

    const size_t register_count = (size_t)byte_count / 2u;
    if (register_count > BATTERY_PACE_MODBUS_REGISTER_COUNT)
    {
        return false;
    }

    memset(data, 0, sizeof(*data));
    data->pack_id = pack_id;
    data->source = BATTERY_SOURCE_SERIAL;
    data->valid_fields = BATTERY_FIELD_PACK_ID;

    uint16_t registers[BATTERY_PACE_MODBUS_REGISTER_COUNT];
    memset(registers, 0, sizeof(registers));
    for (size_t i = 0; i < register_count; ++i)
    {
        const size_t offset = 3u + i * 2u;
        registers[i] =
            (uint16_t)((uint16_t)frame[offset] << 8u) |
            (uint16_t)frame[offset + 1u];
    }

    if (register_count > 1u)
    {
        const int16_t raw_current = (int16_t)registers[0];
        data->current_mA = (int32_t)raw_current * 10;
        data->voltage_mV = (uint32_t)registers[1] * 10u;
        data->valid_fields |=
            BATTERY_FIELD_CURRENT | BATTERY_FIELD_VOLTAGE;
    }
    if (register_count > 2u)
    {
        data->soc_pct = (uint8_t)registers[2];
        data->valid_fields |= BATTERY_FIELD_SOC;
    }
    if (register_count > 4u)
    {
        data->remaining_mAh = (uint32_t)registers[4] * 10u;
        data->valid_fields |= BATTERY_FIELD_REMAINING_CAPACITY;
    }
    if (register_count > 6u)
    {
        data->nominal_mAh = (uint32_t)registers[6] * 10u;
        data->valid_fields |= BATTERY_FIELD_NOMINAL_CAPACITY;
    }
    if (register_count > 7u)
    {
        data->cycles = registers[7];
        data->valid_fields |= BATTERY_FIELD_CYCLES;
    }

    if (register_count > 30u)
    {
        data->cell_count = 16u;
        for (uint8_t i = 0; i < data->cell_count; ++i)
        {
            data->cell_mV[i] = registers[15u + i];
        }
        data->valid_fields |= BATTERY_FIELD_CELLS;
    }

    if (register_count > 34u)
    {
        data->temp_count = 4u;
        for (uint8_t i = 0; i < data->temp_count; ++i)
        {
            data->temp_dC[i] = (int16_t)registers[31u + i];
        }
        data->valid_fields |= BATTERY_FIELD_TEMPERATURES;
    }

    if (register_count > 11u)
    {
        data->alarms = 0u;
        data->valid_fields |= BATTERY_FIELD_ALARMS;
    }

    return battery_data_is_valid(data);
}

bool battery_pace_modbus_build_read_response(
    uint8_t slave_address,
    const uint16_t *registers,
    size_t register_count,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length)
{
    if (slave_address == 0u || slave_address > 247u ||
        registers == NULL || register_count == 0u ||
        register_count > BATTERY_PACE_MODBUS_REGISTER_COUNT ||
        frame == NULL || frame_length == NULL)
    {
        return false;
    }

    const size_t payload_length = register_count * 2u;
    if (frame_capacity < payload_length + 5u)
    {
        return false;
    }

    frame[0] = slave_address;
    frame[1] = 0x03u;
    frame[2] = (uint8_t)payload_length;
    for (size_t i = 0u; i < register_count; ++i)
    {
        frame[3u + i * 2u] = (uint8_t)(registers[i] >> 8u);
        frame[4u + i * 2u] = (uint8_t)(registers[i] & 0xffu);
    }
    append_crc(frame, payload_length + 3u);
    *frame_length = payload_length + 5u;
    return true;
}

bool battery_pace_modbus_handle_read_request(
    const uint8_t *request,
    size_t request_length,
    uint8_t slave_address,
    const uint16_t *registers,
    size_t register_count,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length)
{
    if (request == NULL || request_length != BATTERY_PACE_MODBUS_REQUEST_LENGTH ||
        registers == NULL || response == NULL ||
        response_length == NULL || register_count > 65536u)
    {
        return false;
    }
    if (!crc_matches(request, request_length) ||
        request[0] != slave_address || request[1] != 0x03u)
    {
        return false;
    }

    const uint16_t start =
        (uint16_t)((uint16_t)request[2] << 8u) |
        (uint16_t)request[3];
    const uint16_t count =
        (uint16_t)((uint16_t)request[4] << 8u) |
        (uint16_t)request[5];
    if (count == 0u ||
        (size_t)start + (size_t)count > register_count ||
        count > BATTERY_PACE_MODBUS_REGISTER_COUNT)
    {
        return false;
    }

    return battery_pace_modbus_build_read_response(
        slave_address,
        registers + start,
        count,
        response,
        response_capacity,
        response_length);
}
