#include "stm32f10x.h"

/*
 * 内核故障入口。
 *
 * 启动文件把 HardFault/MemManage/BusFault/UsageFault 都声明成了弱符号，
 * 默认指向 Default_Handler —— 那是个 `b .` 死循环。也就是说：
 * 一旦跑飞，板子就静悄悄地卡在那里，现场什么线索都没留下。
 *
 * 这里自己接管这四个入口：先把故障状态寄存器存下来（接调试器就能看到是
 * 哪类故障），再关中断停住。
 *
 * 本工程没有独立看门狗（这一点和 SmartGarden 不同），所以故障之后不会
 * 自动复位，而是"停住 + 留下现场"。对 bring-up 阶段来说，
 * 这比反复复位然后什么都不说要有用得多。
 *
 * 一个已知的盲区：如果故障发生在 board_init() 里，程序会停在这里，
 * 什么都不会发生。这一段的排查只能靠调试器 —— 但那本来也是
 * bring-up 阶段唯一靠谱的手段。
 */

volatile uint32_t g_fault_cfsr;   /* SCB->CFSR：UsageFault/BusFault/MemManage 明细 */
volatile uint32_t g_fault_hfsr;   /* SCB->HFSR：HardFault 明细                    */
volatile uint32_t g_fault_fault;  /* 哪个入口进来的：见下面 FAULT_* 常量          */

#define FAULT_HARD       1u
#define FAULT_MEMMANAGE  2u
#define FAULT_BUS        3u
#define FAULT_USAGE      4u

static void fault_halt(uint32_t which)
{
    g_fault_cfsr = SCB->CFSR;
    g_fault_hfsr = SCB->HFSR;
    g_fault_fault = which;

    /*
     * 清掉故障标志并关中断后停住。
     * 清标志是必须的：CFSR 里对应位不清，同一条指令再次执行会立刻重新触发，
     * 之后就没法在调试器里单步往回找了。
     */
    SCB->CFSR = SCB->CFSR;
    SCB->HFSR = SCB->HFSR;

    __disable_irq();
    for (;;) { }
}

void HardFault_Handler(void)   { fault_halt(FAULT_HARD); }
void MemManage_Handler(void)   { fault_halt(FAULT_MEMMANAGE); }
void BusFault_Handler(void)    { fault_halt(FAULT_BUS); }
void UsageFault_Handler(void)  { fault_halt(FAULT_USAGE); }
