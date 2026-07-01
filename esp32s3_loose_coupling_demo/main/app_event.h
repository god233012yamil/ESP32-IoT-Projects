#ifndef APP_EVENT_H
#define APP_EVENT_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    APP_EVENT_NONE = 0,
    APP_EVENT_TEMPERATURE_SAMPLE,
    APP_EVENT_BUTTON_PRESSED,
    APP_EVENT_BUTTON_RELEASED,
    APP_EVENT_FAN_STATE_CHANGED
} app_event_type_t;

typedef struct
{
    app_event_type_t type;

    union
    {
        int16_t temperature_c;
        uint8_t fan_percent;
    } data;
} app_event_t;

#ifdef __cplusplus
}
#endif

#endif
