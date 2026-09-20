#include "actuators.h"

/* 最近一次下发的占空比，便于上层回读当前执行器状态 */
static float g_duty[CM_PWM_CH_COUNT];

static void apply(cm_pwm_ch_t ch, float duty_pct)
{
    if (duty_pct < 0.0f)   duty_pct = 0.0f;
    if (duty_pct > 100.0f) duty_pct = 100.0f;
    g_duty[ch] = duty_pct;
    board_pwm_set(ch, duty_pct);
}

void actuators_init(void)
{
    int i;
    for (i = 0; i < CM_PWM_CH_COUNT; i++) g_duty[i] = 0.0f;
    actuators_all_off();
}

void heater_set_duty(float duty_pct)   { apply(CM_PWM_HEATER,   duty_pct); }
void pump_set_duty(float duty_pct)     { apply(CM_PWM_PUMP,     duty_pct); }
void milkfoam_set_duty(float duty_pct) { apply(CM_PWM_MILKFOAM, duty_pct); }

void actuators_all_off(void)
{
    apply(CM_PWM_HEATER,   0.0f);
    apply(CM_PWM_PUMP,     0.0f);
    apply(CM_PWM_MILKFOAM, 0.0f);
}

float actuators_get_duty(cm_pwm_ch_t ch)
{
    if (ch >= CM_PWM_CH_COUNT) return 0.0f;
    return g_duty[ch];
}
