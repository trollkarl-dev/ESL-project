#include "my_led.h"
#include "nrf_gpio.h"

static const int my_led_mappings[] = {
    NRF_GPIO_PIN_MAP(0,  6),
    NRF_GPIO_PIN_MAP(0,  8),
    NRF_GPIO_PIN_MAP(1,  9),
    NRF_GPIO_PIN_MAP(0, 12),
};

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
