#ifndef CM_BREW_H
#define CM_BREW_H

#include <stdint.h>
#include "pid.h"

/*
 * 冲煮流程状态机：把水温、萃取压力、奶泡温度三个控制回路按时间顺序编起来。
 *
 *   IDLE ──start──► HEATING ──水温到位并保持──► EXTRACT ──计时到──► MILKFOAM ──奶泡达标──► DONE
 *                     │                            │                    │
 *                     └──────────── 保护触发 ──────┴────────────────────┴──► FAULT（全关输出）
 *
 * 三个回路共用一份增量式 PID（common/pid.c），只是参数与目标值不同：
 * 水温回路全程参与（萃取与奶泡阶段也要维持水温），另外两路只在各自阶段投入。
 */

typedef enum {
    BREW_IDLE = 0,
    BREW_HEATING,
    BREW_EXTRACT,
    BREW_MILKFOAM,
    BREW_DONE,
    BREW_FAULT
} brew_state_t;

/* 冲煮流程参数（量产机型上这些值来自面板/EEPROM，此处给默认值） */
typedef struct {
    float    temp_setpoint;   /* 水温目标 ℃ */
    float    press_setpoint;  /* 萃取压力目标 bar */
    float    milk_setpoint;   /* 奶泡温度目标 ℃ */
    float    temp_tol;        /* 水温到位容差 ℃ */
    float    milk_tol;        /* 奶泡到位容差 ℃ */
    uint32_t temp_hold_ms;    /* 水温需在容差内保持多久才进入萃取 */
    uint32_t extract_ms;      /* 萃取时长 */
    uint32_t done_hold_ms;    /* 完成后保持多久回到 IDLE */
} brew_cfg_t;

typedef struct {
    brew_state_t state;
    uint32_t     state_since;      /* 进入当前状态的时刻 */
    uint32_t     in_range_since;   /* 水温进入容差的时刻；0 表示尚未进入 */
    brew_cfg_t   cfg;

    pid_t pid_temp;
    pid_t pid_press;
    pid_t pid_milk;

    /* 最近一次计算出的执行器输出（%） */
    float out_heater;
    float out_pump;
    float out_milkfoam;

    /* 最近一次测量值，供显示/上报使用 */
    float temp;
    float press;
    float milk;
} brew_ctx_t;

void brew_init(brew_ctx_t *b);

/* 启动一次冲煮流程；已在运行中则忽略 */
void brew_start(brew_ctx_t *b, uint32_t now_ms);

/* 中止流程并回到 IDLE */
void brew_stop(brew_ctx_t *b);

/*
 * 每控制周期调用一次。
 * fault：保护模块是否已锁存故障（非 0 则切到 BREW_FAULT 并关断全部输出）
 */
void brew_update(brew_ctx_t *b, uint32_t now_ms,
                 float temp, float press, float milk, int fault);

/* 把 ctx 中的输出下发到执行器 */
void brew_apply(const brew_ctx_t *b);

const char *brew_state_name(brew_state_t s);

#endif /* CM_BREW_H */
