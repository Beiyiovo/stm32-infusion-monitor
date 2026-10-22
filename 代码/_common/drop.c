/**
 ******************************************************************************
 *  drop.c —— 滴速检测实现
 ******************************************************************************
 *  ★ 计数放在中断里，但只做"加一"这种最短操作
 *  ★ 滴速换算放在主循环（drop_tick），每 10 秒结算一次
 ******************************************************************************
 */
#include "drop.h"
#include "stm32f10x_exti.h"   /* EXTI_InitTypeDef / EXTI_Line12（conf 里没默认包含） */

static volatile uint16_t s_win_cnt   = 0;   /* 本窗口计数 */
static volatile uint32_t s_last_edge = 0;   /* 上次中断时刻（防抖） */
static volatile uint32_t s_total     = 0;   /* 累计滴数 */

static uint16_t s_win_drops  = 0;           /* 上一完整窗口的滴数 */
static float    s_rate       = 0.0f;        /* 滴/分 */
static uint32_t s_win_start  = 0;

/* ==================== 初始化 ==================== */
void drop_init(void)
{
    GPIO_InitTypeDef gpio;
    EXTI_InitTypeDef exti;
    NVIC_InitTypeDef nvic;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    /* PB12 上拉输入 */
    gpio.GPIO_Pin  = IR_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(IR_GPIO, &gpio);

    /* PB12 → EXTI12，下降沿触发（液滴遮光） */
    GPIO_EXTILineConfig(GPIO_PortSourceGPIOB, GPIO_PinSource12);

    exti.EXTI_Line    = EXTI_Line12;
    exti.EXTI_Mode    = EXTI_Mode_Interrupt;
    exti.EXTI_Trigger = EXTI_Trigger_Falling;
    exti.EXTI_LineCmd = ENABLE;
    EXTI_Init(&exti);

    nvic.NVIC_IRQChannel                   = EXTI15_10_IRQn;
    nvic.NVIC_IRQChannelPreemptionPriority = 1;
    nvic.NVIC_IRQChannelSubPriority        = 0;
    nvic.NVIC_IRQChannelCmd                = ENABLE;
    NVIC_Init(&nvic);

    s_win_start = millis();
}

/* ==================== 中断：数滴 ==================== */
void EXTI15_10_IRQHandler(void)
{
    if (EXTI_GetITStatus(EXTI_Line12) != RESET) {
        uint32_t now = millis();

        /* 防抖：两滴间隔小于 30ms 的当作干扰（相当于 >2000 滴/分） */
        if ((now - s_last_edge) >= DROP_DEBOUNCE_MS) {
            s_win_cnt++;
            s_total++;
            s_last_edge = now;
        }
        EXTI_ClearITPendingBit(EXTI_Line12);
    }
}

/* ==================== 主循环结算 ==================== */
void drop_tick(void)
{
    uint32_t now = millis();

    if ((now - s_win_start) >= DROP_WINDOW_MS) {
        uint16_t cnt;

        /* 关中断读，避免读到一半被改 */
        __disable_irq();
        cnt = s_win_cnt;
        s_win_cnt = 0;
        __enable_irq();

        s_win_drops = cnt;
        /* 滴/分 = 窗口滴数 × (60000 / 窗口毫秒) */
        s_rate = (float)cnt * (60000.0f / (float)DROP_WINDOW_MS);
        s_win_start = now;
    }
}

void drop_reset(void)
{
    __disable_irq();
    s_win_cnt = 0;
    s_total   = 0;
    __enable_irq();
    s_win_drops = 0;
    s_rate      = 0.0f;
    s_win_start = millis();
}

uint16_t drop_window_drops(void) { return s_win_drops; }
float    drop_rate_dpm(void)     { return s_rate; }
uint32_t drop_total(void)        { return s_total; }
