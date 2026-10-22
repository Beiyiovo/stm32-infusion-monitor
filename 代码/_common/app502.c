/**
 ******************************************************************************
 *  app502.c —— 502 新增硬件的驱动实现（黄灯 / KEY4）
 ******************************************************************************
 *  ★ 只做项目1 没有的部分；已有功能仍走 bsp.c，避免改动已验证代码
 ******************************************************************************
 */
#include "app502.h"

/* ==================== KEY4 消抖状态 ==================== */
static uint8_t  s_key4_raw    = 1;      /* 上一次采到的原始电平（1=松开） */
static uint8_t  s_key4_stable = 1;      /* 消抖后的稳定电平 */
static uint32_t s_key4_t      = 0;      /* 电平变化时刻 */
static uint32_t s_key4_down_t = 0;      /* 按下时刻 */
static uint8_t  s_key4_event  = 0;

#define KEY_DEBOUNCE_MS     20
#define KEY_LONG_MS         800

/* ==================== 初始化 ==================== */
void app502_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB, ENABLE);

    /* --- PB5 黄灯：推挽输出，默认灭 --- */
    gpio.GPIO_Pin   = LED_Y_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_Y_GPIO, &gpio);
    GPIO_ResetBits(LED_Y_GPIO, LED_Y_PIN);

    /* --- PA6 KEY4：上拉输入（按下=低） --- */
    gpio.GPIO_Pin  = KEY4_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(KEY4_GPIO, &gpio);

    /* --- HX711 / 红外：引脚方向在各自模块里初始化，这里只开时钟 --- */
}

/* ==================== 黄灯 ==================== */
void led_yellow(int on)
{
    if (on) GPIO_SetBits(LED_Y_GPIO, LED_Y_PIN);
    else    GPIO_ResetBits(LED_Y_GPIO, LED_Y_PIN);
}

/* ==================== KEY4 扫描（主循环每 5~10ms 调一次） ==================== */
static void key4_scan(void)
{
    uint8_t  now = GPIO_ReadInputDataBit(KEY4_GPIO, KEY4_PIN) ? 1 : 0;
    uint32_t t   = millis();

    if (now != s_key4_raw) {          /* 原始电平变了，重新计时 */
        s_key4_raw = now;
        s_key4_t   = t;
        return;
    }

    /* 稳定超过消抖时间才算真变化 */
    if ((t - s_key4_t) >= KEY_DEBOUNCE_MS && s_key4_stable != now) {
        s_key4_stable = now;
        if (now == 0) {               /* 按下 */
            s_key4_down_t = t;
        } else {                      /* 松开 → 判定短按/长按 */
            uint32_t held = t - s_key4_down_t;
            if (held >= KEY_LONG_MS)      s_key4_event = 2;
            else if (held >= KEY_DEBOUNCE_MS) s_key4_event = 1;
        }
    }
}

uint8_t key4_pressed(void)
{
    key4_scan();
    return (s_key4_stable == 0) ? 1 : 0;
}

uint8_t key4_event(void)
{
    uint8_t e;

    /* ★★ 2026-10-22 修：原来这里没调用 key4_scan()，
     *    而 key4_scan() 只会被 key4_pressed() 调用。
     *    主程序如果只调 key4_event()（13_ui / 09_main / 04_key 都是这样），
     *    扫描就永远不会跑 → s_key4_event 永远是 0 → K4 完全没有反应。 */
    key4_scan();

    e = s_key4_event;
    s_key4_event = 0;
    return e;
}
