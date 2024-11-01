#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_clock.h"

#include "app_timer.h"

#include "lib/my_board.h"
#include "lib/button.h"

/* static const uint8_t device_id[] = {6, 5, 8, 2}; */

APP_TIMER_DEF(button_timer);

static volatile struct button main_button;

static bool main_button_is_pressed(void)
{
    return my_btn_is_pressed(0);
}

static void main_button_callback(uint8_t clicks)
{
    my_led_invert(--clicks);
}

static void button_timer_handler(void *ctx)
{
    button_check((struct button *) &main_button);
}

int main(void)
{
    int led_idx;

    for (led_idx = my_led_first; led_idx <= my_led_last; led_idx++)
    {
        my_led_init(led_idx);
        my_led_off(led_idx);
    }

    my_btn_init(0);

    main_button.is_pressed = main_button_is_pressed;
    main_button.callback = main_button_callback;

    button_init((struct button *) &main_button);

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&button_timer, APP_TIMER_MODE_REPEATED, button_timer_handler);
    app_timer_start(button_timer, APP_TIMER_TICKS(1), NULL);

    while (true)
    {
    }
}
