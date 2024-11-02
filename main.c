#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "nrf_drv_clock.h"
#include "nrfx_systick.h"
#include "app_timer.h"

#include "lib/my_board.h"
#include "lib/soft_pwm.h"
#include "lib/button.h"

APP_TIMER_DEF(button_timer);

enum { soft_pwm_channels_amount = 4 };
enum { soft_pwm_period_us = 1000 }; /* 1 kHz */

enum { duty_cycle_update_period_us = 5000 };

enum { button_idx = 0 };

static volatile bool do_blinky = false;
static volatile struct button main_button;
static const uint8_t device_id[] = {6, 5, 8, 2};

static struct soft_pwm pwm;
static struct soft_pwm_channel pwm_channels[soft_pwm_channels_amount] = {
    {0}, {1}, {2}, {3} /* ID - LED number on the board */
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
    return my_btn_is_pressed(button_idx);
}

static void main_button_callback(uint8_t clicks)
{
    if (clicks == 2)
    {
        do_blinky = !do_blinky;
    }
}

static void button_timer_handler(void *ctx)
{
    button_check((struct button *) &main_button);
}

struct blinky_data {
    int led_idx;
    int duty_cycle;
};

static int duty_cycle_transform(int duty_cycle)
{
    return soft_pwm_max_pct - abs((duty_cycle % (2*soft_pwm_max_pct))-soft_pwm_max_pct);
}

static struct blinky_data blinky = {my_led_first, 0};

static void blinker(struct blinky_data *data)
{
    if (data->duty_cycle >= soft_pwm_max_pct * 2 * device_id[data->led_idx])
    {
        data->duty_cycle = 0;

        if (data->led_idx >= my_led_last)
        {
            data->led_idx = my_led_first;
        }
        else
        {
            data->led_idx++;
        }
    }
    else
    {
        data->duty_cycle++;
    }

    soft_pwm_set_duty_cycle_pct(&pwm,
                                data->led_idx,
                                duty_cycle_transform(data->duty_cycle));
}

int main(void)
{
    enum soft_pwm_init_result pwm_res;
    enum button_init_result btn_res;

    nrfx_systick_state_t blinky_timestamp;

    my_board_init();

    pwm_res = soft_pwm_init(&pwm,
                            pwm_channels,
                            soft_pwm_channels_amount,
                            soft_pwm_period_us,
                            pwm_channel_on,
                            pwm_channel_off);

    if (pwm_res != soft_pwm_init_success)
    {
        while (true)
        {

        }
    }

    btn_res = button_init((struct button *) &main_button,
                          main_button_is_pressed,
                          main_button_callback);

    if (btn_res != button_init_success)
    {
        while (true)
        {

        }
    }

    nrfx_systick_init();

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    app_timer_init();
    app_timer_create(&button_timer, APP_TIMER_MODE_REPEATED, button_timer_handler);
    app_timer_start(button_timer, APP_TIMER_TICKS(1), NULL);

    while (true)
    {
        soft_pwm_routine(&pwm);

        if (!nrfx_systick_test(&blinky_timestamp, duty_cycle_update_period_us))
        {
            continue;
        }

        nrfx_systick_get(&blinky_timestamp);

        if (do_blinky)
        {
            blinker(&blinky);
        }
    }
}
