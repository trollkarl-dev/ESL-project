#include <stdint.h>
#include "nrfx_systick.h"
#include "soft_pwm_time.h"
#include "my_board.h"

void soft_pwm_systick_get(soft_pwm_timestamp_t * timestamp)
{
    nrfx_systick_get(timestamp);

}

bool soft_pwm_systick_test(soft_pwm_timestamp_t const * timestamp,
                           uint32_t us)
{
    return nrfx_systick_test(timestamp, us);
}

void soft_pwm_channel_on(uint16_t id)
{
    my_led_on(id);
}

void soft_pwm_channel_off(uint16_t id)
{
    my_led_off(id);
}
