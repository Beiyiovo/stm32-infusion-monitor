/**
 ******************************************************************************
 *  502 步骤 9：整机固件（传感器 + 两级告警 + 阈值设置 + WiFi 上报）
 ******************************************************************************
 *  按键：
 *      KEY1 短按：循环切换 运行 → 设温度阈值 → 设重量阈值 → 运行
 *      KEY2 / KEY3：在设置界面里 加 / 减
 *      KEY4 短按：设置界面里确认并进入下一项
 *      KEY4 长按：去皮（运行界面）
 *      KEY2 短按（运行界面）：消音 10 秒
 *
 *  屏幕：
 *      ┌──────────────┐
 *      │ 输液监测      │  第 1-2 页（中文）
 *      │ T:36.5C      │  第 3 页
 *      │ W: 480g      │  第 4 页
 *      │ D:  60dpm    │  第 5 页
 *      │ OK / WARN / ALARM │ 第 6 页
 *      └──────────────┘
 *
 *  ★ 温度采集用非阻塞状态机：ds18b20_get() 会阻塞约 800ms，
 *    那段时间会漏掉滴速中断与网络收包，所以改成 start/read 两段式。
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

/* ==================== UI 状态 ==================== */
#define UI_RUN          0
#define UI_SET_TEMP     1
#define UI_SET_WEIGHT   2

static uint8_t s_ui = UI_RUN;

/* ==================== 数据 ==================== */
static int16_t  s_temp10 = 0;
static int16_t  s_grams  = 0;
static int32_t  s_raw    = 0;
static uint8_t  s_level  = AL_OK;

static uint32_t s_t_conv   = 0;
static uint32_t s_t_send   = 0;
static uint32_t s_t_print  = 0;
static uint8_t  s_converting = 0;

/* ==================== 显示 ==================== */
static void ui_draw_run_norefresh(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "输液监测");

    snprintf(buf, sizeof(buf), "T:%d.%dC   ", s_temp10 / 10, (s_temp10 % 10 + 10) % 10);
    oled_draw_str(0, 3, buf);

    snprintf(buf, sizeof(buf), "W:%4dg   ", s_grams);
    oled_draw_str(0, 4, buf);

    snprintf(buf, sizeof(buf), "D:%4ddpm", (int)(drop_rate_dpm() + 0.5f));
    oled_draw_str(0, 5, buf);

    oled_draw_str(0, 6, s_level == AL_ALARM ? "** ALARM **" :
                        (s_level == AL_WARN  ? "!  WARN  ! " : "   OK     "));
    oled_draw_str(0, 7, "K1:SET K2:MUTE");
}

static void ui_draw_set_norefresh(void)
{
    char buf[24];
    alert_cfg_t *c = alert502_cfg();

    if (s_ui == UI_SET_TEMP) {
        oled_draw_cn(0, 0, "设温度阈值");
        snprintf(buf, sizeof(buf), "ALM: %d.%d C ", c->temp_alarm / 10, c->temp_alarm % 10);
        oled_draw_str(0, 3, buf);
        snprintf(buf, sizeof(buf), "WRN: %d.%d C ", c->temp_warn / 10, c->temp_warn % 10);
        oled_draw_str(0, 4, buf);
    } else {
        oled_draw_cn(0, 0, "设重量阈值");
        snprintf(buf, sizeof(buf), "ALM: %4d g ", c->w_alarm);
        oled_draw_str(0, 3, buf);
        snprintf(buf, sizeof(buf), "WRN: %4d g ", c->w_warn);
        oled_draw_str(0, 4, buf);
    }
    oled_draw_str(0, 6, "K2:-  K3:+");
    oled_draw_str(0, 7, "K4:OK  K1:NEXT");
}

