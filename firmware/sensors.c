#include "sensors.h"
#include "calib.h"

#define ADC_MAX   CM_ADC_MAX
#define ADC_FULL  (ADC_MAX - 20)   /* 接近满量程判定为断线（分压点上拉） */
#define ADC_ZERO  5                /* 接近零判定为短路（分压点下拉到地） */

int sensor_fault_check(uint16_t adc_raw)
{
    if (adc_raw >= ADC_FULL) return 1;   /* 断线 */
    if (adc_raw <= ADC_ZERO) return 2;   /* 短路 */
    return 0;
}

/*
 * ADC 原始值 -> NTC 温度。
 * 先把分压关系还原成阻值，再在标定表上线性插值。
 * 阻值落在标定表之外时返回 0（读数不可信），交给保护逻辑处理。
 */
static int ntc_to_temp_x10(uint16_t adc_raw, int16_t *temp_x10)
{
    int32_t r;
    int i;

    if (sensor_fault_check(adc_raw)) return 0;

    r = (int32_t)CM_NTC_R_FIXED * (int32_t)(ADC_MAX - adc_raw) / (int32_t)(adc_raw + 1);
    if (r <= 0) return 0;

    for (i = 0; i < (int)CM_NTC_N - 1; i++) {
        int32_t r0 = cm_ntc_table[i][1],     r1 = cm_ntc_table[i + 1][1];
        int32_t t0 = (int32_t)cm_ntc_table[i][0] * 10;
        int32_t t1 = (int32_t)cm_ntc_table[i + 1][0] * 10;

        if ((r <= r0 && r >= r1) || (r >= r0 && r <= r1)) {
            *temp_x10 = (int16_t)(t0 + (t1 - t0) * (r0 - r) / (r0 - r1));
            return 1;
        }
    }
    return 0;
}

int temp_read_x10(uint16_t adc_raw, int16_t *temp_x10)
{
    return ntc_to_temp_x10(adc_raw, temp_x10);
}

int milk_read_x10(uint16_t adc_raw, int16_t *milk_x10)
{
    return ntc_to_temp_x10(adc_raw, milk_x10);
}

int pressure_read(uint16_t adc_raw, uint16_t *press_x10)
{
    int32_t mv, span, p;

    if (sensor_fault_check(adc_raw)) return 0;

    mv = (int32_t)((uint32_t)adc_raw * CM_ADC_VREF_MV / (ADC_MAX + 1));

    /* 落在 0-16bar 之外（含分压电阻失效导致的偏离）一律判为读数无效。
       注意全程用有符号运算：满量程 16bar 对应引脚 3000mV，
       0bar 附近 mv 可能略低于 CM_PRESS_MV_MIN，无符号相减会回绕成极大值，
       被误判成过压。 */
    if (mv < (int32_t)CM_PRESS_MV_MIN - 30 || mv > (int32_t)CM_PRESS_MV_MAX + 100) return 0;

    span = (int32_t)CM_PRESS_MV_MAX - (int32_t)CM_PRESS_MV_MIN;
    p    = ((mv - (int32_t)CM_PRESS_MV_MIN) * (int32_t)CM_PRESS_FS_X10) / span;
    if (p < 0) p = 0;

    *press_x10 = (uint16_t)p;
    return 1;
}
