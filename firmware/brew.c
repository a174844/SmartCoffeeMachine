#include <math.h>
#include "brew.h"
#include "actuators.h"
#include "calib.h"

/*
 * PID 整定说明。三路的量纲、工作点和响应速度差别很大，所以增益与策略都不同：
 *
 *  水温：被控对象是大热容（水 + 锅炉），升温慢、滞后大 -> 较强的 P 和较大的 D。
 *        积分分离阈值取 10℃：冷机启动时误差可达 70℃，这一段本来就不该投入积分。
 *
 *  压力：水泵响应快，且**工作点已知**（变送器满量程 16bar，水泵满占空比对应
 *        16bar，所以 9bar 的名义占空比就是 56.25%）。这里用「前馈 + 增量式微调」：
 *        进入萃取时把名义占空比直接灌进 pid 的 out（pid_set_output），
 *        PID 只修正偏差。不这么做的话，增量式要从 0 靠 Δu 一点点爬到 56%，
 *        实测会先在 0bar 和满量程之间冲几个来回才收敛，中间还会瞬时触发过压保护。
 *
 *  奶泡：同为加热对象但热容小得多，升温快 -> P 取中等，D 取小。
 */
static void load_defaults(brew_cfg_t *c)
{
    c->temp_setpoint  = 93.0f;
    c->press_setpoint = 9.0f;
    c->milk_setpoint  = 65.0f;
    c->temp_tol       = 1.0f;
    c->milk_tol       = 0.5f;
    c->temp_hold_ms   = 2000;
    c->extract_ms     = 25000;
    c->done_hold_ms   = 3000;
}

void brew_init(brew_ctx_t *b)
{
    load_defaults(&b->cfg);

    b->state          = BREW_IDLE;
    b->state_since    = 0;
    b->in_range_since = 0;
    b->out_heater     = 0.0f;
    b->out_pump       = 0.0f;
    b->out_milkfoam   = 0.0f;
    b->temp = b->press = b->milk = 0.0f;

    pid_init(&b->pid_temp,  8.0f, 0.15f, 2.0f,
             b->cfg.temp_setpoint,  0.0f, 100.0f, 10.0f);
    pid_init(&b->pid_press, 0.5f, 0.8f, 0.02f,
             b->cfg.press_setpoint, 0.0f, 100.0f, 0.0f);
    pid_init(&b->pid_milk,  5.0f, 0.10f, 0.5f,
             b->cfg.milk_setpoint,  0.0f, 100.0f, 8.0f);
}

/* 设定压力下的水泵名义占空比，用作前馈 */
static float press_nominal_duty(float setpoint_bar)
{
    float fs = (float)CM_PRESS_FS_X10 / 10.0f;
    if (fs <= 0.0f) return 0.0f;
    return setpoint_bar / fs * 100.0f;
}

void brew_start(brew_ctx_t *b, uint32_t now_ms)
{
    if (b->state != BREW_IDLE && b->state != BREW_DONE) return;

    pid_reset(&b->pid_temp);
    pid_reset(&b->pid_press);
    pid_reset(&b->pid_milk);

    b->state          = BREW_HEATING;
    b->state_since    = now_ms;
    b->in_range_since = 0;
}

void brew_stop(brew_ctx_t *b)
{
    b->state          = BREW_IDLE;
    b->state_since    = 0;
    b->in_range_since = 0;
    pid_reset(&b->pid_temp);
    pid_reset(&b->pid_press);
    pid_reset(&b->pid_milk);
}

void brew_update(brew_ctx_t *b, uint32_t now_ms,
                 float temp, float press, float milk, int fault)
{
    b->temp  = temp;
    b->press = press;
    b->milk  = milk;

    if (fault) {
        if (b->state != BREW_FAULT) b->state_since = now_ms;
        b->state        = BREW_FAULT;
        b->out_heater   = 0.0f;
        b->out_pump     = 0.0f;
        b->out_milkfoam = 0.0f;
        return;
    }

    switch (b->state) {
    case BREW_IDLE:
        b->out_heater   = 0.0f;
        b->out_pump     = 0.0f;
        b->out_milkfoam = 0.0f;
        break;

    case BREW_HEATING:
        /* 只开加热，目标水温；到位并稳定保持一段时间后进入萃取 */
        b->out_heater   = pid_update(&b->pid_temp, temp);
        b->out_pump     = 0.0f;
        b->out_milkfoam = 0.0f;

        if (fabsf(temp - b->cfg.temp_setpoint) <= b->cfg.temp_tol) {
            if (b->in_range_since == 0) {
                b->in_range_since = now_ms;
            } else if ((now_ms - b->in_range_since) >= b->cfg.temp_hold_ms) {
                b->state          = BREW_EXTRACT;
                b->state_since    = now_ms;
                b->in_range_since = 0;
                /* 先把水泵推到名义工作点，再交给 PID 微调 */
                pid_reset(&b->pid_press);
                pid_set_output(&b->pid_press, press_nominal_duty(b->cfg.press_setpoint));
            }
        } else {
            b->in_range_since = 0;   /* 掉出容差区间则重新计时 */
        }
        break;

    case BREW_EXTRACT:
        /* 加热回路继续维持水温（这是"水温维持"的实际含义：
           萃取时冷水进入锅炉会拉低温度，需要持续补偿），水泵做压力闭环 */
        b->out_heater   = pid_update(&b->pid_temp, temp);
        b->out_pump     = pid_update(&b->pid_press, press);
        b->out_milkfoam = 0.0f;

        if ((now_ms - b->state_since) >= b->cfg.extract_ms) {
            b->state       = BREW_MILKFOAM;
            b->state_since = now_ms;
            pid_reset(&b->pid_milk);
        }
        break;

    case BREW_MILKFOAM:
        /* 萃取结束，水泵停；加热维持水温；奶泡阀做温度闭环 */
        b->out_heater   = pid_update(&b->pid_temp, temp);
        b->out_pump     = 0.0f;
        b->out_milkfoam = pid_update(&b->pid_milk, milk);

        if (milk >= (b->cfg.milk_setpoint - b->cfg.milk_tol)) {
            b->state       = BREW_DONE;
            b->state_since = now_ms;
        }
        break;

    case BREW_DONE:
        b->out_heater   = 0.0f;
        b->out_pump     = 0.0f;
        b->out_milkfoam = 0.0f;
        if ((now_ms - b->state_since) >= b->cfg.done_hold_ms) {
            b->state = BREW_IDLE;
        }
        break;

    case BREW_FAULT:
    default:
        b->out_heater   = 0.0f;
        b->out_pump     = 0.0f;
        b->out_milkfoam = 0.0f;
        break;
    }
}

void brew_apply(const brew_ctx_t *b)
{
    heater_set_duty(b->out_heater);
    pump_set_duty(b->out_pump);
    milkfoam_set_duty(b->out_milkfoam);
}

const char *brew_state_name(brew_state_t s)
{
    switch (s) {
    case BREW_IDLE:     return "IDLE";
    case BREW_HEATING:  return "HEATING";
    case BREW_EXTRACT:  return "EXTRACT";
    case BREW_MILKFOAM: return "MILKFOAM";
    case BREW_DONE:     return "DONE";
    case BREW_FAULT:    return "FAULT";
    default:            return "?";
    }
}
