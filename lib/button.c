#include <stddef.h>

#include "button.h"

void button_init(struct button *b)
{
    b->shift_reg = 0;
    b->pressed_flag = 0;
}

void button_check(struct button *b)
{
    b->shift_reg = (b->shift_reg << 1) | b->is_pressed();
    b->prev_timer = (b->prev_timer != 0) ? b->prev_timer - 1
                                         : 0;

    if (!b->pressed_flag && b->shift_reg == b_shift_reg_top)
    {
        b->pressed_flag = true;
        b->click_counter++;
    }

    if (b->pressed_flag && b->shift_reg == b_shift_reg_down)
    {
        b->pressed_flag = false;
        b->prev_timer = b_prev_timer_top;
    }

    if (b->prev_timer == 0 && !b->pressed_flag && b->click_counter != 0)
    {
        b->callback(b->click_counter);
        b->click_counter = 0;
    }
}
