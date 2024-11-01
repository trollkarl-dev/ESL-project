#include "my_board.h"

#include "nrf_gpio.h"

static const int my_led_mappings[] = {
    NRF_GPIO_PIN_MAP(0,  6),
    NRF_GPIO_PIN_MAP(0,  8),
    NRF_GPIO_PIN_MAP(1,  9),
    NRF_GPIO_PIN_MAP(0, 12),
};

static const int my_btn_mappings[] = {
    NRF_GPIO_PIN_MAP(1,  6)
};

static bool my_btn_check_idx(int idx)
{
    return ((idx >= my_btn_first) && (idx <= my_btn_last));
}

void my_btn_init(int idx)
{
    if (my_btn_check_idx(idx))
        nrf_gpio_cfg_input(my_btn_mappings[idx], my_btn_active_level == 0 ? NRF_GPIO_PIN_PULLUP
                                                                          : NRF_GPIO_PIN_PULLDOWN);
}

bool my_btn_is_pressed(int idx)
{
    return nrf_gpio_pin_read(my_btn_mappings[idx]) == my_btn_active_level;
}

static bool my_led_check_idx(int idx)
{
    return ((idx >= my_led_first) && (idx <= my_led_last));
}

void my_led_init(int idx)
{
    if (my_led_check_idx(idx))
        nrf_gpio_cfg_output(my_led_mappings[idx]);
}

void my_led_on(int idx)
{
    if (my_led_check_idx(idx))
        nrf_gpio_pin_write(my_led_mappings[idx], my_led_active_level);
}

void my_led_off(int idx)
{
    if (my_led_check_idx(idx))
        nrf_gpio_pin_write(my_led_mappings[idx], !my_led_active_level);
}

void my_led_invert(int idx)
{
    if (my_led_check_idx(idx))
        nrf_gpio_pin_toggle(my_led_mappings[idx]);
}