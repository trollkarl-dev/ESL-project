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

APP_TIMER_DEF(dispmode_timer);
APP_TIMER_DEF(debounce_timer);
APP_TIMER_DEF(btnhold_timer);
APP_TIMER_DEF(dblclk_timer);

enum { brd_btn_idx = 0 };
enum { btn_dblclk_pause = 200 };

enum { max_pct = 100 };

enum { btnhold_period_ms = 100 };
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

enum { max_hue = 360 };
enum { max_saturation = max_pct };
enum { max_brightness = max_pct };

static const uint16_t
default_hue = ((device_id[2]*10 + device_id[3]) * max_hue) / max_pct;

typedef struct {
    uint16_t h;
    uint8_t s;
    uint8_t v;
} hsv_t;

typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_t;

static rgb_t hsv2rgb(hsv_t hsv)
{
    const int sector = (hsv.h / 60) % 6;
    const int vmin = (((hsv.v * (100 - hsv.s)) << 8) / 100) >> 8;
    const int a = ((((hsv.v - vmin) * (hsv.h % 60)) << 8) / 60) >> 8;
    const int vinc = vmin + a;
    const int vdec = hsv.v - a;

    switch (sector)
    {
        case 0:  return (rgb_t) {hsv.v, vinc, vmin}; break;
        case 1:  return (rgb_t) {vdec, hsv.v, vmin}; break;
        case 2:  return (rgb_t) {vmin, hsv.v, vinc}; break;
        case 3:  return (rgb_t) {vmin, vdec, hsv.v}; break;
        case 4:  return (rgb_t) {vinc, vmin, hsv.v}; break;
        case 5:
        default: return (rgb_t) {hsv.v, vmin, vdec}; break;
    }
}

typedef enum {
    cp_state_noinpt = 0,
    cp_state_huemod,
    cp_state_satmod,
    cp_state_valmod
} colorpicker_state_t;

typedef struct {
    colorpicker_state_t state;
    uint8_t sat_counter;
    uint8_t val_counter;
    hsv_t color;
    int dispmode_counter;
} colorpicker_t;

static volatile colorpicker_t colorpicker =
{
    .state = cp_state_noinpt,
    .sat_counter = 0,
    .val_counter = 0,
    .color =
    {
        .h = default_hue,
        .s = max_saturation,
        .v = max_brightness
    },
    .dispmode_counter = 0
};

typedef struct {
    bool pressed_flag;
    uint8_t clicks_counter;
} button_t;

static volatile button_t button =
{
    .pressed_flag = false,
    .clicks_counter = 0
};

static bool button_read(void)
{
    return my_btn_is_pressed(brd_btn_idx);
}

static const char *colorpicker_modes[] =
{
    "No Input",
    "Hue Modification",
    "Saturation Modification",
    "Brightness Modification"
};

static void button_onclick(uint8_t clicks)
{
    NRF_LOG_INFO("Clicks - %d", button.clicks_counter);

    if (button.clicks_counter == 2)
    {
        if (colorpicker.state >= cp_state_valmod)
            colorpicker.state = cp_state_noinpt;
        else
            colorpicker.state++;

        NRF_LOG_INFO("%s mode is in progress",
                     colorpicker_modes[colorpicker.state]);
        colorpicker.dispmode_counter = 0;
    }
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

static void colorpicker_show(colorpicker_t *c)
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

static void btnhold_timer_handler(void *ctx)
{
    volatile colorpicker_t *cp = (volatile colorpicker_t *) ctx;

    switch (colorpicker.state)
    {
        case cp_state_huemod:
            cp->color.h = (cp->color.h + 1) % max_hue;
            break;
        case cp_state_satmod:
            cp->sat_counter++;
            cp->sat_counter %= 2*max_saturation;
            cp->color.s = abs(cp->sat_counter - max_saturation);
            break;
        case cp_state_valmod:
            cp->val_counter++;
            cp->val_counter %= 2*max_brightness;
            cp->color.v = abs(cp->val_counter - max_brightness);
            break;
        default:
            break;
    }

    if (cp->state != cp_state_noinpt)
    {
        colorpicker_show((colorpicker_t *) ctx);
    }

    if (button_read() && button.pressed_flag)
    {
        app_timer_start(btnhold_timer,
                        APP_TIMER_TICKS(btnhold_period_ms),
                        ctx);
    }
}

static void dblclk_timer_handler(void *ctx)
{
    /* button is released? */
    if (!button.pressed_flag &&
        button.clicks_counter != 0)
    {
        button_onclick(button.clicks_counter);
        button.clicks_counter = 0;
    }
}

static void dispmode_timer_handler(void *ctx)
{
    uint32_t period;
    uint8_t duty_cycle;
    volatile colorpicker_t *cp = (volatile colorpicker_t *) ctx;

    if (colorpicker.state == cp_state_satmod)
        period = APP_TIMER_TICKS(dispmode_fast_ms);
    else
        period = APP_TIMER_TICKS(dispmode_slow_ms);

    switch (cp->state)
    {
        case cp_state_noinpt:
            duty_cycle = 0;
            break;
        case cp_state_valmod:
            duty_cycle = max_pct;
            break;
        default:
            cp->dispmode_counter = (cp->dispmode_counter + 1) % (2*max_pct);
            duty_cycle = max_pct - abs(cp->dispmode_counter - max_pct);
    }

    pwm_set_duty_cycle(&my_pwm, pwm_channel_indicator, duty_cycle);
    app_timer_start(dispmode_timer, period, ctx);
}

static void debounce_timer_handler(void *ctx)
{
    /* rising */
    if (button_read() && !button.pressed_flag)
    {
        button.pressed_flag = true;
        button.clicks_counter++;

        app_timer_start(btnhold_timer,
                        APP_TIMER_TICKS(btnhold_period_ms),
                        (void *) &colorpicker);
    }

    /* falling */
    if (!button_read() && button.pressed_flag)
    {
        button.pressed_flag = false;

        app_timer_start(dblclk_timer,
                        APP_TIMER_TICKS(btn_dblclk_pause),
                        NULL);
    }
}

void gpiote_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    app_timer_start(debounce_timer,
                    APP_TIMER_TICKS(debounce_period_ms),
                    NULL);
}

void logs_init()
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

int main(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    pwm_init(&my_pwm);
    pwm_run(&my_pwm);

    nrfx_gpiote_init();

    logs_init();
    NRF_LOG_INFO("Starting up the test project with USB logging");

    app_timer_init();

    app_timer_create(&debounce_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     debounce_timer_handler);

    app_timer_create(&dblclk_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     dblclk_timer_handler);

    app_timer_create(&btnhold_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     btnhold_timer_handler);

    app_timer_create(&dispmode_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     dispmode_timer_handler);

    gpiote_init_on_toggle(my_btn_mappings[brd_btn_idx],
                          NRF_GPIO_PIN_PULLUP,
                          gpiote_handler);

    app_timer_start(dispmode_timer,
                    APP_TIMER_TICKS(dispmode_fast_ms),
                    (void *) &colorpicker);

    colorpicker_show((colorpicker_t *) &colorpicker);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        __WFI();
    }
}
