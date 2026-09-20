#ifndef CM_FILTERS_H
#define CM_FILTERS_H

#include <stdint.h>
#include <stddef.h>

#define CM_AVG_MAX 32

typedef struct {
    uint16_t buf[CM_AVG_MAX];
    size_t   idx, len;
    uint32_t sum;
} avg_t;

void     avg_init(avg_t *f, size_t len);

/*
 * 用首次采样值预填整个窗口。
 *
 * 不预填的话，窗口初始全 0，头几次 avg_push 的输出会被 0 拉低 ——
 * 在 ADC 上表现为一次"读数突然掉到接近 0"，会被保护逻辑误判成传感器短路。
 * 上电时用第一个可信读数预填即可消除这个假故障。
 */
void     avg_prime(avg_t *f, uint16_t v);

uint16_t avg_push(avg_t *f, uint16_t v);

#endif
