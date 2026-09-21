#ifndef CM_STM32F10X_COMPAT_H
#define CM_STM32F10X_COMPAT_H

/*
 * 兼容转发头 —— 只用于 ARM 构建。
 *
 * firmware/board_stm32f1.c 里写的是 #include "stm32f10x.h"，
 * 那是 ST **标准外设库（StdPeriph Lib）** 时代的头文件名。
 * 那套库早已停止维护，也不再随 CMSIS 分发；现在 CMSIS 里的设备头叫
 *     stm32f1xx.h   （家族入口，按型号条件包含）
 *     stm32f103xb.h （型号级，寄存器结构体与位掩码都在这里）
 *
 * 两者的寄存器定义是同一套（RCC->APB2ENR、GPIOB->CRL、ADC1->CR2、
 * TIM3->CCR1、SysTick->LOAD 这些名字都一致），而 board_stm32f1.c
 * 只做直写寄存器、没有调用任何 SPL 函数（没有 RCC_APB2PeriphClockCmd
 * 这类东西），所以这里做一次等价转发就够了 —— 源文件一行都不用改，
 * 也就不必往仓库里塞一份 2011 年的老库。
 *
 * 这个头只放在 ARM 构建的 include 路径里（firmware/freestanding），
 * 不参与宿主侧的编译。
 */

#ifndef STM32F103xB
#define STM32F103xB     /* 中容量：STM32F103C8/R8/CB/RB 等，64-128KB Flash */
#endif

#include "stm32f1xx.h"

#endif /* CM_STM32F10X_COMPAT_H */
