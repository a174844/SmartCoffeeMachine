#include "protection.h"
#include "actuators.h"
#include "board.h"

/* 阈值（测量值单位：0.1℃ / 0.1bar） */
#define TEMP_LIMIT_X10  1300    /* 水温上限 130.0℃ */
#define PRESS_LIMIT_X10  150    /* 萃取压力上限 15.0bar（量程 16bar） */
#define MILK_LIMIT_X10   750    /* 奶泡温度上限 75.0℃ */

static cm_fault_t g_fault = CM_FAULT_NONE;

void protection_init(void)
{
    g_fault = CM_FAULT_NONE;
    board_alarm_clear();
}

void protection_alarm(cm_fault_t code)
{
    /* 先停机，再报警：顺序不能反，否则报警指示本身也可能被故障影响 */
    actuators_all_off();
    g_fault = code;
    board_alarm_indicate((int)code);
}

cm_fault_t protection_check(int16_t temp_x10, uint16_t press_x10, int16_t milk_x10,
                            int temp_ok, int press_ok, int milk_ok)
{
    /* 已处于故障态则保持，不再重复判定 */
    if (g_fault != CM_FAULT_NONE) return g_fault;

    if (!temp_ok)  { protection_alarm(CM_FAULT_TEMP_OPEN);  return g_fault; }
    if (!press_ok) { protection_alarm(CM_FAULT_PRESS_OPEN); return g_fault; }
    if (!milk_ok)  { protection_alarm(CM_FAULT_MILK_OPEN);  return g_fault; }

    if (temp_x10  > TEMP_LIMIT_X10)  { protection_alarm(CM_FAULT_TEMP_OVER);  return g_fault; }
    if (press_x10 > PRESS_LIMIT_X10) { protection_alarm(CM_FAULT_PRESS_OVER); return g_fault; }
    if (milk_x10  > MILK_LIMIT_X10)  { protection_alarm(CM_FAULT_MILK_OVER);  return g_fault; }

    return CM_FAULT_NONE;
}

cm_fault_t protection_fault(void)
{
    return g_fault;
}

int protection_is_latched(void)
{
    return g_fault != CM_FAULT_NONE;
}

void protection_reset(void)
{
    g_fault = CM_FAULT_NONE;
    board_alarm_clear();
}

const char *protection_fault_name(cm_fault_t f)
{
    switch (f) {
    case CM_FAULT_NONE:       return "normal";
    case CM_FAULT_TEMP_OPEN:  return "water-temp sensor open";
    case CM_FAULT_TEMP_OVER:  return "water over-temperature";
    case CM_FAULT_PRESS_OPEN: return "pressure sensor open";
    case CM_FAULT_PRESS_OVER: return "over-pressure";
    case CM_FAULT_MILK_OPEN:  return "milk-temp sensor open";
    case CM_FAULT_MILK_OVER:  return "milk over-temperature";
    default:                  return "unknown";
    }
}
