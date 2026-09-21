/*
 * freestanding 构建下需要自己补的那几个 libc 符号。
 *
 * 本工程的 ARM 固件用 `zig cc -target thumb-freestanding-eabi` 编译，
 * 目标平台没有 libc：编译器自带的内建头里既没有 <string.h> 也没有 <math.h>，
 * 也没有链接任何 C 运行库。
 *
 * 这里补两类东西：
 *   1) 内存函数 memcpy/memmove/memset/strlen —— 编译器会把结构体赋值、
 *      数组清零这类语句变成对它们的调用，缺一个就链接不过；
 *   2) fabsf —— common/pid.c 与 firmware/brew.c 里各用了一次，
 *      实现只是清符号位（见下）。
 *   3) __libc_init_array —— 启动文件在跳 main 之前会调它。
 *
 * 不用 newlib 的理由很直接：为了这几个函数把整个 C 库拉进固件，
 * 光 flash 就多占十几 KB，还会带进 _sbrk/_write 那一串根本用不上的桩。
 * 本工程 64KB Flash 的资源约束下，这种"顺手引一个库"的代价是实打实的。
 */

#include <stddef.h>
#include <stdint.h>

void *memcpy(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    /* 按字搬比按字节快得多，这里对齐情况不固定，所以不做对齐优化，
       先保证正确：源和目的可能重叠的情况由 memmove 负责，memcpy 不管。 */
    while (n--) *d++ = *s++;
    return dst;
}

void *memmove(void *dst, const void *src, size_t n)
{
    uint8_t       *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;

    if (d == s || n == 0) return dst;

    /* 目的在前、且两段重叠时必须从后往前搬，否则前几个字节就把源数据盖掉了。
       ble_frame.c 的字节流组帧器每丢一个字节就 memmove 一次整段，
       正是这种重叠场景。 */
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n;
        s += n;
        while (n--) *--d = *--s;
    }
    return dst;
}

void *memset(void *dst, int c, size_t n)
{
    uint8_t *d = (uint8_t *)dst;

    while (n--) *d++ = (uint8_t)c;
    return dst;
}

size_t strlen(const char *s)
{
    const char *p = s;

    while (*p) p++;
    return (size_t)(p - s);
}

/*
 * 启动文件（startup_stm32f103xb.s）在跳 main 之前会调 __libc_init_array，
 * 正常由 newlib 提供。这里按它的语义实现：依次执行 .init_array 段里的函数指针。
 *
 * 本工程没有 C++ 全局对象，但编译器/链接器仍可能往 .init_array 里放东西，
 * 所以还是老老实实走一遍，而不是给个空函数糊过去 ——
 * 空函数在"现在没事"和"以后加了带构造的东西就悄悄不执行"之间选错了。
 */
typedef void (*init_fn_t)(void);

extern init_fn_t __init_array_start[];
extern init_fn_t __init_array_end[];

void __libc_init_array(void)
{
    size_t i;
    size_t n = (size_t)(__init_array_end - __init_array_start);

    for (i = 0; i < n; i++) {
        __init_array_start[i]();
    }
}

/*
 * fabsf：common/pid.c 和 firmware/brew.c 用它做阈值比较
 * （|误差| >= 阈值 / |温度-设定| <= 容差）。
 *
 * 直接清符号位：IEEE-754 单精度的 bit31 就是符号位，把它清掉即为绝对值。
 * 这样写不引入分支，也不会碰到 NaN/Inf 的行为差异 —— 用 `x < 0 ? -x : x`
 * 那种写法在 -0.0f 上会返回 -0.0f，虽然比较起来相等，但一旦被拿去参与
 * 后续的除法就可能出现意外的符号。清位对 -0.0f 会得到 +0.0f，更干净。
 *
 * 用 memcpy 在 float 与 uint32_t 之间转换，而不是指针强转 ——
 * 强转会踩到严格别名规则（-O2 下编译器有权假设两者不重叠）。
 */
float fabsf(float x)
{
    uint32_t bits;

    memcpy(&bits, &x, sizeof(bits));
    bits &= 0x7FFFFFFFu;
    memcpy(&x, &bits, sizeof(x));
    return x;
}
