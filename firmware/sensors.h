#ifndef CM_SENSORS_H
#define CM_SENSORS_H

#include <stdint.h>

/* NTC 热敏电阻分压 -> 温度(0.1℃)；返回 1 表示读数有效（既没断线也没短路） */
int temp_read_x10(uint16_t adc_raw, int16_t *temp_x10);

/* 奶泡温度：同型号 NTC，只是在另一路 ADC 上，标定表一致 */
int milk_read_x10(uint16_t adc_raw, int16_t *milk_x10);

/* 压力变送器 -> 压力(0.1 bar) */
int pressure_read(uint16_t adc_raw, uint16_t *press_x10);

/* 通道级故障检测：返回 0 正常，1 断线（拉到满量程），2 短路（拉到地），3 超量程 */
int sensor_fault_check(uint16_t adc_raw);

#endif /* CM_SENSORS_H */
