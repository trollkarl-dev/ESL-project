#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "nrf_drv_clock.h"
#include "app_timer.h"
#include "nrfx_pwm.h"
#include "nrfx_gpiote.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_log_backend_usb.h"

#include "app_usbd.h"
#include "app_usbd_serial_num.h"

#include "lib/my_board.h"
#include "lib/button.h"


/* APP_TIMER_DEF(button_timer); */
APP_TIMER_DEF(blinky_timer);

enum { board_button_idx = 0 };
/*enum { btn_double_click_pause = 200 }; */

enum { max_pct = 100 };
enum { duty_cycle_update_period_ms = 5 };

static volatile bool do_blinky = true;

static const uint8_t device_id[] = {6, 5, 8, 2};

static nrfx_pwm_t my_pwm_instance = NRFX_PWM_INSTANCE(0);
static nrf_pwm_values_individual_t my_pwm_seq_values;
static nrf_pwm_sequence_t const my_pwm_seq =
{
    .values.p_individual = &my_pwm_seq_values,
    .length              = NRF_PWM_VALUES_LENGTH(my_pwm_seq_values),
    .repeats             = 0,
    .end_delay           = 0
};

typedef struct {
    nrfx_pwm_t *pwm;
    nrf_pwm_values_individual_t *seq_values;
    nrf_pwm_sequence_t const * seq;
} pwm_wrapper_t;

static pwm_wrapper_t my_pwm =
{
    .pwm = &my_pwm_instance,
    .seq_values = &my_pwm_seq_values,
    .seq = &my_pwm_seq
};

static bool pwm_init(pwm_wrapper_t *pwm)
{
    static nrfx_pwm_config_t const my_pwm_config =
    {
        .output_pins =
        {
            my_led_mappings[0] | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[1] | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[2] | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[3] | NRFX_PWM_PIN_INVERTED,
        },
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = max_pct,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO,
    };

    return (nrfx_pwm_init(pwm->pwm,
                          &my_pwm_config,
                          NULL) == NRF_SUCCESS);
}

static void pwm_run(pwm_wrapper_t *pwm)
{
    nrfx_pwm_simple_playback(pwm->pwm, pwm->seq, 1, NRFX_PWM_FLAG_LOOP);
}

static void pwm_set_duty_cycle(pwm_wrapper_t *pwm,
                               uint8_t channel,
                               uint32_t duty_cycle)
{
    duty_cycle %= 1 + max_pct;

    switch (channel)
    {
        case 0: pwm->seq_values->channel_0 = duty_cycle; break;
        case 1: pwm->seq_values->channel_1 = duty_cycle; break;
        case 2: pwm->seq_values->channel_2 = duty_cycle; break;
        case 3: pwm->seq_values->channel_3 = duty_cycle; break;
    }
}

/*
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
*/

struct blinky_data {
    int led_idx;
    int counter;
};

static struct blinky_data blinky = {my_led_first, 0};

static int blinky_counter_to_duty_cycle(int blinky_counter)
{
    return max_pct -
           abs((blinky_counter % (2*max_pct))-max_pct);
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
    const int counter_top = max_pct * 2 * device_id[data->led_idx];

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

    pwm_set_duty_cycle(&my_pwm,
                       data->led_idx,
                       blinky_counter_to_duty_cycle(data->counter));

    if (data->led_idx != old_led_idx)
    {
        NRF_LOG_INFO("LED #%d (%s) is blinking",
                     data->led_idx,
                     led_color[data->led_idx]);
    }
}

static void blinky_timer_handler(void *ctx)
{
    if (do_blinky)
    {
        blinker(&blinky);
    }
}

void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static nrfx_gpiote_in_config_t
gpiote_config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

void my_btn_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    do_blinky = !do_blinky;
}

int main(void)
{
/*    enum button_init_result btn_res; */

/*    my_btn_init(board_button_idx); */

    nrfx_gpiote_init();

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    pwm_init(&my_pwm);
    pwm_run(&my_pwm);

    gpiote_config.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(my_btn_mappings[board_button_idx], &gpiote_config, my_btn_handler);
    nrfx_gpiote_in_event_enable(my_btn_mappings[board_button_idx], true);

    logs_init();
    NRF_LOG_INFO("Starting up the test project with USB logging");

/*
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
*/
    app_timer_init();
/*
    app_timer_create(&button_timer,
                     APP_TIMER_MODE_REPEATED,
                     button_timer_handler);
    app_timer_start(button_timer, APP_TIMER_TICKS(1), NULL);
*/
    app_timer_create(&blinky_timer,
                     APP_TIMER_MODE_REPEATED,
                     blinky_timer_handler);
    app_timer_start(blinky_timer, APP_TIMER_TICKS(duty_cycle_update_period_ms), NULL);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        __WFI();
    }
}