/* ==================== 按键 ==================== */
static void handle_keys(void)
{
    uint8_t  k  = key_any_just_pressed();     /* 1/2/3 */
    uint8_t  e4 = key4_event();               /* 1 短按 / 2 长按 */
    alert_cfg_t *c = alert502_cfg();

    /* ---------- 运行界面 ---------- */
    if (s_ui == UI_RUN) {
        if (k == 1) { s_ui = UI_SET_TEMP;   return; }
        if (k == 2) { alert502_mute(10000); printf("[UI] 消音 10s\r\n"); return; }
        if (e4 == 2) {
            hx711_tare(20);
            printf("[UI] 去皮完成，零点=%ld\r\n", (long)hx711_get_offset());
            return;
        }
    }

    /* ---------- 设置界面 ---------- */
    if (s_ui == UI_SET_TEMP) {
        if (k == 2) { c->temp_alarm--; c->temp_warn = c->temp_alarm - 5; }
        if (k == 3) { c->temp_alarm++; c->temp_warn = c->temp_alarm - 5; }
        if (e4 == 1) { s_ui = UI_SET_WEIGHT; return; }
        if (k == 1) { s_ui = UI_RUN; return; }
    } else if (s_ui == UI_SET_WEIGHT) {
        if (k == 2) { c->w_alarm -= 10; if (c->w_alarm < 0) c->w_alarm = 0; c->w_warn = c->w_alarm + 50; }
        if (k == 3) { c->w_alarm += 10; c->w_warn = c->w_alarm + 50; }
        if (e4 == 1) { s_ui = UI_RUN; return; }
        if (k == 1) { s_ui = UI_RUN; return; }
    }
}

/* ==================== 主程序 ==================== */
int main(void)
{
    char frame[64];

    bsp_init();
    app502_init();
    uart1_init();
    printf("\r\n[BOOT] 502 整机固件 v1.0\r\n");

    /* --- 传感器 --- */
    ds18b20_init();
    (void)ds18b20_get();          /* 丢弃上电首次 85℃ */
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

    /* --- 网络（失败也继续跑，本地功能不受影响） --- */
    if (!report_init()) {
        printf("[NET] 初始化失败：本地功能继续，联网稍后重试\r\n");
    }

    while (1) {
        /* 1) 滴速结算 + 收包（都必须高频调用） */
        drop_tick();
        report_poll();

        /* 2) 温度：两段式，避免阻塞 */
        if (!s_converting && (millis() - s_t_conv) >= 1000) {
            ds18b20_start();
            s_converting = 1;
        }
        if (s_converting && (millis() - s_t_conv) >= 800) {
            s_converting = 0;
            s_t_conv     = millis();
            s_temp10     = ds18b20_read();

            s_raw    = hx711_read_raw();
            s_grams  = (int16_t)hx711_grams(s_raw);
        }

        /* 告警执行每轮都跑：LED 2Hz 闪烁与蜂鸣器计时都在里面 */
        s_level = alert502_update(s_temp10, s_temp10 != DS18B20_ERR,
                                  s_grams, s_raw != HX711_ERR,
                                  (uint16_t)(drop_rate_dpm() + 0.5f), 1);
        {

            if ((millis() - s_t_print) >= 2000) {
                s_t_print = millis();
                printf("[RUN] T=%d.%d W=%d D=%d L=%u\r\n",
                       s_temp10 / 10, s_temp10 % 10, s_grams,
                       (int)(drop_rate_dpm() + 0.5f), s_level);
            }
        }

        /* 3) 按键 */
        handle_keys();

        /* 4) 界面：重绘只写缓冲区（便宜）；送到屏幕按"一次一页"分摊，
         *    因为整屏刷新要 ~374ms，会把这期间的滴速中断与网络收包饿死 */
        {
            static uint32_t last_draw = 0;
            static uint32_t last_page = 0;
            static uint8_t  page = 0;

            if ((millis() - last_draw) >= 200) {
                last_draw = millis();
                if (s_ui == UI_RUN) ui_draw_run_norefresh();
                else                ui_draw_set_norefresh();
            }
            if ((millis() - last_page) >= 40) {
                last_page = millis();
                oled_refresh_page(page);
                page = (uint8_t)((page + 1) & 7);
            }
        }

        /* 5) 上报（每 2 秒，有客户端才发） */
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
