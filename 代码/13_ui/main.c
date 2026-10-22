/**
 ******************************************************************************
 *  502 步骤 13：整机 UI 固件（运行界面 + 系统面板）
 ******************************************************************************
 *  按键：
 *      KEY1 短按：运行界面 ⇄ 系统面板
 *      KEY2 短按：消音 10 秒（报警时）
 *      KEY4 长按：去皮（HX711 接上后有效）
 *
 *  运行界面（默认）：
 *      ┌────────────────┐
 *      │   输液监测      │  第 0-1 页（16×16 中文）
 *      │ T: 34.2 C      │  第 2 页
 *      │ W:  ---- g     │  第 3 页（称重未接显示 ----）
 *      │ D:  --- dpm    │  第 4 页
 *      │ OK / WARN / ALARM │ 第 5 页
 *      │ UP 00:12       │  第 6 页（开机时长）
 *      │ K1:面板         │  第 7 页
 *      └────────────────┘
 *
 *  系统面板（按 KEY1）：
 *      ┌────────────────┐
 *      │   系统面板      │
 *      │ Ver  1.0       │
 *      │ TEMP  OK       │  温度传感器在线状态
 *      │ HX711 --       │  称重：未接显示 --
 *      │ IR    --       │  红外：未接/未计数显示 --
 *      │ NET   --       │  ESP32 是否连上
 *      │ K1:返回         │
 *      └────────────────┘
 *
 *  ★ 传感器没接时不会卡死：HX711 读失败、红外无计数、ESP32 初始化失败，界面都显示 --
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

#define UI_RUN      0
#define UI_PANEL    1

static uint8_t s_ui = UI_RUN;

/* 数据 */
static int16_t  s_temp10 = 0;
static uint8_t  s_temp_ok = 0;
static int32_t  s_raw = 0;
static int16_t  s_grams = 0;
static uint8_t  s_hx_ok = 0;
static uint8_t  s_level = AL_OK;
static uint8_t  s_net_ok = 0;
static uint8_t  s_ir_seen = 0;          /* 上电后是否见过一次滴速边沿 */

/* 时序 */
static uint32_t s_t_conv = 0;
static uint32_t s_t_send = 0;
static uint8_t  s_converting = 0;

/* ==================== 工具 ==================== */
static void fmt_uptime(char *buf, int n)
{
    uint32_t s = millis() / 1000u;
    uint32_t mm = s / 60u;
    uint32_t ss = s % 60u;
    snprintf(buf, n, "UP %02lu:%02lu", (unsigned long)(mm % 100), (unsigned long)ss);
}

/* ==================== 运行界面 ==================== */
static void draw_run(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "输液监测");

    if (s_temp_ok) {
        snprintf(buf, sizeof(buf), "T: %2d.%d C ", s_temp10 / 10, (s_temp10 % 10 + 10) % 10);
    } else {
        snprintf(buf, sizeof(buf), "T:  --.- C ");
    }
    oled_draw_str(0, 2, buf);

    if (s_hx_ok) {
        int32_t g10 = hx711_grams_x10(s_raw);
        snprintf(buf, sizeof(buf), "W:%4ld.%ld g", (long)(g10 / 10), (long)((g10 % 10 + 10) % 10));
    } else {
        snprintf(buf, sizeof(buf), "W:  ---- g");
    }
    oled_draw_str(0, 3, buf);

    if (s_ir_seen) {
        snprintf(buf, sizeof(buf), "D:%4d dpm", (int)(drop_rate_dpm() + 0.5f));
    } else {
        snprintf(buf, sizeof(buf), "D:  --- dpm");
    }
    oled_draw_str(0, 4, buf);

    oled_draw_str(0, 5, s_level == AL_ALARM ? "** ALARM **" :
                        (s_level == AL_WARN  ? "!  WARN  ! " : "   OK     "));

    fmt_uptime(buf, sizeof(buf));
    oled_draw_str(0, 6, buf);

    oled_draw_str(0, 7, "K1:PANEL");
}

