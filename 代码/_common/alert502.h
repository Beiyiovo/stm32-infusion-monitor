/**
 ******************************************************************************
 *  alert502.h —— 502 两级告警（黄=预警 / 红=超限）
 ******************************************************************************
 *  课题要求：超过阈值时蜂鸣器报警 + LED 闪烁；任务书另要求"黄/红两级分级报警"。
 *
 *  默认阈值（可在 main 里改，也可由按键改完再传进来）：
 *      温度：> 41.5℃ 黄色预警      > 42.0℃ 红色报警
 *      重量：< 100g  黄色预警      < 50g   红色报警
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

/* 默认阈值 */
#define TH_TEMP_WARN    415     /* ×10 → 41.5℃ */
#define TH_TEMP_ALARM   420     /* ×10 → 42.0℃ */
#define TH_W_WEIGHT     100     /* 克：低于此值预警 */
#define TH_A_WEIGHT     50      /* 克：低于此值报警 */

typedef struct {
    int16_t temp_warn, temp_alarm;   /* ×10 */
    int16_t w_warn,   w_alarm;       /* 克 */
} alert_cfg_t;

void     alert502_init(void);
void     alert502_defaults(void);
alert_cfg_t *alert502_cfg(void);

/**
 * 周期调用（建议 50~100ms）
 *   temp10 温度×10（无效传 0 且 t_valid=0）
 *   grams  重量（克）
 * 返回当前等级 AL_OK / AL_WARN / AL_ALARM
 */
uint8_t alert502_update(int16_t temp10, uint8_t t_valid, int16_t grams, uint8_t w_valid);

uint8_t alert502_level(void);
void    alert502_mute(uint32_t ms);     /* 消音：只关蜂鸣器，红灯继续闪 */
uint8_t alert502_is_muted(void);

#endif /* __ALERT502_H */
