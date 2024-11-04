#include <stddef.h>

#include "button.h"

enum button_init_result
button_init(struct button *btn,
            uint16_t double_click_pause,
            bool (*is_pressed)(void),
            void (*callback)(uint8_t))
{
    if (btn == NULL ||
        is_pressed == NULL ||
        callback == NULL ||
        double_click_pause == 0)
    {
        return button_init_fail;
    }

    btn->shift_reg = 0;
    btn->pressed_flag = false;
    btn->prev_click_timer = 0;
    btn->click_counter = 0;
    btn->double_click_pause = double_click_pause;

    btn->is_pressed = is_pressed;
    btn->callback = callback;

    return button_init_success;
}

static bool button_is_rising(struct button const *b)
{
    return !b->pressed_flag &&
           b->shift_reg == b_shift_reg_top;
}

static bool button_is_falling(struct button const *b)
{
    return b->pressed_flag &&
           b->shift_reg == b_shift_reg_down;
}

static bool button_is_released(struct button const *b)
{
    return b->prev_click_timer == 0 &&
           !b->pressed_flag &&
           b->click_counter != 0;
}

void button_check(struct button *b)
{
    b->shift_reg = (b->shift_reg << 1) | b->is_pressed();

    if (b->prev_click_timer != 0)
    {
        b->prev_click_timer--;
    }

    if (button_is_rising(b))
    {
        b->pressed_flag = true;
        b->click_counter++;
    }

    if (button_is_falling(b))
    {
        b->pressed_flag = false;
        b->prev_click_timer = b->double_click_pause;
    }

    if (button_is_released(b))
    {
        b->callback(b->click_counter);
        b->click_counter = 0;
    }
}
