#include "app.h"
#include "board.h"
#include "sensors.h"
#include "actuators.h"
#include "protection.h"
#include "brew.h"
#include "filters.h"

/* 控制周期。500ms 对水温这种大热容对象足够，也留出余量给采样与上报 */
#define CTRL_PERIOD_MS 500

/* ADC 滑动平均窗口：抑制加热器/水泵通断引起的电源纹波 */
#define ADC_AVG_LEN 8

static brew_ctx_t g_brew;
static avg_t      g_f_temp, g_f_press, g_f_milk;
static int        g_autostart_done;   /* 上电后自动走一次流程，之后由面板按键触发 */

void app_setup(void)
{
    uint16_t raw;

    board_init();
    actuators_init();
    protection_init();
    brew_init(&g_brew);

    avg_init(&g_f_temp,  ADC_AVG_LEN);
    avg_init(&g_f_press, ADC_AVG_LEN);
    avg_init(&g_f_milk,  ADC_AVG_LEN);

    /* 用首个读数预填窗口，避免启动瞬间均值被 0 拉偏而被误判为传感器短路 */
    raw = board_adc_read(CM_ADC_TEMP);
    avg_prime(&g_f_temp, raw);
    raw = board_adc_read(CM_ADC_PRESS);
    avg_prime(&g_f_press, raw);
    raw = board_adc_read(CM_ADC_MILK);
    avg_prime(&g_f_milk, raw);

    g_autostart_done = 0;
}

void app_trigger_brew(void)
{
    g_autostart_done = 1;
    brew_start(&g_brew, board_millis());
}

void app_step(void)
{
    uint32_t now = board_millis();
    uint16_t raw_t, raw_p, raw_m;       /* 瞬时采样值 */
    uint16_t flt_t, flt_p, flt_m;       /* 滑动平均后的值 */
    int16_t  temp_x10 = 0, milk_x10 = 0;
    uint16_t press_x10 = 0;
    int ok_t, ok_p, ok_m;

    /* 1. 采样：先取瞬时值，再各自滑动平均 */
    raw_t = board_adc_read(CM_ADC_TEMP);
    raw_p = board_adc_read(CM_ADC_PRESS);
    raw_m = board_adc_read(CM_ADC_MILK);

    flt_t = avg_push(&g_f_temp,  raw_t);
    flt_p = avg_push(&g_f_press, raw_p);
    flt_m = avg_push(&g_f_milk,  raw_m);

    /*
     * 故障判定用**瞬时值**，控制用滤波后的值。
     * 原因是滑动平均会把断线后的满量程读数稀释掉：8 点窗口要 8 个周期
     * （4 秒）才能完全跟上，这段过渡期里读数落在"合法但偏高"的区间，
     * 会被误判成过压而不是断线。用瞬时值判定可以一拍就报出来。
     */
    ok_t = (sensor_fault_check(raw_t) == 0) && temp_read_x10(flt_t, &temp_x10);
    ok_p = (sensor_fault_check(raw_p) == 0) && pressure_read(flt_p, &press_x10);
    ok_m = (sensor_fault_check(raw_m) == 0) && milk_read_x10(flt_m, &milk_x10);

    /* 2. 保护优先于流程：先判故障，再决定是否继续冲煮。
          转换失败（断线/短路）时测量值不可信，按故障处理。 */
    protection_check(temp_x10, press_x10, milk_x10, ok_t, ok_p, ok_m);

    /* 3. 流程调度 */
    if (!protection_is_latched()) {
        if (g_brew.state == BREW_FAULT) {
            brew_stop(&g_brew);              /* 故障已排除，回待机 */
        }
        if (!g_autostart_done && g_brew.state == BREW_IDLE) {
            app_trigger_brew();              /* 上电自动执行一次 */
        }
    }

    brew_update(&g_brew, now,
                (float)temp_x10  / 10.0f,
                (float)press_x10 / 10.0f,
                (float)milk_x10  / 10.0f,
                protection_is_latched());

    /* 4. 下发执行器 */
    brew_apply(&g_brew);

    board_delay_ms(CTRL_PERIOD_MS);
}

const char *app_state_name(void)  { return brew_state_name(g_brew.state); }
float       app_water_temp(void)  { return g_brew.temp;  }
float       app_pressure(void)    { return g_brew.press; }
float       app_milk_temp(void)   { return g_brew.milk;  }
int         app_fault(void)       { return (int)protection_fault(); }
