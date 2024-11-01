#include <stdbool.h>
#include <stdint.h>

#include "nrf_delay.h"

#include "lib/my_board.h"

enum { blink_period_ms = 500 };

static const uint8_t device_id[] = {6, 5, 8, 2};

int main(void)
{
    int led_idx;
    int blinky_counter;

    /* Configure board. */
    for (led_idx = my_led_first; led_idx <= my_led_last; led_idx++)
    {
        my_led_init(led_idx);
        my_led_off(led_idx);
    }

    my_btn_init(0);

    led_idx = my_led_first;
    blinky_counter = 1;

    /* Toggle LEDs. */
    while (true)
    {
        if (!my_btn_is_pressed(0))
        {
            continue;
        }

        my_led_invert(led_idx);
        nrf_delay_ms(blink_period_ms);

        if (blinky_counter >= 2*device_id[led_idx])
        {
            blinky_counter = 1;
            led_idx = (led_idx >= my_led_last) ? my_led_first
                                               : led_idx+1;
        }
        else
        {
            blinky_counter++;
        }
    }
}
