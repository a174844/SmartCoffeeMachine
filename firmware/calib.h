#ifndef CM_CALIB_H
#define CM_CALIB_H

#include <stdint.h>

/*
 * 传感器标定参数。
 *
 * 为什么单独抽一个头文件：ADC 原始值与工程量之间的换算关系（分压电阻、
 * NTC 查表、变送器量程）集中在这里定义，sensors.c 与标定时的换算工具共用
 * 同一份，避免同一套公式在两处各写一遍之后对不上。
 */

/* ---------------- ADC ---------------- */

#define CM_ADC_BITS    12
#define CM_ADC_MAX     4095
#define CM_ADC_VREF_MV 3300u

/* ---------------- NTC 10K / B=3950 ---------------- */
/*
 * 分压电路：VCC -- NTC -- [ADC 采样点] -- R_FIXED -- GND
 *   R_ntc = R_FIXED * (ADC_MAX - raw) / raw
 *   raw   = ADC_MAX * R_FIXED / (R_ntc + R_FIXED)
 */

#define CM_NTC_R_FIXED 10000

static const int16_t cm_ntc_table[][2] = {
    {   0, 32000}, {  10, 20000}, {  20, 12500}, {  25, 10000},
    {  40,  5300}, {  60,  2500}, {  80,  1250}, {  93,   800},
    { 100,   650}, { 120,   380}, { 130,   250}, { 150,   150},
};
#define CM_NTC_N (sizeof(cm_ntc_table) / sizeof(cm_ntc_table[0]))

/* ---------------- 压力变送器 ---------------- */
/*
 * 变送器输出 0.5V-4.5V 对应 0-16.0 bar。0-3.3V 的 ADC 读不了 4.5V，
 * 所以前面加了 3:2 分压，落到 ADC 引脚上是 333mV-3000mV。
 *   p_x10 = (pin_mv - CM_PRESS_MV_MIN) * CM_PRESS_FS_X10
 *           / (CM_PRESS_MV_MAX - CM_PRESS_MV_MIN)
 */

#define CM_PRESS_MV_MIN 333u    /* 0 bar 时引脚电压(mV) */
#define CM_PRESS_MV_MAX 3000u   /* 16.0 bar 时引脚电压(mV) */
#define CM_PRESS_FS_X10 160u    /* 满量程 16.0 bar，单位 0.1bar */

#endif /* CM_CALIB_H */
