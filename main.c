#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "nrf_drv_clock.h"
#include "nrfx_systick.h"
#include "app_timer.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_log_backend_usb.h"

#include "app_usbd.h"
#include "app_usbd_serial_num.h"

#include "lib/my_board.h"
#include "lib/soft_pwm.h"
#include "lib/button.h"

APP_TIMER_DEF(button_timer);

enum { pwm_channels_amount = 4 };
enum { pwm_period_us = 1000 }; /* 1 kHz */

enum { board_button_idx = 0 };
enum { btn_double_click_pause = 200 };

enum { duty_cycle_update_period_us = 5000 };

static volatile bool do_blinky = true;

static const uint8_t device_id[] = {6, 5, 8, 2};

static struct soft_pwm pwm;
static struct soft_pwm_channel pwm_channels[pwm_channels_amount];
static uint16_t pwm_channels_id[] = {0, 1, 2, 3}; /* LED number on the board */

static volatile struct button main_button;

static bool main_button_read(void)
{
    return my_btn_is_pressed(board_button_idx);
}

static void main_button_click_callback(uint8_t clicks)
{
    if (clicks == 2)
    {
        do_blinky = !do_blinky;
        NRF_LOG_INFO("Double click -> %s smooth blinking",
                     do_blinky ? "Start" : "Freeze");
    }
    else
    {
        NRF_LOG_INFO("The reaction to this number "
                     "of clicks (%d) is not defined.", clicks);
    }
}

static void button_timer_handler(void *ctx)
{
    button_check((struct button *) &main_button);
}

struct blinky_data {
    int led_idx;
    int counter;
    nrfx_systick_state_t timestamp;
};

static struct blinky_data blinky = {my_led_first, 0};

static int blinky_counter_to_duty_cycle(int blinky_counter)
{
    return soft_pwm_max_pct -
           abs((blinky_counter % (2*soft_pwm_max_pct))-soft_pwm_max_pct);
}

static const char *led_color[] = {
    "\e[33myellow\e[0m",
    "\e[31mred\e[0m",
    "\e[32mgreen\e[0m",
    "\e[34mblue\e[0m"
};

static void blinker(struct blinky_data *data)
{
    int old_led_idx = data->led_idx;
    const int counter_top = soft_pwm_max_pct * 2 * device_id[data->led_idx];

    if (!nrfx_systick_test(&data->timestamp,
                           duty_cycle_update_period_us))
    {
        return;
    }

    nrfx_systick_get(&data->timestamp);

    if (data->counter >= counter_top)
    {
        data->counter = 0;

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
        data->counter++;
    }

    soft_pwm_set_duty_cycle_pct(&pwm,
                                data->led_idx,
                                blinky_counter_to_duty_cycle(data->counter));

    if (data->led_idx != old_led_idx)
    {
        NRF_LOG_INFO("LED #%d (%s) is blinking",
                     data->led_idx,
                     led_color[data->led_idx]);
    }
}

void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

int main(void)
{
    enum soft_pwm_init_result pwm_res;
    enum button_init_result btn_res;

    my_board_init();

    nrfx_systick_init();

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    logs_init();
    NRF_LOG_INFO("Starting up the test project with USB logging");

    pwm_res = soft_pwm_init(&pwm,
                            pwm_channels,
                            pwm_channels_id,
                            pwm_channels_amount,
                            pwm_period_us);

    if (pwm_res != soft_pwm_init_success)
    {
        NRF_LOG_INFO("Unable to init Soft PWM!");
        while (true)
        {

        }
    }

    btn_res = button_init((struct button *) &main_button,
                          btn_double_click_pause,
                          main_button_read,
                          main_button_click_callback);

    if (btn_res != button_init_success)
    {
        NRF_LOG_INFO("Unable to init button!");
        while (true)
        {

        }
    }

    app_timer_init();
    app_timer_create(&button_timer,
                     APP_TIMER_MODE_REPEATED,
                     button_timer_handler);
    app_timer_start(button_timer, APP_TIMER_TICKS(1), NULL);

    while (true)
    {
        soft_pwm_routine(&pwm);

        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        if (do_blinky)
        {
            blinker(&blinky);
        }
    }
}
