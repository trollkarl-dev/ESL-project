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
#include "lib/colorspaces.h"
#include "lib/button.h"

APP_TIMER_DEF(dispmode_timer);
APP_TIMER_DEF(debounce_timer);
APP_TIMER_DEF(btnhold_timer);
APP_TIMER_DEF(btnclk_timer);

enum { brd_btn_idx = 0 };
enum { btn_dblclk_pause = 200 };

enum { btnhold_period_first_ms = 500 };
enum { btnhold_period_next_ms = 50 };

enum { debounce_period_ms = 50 };

enum { dispmode_slow_ms = 10 };
enum { dispmode_fast_ms = 2 };

enum {
    pwm_channel_indicator = my_led_first,
    pwm_channel_red = my_led_first + 1,
    pwm_channel_green = my_led_first + 2,
    pwm_channel_blue = my_led_first + 3
};

static const uint8_t device_id[] =  {6, 5, 8, 2};

static const uint16_t
default_hue = ((device_id[2]*10 + device_id[3]) * max_hue) / max_pct;

typedef enum {
    cp_state_noinpt = 0,
    cp_state_huemod,
    cp_state_satmod,
    cp_state_valmod
} colorpicker_state_t;

typedef struct {
    colorpicker_state_t state;
    int16_t sat_cnt;
    int16_t val_cnt;
    int16_t dispmode_cnt;
    hsv_t color;
} colorpicker_t;

static void colorpicker_init(colorpicker_t *c, hsv_t initial_color)
{
    c->state = cp_state_noinpt;
    c->sat_cnt = 0;
    c->val_cnt = 0;
    c->dispmode_cnt = 0;
    c->color = initial_color;
}

static volatile colorpicker_t cpicker;

static volatile button_t button;

static bool button_is_pressed(void)
{
    return my_btn_is_pressed(brd_btn_idx);
}

static const char *cpicker_modenames[] =
{
    "No Input",
    "Hue Modification",
    "Saturation Modification",
    "Brightness Modification"
};

static void button_onclick(uint8_t clicks)
{
    NRF_LOG_INFO("Clicks - %d", clicks);

    if (clicks != 2)
        return;

    if (cpicker.state >= cp_state_valmod)
        cpicker.state = cp_state_noinpt;
    else
        cpicker.state++;

    NRF_LOG_INFO("%s mode is in progress",
                 cpicker_modenames[cpicker.state]);

    cpicker.dispmode_cnt = 0;
    app_timer_start(dispmode_timer,
                    APP_TIMER_TICKS(dispmode_fast_ms),
                    (void *) &cpicker);
}

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
            my_led_mappings[pwm_channel_indicator] | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[pwm_channel_red]       | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[pwm_channel_green]     | NRFX_PWM_PIN_INVERTED,
            my_led_mappings[pwm_channel_blue]      | NRFX_PWM_PIN_INVERTED,
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
        case pwm_channel_indicator:
            pwm->seq_values->channel_0 = duty_cycle; break;
        case pwm_channel_red:
            pwm->seq_values->channel_1 = duty_cycle; break;
        case pwm_channel_green:
            pwm->seq_values->channel_2 = duty_cycle; break;
        case pwm_channel_blue:
            pwm->seq_values->channel_3 = duty_cycle; break;
    }
}

static void colorpicker_show_color_hsv(colorpicker_t *c)
{
    rgb_t rgb = hsv2rgb(c->color);

    pwm_set_duty_cycle(&my_pwm, pwm_channel_red, rgb.r);
    pwm_set_duty_cycle(&my_pwm, pwm_channel_green, rgb.g);
    pwm_set_duty_cycle(&my_pwm, pwm_channel_blue, rgb.b);

    NRF_LOG_INFO("HSV: (%d, %d, %d), RGB (%%): (%d, %d, %d)",
                 c->color.h,
                 c->color.s,
                 c->color.v,
                 rgb.r, rgb.g, rgb.b);
}

static void button_onhold(void)
{
    switch (cpicker.state)
    {
        case cp_state_huemod:
            cpicker.color.h = (cpicker.color.h + 1) % max_hue;
            break;
        case cp_state_satmod:
            cpicker.sat_cnt = ((cpicker.sat_cnt+1) % (2*max_saturation));
            cpicker.color.s = abs(cpicker.sat_cnt - max_saturation);
            break;
        case cp_state_valmod:
            cpicker.val_cnt = ((cpicker.val_cnt+1) % (2*max_brightness));
            cpicker.color.v = abs(cpicker.val_cnt - max_brightness);
            break;
	case cp_state_noinpt:
	    break;
    }

    if (cpicker.state != cp_state_noinpt)
        colorpicker_show_color_hsv((colorpicker_t *) &cpicker);
}

