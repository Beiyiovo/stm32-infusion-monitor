/**
 ******************************************************************************
 *  alert502.h —— 502 分级告警（黄=预警 / 红=报警）
 ******************************************************************************
 *  判定规则（全部可在 alert_cfg_t 里改）：
 *
 *      红色报警 AL_ALARM
 *          · 温度 ≥ 42.0℃          （过热）
 *          · 重量 < 50 g            （药液将尽）
 *          · 滴速 ≥ 120 滴/分       （流速异常过快）
 *
 *      黄色预警 AL_WARN
 *          · 温度 41.5 ~ 42.0℃      （偏高）
 *          · 温度 < 32.0℃           （过低，药液/环境过冷）
 *          · 重量 50 ~ 100 g        （偏少）
 *
 *  输出行为：
 *      AL_OK     ：红灭、黄灭、蜂鸣器静音
 *      AL_WARN   ：黄灯 1Hz 慢闪，蜂鸣器不响（只做视觉提示）
 *      AL_ALARM  ：红灯 2Hz 闪 + 蜂鸣器【3 声短促哔后自停】，红灯继续闪
 *      滴速过快   ：红灯 4Hz 快闪（区别于普通报警的 2Hz）
 *
 *  ⚠️ 阈值是"设计值"，不是医学标准；答辩时按课题要求说明来源。
 ******************************************************************************
 */
#ifndef __ALERT502_H
#define __ALERT502_H

#include "bsp.h"

#define AL_OK       0       /* 正常 */
#define AL_WARN     1       /* 黄色预警 */
#define AL_ALARM    2       /* 红色报警 */

/* --- 报警原因位掩码（可同时多个） --- */
#define R_TEMP_HIGH     0x01    /* 温度过高 */
#define R_TEMP_LOW      0x02    /* 温度过低 */
#define R_WEIGHT_LOW    0x04    /* 药液将尽 */
#define R_RATE_FAST     0x08    /* 滴速过快 */

/* 默认阈值 */
#define TH_TEMP_WARN    415     /* ×10 → 41.5℃ */
#define TH_TEMP_ALARM   420     /* ×10 → 42.0℃ */
#define TH_TEMP_LOW     320     /* ×10 → 32.0℃ */
#define TH_W_WEIGHT     100     /* 克：低于此值预警 */
#define TH_A_WEIGHT     50      /* 克：低于此值报警 */
#define TH_RATE_HIGH    120     /* 滴/分：高于此值报警 */

/* ★ 2026-10-27：报警蜂鸣改为「3 声短促哔」（原来持续响 3 秒，用户反馈太吵） */
#define AL_BEEP_ON_MS   120     /* 每声长度 ms */
#define AL_BEEP_OFF_MS  120     /* 声与声之间的间隔 ms */
#define AL_BEEP_COUNT   3       /* 声数（想改 4 声改这里） */

typedef struct {
    int16_t temp_warn, temp_alarm;   /* ×10 */
    int16_t temp_low;                /* ×10 */
    int16_t w_warn,   w_alarm;       /* 克 */
    int16_t rate_high;               /* 滴/分 */
} alert_cfg_t;

void     alert502_init(void);
void     alert502_defaults(void);
alert_cfg_t *alert502_cfg(void);

/**
 * 周期调用（建议 50~100ms）
 *   temp10  温度×10（无效时 t_valid=0）
 *   grams   重量（克，无效时 w_valid=0）
 *   dpm     滴速（滴/分，无效时 d_valid=0）
 * 返回当前等级 AL_OK / AL_WARN / AL_ALARM
 */
uint8_t alert502_update(int16_t temp10, uint8_t t_valid,
                        int16_t grams,  uint8_t w_valid,
                        uint16_t dpm,   uint8_t d_valid);

uint8_t alert502_level(void);
uint8_t alert502_reason(void);          /* 位掩码 R_xxx */
void    alert502_mute(uint32_t ms);     /* 消音：只关蜂鸣器，灯照常 */
uint8_t alert502_is_muted(void);
void    alert502_clear(void);           /* 复位到正常（换袋后调用） */

#endif /* __ALERT502_H */
