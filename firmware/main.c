#include "app.h"

/* 目标板入口。板级初始化、PWM、ADC、SysTick 都在 board_init() 里，见 board_stm32f1.c */
int main(void)
{
    app_setup();
    for (;;) {
        app_step();
    }
}
