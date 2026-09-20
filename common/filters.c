#include "filters.h"

void avg_init(avg_t *f, size_t len)
{
    size_t i;
    if (len > CM_AVG_MAX) len = CM_AVG_MAX;
    if (len == 0) len = 1;
    for (i = 0; i < CM_AVG_MAX; i++) f->buf[i] = 0;
    f->idx = 0; f->len = len; f->sum = 0;
}

void avg_prime(avg_t *f, uint16_t v)
{
    size_t i;
    for (i = 0; i < CM_AVG_MAX; i++) f->buf[i] = 0;
    for (i = 0; i < f->len; i++) f->buf[i] = v;
    f->sum = (uint32_t)v * (uint32_t)f->len;
    f->idx = 0;
}

uint16_t avg_push(avg_t *f, uint16_t v)
{
    /* 减去即将被覆盖的旧值，再加上新值，保持 O(1) */
    f->sum -= f->buf[f->idx];
    f->buf[f->idx] = v;
    f->sum += v;
    f->idx = (f->idx + 1) % f->len;
    return (uint16_t)(f->sum / f->len);
}
