#ifndef TEMPERATURE_SOURCE_H
#define TEMPERATURE_SOURCE_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    bool (*read_celsius)(int16_t *temperature_c);
} temperature_source_t;

#ifdef __cplusplus
}
#endif

#endif
