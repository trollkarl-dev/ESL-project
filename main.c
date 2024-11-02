#include <stdbool.h>
#include <stdint.h>

#include "nrf_drv_clock.h"
#include "nrfx_systick.h"
#include "app_timer.h"

#include "lib/my_board.h"
#include "lib/button.h"
#include "lib/soft_pwm.h"

/* static const uint8_t device_id[] = {6, 5, 8, 2}; */

APP_TIMER_DEF(button_timer);

static volatile struct button main_button;

enum { soft_pwm_channels_amount = 4 };

static struct soft_pwm pwm;
static struct soft_pwm_channel pwm_channels[soft_pwm_channels_amount] = {
    {0, 0},
    {1, 0},
    {2, 0},
    {3, 0}
};

static void pwm_channel_on(uint16_t id)
{
    my_led_on(id);
}

static void pwm_channel_off(uint16_t id)
{
    my_led_off(id);
}

static bool main_button_is_pressed(void)
{
    return my_btn_is_pressed(0);
}

static void main_button_callback(uint8_t clicks)
{
    clicks--;

    if (clicks >= my_led_first && clicks <= my_led_last)
    {
        uint8_t value = soft_pwm_get_channel(&pwm, clicks);
        soft_pwm_set_channel(&pwm, clicks, value + 10);
    }
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

    pwm.channels = pwm_channels;
    pwm.amount = soft_pwm_channels_amount;
    pwm.channel_on = pwm_channel_on;
    pwm.channel_off = pwm_channel_off;

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    nrfx_systick_init();

    app_timer_init();
    app_timer_create(&button_timer, APP_TIMER_MODE_REPEATED, button_timer_handler);
    app_timer_start(button_timer, APP_TIMER_TICKS(1), NULL);

    while (true)
    {
        soft_pwm_routine(&pwm);
    }
}
