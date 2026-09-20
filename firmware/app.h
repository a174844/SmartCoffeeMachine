#ifndef CM_APP_H
#define CM_APP_H

/*
 * 应用层：一个控制周期的完整逻辑。
 *
 * 拆成 app_setup() / app_step() 而不是全塞进 main() 的 for(;;)，
 * 是为了让同一份逻辑既能跑在目标板上（main.c），
 * 也能在换成别的板级实现后单独重复调用，便于逐周期确认行为。
 */

void app_setup(void);
void app_step(void);

/* 触发一次冲煮流程，由上层按需调用 */
void app_trigger_brew(void);

/* 供测试/显示读取最近一周期的状态 */
const char *app_state_name(void);
float       app_water_temp(void);
float       app_pressure(void);
float       app_milk_temp(void);
int         app_fault(void);

#endif /* CM_APP_H */
