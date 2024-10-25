#ifndef MY_LED_H
#define MY_LED_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrf_gpio.h"

enum {
    my_led_first = 0,
    my_led_last = 3,
    my_led_amount = my_led_last - my_led_first + 1
};

void my_led_init(int idx);
void my_led_on(int idx);
void my_led_off(int idx);
void my_led_invert(int idx);

#ifdef __cplusplus
}
#endif

#endif /* MY_LED_H */
