#ifndef CM_BOARD_H
#define CM_BOARD_H

#include <stdint.h>

/*
 * 板级抽象层。
 *
 * 把「碰寄存器」的部分集中到这一层，好处有两个：
 *   1. 控制逻辑（brew / pid / protection）不再直接依赖 stm32f10x.h，
 *      移植到别的 MCU 只需要换一份 board_xxx.c；
 *   2. 换一份实现就能把整条控制逻辑单独跑起来，不必依赖具体硬件。
 *
 * 目标板的实现是 board_stm32f1.c。
 */

/* ADC 通道 */
typedef enum {
    CM_ADC_TEMP  = 0,   /* 水温 NTC 分压 */
    CM_ADC_PRESS = 1,   /* 萃取压力变送器 */
    CM_ADC_MILK  = 2,   /* 奶泡温度 NTC 分压 */
    CM_ADC_CH_COUNT
} cm_adc_ch_t;

/* PWM 通道 */
typedef enum {
    CM_PWM_HEATER   = 0,   /* 加热单元 */
    CM_PWM_PUMP     = 1,   /* 萃取水泵 */
    CM_PWM_MILKFOAM = 2,   /* 奶泡蒸汽阀 */
    CM_PWM_CH_COUNT
} cm_pwm_ch_t;

/* 初始化时钟、ADC、PWM、故障指示 IO。返回 0 成功 */
int      board_init(void);

/* 采样一次 ADC，返回 12 bit 原始值（0-4095） */
uint16_t board_adc_read(cm_adc_ch_t ch);

/* 设置 PWM 占空比，单位 %（0.0 - 100.0） */
void     board_pwm_set(cm_pwm_ch_t ch, float duty_pct);

/* 故障指示：点亮故障灯并让蜂鸣器响，code 用于区分故障类型 */
void     board_alarm_indicate(int code);

/* 清故障指示 */
void     board_alarm_clear(void);

/* 毫秒延时 / 自由运行计时 */
void     board_delay_ms(uint32_t ms);
uint32_t board_millis(void);

#endif /* CM_BOARD_H */
