/**
 ******************************************************************************
 *  app502.h —— 502 项目新增硬件（相对项目1 的增量）
 ******************************************************************************
 *  项目1 已有的（bsp.h）：DS18B20 PA0 / 蜂鸣器 PB0 / 红LED PB1 / 三键 PA1 PA4 PA5
 *                        I2C PB6 PB7 / USART1 PA9 PA10 / USART2 PA2 PA3 / PC13
 *
 *  本项目【新增】的引脚都集中在这里：
 *      PB5   黄色 LED（预警）
 *      PA6   KEY4 确认 / 长按去皮
 *      PB10  HX711 SCK
 *      PB11  HX711 DOUT
 *      PB12  红外对射 OUT（外部中断数滴）
 ******************************************************************************
 */
#ifndef __APP502_H
#define __APP502_H

#include "bsp.h"

/* ==================== 新增引脚 ==================== */
#define LED_Y_GPIO          GPIOB
#define LED_Y_PIN           GPIO_Pin_5      /* PB5  黄灯，高电平亮 */

#define KEY4_GPIO           GPIOA
#define KEY4_PIN            GPIO_Pin_6      /* PA6  KEY4，另一端接 GND */

#define HX_SCK_GPIO         GPIOB
#define HX_SCK_PIN          GPIO_Pin_10     /* PB10 HX711 SCK */
#define HX_DT_GPIO          GPIOB
#define HX_DT_PIN           GPIO_Pin_11     /* PB11 HX711 DOUT */

#define IR_GPIO             GPIOB
#define IR_PIN              GPIO_Pin_12     /* PB12 红外对射输出 */

/* ==================== 接口 ==================== */
void app502_init(void);          /* 黄灯 + KEY4 + HX711 + 红外 的 GPIO 初始化 */
void led_yellow(int on);         /* 1 = 亮 */
uint8_t key4_pressed(void);      /* 1 = 当前被按下（已消抖） */
uint8_t key4_event(void);        /* 0=无 1=短按 2=长按（调用后清除） */

#endif /* __APP502_H */
