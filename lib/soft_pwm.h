#ifndef SOFT_PWM_H
#define SOFT_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "nrfx_systick.h"

enum { soft_pwm_period_us = 1000 } /* f = 1 kHz */;

struct soft_pwm_channel {
    uint16_t id;
    uint8_t value;
};

struct soft_pwm {
    struct soft_pwm_channel *channels;
    nrfx_systick_state_t state;
    uint16_t amount;

    void (*channel_on)(uint16_t id);
    void (*channel_off)(uint16_t id);
};

void soft_pwm_set_channel(struct soft_pwm *pwm, uint16_t ch, uint8_t value);
uint8_t soft_pwm_get_channel(struct soft_pwm *pwm, uint16_t ch);
void soft_pwm_routine(struct soft_pwm *pwm); /* must be called periodically */

#ifdef __cplusplus
}
#endif

#endif /* SOFT_PWM_H */
