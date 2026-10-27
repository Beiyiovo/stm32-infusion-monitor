/**
 ******************************************************************************
 *  alert502.c —— 分级告警实现
 ******************************************************************************
 *  输出优先级：红色 > 黄色 > 正常
 *  蜂鸣器：进入报警时响 3 声短促哔后自停（2026-10-27 由 3 秒长响改短促）；
 *          消音期间不响；解除后重新武装
 ******************************************************************************
 */
#include "alert502.h"
#include "app502.h"      /* led_yellow() */

static alert_cfg_t s_cfg = {
    TH_TEMP_WARN, TH_TEMP_ALARM, TH_TEMP_LOW,
    TH_W_WEIGHT, TH_A_WEIGHT, TH_RATE_HIGH,
    TH_W_HIGH, TH_RATE_LOW
};

static uint8_t  s_level  = AL_OK;
static uint8_t  s_reason = 0;
static uint8_t  s_was_alarm = 0;        /* 上一轮是否处于报警（用于"重新武装"蜂鸣器） */

static uint32_t s_mute_until = 0;
static uint32_t s_blink_t    = 0;
static uint8_t  s_blink_on   = 0;

/* ★ 2026-10-27：报警蜂鸣改为「N 声短促哔」状态机（原来是持续响 3 秒，太吵） */
static uint8_t  s_beep_left  = 0;       /* 剩余待响声数 */
static uint8_t  s_beep_on    = 0;       /* 当前这一声是响还是停 */
static uint32_t s_beep_t     = 0;       /* 当前状态的起始时刻 */

/* ==================== 初始化 ==================== */
void alert502_init(void)
{
    buzzer(0);
    led_red(0);
    led_yellow(0);
}

void alert502_defaults(void)
{
    s_cfg.temp_warn  = TH_TEMP_WARN;
    s_cfg.temp_alarm = TH_TEMP_ALARM;
    s_cfg.temp_low   = TH_TEMP_LOW;
    s_cfg.w_warn     = TH_W_WEIGHT;
    s_cfg.w_alarm    = TH_A_WEIGHT;
    s_cfg.rate_high  = TH_RATE_HIGH;
    s_cfg.w_high     = TH_W_HIGH;
    s_cfg.rate_low   = TH_RATE_LOW;
}

alert_cfg_t *alert502_cfg(void) { return &s_cfg; }

void alert502_mute(uint32_t ms)
{
    s_mute_until = millis() + ms;
    s_beep_left  = 0;                   /* 消音同时掐掉还没响完的蜂鸣 */
    s_beep_on    = 0;
    buzzer(0);
}

uint8_t alert502_is_muted(void)
{
    return (millis() < s_mute_until) ? 1 : 0;
}

void alert502_clear(void)
{
    s_was_alarm = 0;
    s_beep_left = 0;
    s_beep_on   = 0;
    s_level = AL_OK;
    s_reason = 0;
    buzzer(0);
    led_red(0);
    led_yellow(0);
}

/* ==================== 闪烁 ==================== */
static void blink_update(uint32_t period, uint8_t *on)
{
    uint32_t now = millis();
    if ((now - s_blink_t) >= period) {
        s_blink_t = now;
        *on = *on ? 0 : 1;
    }
}

/* ==================== 主判定 ==================== */
uint8_t alert502_update(int16_t temp10, uint8_t t_valid,
                        int16_t grams,  uint8_t w_valid,
                        uint16_t dpm,   uint8_t d_valid)
{
    uint8_t lv = AL_OK;
    uint8_t rs = 0;

    /* --- 温度 --- */
    if (t_valid) {
        if (temp10 >= s_cfg.temp_alarm) {
            lv = AL_ALARM;  rs |= R_TEMP_HIGH;
        } else if (temp10 > s_cfg.temp_warn) {
            if (lv < AL_WARN) lv = AL_WARN;
            rs |= R_TEMP_HIGH;
        } else if (temp10 < s_cfg.temp_low) {
            if (lv < AL_WARN) lv = AL_WARN;
            rs |= R_TEMP_LOW;
        }
    }

    /* --- 重量（越低越危险） --- */
    if (w_valid) {
        if (grams < s_cfg.w_alarm) {
            lv = AL_ALARM;  rs |= R_WEIGHT_LOW;
        } else if (grams < s_cfg.w_warn) {
            if (lv < AL_WARN) lv = AL_WARN;
            rs |= R_WEIGHT_LOW;
        }
    }

    /* --- 滴速过快 --- */
    if (d_valid && dpm >= (uint16_t)s_cfg.rate_high) {
        lv = AL_ALARM;
        rs |= R_RATE_FAST;
    }

    /* --- ★ 进阶预警（2026-10-27）：只黄色提示，不红不响，与常规报警不冲突 ---
     *     重量上限 / 滴速下限默认 0 = 关闭；温度下限原本就有（默认 32.0℃） */
    if (w_valid && s_cfg.w_high > 0 && grams > s_cfg.w_high) {
        if (lv < AL_WARN) lv = AL_WARN;
        rs |= R_WEIGHT_HIGH;
    }
    if (d_valid && s_cfg.rate_low > 0 && dpm < (uint16_t)s_cfg.rate_low) {
        if (lv < AL_WARN) lv = AL_WARN;
        rs |= R_RATE_SLOW;
    }

    /* --- 蜂鸣器"重新武装"：从正常/预警 进入 报警 的那一刻开始计时 --- */
    if (lv == AL_ALARM && !s_was_alarm) {
        if (!alert502_is_muted()) {
            s_beep_left = AL_BEEP_COUNT;    /* 重新武装：3 声短促哔 */
            s_beep_on   = 1;
            s_beep_t    = millis();
        }
    }
    s_was_alarm = (lv == AL_ALARM) ? 1 : 0;

    s_level  = lv;
    s_reason = rs;

    /* ==================== 执行输出 ==================== */
    switch (lv) {
    case AL_ALARM:
        led_yellow(0);

        /* 滴速过快用 4Hz 快闪，与普通报警的 2Hz 区分开 */
        blink_update((rs & R_RATE_FAST) ? 125 : 250, &s_blink_on);
        led_red(s_blink_on);

        /* 蜂鸣器：3 声短促哔，响完自停 */
        if (s_beep_left == 0 || alert502_is_muted()) {
            buzzer(0);
        } else if (s_beep_on) {
            buzzer(1);
            if ((millis() - s_beep_t) >= AL_BEEP_ON_MS) {
                s_beep_left--;
                s_beep_on = 0;
                s_beep_t  = millis();
            }
        } else {
            buzzer(0);
            if ((millis() - s_beep_t) >= AL_BEEP_OFF_MS) {
                s_beep_on = 1;
                s_beep_t  = millis();
            }
        }
        break;

    case AL_WARN:
        led_red(0);
        blink_update(500, &s_blink_on);     /* 黄灯 1Hz 慢闪 */
        led_yellow(s_blink_on);
        buzzer(0);
        break;

    default:
        s_blink_on   = 0;
        s_beep_left  = 0;
        s_beep_on    = 0;
        led_red(0);
        led_yellow(0);
        buzzer(0);
        break;
    }

    return lv;
}

uint8_t alert502_level(void)  { return s_level;  }
uint8_t alert502_reason(void) { return s_reason; }
