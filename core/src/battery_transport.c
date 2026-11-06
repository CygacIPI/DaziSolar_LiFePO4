#include "battery_transport.h"

#include <stdio.h>
#include <string.h>

bool battery_transport_status_init(battery_transport_status_t *status,
                                  battery_transport_source_t source)
{
    if (status == NULL)
    {
        return false;
    }

    memset(status, 0, sizeof(*status));
    status->source = source;
    status->read_only = true;
    status->demo_mode = (source == BATTERY_TRANSPORT_SOURCE_SIMULATOR);
    status->protocol_status =
        (source == BATTERY_TRANSPORT_SOURCE_SIMULATOR)
            ? BATTERY_PROTOCOL_STATUS_DOCUMENTED
            : BATTERY_PROTOCOL_STATUS_PENDING_CONFIRMATION;
    status->pack_id = 0;
    status->connected = false;
    status->last_update_ms = 0;
    status->last_error[0] = '\0';
    return true;
}

bool battery_transport_configure(battery_transport_status_t *status,
                                 const battery_transport_config_t *config)
{
    if (status == NULL || config == NULL)
    {
        return false;
    }

    memset(status, 0, sizeof(*status));
    status->source = config->source;
    status->pack_id = config->target_pack_id;
    status->connected = false;
    status->read_only = config->allow_read_only;
    status->demo_mode = (config->source == BATTERY_TRANSPORT_SOURCE_SIMULATOR);
    status->protocol_status =
        (config->source == BATTERY_TRANSPORT_SOURCE_SIMULATOR)
            ? BATTERY_PROTOCOL_STATUS_DOCUMENTED
            : (config->require_documented_protocol
                   ? BATTERY_PROTOCOL_STATUS_PENDING_CONFIRMATION
                   : BATTERY_PROTOCOL_STATUS_UNKNOWN);
    if (config->source == BATTERY_TRANSPORT_SOURCE_SIMULATOR)
    {
        status->read_only = true;
    }
    status->last_error[0] = '\0';
    return true;
}

bool battery_transport_set_error(battery_transport_status_t *status,
                                const char *message)
{
    if (status == NULL)
    {
        return false;
    }

    if (message == NULL || *message == '\0')
    {
        status->last_error[0] = '\0';
        return true;
    }

    snprintf(status->last_error, sizeof(status->last_error), "%s", message);
    return true;
}

bool battery_transport_mark_connected(battery_transport_status_t *status,
                                     bool connected)
{
    if (status == NULL)
    {
        return false;
    }

    status->connected = connected;
    if (!connected)
    {
        status->last_update_ms = 0;
    }
    return true;
}

bool battery_transport_set_demo_mode(battery_transport_status_t *status,
                                     bool demo_mode)
{
    if (status == NULL)
    {
        return false;
    }

    status->demo_mode = demo_mode;
    if (demo_mode)
    {
        status->source = BATTERY_TRANSPORT_SOURCE_SIMULATOR;
        status->protocol_status = BATTERY_PROTOCOL_STATUS_DOCUMENTED;
    }
    return true;
}

bool battery_transport_source_supported(battery_transport_source_t source)
{
    switch (source)
    {
        case BATTERY_TRANSPORT_SOURCE_NONE:
        case BATTERY_TRANSPORT_SOURCE_SIMULATOR:
        case BATTERY_TRANSPORT_SOURCE_WIFI:
        case BATTERY_TRANSPORT_SOURCE_CAN:
        case BATTERY_TRANSPORT_SOURCE_RS485:
        case BATTERY_TRANSPORT_SOURCE_RS232:
            return true;
        default:
            return false;
    }
}
