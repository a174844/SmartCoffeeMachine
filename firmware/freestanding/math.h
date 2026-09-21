#ifndef CM_FREESTANDING_MATH_H
#define CM_FREESTANDING_MATH_H

/*
 * freestanding 构建下的 <math.h> 替身。
 *
 * 为什么要自己写这个头：`zig cc -target thumb-freestanding-eabi` 的目标平台
 * 没有 libc/libm，编译器自带的内建头里也没有 <math.h>，而本工程有两个文件
 * 包含它：
 *     common/pid.c:1     firmware/brew.c:1
 *
 * 先查清到底用到了什么，再决定这个头里放什么 —— 两个文件里真正的 libm 调用
 * 只有一处：fabsf，出现在
 *     common/pid.c:47          if (... && fabsf(e) >= p->err_thresh)
 *     firmware/brew.c:112      if (fabsf(temp - b->cfg.setpoint) <= tol)
 * 其余用到 <math.h> 的地方只是顺手 include，没有调用。
 *
 * 所以这里只声明 fabsf，实现放在 firmware/libc_min.c：
 * 取绝对值只要清掉符号位，一行就够，没有必要为了它把整个 libm 拉进来。
 * 如果以后真要用 sqrtf / powf 这些，应该显式在 build_arm.py 里链接实现，
 * 而不是在这个头里塞一份"看起来能用"的替身。
 */

float fabsf(float x);

#endif /* CM_FREESTANDING_MATH_H */
