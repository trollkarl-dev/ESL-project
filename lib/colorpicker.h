#ifndef COLORPICKER_H
#define COLORPICKER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

#include "app_util.h"

#include "colorspaces.h"

typedef enum {
    cp_state_noinpt = 0,
    cp_state_huemod,
    cp_state_satmod,
    cp_state_valmod
} colorpicker_state_t;

typedef struct {
    colorpicker_state_t state;
    int16_t sat_increment;
    int16_t val_increment;
    int16_t dispmode_cnt;
    int16_t max_dispmode_duty_cycle;
    hsv_t color;
} colorpicker_t;

typedef struct {
    colorpicker_state_t state;
    uint32_t period_ms;
    uint8_t duty_cycle;
} colorpicker_dispmode_data_t;

typedef struct {
    hsv_t color;
    int16_t sat_increment;
    int16_t val_increment;
} colorpicker_save_t;

STATIC_ASSERT(sizeof(colorpicker_save_t) == 8, "Bad size!");

void colorpicker_init(colorpicker_t *c,
                      hsv_t initial_color,
                      int16_t max_dispmode_duty_cycle);

void colorpicker_next_state(colorpicker_t *c);
colorpicker_state_t colorpicker_get_state(colorpicker_t *c);

hsv_t colorpicker_get_color(colorpicker_t *c);
void colorpicker_next_color(colorpicker_t *c);

void colorpicker_update_dispmode_data(colorpicker_t *c,
                                      colorpicker_dispmode_data_t *data);

void colorpicker_dump(colorpicker_t const *c, colorpicker_save_t *s);
void colorpicker_load(colorpicker_t *c, colorpicker_save_t const *s);

#ifdef __cplusplus
}
#endif

#endif /* COLORPICKER_H */

