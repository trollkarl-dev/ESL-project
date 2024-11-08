#ifndef SOFT_PWM_TIME_H
#define SOFT_PWM_TIME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "nrfx_systick.h"

typedef nrfx_systick_state_t soft_pwm_timestamp_t;

extern void soft_pwm_systick_get(soft_pwm_timestamp_t * timestamp);
extern bool soft_pwm_systick_test(soft_pwm_timestamp_t const * timestamp,
                                  uint32_t us);

#ifdef __cplusplus
}
#endif

#endif /* SOFT_PWM_TIME_H */
