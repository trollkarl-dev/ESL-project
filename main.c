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

APP_TIMER_DEF(displaymode_timer);
APP_TIMER_DEF(debounce_timer);
APP_TIMER_DEF(blinky_timer);
APP_TIMER_DEF(dblclk_timer);

enum { brd_btn_idx = 0 };
enum { btn_dblclk_pause = 200 };

enum { max_pct = 100 };

enum { leds_upd_period_ms = 50 };
enum { debounce_period_ms = 50 };

enum { displaymode_slow_ms = 10 };
enum { displaymode_fast_ms = 2 };

static volatile int displaymode_counter = 0;

static volatile bool btn_is_pressed = false;
static volatile uint8_t clicks_counter = 0;

static const uint8_t device_id[] =  {6, 5, 8, 2};

enum { max_hue = 360 };
enum { max_saturation = max_pct };
enum { max_brightness = max_pct };

static const uint16_t default_hue = ((device_id[2]*10 + device_id[3]) * max_hue) / max_pct;

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

static volatile hsv_t hsv_color =
{
    default_hue,
    max_saturation,
    max_brightness
};

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
    cp_state_noinpt,
    cp_state_huemod,
    cp_state_satmod,
    cp_state_valmod
} colorpicker_state_t;

static volatile colorpicker_state_t current_state = cp_state_noinpt;

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

static void colorpicker_update_led(hsv_t hsv)
{
    rgb_t rgb = hsv2rgb(hsv);

    pwm_set_duty_cycle(&my_pwm, 1, rgb.r);
    pwm_set_duty_cycle(&my_pwm, 2, rgb.g);
    pwm_set_duty_cycle(&my_pwm, 3, rgb.b);

    NRF_LOG_INFO("(%d, %d, %d)", rgb.r, rgb.g, rgb.b);
}

static uint8_t colorpicker_sat_counter = 0;
static uint8_t colorpicker_val_counter = 0;

static void blinky_timer_handler(void *ctx)
{
    colorpicker_update_led(hsv_color);

    switch (current_state)
    {
        case cp_state_huemod:
            hsv_color.h = (hsv_color.h + 1) % max_hue;
            break;
        case cp_state_satmod:
            colorpicker_sat_counter++;
            colorpicker_sat_counter %= 2*max_pct;
            hsv_color.s = abs(colorpicker_sat_counter - max_pct);
            break;
        case cp_state_valmod:
            colorpicker_val_counter++;
            colorpicker_val_counter %= 2*max_pct;
            hsv_color.v = abs(colorpicker_val_counter - max_pct);
            break;
        default:
            break;
    }

    if (btn_is_pressed && my_btn_is_pressed(brd_btn_idx))
    {
        app_timer_start(blinky_timer, APP_TIMER_TICKS(leds_upd_period_ms), NULL);
    }
}

static void dblclk_timer_handler(void *ctx)
{
    if (!btn_is_pressed && clicks_counter != 0)
    {
        NRF_LOG_INFO("Clicks - %d", clicks_counter);

        if (clicks_counter == 2)
        {
            current_state = (current_state == cp_state_valmod) ? cp_state_noinpt : current_state + 1;
            displaymode_counter = 0;
        }

        clicks_counter = 0;
    }
}

static void displaymode_timer_handler(void *ctx)
{
    uint32_t period = APP_TIMER_TICKS(current_state == cp_state_satmod ? displaymode_fast_ms : displaymode_slow_ms);

    switch (current_state)
    {
        case cp_state_noinpt:
            pwm_set_duty_cycle(&my_pwm, 0, 0);
            break;
        case cp_state_valmod:
            pwm_set_duty_cycle(&my_pwm, 0, max_pct);
            break;
        default:
            displaymode_counter = (displaymode_counter + 1) % (2*max_pct);
            pwm_set_duty_cycle(&my_pwm, 0, max_pct - abs(displaymode_counter - max_pct));
    }

    app_timer_start(displaymode_timer, period, NULL);
}

static void debounce_timer_handler(void *ctx)
{
    /* rising */
    if (my_btn_is_pressed(brd_btn_idx) && !btn_is_pressed)
    {
        btn_is_pressed = true;
        clicks_counter++;
        app_timer_start(blinky_timer, APP_TIMER_TICKS(leds_upd_period_ms), NULL);
    }

    /* falling */
    if (!my_btn_is_pressed(brd_btn_idx) && btn_is_pressed)
    {
        btn_is_pressed = false;
        app_timer_start(dblclk_timer, APP_TIMER_TICKS(btn_dblclk_pause), NULL);
    }
}

void btn_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    app_timer_start(debounce_timer, APP_TIMER_TICKS(debounce_period_ms), NULL);
}

void logs_init()
{
    ret_code_t ret = NRF_LOG_INIT(NULL);
    APP_ERROR_CHECK(ret);

    NRF_LOG_DEFAULT_BACKENDS_INIT();
}

static nrfx_gpiote_in_config_t
gpiote_config = NRFX_GPIOTE_CONFIG_IN_SENSE_TOGGLE(false);

int main(void)
{
    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    pwm_init(&my_pwm);
    pwm_run(&my_pwm);

    nrfx_gpiote_init();

    gpiote_config.pull = NRF_GPIO_PIN_PULLUP;
    nrfx_gpiote_in_init(my_btn_mappings[brd_btn_idx], &gpiote_config, btn_handler);
    nrfx_gpiote_in_event_enable(my_btn_mappings[brd_btn_idx], true);

    logs_init();
    NRF_LOG_INFO("Starting up the test project with USB logging");

    app_timer_init();

    app_timer_create(&debounce_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     debounce_timer_handler);

    app_timer_create(&dblclk_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     dblclk_timer_handler);

    app_timer_create(&blinky_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     blinky_timer_handler);

    app_timer_create(&displaymode_timer,
                     APP_TIMER_MODE_SINGLE_SHOT,
                     displaymode_timer_handler);
    app_timer_start(displaymode_timer, APP_TIMER_TICKS(displaymode_fast_ms), NULL);

    colorpicker_update_led(hsv_color);

    while (true)
    {
        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        __WFI();
    }
}
