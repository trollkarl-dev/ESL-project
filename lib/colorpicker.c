#include <stdlib.h>

#include "colorpicker.h"

void colorpicker_init(colorpicker_t *c,
                      hsv_t initial_color,
                      int16_t max_dispmode_duty_cycle)
{
    c->state = cp_state_noinpt;
    c->sat_increment = 1;
    c->val_increment = 1;
    c->dispmode_cnt = 0;
    c->max_dispmode_duty_cycle = max_dispmode_duty_cycle;
c->color = initial_color;
}

void colorpicker_next_state(colorpicker_t *c)
{
    if (c->state >= cp_state_valmod)
        c->state = cp_state_noinpt;
    else
        c->state++;

    c->dispmode_cnt = 0;
}

colorpicker_state_t colorpicker_get_state(colorpicker_t *c)
{
    return c->state;
}

hsv_t colorpicker_get_color(colorpicker_t *c)
{
    return c->color;
}

void colorpicker_set_color(colorpicker_t *c, const hsv_t *color)
{
    c->color = *color;
}

void colorpicker_next_color(colorpicker_t *c)
{
    switch (c->state)
    {
        case cp_state_huemod:
            c->color.h = (c->color.h + 1) % max_hue;
            break;
        case cp_state_satmod:
            if ((c->color.s == max_saturation && c->sat_increment == 1) ||
                (c->color.s == 0 && c->sat_increment == -1))
            {
                c->sat_increment = -(c->sat_increment);
            }
            c->color.s  += c->sat_increment;

            break;
        case cp_state_valmod:
            if ((c->color.v == max_brightness && c->val_increment == 1) ||
                (c->color.v == 0 && c->val_increment == -1))
            {
                c->val_increment = -(c->val_increment);
            }
            c->color.v  += c->val_increment;

            break;
        case cp_state_noinpt:
            break;
    }
}

void colorpicker_update_dispmode_data(colorpicker_t *c,
                                      colorpicker_dispmode_data_t *data)
{
    int16_t max_pct = c->max_dispmode_duty_cycle;
    data->state = c->state;

    switch (c->state)
    {
        case cp_state_noinpt:
            data->duty_cycle = 0;
            break;
        case cp_state_valmod:
            data->duty_cycle = max_pct;
            break;
        case cp_state_satmod:
        case cp_state_huemod:
            c->dispmode_cnt = (c->dispmode_cnt + 1) % (2*max_pct);
            data->duty_cycle = max_pct - abs(c->dispmode_cnt - max_pct);
    }
}

void colorpicker_dump(colorpicker_t const *c, colorpicker_save_t *s)
{
    s->color = c->color;
    s->sat_increment = c->sat_increment;
    s->val_increment = c->val_increment;
}

void colorpicker_load(colorpicker_t *c, colorpicker_save_t const *s)
{
    c->color = s->color;
    c->sat_increment = s->sat_increment;
    c->val_increment = s->val_increment;
}
