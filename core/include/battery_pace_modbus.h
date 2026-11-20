#ifndef LIFEPO4_BATTERY_PACE_MODBUS_H
#define LIFEPO4_BATTERY_PACE_MODBUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "battery.h"

#define BATTERY_PACE_MODBUS_REGISTER_COUNT 37u
#define BATTERY_PACE_MODBUS_REQUEST_LENGTH 8u
#define BATTERY_PACE_MODBUS_RESPONSE_LENGTH 79u

bool battery_pace_modbus_crc16(const uint8_t *data,
                               size_t length,
                               uint16_t *crc);

bool battery_pace_modbus_build_read_request(uint8_t slave_address,
                                            uint16_t starting_register,
                                            uint16_t register_count,
                                            uint8_t *frame,
                                            size_t frame_capacity,
                                            size_t *frame_length);

bool battery_pace_modbus_parse_read_response(
    const uint8_t *frame,
    size_t frame_length,
    uint8_t slave_address,
    uint8_t pack_id,
    battery_data_t *data);

bool battery_pace_modbus_build_read_response(
    uint8_t slave_address,
    const uint16_t *registers,
    size_t register_count,
    uint8_t *frame,
    size_t frame_capacity,
    size_t *frame_length);

bool battery_pace_modbus_handle_read_request(
    const uint8_t *request,
    size_t request_length,
    uint8_t slave_address,
    const uint16_t *registers,
    size_t register_count,
    uint8_t *response,
    size_t response_capacity,
    size_t *response_length);

#endif
