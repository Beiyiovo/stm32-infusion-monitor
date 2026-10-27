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
/* ★ 2026-10-27 实测标定（标准砝码组 5/10/20/20/50 g，11_calib 三次复核）：
 *      ZERO  = 765410（空秤实测；整机开机还会自动去皮重采）
 *      SCALE = 742.35 counts/g（50g/100g 线性度 0.14%，砝码复核最大偏差 +0.1 g）
 *   ⚠ 换传感器、重装秤架、打板装盒后该值一定变化——
 *     10-24 用水初标是 802.0，秤架重装后实测 742.35（差 8%）就是实证。装盒后必须重标。 */
#define HX711_DEFAULT_SCALE 742.35f          /* counts/克（2026-10-27 砝码实测标定值） */

void    hx711_init(void);
uint8_t hx711_ready(void);                  /* 1 = 有数据可读 */
int32_t hx711_read_raw(void);               /* 读原始值；超时返回 HX711_ERR */

int32_t hx711_tare(uint16_t samples);       /* 去皮：采 N 次求平均作为零点，返回零点值 */
void    hx711_set_offset(int32_t offset);   /* ★ 直接写入零点（11_calib 标定完喂给驱动用） */
void    hx711_set_scale(float g_per_count); /* 设置换算系数 */
float   hx711_get_scale(void);
float   hx711_grams(int32_t raw);           /* 原始值 → 克（已减零点） */
int32_t hx711_grams_x10(int32_t raw);       /* 克 ×10 的四舍五入整数（打印用，避免浮点 printf） */
int32_t hx711_get_offset(void);

#endif /* __HX711_H */
