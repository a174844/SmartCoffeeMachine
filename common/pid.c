#include <math.h>
#include "pid.h"

void pid_init(pid_t *p, float kp, float ki, float kd,
              float setpoint, float out_min, float out_max, float err_thresh)
{
    p->kp = kp; p->ki = ki; p->kd = kd;
    p->setpoint = setpoint;
    p->out_min = out_min; p->out_max = out_max;
    p->err_thresh = err_thresh;
    pid_reset(p);
}

void pid_reset(pid_t *p)
{
    p->prev_err = 0.0f;
    p->prev2_err = 0.0f;
    p->out = p->out_min;
}

void pid_set_target(pid_t *p, float setpoint)
{
    p->setpoint = setpoint;
}

void pid_set_output(pid_t *p, float out)
{
    if (out > p->out_max) out = p->out_max;
    if (out < p->out_min) out = p->out_min;
    p->out = out;
}

/*
 * 增量式 PID：
 *   Δu = Kp·(e[k]-e[k-1]) + Ki·e[k] + Kd·(e[k] - 2·e[k-1] + e[k-2])
 *   u[k] = u[k-1] + Δu
 *
 * 积分分离：|e[k]| >= err_thresh 时把 Ki·e[k] 这一项置 0。
 * 增量形式下"不累积"就等于"不积分"，因此不需要额外的积分累加器，
 * 也就不会出现传统位置式 PID 那种需要单独做抗饱和处理的积分状态。
 */
float pid_update(pid_t *p, float meas)
{
    float e = p->setpoint - meas;
    float out;

    if (p->err_thresh > 0.0f && fabsf(e) >= p->err_thresh) {
        /*
         * 积分分离（大偏差）：切到位置式 PD，不投入积分作用。
         *
         * 注意：不能简单地在增量式里把 Ki·e 这一项置 0 ——
         * 增量式的输出是靠 Δu 一拍一拍累加出来的，丢掉 Ki·e 后，
         * 升温阶段误差持续为正但变化很小，Δu≈0，输出会永远停在初始值 0，
         * 加热根本建立不起来（偏差能到 -67℃，一直卡在环境温度）。
         * 位置式 PD 用 Kp·e 直接给出输出，才能在大偏差时立刻满功率。
         */
        out = p->kp * e + p->kd * (e - p->prev_err);
    } else {
        /*
         * 小偏差：增量式 PID，在上一拍输出基础上微调。
         *   Δu = Kp·(e[k]-e[k-1]) + Ki·e[k] + Kd·(e[k]-2·e[k-1]+e[k-2])
         *   u[k] = u[k-1] + Δu
         * 此时积分项只负责消除稳态误差，作用区间很窄，不会累积到饱和。
         */
        float du = p->kp * (e - p->prev_err)
                 + p->ki * e
                 + p->kd * (e - 2.0f * p->prev_err + p->prev2_err);
        out = p->out + du;
    }

    if (out > p->out_max) out = p->out_max;
    if (out < p->out_min) out = p->out_min;

    p->prev2_err = p->prev_err;
    p->prev_err  = e;
    p->out = out;
    return out;
}
