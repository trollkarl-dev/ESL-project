#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <ctype.h>

#include "nrf_drv_clock.h"
#include "app_timer.h"

#include "nrfx_pwm.h"
#include "nrfx_gpiote.h"
#include "nrfx_nvmc.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "nrf_log_default_backends.h"

#include "nrf_log_backend_usb.h"

#include "app_usbd.h"
#include "app_usbd_serial_num.h"
#include "app_usbd_cdc_acm.h"

#include "nrf_bootloader_info.h"
#include "nrf_dfu_types.h"

#include "lib/my_board.h"
#include "lib/colorspaces.h"
#include "lib/button.h"
#include "lib/colorpicker.h"
#include "lib/flash_storage.h"
#include "lib/cli.h"
#include "lib/color_list.h"

APP_TIMER_DEF(dispmode_timer);
APP_TIMER_DEF(debounce_timer);
APP_TIMER_DEF(btnhold_timer);
APP_TIMER_DEF(btnclk_timer);

enum { max_pct = 100 };

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

static const hsv_t default_color_hsv = {
    default_hue,
    max_saturation,
    max_brightness
};

static volatile colorpicker_t cpicker;
static volatile button_t button;

static const char *cpicker_modenames[] =
{
    "No Input",
    "Hue Modification",
    "Saturation Modification",
    "Brightness Modification"
};

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

static bool pwm_init(pwm_wrapper_t *pwm,
                     uint8_t const *channels,
                     uint16_t pwm_top_value,
                     bool invert)
{
    int i;

    nrfx_pwm_config_t my_pwm_config =
    {
        .irq_priority = APP_IRQ_PRIORITY_LOWEST,
        .base_clock   = NRF_PWM_CLK_1MHz,
        .count_mode   = NRF_PWM_MODE_UP,
        .top_value    = pwm_top_value,
        .load_mode    = NRF_PWM_LOAD_INDIVIDUAL,
        .step_mode    = NRF_PWM_STEP_AUTO,
    };

    for (i = 0; i < NRF_PWM_CHANNEL_COUNT; i++)
    {
        my_pwm_config.output_pins[i] = channels[i];
        if (invert)
        {
            my_pwm_config.output_pins[i] |= NRFX_PWM_PIN_INVERTED;
        }
    }

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

static void colorpicker_show_color(colorpicker_t *c)
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
    button_hold_check((button_t *) &button);
}

static void btnclk_timer_handler(void *ctx)
{
    button_click_check((button_t *) &button);
}

static void dispmode_timer_handler(void *ctx)
{
    colorpicker_dispmode_data_t data = {0};
    colorpicker_update_dispmode_data((colorpicker_t *) &cpicker, &data);

    pwm_set_duty_cycle(&my_pwm, pwm_channel_indicator, data.duty_cycle);

    if (data.state == cp_state_huemod)
        app_timer_start(dispmode_timer,
                        APP_TIMER_TICKS(dispmode_slow_ms),
                        NULL);

    if (data.state == cp_state_satmod)
        app_timer_start(dispmode_timer,
                        APP_TIMER_TICKS(dispmode_fast_ms),
                        NULL);
}

static void debounce_timer_handler(void *ctx)
{
    button_debounce_check((button_t *) &button);
}

static void gpiote_handler(nrfx_gpiote_pin_t pin, nrf_gpiote_polarity_t action)
{
    button_first_run((button_t *) &button);
}

static void logs_init(void)
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

static void save_state(void);

static void button_onclick(uint8_t clicks)
{
    colorpicker_state_t cpicker_state;

    NRF_LOG_INFO("Clicks - %d", clicks);

    if (clicks != 2)
    {
        return;
    }

    colorpicker_next_state((colorpicker_t *) &cpicker);
    cpicker_state = colorpicker_get_state((colorpicker_t *) &cpicker);

    if (cpicker_state == cp_state_noinpt)
    {
        save_state();
    }

    NRF_LOG_INFO("%s mode is in progress", cpicker_modenames[cpicker_state]);

    app_timer_start(dispmode_timer, APP_TIMER_TICKS(dispmode_fast_ms), NULL);
}

static void button_onhold(void)
{
    colorpicker_next_color((colorpicker_t *) &cpicker);

    if (colorpicker_get_state((colorpicker_t *) &cpicker) != cp_state_noinpt)
        colorpicker_show_color((colorpicker_t *) &cpicker);
}

