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

    if (!b->pressed_flag)
    {
        if (b->shift_reg == b_shift_reg_top)
        {
            b->pressed_flag = true;

            if (b->pressed_callback != NULL)
                b->pressed_callback();
        }
    }
    else {
        if (b->shift_reg == b_shift_reg_down)
        {
            b->pressed_flag = false;

            if (b->released_callback != NULL)
                b->released_callback();
        }
    }
}
