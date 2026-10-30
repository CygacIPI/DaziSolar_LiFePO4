#ifndef LFEPO4_BATTERY_CSV_H
#define LFEPO4_BATTERY_CSV_H

#include "battery.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct
{
    int64_t timestamp_unix_s;
    battery_data_t data;
} battery_csv_sample_t;

bool battery_csv_write(FILE *file,
                       const battery_csv_sample_t *samples,
                       size_t sample_count);

#endif