static bool button_is_pressed(void)
{
    return my_btn_is_pressed(brd_btn_idx);
}

static void button_schedule_click_check(uint32_t delay_ms)
{
    app_timer_start(btnclk_timer, APP_TIMER_TICKS(delay_ms), NULL);
}

static void button_schedule_hold_check(uint32_t delay_ms)
{
    app_timer_start(btnhold_timer, APP_TIMER_TICKS(delay_ms), NULL);
}

static void button_schedule_debounce_check(uint32_t delay_ms)
{
    app_timer_start(debounce_timer, APP_TIMER_TICKS(delay_ms), NULL);
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

void flash_storage_page_erase(uint32_t address)
{
    nrfx_nvmc_page_erase(address);
}

void flash_storage_write_bytes(uint32_t address,
                               void const *bytes,
                               uint32_t num)
{
    nrfx_nvmc_bytes_write(address, bytes, num);
}

bool flash_storage_write_done_check(void)
{
    return nrfx_nvmc_write_done_check();
}

static colorpicker_save_t default_save = {
    .color = default_color_hsv,
    .sat_increment = 1,
    .val_increment = 1
};

static volatile flash_storage_t cpicker_storage;
enum { cpicker_storage_size_items = 64 };

void *cpicker_storage_top =
(void *) (BOOTLOADER_START_ADDR - NRF_DFU_APP_DATA_AREA_SIZE);

static void save_state(void)
{
    bool res;

    colorpicker_save_t save;
    colorpicker_dump((colorpicker_t *) &cpicker, &save);
    res = flash_storage_write((flash_storage_t *) &cpicker_storage, &save);

    NRF_LOG_INFO(res ? "The selected color is stored in the memory"
                     : "Failed to save the selected color");
}



#define READ_SIZE 1

static char m_rx_buffer[READ_SIZE];

static void usb_ev_handler(app_usbd_class_inst_t const * p_inst,
                           app_usbd_cdc_acm_user_event_t event);

/* Make sure that they don't intersect with LOG_BACKEND_USB_CDC_ACM */
#define CDC_ACM_COMM_INTERFACE  2
#define CDC_ACM_COMM_EPIN       NRF_DRV_USBD_EPIN3

#define CDC_ACM_DATA_INTERFACE  3
#define CDC_ACM_DATA_EPIN       NRF_DRV_USBD_EPIN4
#define CDC_ACM_DATA_EPOUT      NRF_DRV_USBD_EPOUT4

APP_USBD_CDC_ACM_GLOBAL_DEF(usb_cdc_acm,
                            usb_ev_handler,
                            CDC_ACM_COMM_INTERFACE,
                            CDC_ACM_DATA_INTERFACE,
                            CDC_ACM_COMM_EPIN,
                            CDC_ACM_DATA_EPIN,
                            CDC_ACM_DATA_EPOUT,
                            APP_USBD_CDC_COMM_PROTOCOL_NONE);

static volatile cli_t cpicker_cli;


static volatile bool gs_do_cli_process = false;
static volatile bool gs_tx_done = false;
static volatile bool gs_port_opened = false;

static void usb_ev_handler(app_usbd_class_inst_t const * p_inst,
                           app_usbd_cdc_acm_user_event_t event)
{
    switch (event)
    {
    case APP_USBD_CDC_ACM_USER_EVT_PORT_OPEN:
    {
        ret_code_t ret;
        ret = app_usbd_cdc_acm_read(&usb_cdc_acm, m_rx_buffer, READ_SIZE);
        UNUSED_VARIABLE(ret);

        gs_port_opened = true;
        break;
    }
    case APP_USBD_CDC_ACM_USER_EVT_PORT_CLOSE:
    {
        gs_port_opened = false;
        break;
    }
    case APP_USBD_CDC_ACM_USER_EVT_TX_DONE:
    {
        NRF_LOG_INFO("tx done");
        gs_tx_done = true;
        break;
    }
    case APP_USBD_CDC_ACM_USER_EVT_RX_DONE:
    {
        ret_code_t ret;
        do
        {
            /*Get amount of data transfered*/
            size_t size = app_usbd_cdc_acm_rx_size(&usb_cdc_acm);
            NRF_LOG_INFO("rx size: %d", size);

            /* It's the simple version of an echo. Note that writing doesn't
             * block execution, and if we have a lot of characters to read and
             * write, some characters can be missed.
             */
            if (m_rx_buffer[0] == '\r' || m_rx_buffer[0] == '\n')
            {
                ret = app_usbd_cdc_acm_write(&usb_cdc_acm, "\r\n", 2);
                gs_do_cli_process = true;
            }
            else if (m_rx_buffer[0] == '\b')
            {
            }
            else
            {
                ret = app_usbd_cdc_acm_write(&usb_cdc_acm,
                                             m_rx_buffer,
                                             READ_SIZE);

                cli_push_char((cli_t *) &cpicker_cli, m_rx_buffer[0]);
            }

            /* Fetch data until internal buffer is empty */
            ret = app_usbd_cdc_acm_read(&usb_cdc_acm,
                                        m_rx_buffer,
                                        READ_SIZE);
        } while (ret == NRF_SUCCESS);

        break;
    }
    default:
        break;
    }
}

volatile color_list_t cli_colors_list;

cli_result_t cpicker_cli_set_hsv(char const ** tokens, int tokens_amount, char *msg, uint32_t *msglen)
{
    colorpicker_save_t save;
    int i;
    long colors[3];

    *msglen = 0;

    if (tokens_amount != 4)
        return cli_error;

    for (i = 1; i < tokens_amount; i++)
    {
        char *end;
        colors[i-1] = strtol(tokens[i], &end, 10);

        if  ((*end != ' ') && (*end != '\0'))
            return cli_error;

    }

    if ((colors[0] < 0) || (colors[1] > max_hue))
        return cli_error;
    if ((colors[1] < 0) || (colors[1] > max_saturation))
        return cli_error;
    if ((colors[2] < 0) || (colors[1] > max_brightness))
        return cli_error;

    save.color = hsv(colors[0], colors[1], colors[2]);
    save.sat_increment = 1;
    save.val_increment = 1;

    colorpicker_load((colorpicker_t *) &cpicker, &save);
    colorpicker_show_color((colorpicker_t *) &cpicker);

    *msglen = snprintf(msg,
                       cli_max_outbuf_len,
                       "Color set to H=%ld, S=%ld, V=%ld\r\n",
                       colors[0],
                       colors[1],
                       colors[2]);

    return cli_success;
}

cli_result_t cpicker_cli_set_rgb(char const ** tokens, int tokens_amount, char *msg, uint32_t *msglen)
{
    colorpicker_save_t save;
    int i;
    long colors[3];

    *msglen = 0;

    if (tokens_amount != 4)
        return cli_error;

    for (i = 1; i < tokens_amount; i++)
    {
        char *end;
        colors[i-1] = strtol(tokens[i], &end, 10);

        if  ((*end != ' ') && (*end != '\0'))
            return cli_error;

        if ((colors[i-1] > max_pct) || (colors[i-1] < 0))
            return cli_error;
    }

    save.color = rgb2hsv(rgb(colors[0], colors[1], colors[2]));
    save.sat_increment = 1;
    save.val_increment = 1;

    colorpicker_load((colorpicker_t *) &cpicker, &save);
    colorpicker_show_color((colorpicker_t *) &cpicker);

    *msglen = snprintf(msg,
                       cli_max_outbuf_len,
                       "Color set to R=%ld, G=%ld, B=%ld\r\n",
                       colors[0],
                       colors[1],
                       colors[2]);

    return cli_success;
}

cli_result_t cpicker_cli_add_rgb_color(char const ** tokens, int tokens_amount, char *msg, uint32_t *msglen)
{
    int i;
    long colors[3];
    color_list_item_t color;

    *msglen = 0;

    if (tokens_amount != 5)
        return cli_error;

    for (i = 1; i < 4; i++)
    {
        char *end;
        colors[i-1] = strtol(tokens[i], &end, 10);

        if  ((*end != ' ') && (*end != '\0'))
            return cli_error;

        if ((colors[i-1] > max_pct) || (colors[i-1] < 0))
            return cli_error;
    }

    i = 0;
    while (0 != isalpha((int) (tokens[4][i])))
    {
        i++;
    }

    if ((i == 0) ||
        (i >= color_list_max_name_length) ||
        ((tokens[4][i] != ' ') && (tokens[4][i] != '\0'))
       )
        return cli_error;

    color.color = rgb2hsv(rgb(colors[0], colors[1], colors[2]));
    strncpy(color.name, tokens[4], i);
    color.name[i] = '\0';
    
    if (color_list_error == color_list_push((color_list_t *) &cli_colors_list, &color))
    {
        *msglen = snprintf(msg,
                           cli_max_outbuf_len,
                           "Unable to store, memory is full!\r\n");
        return cli_success;
    }

    *msglen = snprintf(msg,
                       cli_max_outbuf_len,
                       "Color (R=%ld, G=%ld, B=%ld) with name \"%s\" is stored in the volatile memory\r\n",
                       colors[0],
                       colors[1],
                       colors[2],
                       color.name);

    return cli_success;
    
}

static const cli_command_t cpicker_commands[] =
{
    {
        .name = "HSV",
        .usage = "<H> [0..360] <S> [0..100] <V> [0..100]\r\n",
        .description = "Set current color to specified one (HSV color model)\r\n",
        .worker = cpicker_cli_set_hsv
    },
    {
        .name = "RGB",
        .usage = "<R> [0..100] <G> [0..100] <B> [0..100]\r\n",
        .description = "Set current color to specified one (RGB color model)\r\n",
        .worker = cpicker_cli_set_rgb
    },
    {
        .name = "add_rgb_color",
        .usage = "<R> [0..100] <G> [0..100] <B> [0..100] <color_name> [A..Za..z]\r\n",
        .description = "Save specified color (RGB color model) to volatile memory\r\n",
        .worker = cpicker_cli_add_rgb_color
    }
};

void cli_puts(cli_t *cli, const char *s)
{
    uint32_t len = strlen(s);

    if (!gs_port_opened)
    {
        return;
    }

    size_t offset = 0;
    do
    {
        size_t tx_len = len - offset;

        while (!gs_tx_done)
        {
            while (app_usbd_event_queue_process())
            {
            }
        }

        if (tx_len > NRFX_USBD_EPSIZE)
        {
            tx_len = NRFX_USBD_EPSIZE;
        }

        gs_tx_done = false;
        ret_code_t ret = app_usbd_cdc_acm_write(&usb_cdc_acm,
                                                s + offset,
                                                tx_len);
        APP_ERROR_CHECK(ret);

        offset += tx_len;
    } while (offset < len);
}

int main(void)
{
    void *cpicker_saved_state;

    static uint8_t channels[NRF_PWM_CHANNEL_COUNT] = {
        my_led_mappings[pwm_channel_indicator],
        my_led_mappings[pwm_channel_red],
        my_led_mappings[pwm_channel_green],
        my_led_mappings[pwm_channel_blue],
    };

    nrf_drv_clock_init();
    nrf_drv_clock_lfclk_request(NULL);

    pwm_init(&my_pwm, channels, max_pct, true);
    pwm_run(&my_pwm);

    logs_init();
    NRF_LOG_INFO("Starting up the HSV color-picker device");

    app_timer_init();
    create_timers();

    colorpicker_init((colorpicker_t *) &cpicker,
                     default_color_hsv,
                     max_pct);

    flash_storage_init((flash_storage_t *) &cpicker_storage,
                       sizeof(colorpicker_save_t),
                       cpicker_storage_size_items,
                       cpicker_storage_top);

    cpicker_saved_state =
    flash_storage_read_last((flash_storage_t *) &cpicker_storage);

    if (cpicker_saved_state == NULL)
        colorpicker_load((colorpicker_t *) &cpicker, &default_save);
    else
        colorpicker_load((colorpicker_t *) &cpicker, cpicker_saved_state);

    colorpicker_show_color((colorpicker_t *) &cpicker);

    button_init((button_t *) &button,
                &button_timings,
                &button_callbacks);

    nrfx_gpiote_init();
    gpiote_init_on_toggle(my_btn_mappings[brd_btn_idx],
                          NRF_GPIO_PIN_PULLUP,
                          gpiote_handler);

    color_list_init((color_list_t *) &cli_colors_list);

    cli_init((cli_t *) &cpicker_cli, cpicker_commands, 3);

    app_usbd_class_inst_t const * class_cdc_acm = app_usbd_cdc_acm_class_inst_get(&usb_cdc_acm);
    ret_code_t ret = app_usbd_class_append(class_cdc_acm);
    APP_ERROR_CHECK(ret);

    while (true)
    {
        while (app_usbd_event_queue_process())
        {
        }

        LOG_BACKEND_USB_PROCESS();
        NRF_LOG_PROCESS();

        if (gs_do_cli_process)
        {
            gs_do_cli_process = false;
            cli_process((cli_t *) &cpicker_cli);
        }

        __WFI();
    }
}
