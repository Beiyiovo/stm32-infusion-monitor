/**
 ******************************************************************************
 *  report_tcp.h —— 数据上报层（局域网 TCP 版）
 ******************************************************************************
 *  链路：STM32 ──USART2(AT)──> ESP32-C3（AP + TCP Server:8080）──> 手机 App
 *
 *  ★ 这一层是"可替换"的：以后要上云（MQTT），只换 report_mqtt.c，
 *    main.c 里调用的仍是 report_send()，别的都不用动。
 *
 *  ⚠️ 本层【尚未在硬件上验证】，AT 序列按项目1 已验证的 AP+TCP Server 流程编写，
 *     首次上板时按《502可行性验证方案》阶段 7 逐步验。
 ******************************************************************************
 */
#ifndef __REPORT_TCP_H
#define __REPORT_TCP_H

#include "bsp.h"

#define REPORT_SSID     "502-Infusion"      /* ESP32 热点名 */
#define REPORT_PASS     "12345678"          /* 热点密码（≥8 位） */
#define REPORT_PORT     8080                /* TCP 服务端口 */

/**
 * 初始化 ESP32：AT → CWMODE=2 → CWSAP → CIPMUX=1 → CIPSERVER=1,8080
 * 返回 1 = 全部 OK；0 = 中途失败（串口会打印是哪一条失败）
 */
uint8_t report_init(void);

/** 主循环里周期调用：解析 "+IPD,<id>,<len>:" 与 "<id>,CONNECT"，维护当前连接 */
void report_poll(void);

/** 有客户端连着返回 1 */
uint8_t report_connected(void);

/** 向当前客户端发一帧；无连接返回 0 */
uint8_t report_send(const char *payload);

#endif /* __REPORT_TCP_H */
