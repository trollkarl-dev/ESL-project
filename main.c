#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "nrf_drv_clock.h"
#include "nrfx_systick.h"

#include "lib/my_board.h"
#include "lib/soft_pwm.h"

static const uint8_t device_id[] = {6, 5, 8, 2};

enum { duty_cycle_update_period = 5000 };

enum { soft_pwm_channels_amount = 4 };
enum { soft_pwm_period_us = 1000 }; /* 1 kHz */

enum { button_idx = 0 };

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
    enum soft_pwm_init_result res;
    nrfx_systick_state_t blinky_timestamp;

    my_board_init();

    res = soft_pwm_init(&pwm,
                        pwm_channels,
                        soft_pwm_channels_amount,
                        soft_pwm_period_us,
                        pwm_channel_on,
                        pwm_channel_off);

    if (res != soft_pwm_init_success)
    {
        while (true)
        {

        }
    }

    nrfx_systick_init();

    while (true)
    {
        soft_pwm_routine(&pwm);

        if (!nrfx_systick_test(&blinky_timestamp, duty_cycle_update_period))
        {
            continue;
        }

        nrfx_systick_get(&blinky_timestamp);

        if (my_btn_is_pressed(button_idx))
        {
            blinker(&blinky);
        }
    }
}
