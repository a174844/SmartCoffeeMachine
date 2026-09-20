#ifndef CM_ACTUATORS_H
#define CM_ACTUATORS_H

#include "board.h"

/* 执行机构语义封装：把「占空比」翻译成「加热 / 萃取泵 / 奶泡阀」 */

void actuators_init(void);

void heater_set_duty(float duty_pct);     /* 加热单元 */
void pump_set_duty(float duty_pct);       /* 萃取水泵 */
void milkfoam_set_duty(float duty_pct);   /* 奶泡蒸汽阀 */

/* 全部关断，用于保护动作 */
void actuators_all_off(void);

/* 回读最近一次下发的占空比，供上层逻辑使用 */
float actuators_get_duty(cm_pwm_ch_t ch);

#endif /* CM_ACTUATORS_H */
