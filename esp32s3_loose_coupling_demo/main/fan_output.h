#ifndef FAN_OUTPUT_H
#define FAN_OUTPUT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    void (*set_percent)(uint8_t percent);
} fan_output_t;

#ifdef __cplusplus
}
#endif

#endif
