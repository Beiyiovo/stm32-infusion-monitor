/**
 ******************************************************************************
 *  alert502.c —— 两级告警实现
 ******************************************************************************
 *  行为：
 *      AL_OK     ：蜂鸣器关、黄灯灭、红灯灭
 *      AL_WARN   ：黄灯 500ms 闪、蜂鸣器不响（视觉预警）
 *      AL_ALARM  ：红灯 200ms 闪 + 蜂鸣器响（可消音，消音后红灯仍闪）
 ******************************************************************************
 */
#include "alert502.h"
#include "app502.h"      /* ★ led_yellow() 在这里声明（项目1 没有黄灯） */

static alert_cfg_t s_cfg = { TH_TEMP_WARN, TH_TEMP_ALARM, TH_W_WEIGHT, TH_A_WEIGHT };
static uint8_t  s_level = AL_OK;
static uint32_t s_mute_until = 0;
static uint32_t s_blink_t = 0;
static uint8_t  s_blink_on = 0;

void alert502_init(void)
{
    /* 蜂鸣器 / 红灯由 bsp_init() 初始化，这里只确保初始状态一致 */
    buzzer(0);
    led_red(0);
    led_yellow(0);
}

void alert502_defaults(void)
{
    s_cfg.temp_warn  = TH_TEMP_WARN;
    s_cfg.temp_alarm = TH_TEMP_ALARM;
    s_cfg.w_warn     = TH_W_WEIGHT;
    s_cfg.w_alarm    = TH_A_WEIGHT;
}

alert_cfg_t *alert502_cfg(void) { return &s_cfg; }

void alert502_mute(uint32_t ms)
{
    s_mute_until = millis() + ms;
}

uint8_t alert502_is_muted(void)
{
    return (millis() < s_mute_until) ? 1 : 0;
}

static void blink_update(uint32_t period, uint8_t *on)
{
    uint32_t now = millis();
    if ((now - s_blink_t) >= period) {
        s_blink_t = now;
        *on = *on ? 0 : 1;
    }
}

uint8_t alert502_update(int16_t temp10, uint8_t t_valid, int16_t grams, uint8_t w_valid)
{
    uint8_t lv = AL_OK;

    /* --- 判温度 --- */
    if (t_valid) {
        if (temp10 >= s_cfg.temp_alarm)      lv = AL_ALARM;
        else if (temp10 > s_cfg.temp_warn)   { if (lv < AL_WARN) lv = AL_WARN; }
    }

    /* --- 判重量（越低越危险） --- */
    if (w_valid) {
        if (grams < s_cfg.w_alarm)           lv = AL_ALARM;
        else if (grams < s_cfg.w_warn)       { if (lv < AL_WARN) lv = AL_WARN; }
    }

    s_level = lv;

    /* --- 执行输出 --- */
    switch (lv) {
    case AL_ALARM:
        led_yellow(0);
        blink_update(200, &s_blink_on);
        led_red(s_blink_on);
        buzzer(alert502_is_muted() ? 0 : 1);
        break;

    case AL_WARN:
        led_red(0);
        blink_update(500, &s_blink_on);
        led_yellow(s_blink_on);
        buzzer(0);
        break;

    default:
        s_blink_on = 0;
        led_red(0);
        led_yellow(0);
        buzzer(0);
        break;
    }

    return lv;
}

uint8_t alert502_level(void) { return s_level; }
