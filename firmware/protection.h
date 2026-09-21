#ifndef CM_PROTECTION_H
#define CM_PROTECTION_H

#include <stdint.h>

/*
 * 保护逻辑：任何一路传感器异常或任一测量值越限，立即关断全部执行器并报警，
 * 且**保持**在故障态（latch），直到故障排除后由上层显式复位。
 * 之所以要 latch 而不是"恢复即自动重启"，是因为传感器断线经常是间歇性的，
 * 自动恢复会让加热器在故障与正常之间反复通断。
 */

typedef enum {
    CM_FAULT_NONE = 0,
    CM_FAULT_TEMP_OPEN,    /* 水温传感器断线 / 短路 */
    CM_FAULT_TEMP_OVER,    /* 水温超上限 */
    CM_FAULT_PRESS_OPEN,   /* 压力传感器断线 */
    CM_FAULT_PRESS_OVER,   /* 萃取压力超上限 */
    CM_FAULT_MILK_OPEN,    /* 奶泡温度传感器断线 */
    CM_FAULT_MILK_OVER     /* 奶泡温度超上限 */
} cm_fault_t;

void       protection_init(void);

/*
 * 每周期调用一次。
 * temp_x10 / press_x10 / milk_x10：三路测量值（0.1℃ / 0.1bar / 0.1℃）
 * *_ok：该路读数是否有效（由 sensors.c 的转换函数返回）
 * 返回当前故障码（CM_FAULT_NONE 表示正常）
 */
cm_fault_t protection_check(int16_t temp_x10, uint16_t press_x10, int16_t milk_x10,
                            int temp_ok, int press_ok, int milk_ok);

/* 当前故障码；一旦进入故障态会保持，直到 protection_reset() */
cm_fault_t protection_fault(void);
int        protection_is_latched(void);

/* 故障排除后手工复位（通常在故障码消失且硬件确认无误后调用） */
void       protection_reset(void);

/* 直接触发报警（供上层在别处检测到异常时调用） */
void       protection_alarm(cm_fault_t code);

const char *protection_fault_name(cm_fault_t f);

#endif /* CM_PROTECTION_H */
