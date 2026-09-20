#include "board.h"
#include "stm32f10x.h"

/*
 * STM32F103 目标板实现。
 *
 * 引脚与定时器分配（与硬件接线一致）：
 *   PA0  温度 NTC 分压        -> ADC1_IN0
 *   PA1  压力变送器输出        -> ADC1_IN1
 *   PA2  奶泡温度 NTC 分压     -> ADC1_IN2
 *   PA6  TIM3_CH1 加热单元 PWM
 *   PA7  TIM3_CH2 萃取水泵 PWM
 *   PB0  TIM3_CH3 奶泡蒸汽阀 PWM
 *   PC13 故障指示灯（低有效）
 *   PB5  蜂鸣器
 *
 * PWM: TIM3，PSC=71（72MHz/72 = 1MHz），ARR=999 -> 1kHz，占空比分辨率 0.1%。
 */

#define PWM_ARR   999u
#define PWM_CLK_HZ 1000000u

static volatile uint32_t g_ms;

int board_init(void)
{
    /* --- 时钟：GPIOA/GPIOB/GPIOC + ADC1 + TIM3 --- */
    RCC->APB2ENR |= RCC_APB2ENR_IOPAEN | RCC_APB2ENR_IOPBEN
                  | RCC_APB2ENR_IOPCEN | RCC_APB2ENR_ADC1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;

    /* --- ADC1：独立模式、单次转换、软件启动 --- */
    ADC1->CR2 &= ~ADC_CR2_EXTTRIG;
    ADC1->SQR1 = 0;                     /* 规则序列长度 = 1 */
    ADC1->SMPR2 = (7u << 3) | (7u << 6) | (7u << 9); /* 通道 0/1/2 采样时间 239.5 周期 */
    ADC1->CR2 |= ADC_CR2_ADON;          /* 首次置位给 ADC 上电 */

    /* --- TIM3：PWM 模式 1，CH1/CH2/CH3 --- */
    TIM3->PSC  = 71;                    /* 72MHz / 72 = 1MHz */
    TIM3->ARR  = PWM_ARR;
    TIM3->CCMR1 = (6u << 4) | (1u << 3)    /* CH1: PWM1, 预装载 */;
    TIM3->CCMR1 |= (6u << 12) | (1u << 11) /* CH2 */;
    TIM3->CCMR2 = (6u << 4) | (1u << 3)    /* CH3 */;
    TIM3->CCER  = TIM_CCER_CC1E | TIM_CCER_CC2E | TIM_CCER_CC3E;
    TIM3->CR1  |= TIM_CR1_ARPE | TIM_CR1_CEN;

    /* --- PWM 输出 IO ---
     * PA6/PA7/PB0 是 TIM3 的三个通道，必须配成复用推挽输出（CNF=10, MODE=11）。
     * 漏掉这一步不会报任何错：TIM3 的计数器照常走、CCR 写进去读回来也对，
     * 但引脚停在复位默认的浮空输入上，示波器上一点波形都没有。 */
    GPIOA->CRL = (GPIOA->CRL & ~((0xFu << 24) | (0xFu << 28)))
               | (0xBu << 24) | (0xBu << 28);          /* PA6 / PA7 */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 0)) | (0xBu << 0);   /* PB0 */

    /* --- 故障指示 IO --- */
    GPIOC->CRH = (GPIOC->CRH & ~(0xFu << 20)) | (0x2u << 20); /* PC13 推挽输出 */
    GPIOB->CRL = (GPIOB->CRL & ~(0xFu << 20)) | (0x2u << 20); /* PB5  推挽输出 */
    board_alarm_clear();

    /* --- SysTick：1ms 计数 --- */
    SysTick->LOAD = (SystemCoreClock / 1000u) - 1u;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk
                  | SysTick_CTRL_TICKINT_Msk
                  | SysTick_CTRL_ENABLE_Msk;

    board_pwm_set(CM_PWM_HEATER,   0.0f);
    board_pwm_set(CM_PWM_PUMP,     0.0f);
    board_pwm_set(CM_PWM_MILKFOAM, 0.0f);
    return 0;
}

uint16_t board_adc_read(cm_adc_ch_t ch)
{
    ADC1->SQR3 = (uint32_t)ch;               /* 规则序列第一个转换 = 通道 ch */
    ADC1->CR2 |= ADC_CR2_ADON;               /* 软件启动转换 */
    while (!(ADC1->SR & ADC_SR_EOC)) { }     /* 等待转换完成 */
    return (uint16_t)(ADC1->DR & 0x0FFFu);
}

void board_pwm_set(cm_pwm_ch_t ch, float duty_pct)
{
    uint32_t ccr;

    if (duty_pct < 0.0f)   duty_pct = 0.0f;
    if (duty_pct > 100.0f) duty_pct = 100.0f;
    ccr = (uint32_t)(duty_pct * (float)PWM_ARR / 100.0f + 0.5f);

    switch (ch) {
    case CM_PWM_HEATER:   TIM3->CCR1 = ccr; break;
    case CM_PWM_PUMP:     TIM3->CCR2 = ccr; break;
    case CM_PWM_MILKFOAM: TIM3->CCR3 = ccr; break;
    default: break;
    }
}

void board_alarm_indicate(int code)
{
    (void)code;
    GPIOC->BRR  = (1u << 13);   /* PC13 低有效：点亮故障灯 */
    GPIOB->BSRR = (1u << 5);    /* 蜂鸣器响 */
}

void board_alarm_clear(void)
{
    GPIOC->BSRR = (1u << 13);
    GPIOB->BRR  = (1u << 5);
}

void board_delay_ms(uint32_t ms)
{
    uint32_t start = g_ms;
    while ((g_ms - start) < ms) { __WFI(); }   /* 进低功耗，等 SysTick 唤醒 */
}

uint32_t board_millis(void)
{
    return g_ms;
}

/* SysTick 中断：1ms 递增 */
void SysTick_Handler(void)
{
    g_ms++;
}
