#include "soft_pwm.h"

enum soft_pwm_init_result
soft_pwm_init(struct soft_pwm *pwm,
              struct soft_pwm_channel *channels,
              uint8_t *channels_id,
              uint16_t channels_amount,
              uint32_t period_us,
              void (*channel_on)(uint16_t),
              void (*channel_off)(uint16_t))
{
    uint16_t channel_idx;

    if (pwm == NULL ||
        channels == NULL ||
        channels_id == NULL ||
        channels_amount == 0 ||
        period_us == 0 ||
        channel_on == NULL ||
        channel_off == NULL)
    {
        return soft_pwm_init_fail;
    }

    pwm->channels = channels;
    pwm->channels_amount = channels_amount;
    pwm->period_us = period_us;
    pwm->channel_on = channel_on;
    pwm->channel_off = channel_off;

    for (channel_idx = 0; channel_idx < channels_amount; channel_idx++)
    {
        channels[channel_idx].id = channels_id[channel_idx];
        channels[channel_idx].is_set = false;

        channels[channel_idx].duty_cycle_pct = 0;
        channels[channel_idx].duty_cycle_us = 0;
    }

    return soft_pwm_init_success;
}

void soft_pwm_set_duty_cycle_pct(struct soft_pwm *pwm,
                                 uint16_t ch,
                                 uint8_t duty_cycle_pct)
{
    uint32_t duty_cycle_us;

    duty_cycle_pct %= 1 + soft_pwm_max_pct;
    duty_cycle_us = (((pwm->period_us << 8) / 100) * duty_cycle_pct) >> 8;

    pwm->channels[ch].duty_cycle_pct = duty_cycle_pct;
    pwm->channels[ch].duty_cycle_us = duty_cycle_us;
}

uint8_t soft_pwm_get_duty_cycle_pct(struct soft_pwm const *pwm,
                                    uint16_t ch)
{
    return pwm->channels[ch].duty_cycle_pct;
}

void soft_pwm_routine(struct soft_pwm *pwm)
{
    uint16_t idx;

    /* start of pwm cycle */
    if (soft_pwm_systick_test(&(pwm->timestamp), pwm->period_us))
    {
        soft_pwm_systick_get(&(pwm->timestamp));

        for (idx = 0; idx < pwm->channels_amount; idx++)
        {
            if (!pwm->channels[idx].is_set &&
                pwm->channels[idx].duty_cycle_us != 0)
            {
                pwm->channel_on(pwm->channels[idx].id);
                pwm->channels[idx].is_set = true;
            }
        }

        return;
    }

    /* test channels */
    for (idx = 0; idx < pwm->channels_amount; idx++)
    {
        if (pwm->channels[idx].duty_cycle_us >= pwm->period_us)
        {
            continue;
        }

        if (pwm->channels[idx].is_set &&
            soft_pwm_systick_test(&(pwm->timestamp),
                                  pwm->channels[idx].duty_cycle_us))
        {
            pwm->channel_off(pwm->channels[idx].id);
            pwm->channels[idx].is_set = false;
        }
    }
}
