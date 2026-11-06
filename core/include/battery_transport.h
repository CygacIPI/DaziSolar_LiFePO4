#ifndef LIFEPO4_BATTERY_TRANSPORT_H
#define LIFEPO4_BATTERY_TRANSPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "battery.h"

typedef enum
{
    BATTERY_TRANSPORT_SOURCE_NONE = 0,
    BATTERY_TRANSPORT_SOURCE_SIMULATOR,
    BATTERY_TRANSPORT_SOURCE_WIFI,
    BATTERY_TRANSPORT_SOURCE_CAN,
    BATTERY_TRANSPORT_SOURCE_RS485,
    BATTERY_TRANSPORT_SOURCE_RS232
} battery_transport_source_t;

typedef enum
{
    BATTERY_PROTOCOL_STATUS_UNKNOWN = 0,
    BATTERY_PROTOCOL_STATUS_DOCUMENTED,
    BATTERY_PROTOCOL_STATUS_PENDING_CONFIRMATION,
    BATTERY_PROTOCOL_STATUS_BLOCKED
} battery_protocol_status_t;

typedef struct
{
    battery_transport_source_t source;
    uint8_t pack_id;
    bool connected;
    bool demo_mode;
    bool read_only;
    battery_protocol_status_t protocol_status;
    char last_error[128];
    uint32_t last_update_ms;
} battery_transport_status_t;

typedef struct
{
    battery_transport_source_t source;
    uint8_t target_pack_id;
    bool allow_read_only;
    bool require_documented_protocol;
} battery_transport_config_t;

bool battery_transport_status_init(battery_transport_status_t *status,
                                  battery_transport_source_t source);
bool battery_transport_configure(battery_transport_status_t *status,
                                 const battery_transport_config_t *config);
bool battery_transport_set_error(battery_transport_status_t *status,
                                const char *message);
bool battery_transport_mark_connected(battery_transport_status_t *status,
                                     bool connected);
bool battery_transport_set_demo_mode(battery_transport_status_t *status,
                                     bool demo_mode);
bool battery_transport_source_supported(battery_transport_source_t source);

#endif
