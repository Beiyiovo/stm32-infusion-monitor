/**
 ******************************************************************************
 *  hx711.h —— 24 位称重 ADC 驱动
 ******************************************************************************
 *  接线：VCC→3V3  GND→GND  SCK→PB10  DT(DOUT)→PB11
 *  时序（HX711 datasheet）：
 *      · DOUT 为低 = 数据就绪
 *      · 给 24 个 SCK 脉冲，依次移出 24 位（MSB 先出，补码）
 *      · 第 25 个脉冲决定下一次转换的通道与增益：
 *            25 个脉冲 → 通道 A 增益 128（本项目用这个）
 *            26 个脉冲 → 通道 B 增益 32
 *            27 个脉冲 → 通道 A 增益 64
 ******************************************************************************
 */
#ifndef __HX711_H
#define __HX711_H

#include "bsp.h"
#include "app502.h"

#define HX711_ERR           0x7FFFFFFFL     /* 读失败的返回值 */
/* ★ 2026-10-24 实测标定（500 ml 水 ≈ 500 g，11_calib 跑两次）：
 *      ZERO  = 764832 / 764883
 *      SCALE = 801.990 与 801.920 counts/g（两次相差 0.07，即 0.009%）
 *   取中间值 802.0。换传感器或改挂具结构后必须重新标定。 */
#define HX711_DEFAULT_SCALE 802.0f           /* counts/克（实测标定值） */

void    hx711_init(void);
uint8_t hx711_ready(void);                  /* 1 = 有数据可读 */
int32_t hx711_read_raw(void);               /* 读原始值；超时返回 HX711_ERR */

int32_t hx711_tare(uint16_t samples);       /* 去皮：采 N 次求平均作为零点，返回零点值 */
void    hx711_set_scale(float g_per_count); /* 设置换算系数 */
float   hx711_get_scale(void);
float   hx711_grams(int32_t raw);           /* 原始值 → 克（已减零点） */
int32_t hx711_grams_x10(int32_t raw);       /* 克 ×10 的四舍五入整数（打印用，避免浮点 printf） */
int32_t hx711_get_offset(void);

#endif /* __HX711_H */
