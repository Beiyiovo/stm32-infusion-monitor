/**
 ******************************************************************************
 *  502 步骤 8：WiFi + TCP 上报（ESP32-C3 AT 固件）
 ******************************************************************************
 *  链路：STM32 ──USART2(PA2/PA3)──> ESP32-C3（AP: 502-Infusion）──> 手机 App
 *
 *  接线（只 3 根）：PA2 → ESP32 GPIO6(RX)；PA3 ← GPIO7(TX)；GND ↔ GND
 *                  ESP32 自己 Type-C 供电，★ 不要接主板 3V3
 *
 *  验收：
 *      1) 串口依次出现 AT / CWMODE / CWSAP / CIPMUX / CIPSERVER 全部成功
 *      2) 手机连上 "502-Infusion"（密码 12345678）
 *      3) 用 TCP 调试 App 连 192.168.4.1:8080，应每 2 秒收到一行数据：
 *         T=36.5,W=480,D=60,L=0
 *      4) 拔掉 App 再连，能自动恢复推送
 *
 *  ⚠️ 本段尚未在硬件上跑过；AT 序列沿用项目1 已验证的 AP+TCP Server 流程
 ******************************************************************************
 */
#include "bsp.h"
#include "i2c.h"
#include "oled.h"
#include "app502.h"
#include "ds18b20.h"
#include "hx711.h"
#include "drop.h"
#include "alert502.h"
#include "report_tcp.h"

int main(void)
{
    int16_t  temp10 = 0;
    int32_t  raw    = 0;
    int16_t  grams  = 0;
    uint8_t  lv     = AL_OK;
    uint32_t t_read = 0, t_send = 0, t_conv = 0;
    uint8_t  converting = 0;
    char     frame[64];

    bsp_init();
    app502_init();
    uart1_init();
    printf("\r\n[BOOT] 502 步骤8 · WiFi + TCP 上报\r\n");

    /* --- 传感器 --- */
    ds18b20_init();
    (void)ds18b20_get();                /* 丢弃首次 85℃ */
    hx711_init();
    hx711_tare(10);
    drop_init();
    alert502_init();
    alert502_defaults();

    /* --- 屏幕 --- */
    i2c_init();
    oled_init();
    oled_clear();
    oled_draw_cn(0, 0, "正在联网");
    oled_refresh();

    /* --- ESP32 --- */
    if (report_init()) {
        oled_draw_cn(0, 0, "联网成功");
        oled_refresh();
    } else {
        oled_draw_cn(0, 0, "联网失败");
        oled_refresh();
        printf("[NET] 初始化失败，检查 PA2/PA3 交叉与共地\r\n");
    }

    while (1) {
        /* 1) 滴速结算 + 网络收包 */
        drop_tick();
        report_poll();

        /* 2) 温度：非阻塞（★ ds18b20_get() 会阻塞约 800ms，期间滴速中断与网络收包都会漏）
         *    做法：先启动转换 → 800ms 后再读 → 这 800ms 里主循环照常跑 */
        if (!converting && (millis() - t_read) >= 1000) {
            ds18b20_start();
            converting = 1;
            t_conv     = millis();
        }
        if (converting && (millis() - t_conv) >= 800) {
            converting = 0;
            t_read     = millis();
            temp10     = ds18b20_read();

            raw   = hx711_read_raw();
            grams = (int16_t)hx711_grams(raw);
            lv    = alert502_update(temp10, temp10 != DS18B20_ERR, grams, raw != HX711_ERR);

            printf("[RUN] T=%d.%d W=%d D=%.0f 等级=%u\r\n",
                   temp10 / 10, temp10 % 10, grams, drop_rate_dpm(), lv);
        }

        /* 3) 每 2 秒推一帧给 App（有客户端才发） */
        if ((millis() - t_send) >= 2000) {
            t_send = millis();
            snprintf(frame, sizeof(frame), "T=%d.%d,W=%d,D=%.0f,L=%d\r\n",
                     temp10 / 10, temp10 % 10, grams, drop_rate_dpm(), lv);
            if (report_send(frame)) {
                printf("[NET] 已发送: %s", frame);
            }
        }

        /* 4) KEY4 长按 = 去皮 */
        if (key4_event() == 2) {
            hx711_tare(20);
            printf("[HX] 已去皮\r\n");
        }

        delay_ms(10);
    }
}
