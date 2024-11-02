#include "soft_pwm.h"

void soft_pwm_set_channel(struct soft_pwm *pwm, uint16_t ch, uint8_t value)
{
    pwm->channels[ch].value = value % 100;
    /* pwm[ch].value = ((soft_pwm_period_us << 8) * (value % 100) / 100) >> 8; */
}

uint8_t soft_pwm_get_channel(struct soft_pwm *pwm, uint16_t ch)
{
    return pwm->channels[ch].value;
}

void soft_pwm_routine(struct soft_pwm *pwm)
{
    uint16_t idx;

    /* start of pwm cycle */
    if (nrfx_systick_test(&(pwm->state), 1000))
    {
        nrfx_systick_get(&(pwm->state));

        for (idx = 0; idx < pwm->amount; idx++)
        {
            if (pwm->channels[idx].value != 0)
            {
                pwm->channel_on(pwm->channels[idx].id);
            }
        }

        return;
    }

    /* test channels */
    for (idx = 0; idx < pwm->amount; idx++)
    {
        if (pwm->channels[idx].value == soft_pwm_period_us)
        {
            continue;
        }

        if (nrfx_systick_test(&(pwm->state), (((soft_pwm_period_us << 8) * (pwm->channels[idx].value % 100)) / 100) >> 8))
        {
            pwm->channel_off(pwm->channels[idx].id);
        }
    }
}