static void btnhold_timer_handler(void *ctx)
{
    button_hold_check((button_t *) &button);
}

static void btnclk_timer_handler(void *ctx)
{
    button_click_check((button_t *) &button);
}

static void dispmode_timer_handler(void *ctx)
{
    uint32_t period = APP_TIMER_TICKS(dispmode_slow_ms);
    uint8_t duty_cycle = 0;

    switch (cpicker.state)
    {
        case cp_state_noinpt:
            duty_cycle = 0;
            break;
        case cp_state_valmod:
            duty_cycle = max_pct;
            break;
        case cp_state_satmod:
            period = APP_TIMER_TICKS(dispmode_fast_ms);
            cpicker.dispmode_cnt = (cpicker.dispmode_cnt + 1) % (2*max_pct);
            duty_cycle = max_pct - abs(cpicker.dispmode_cnt - max_pct);
            app_timer_start(dispmode_timer, period, NULL);
	    break;
        case cp_state_huemod:
            period = APP_TIMER_TICKS(dispmode_slow_ms);
            cpicker.dispmode_cnt = (cpicker.dispmode_cnt + 1) % (2*max_pct);
            duty_cycle = max_pct - abs(cpicker.dispmode_cnt - max_pct);
            app_timer_start(dispmode_timer, period, NULL);
    }

    pwm_set_duty_cycle(&my_pwm, pwm_channel_indicator, duty_cycle);
}

static void debounce_timer_handler(void *ctx)
{
    button_debounce_check((button_t *) &button);
}

static void gpiote_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    button.callbacks.schedule_debounce_check(button.timings.debounce_period_ms);
}

static void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static void gpiote_init_on_toggle(nrfx_gpiote_pin_t pin,
                                  nrf_gpio_pin_pull_t pull,
                                  nrfx_gpiote_evt_handler_t handler)
{
    nrfx_gpiote_in_config_t
    gpiote_config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);
    gpiote_config.pull = pull;

    nrfx_gpiote_in_init(pin, &gpiote_config, handler);
    nrfx_gpiote_in_event_enable(pin, true);
}

static void create_timers(void)
{
    app_timer_create(&debounce_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     debounce_timer_handler);

    app_timer_create(&btnclk_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     btnclk_timer_handler);

    app_timer_create(&btnhold_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     btnhold_timer_handler);

    app_timer_create(&dispmode_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     dispmode_timer_handler);
}

static void button_schedule_click_check(uint32_t delay_ms)
{
    app_timer_start(btnclk_timer,
                    APP_TIMER_TICKS(delay_ms),
                    NULL);
}

static void button_schedule_hold_check(uint32_t delay_ms)
{
    app_timer_start(btnhold_timer,
                    APP_TIMER_TICKS(delay_ms),
                    NULL);
}

static void button_schedule_debounce_check(uint32_t delay_ms)
{
    app_timer_start(debounce_timer,
                    APP_TIMER_TICKS(delay_ms),
                    NULL);
}

static button_timings_t const button_timings = {
    .dblclk_pause_ms = btn_dblclk_pause, 
    .hold_pause_short_ms = btnhold_period_next_ms,
    .hold_pause_long_ms = btnhold_period_first_ms,
    .debounce_period_ms = debounce_period_ms
};

static button_callbacks_t const button_callbacks = {
    .onclick = button_onclick,
    .onhold = button_onhold,
    .is_pressed = button_is_pressed,
    .schedule_click_check = button_schedule_click_check,
    .schedule_hold_check = button_schedule_hold_check,
    .schedule_debounce_check = button_schedule_debounce_check
};

int main(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    pwm_init(&my_pwm);
    pwm_run(&my_pwm);

    logs_init();
    NRF_LOG_INFO("Starting up the HSV color-picker device");

    app_timer_init();
    create_timers();

    colorpicker_init((colorpicker_t *) &cpicker,
                     (hsv_t) {default_hue, max_saturation, max_brightness});
    colorpicker_show_color_hsv((colorpicker_t *) &cpicker);

    button_init((button_t *) &button,
                &button_timings,
		&button_callbacks);

    nrfx_gpiote_init();
    gpiote_init_on_toggle(my_btn_mappings[brd_btn_idx],
                          NRF_GPIO_PIN_PULLUP,
                          gpiote_handler);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        __WFI();
    }
}
