#ifndef MY_LED_H
#define MY_LED_H

#include "nrf_gpio.h"

enum {
    my_led_first = 1,
    my_led_last = 4
};

void my_led_init(int idx);
void my_led_on(int idx);
void my_led_off(int idx);
void my_led_invert(int idx);

#endif /* MY_LED_H */
