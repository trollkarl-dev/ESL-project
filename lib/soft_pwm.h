#ifndef SOFT_PWM_H
#define SOFT_PWM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include "soft_pwm_time.h"

enum { soft_pwm_max_pct = 100 };

enum soft_pwm_init_result {
    soft_pwm_init_success = 0,
    soft_pwm_init_fail
};

struct soft_pwm_channel {
    uint32_t duty_cycle_us;
    uint16_t id;
    uint8_t duty_cycle_pct;
    bool is_set;
};

struct soft_pwm {
    struct soft_pwm_channel *channels;
    uint16_t channels_amount;

    soft_pwm_timestamp_t timestamp;
    uint32_t period_us;

    void (*channel_on)(uint16_t id);
    void (*channel_off)(uint16_t id);
};

enum soft_pwm_init_result
soft_pwm_init(struct soft_pwm *pwm,
              struct soft_pwm_channel *channels,
              uint16_t *channels_id,
              uint16_t channels_amount,
              uint32_t period_us,
              void (*channel_on)(uint16_t),
              void (*channel_off)(uint16_t));

void soft_pwm_set_duty_cycle_pct(struct soft_pwm *pwm,
                                 uint16_t ch,
                                 uint8_t duty_cycle_pct);

uint8_t soft_pwm_get_duty_cycle_pct(struct soft_pwm const *pwm,
                                    uint16_t ch);

void soft_pwm_routine(struct soft_pwm *pwm); /* must be called in loop */

#ifdef __cplusplus
}
#endif

#endif /* SOFT_PWM_H */