/* ==================== 系统面板 ==================== */
static void draw_panel(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "系统面板");

    oled_draw_str(0, 2, "Ver   1.0");

    snprintf(buf, sizeof(buf), "TEMP  %s", s_temp_ok ? "OK" : "NG");
    oled_draw_str(0, 3, buf);

    snprintf(buf, sizeof(buf), "HX711 %s", s_hx_ok ? "OK" : "--");
    oled_draw_str(0, 4, buf);

    snprintf(buf, sizeof(buf), "IR    %s", s_ir_seen ? "OK" : "--");
    oled_draw_str(0, 5, buf);

    snprintf(buf, sizeof(buf), "NET   %s", s_net_ok ? "OK" : "--");
    oled_draw_str(0, 6, buf);

    oled_draw_str(0, 7, "K1:BACK");
}

/* ==================== 按键 ==================== */
static void handle_keys(void)
{
    uint8_t k  = key_any_just_pressed();
    uint8_t e4 = key4_event();

    if (k == 1) {                       /* KEY1：切换 运行/面板 */
        s_ui = (s_ui == UI_RUN) ? UI_PANEL : UI_RUN;
        printf("[UI] -> %s\r\n", s_ui == UI_RUN ? "运行界面" : "系统面板");
        return;
    }

    if (k == 2) {                       /* KEY2：消音 */
        alert502_mute(10000);
        printf("[UI] 消音 10s\r\n");
        return;
    }

    if (e4 == 2) {                      /* KEY4 长按：去皮 */
        if (s_hx_ok) {
            hx711_tare(20);
            printf("[UI] 去皮完成 offset=%ld\r\n", (long)hx711_get_offset());
        } else {
            printf("[UI] 称重未就绪，无法去皮\r\n");
        }
    }
}

/* ==================== 主程序 ==================== */
int main(void)
{
    char frame[64];

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n========================================\r\n");
    printf("  502 整机固件 v1.0（运行界面 + 系统面板）\r\n");
    printf("  KEY1 切换面板  KEY2 消音  KEY4 长按去皮\r\n");
    printf("========================================\r\n");

    /* --- 传感器 --- */
    ds18b20_init();
    hx711_init();
    drop_init();
    alert502_init();
    alert502_defaults();

    /* --- 屏幕 --- */
    i2c_init();
    oled_init();
    oled_clear();
    oled_draw_cn(0, 0, "启动中");
    oled_refresh();

    /* --- 网络（失败不影响本地功能） --- */
    s_net_ok = report_init() ? 1 : 0;
    printf("[NET] %s\r\n", s_net_ok ? "初始化成功" : "未连接（本地功能照常）");

    while (1) {
        /* 1) 高频任务 */
        drop_tick();
        report_poll();

        if (drop_total() > 0) s_ir_seen = 1;

        /* 2) 温度：两段式，避免 800ms 阻塞 */
        if (!s_converting && (millis() - s_t_conv) >= 1000) {
            ds18b20_start();
            s_converting = 1;
        }
        if (s_converting && (millis() - s_t_conv) >= 800) {
            s_converting = 0;
            s_t_conv = millis();

            s_temp10 = ds18b20_read();
            s_temp_ok = (s_temp10 != DS18B20_ERR && s_temp10 != 850) ? 1 : 0;

            s_raw = hx711_read_raw();
            s_hx_ok = (s_raw != HX711_ERR) ? 1 : 0;
            if (s_hx_ok) s_grams = (int16_t)hx711_grams(s_raw);

            s_level = alert502_update(s_temp10, s_temp_ok, s_grams, s_hx_ok);
        }

        /* 3) 按键 */
        handle_keys();

        /* 4) 屏幕：重绘写缓冲，按页推送（避免整屏 374ms 阻塞） */
        {
            static uint32_t t_draw = 0, t_page = 0;
            static uint8_t  page = 0;

            if ((millis() - t_draw) >= 200) {
                t_draw = millis();
                if (s_ui == UI_RUN) draw_run();
                else                draw_panel();
            }
            if ((millis() - t_page) >= 40) {
                t_page = millis();
                oled_refresh_page(page);
                page = (uint8_t)((page + 1) & 7);
            }
        }

        /* 5) 上报：每 2 秒（有客户端才发） */
        if ((millis() - s_t_send) >= 2000) {
            s_t_send = millis();
            snprintf(frame, sizeof(frame), "T=%d.%d,W=%d,D=%d,L=%d\r\n",
                     s_temp10 / 10, s_temp10 % 10, s_grams,
                     (int)(drop_rate_dpm() + 0.5f), s_level);
            report_send(frame);
        }

        delay_ms(5);
    }
}
