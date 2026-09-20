#ifndef CM_PID_H
#define CM_PID_H

/* 增量式 PID，带积分分离 */
typedef struct {
    float kp, ki, kd;
    float setpoint;
    float out_min, out_max;   /* 输出限幅（占空比百分比 0-100） */
    float err_thresh;         /* 积分分离阈值：|e| 超过该值时不累积积分 */
    float prev_err, prev2_err;
    float out;                /* 上一次输出，增量在此基础上累加 */
} pid_t;

void  pid_init(pid_t *p, float kp, float ki, float kd,
               float setpoint, float out_min, float out_max, float err_thresh);
void  pid_reset(pid_t *p);
void  pid_set_target(pid_t *p, float setpoint);

/*
 * 直接设定当前输出值（不改动误差历史）。
 *
 * 用途是前馈 + 微调：被控对象的稳态工作点如果已知（例如水泵在 9bar 下
 * 对应约 56% 占空比），把这个值直接灌进 p->out，PID 只需要在此基础上
 * 修正偏差，不必从 0 开始靠积分一点点把工作点建立起来。
 * 注意要在 pid_reset() 之后调用，因为 pid_reset() 会把 out 归到 out_min。
 */
void  pid_set_output(pid_t *p, float out);

/* 输入测量值，返回本次控制输出 */
float pid_update(pid_t *p, float meas);

#endif
