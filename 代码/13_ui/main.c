/**
 ******************************************************************************
 *  502 步骤 13：整机固件 v2（开机自检 + 四键 + 分级告警 + 上报）
 ******************************************************************************
 *  按键（短按 = 按下即松；长按 = 按住 3 秒）
 *  KEY1：运行界面 <-> 系统面板
 *  KEY2：静音 10 分钟 / 取消静音
 *  KEY3：短按进设置并切换参数项；长按 3 秒恢复默认阈值
 *  KEY4：设置里短按 +1 档；长按 3 秒称重去皮
 *
 *  红色报警：温度>=42.0 / 重量<50g / 滴速>=120
 *  黄色预警：温度 41.5~42.0 / 温度<32.0 / 重量 50~100g
 *  蜂鸣器：进入报警响 3 秒后自停；滴速过快时红灯 4Hz 快闪
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
#define UI_SET      2

static uint8_t s_ui = UI_RUN;
static uint8_t s_set_idx = 0;

static int16_t  s_temp10 = 0;
static uint8_t  s_temp_ok = 0;
static int32_t  s_raw = 0;
static int16_t  s_grams = 0;
static uint8_t  s_hx_ok = 0;
static uint8_t  s_level = AL_OK;
static uint8_t  s_reason = 0;
static uint8_t  s_net_ok = 0;
static uint8_t  s_ir_seen = 0;

static uint32_t s_t_conv = 0, s_t_send = 0, s_t_draw = 0, s_t_page = 0;
static uint8_t  s_converting = 0, s_page = 0;

static uint32_t s_k3_down = 0, s_k4_down = 0;
static uint8_t  s_k3_held = 0, s_k4_held = 0;

static const char *reason_text(uint8_t r)
{
    if (r & R_TEMP_HIGH)   return "温度过高";
    if (r & R_WEIGHT_LOW)  return "药液将尽";
    if (r & R_RATE_FAST)   return "滴速过快";
    if (r & R_TEMP_LOW)    return "温度过低";
    return "";
}

static void draw_run(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "输液监测");

    if (s_temp_ok)
        snprintf(buf, sizeof(buf), "T: %2d.%d C ", s_temp10 / 10, (s_temp10 % 10 + 10) % 10);
    else
        snprintf(buf, sizeof(buf), "T:  --.- C ");
    oled_draw_str(0, 2, buf);

    if (s_hx_ok) {
        int32_t g10 = hx711_grams_x10(s_raw);
        snprintf(buf, sizeof(buf), "W:%4ld.%ld g", (long)(g10 / 10), (long)((g10 % 10 + 10) % 10));
    } else {
        snprintf(buf, sizeof(buf), "W:  ---- g");
    }
    oled_draw_str(0, 3, buf);

    if (s_ir_seen)
        snprintf(buf, sizeof(buf), "D:%4d dpm", (int)(drop_rate_dpm() + 0.5f));
    else
        snprintf(buf, sizeof(buf), "D:  --- dpm");
    oled_draw_str(0, 4, buf);

    if (s_level == AL_ALARM)     snprintf(buf, sizeof(buf), "!! %s", reason_text(s_reason));
    else if (s_level == AL_WARN) snprintf(buf, sizeof(buf), "!  %s", reason_text(s_reason));
    else                         snprintf(buf, sizeof(buf), "   OK");
    oled_draw_str(0, 5, buf);

    if (alert502_is_muted())      oled_draw_str(0, 6, "MUTE 10min   ");
    else if (s_level != AL_OK)    oled_draw_str(0, 6, "ALARMING     ");
    else                          oled_draw_str(0, 6, "             ");

    oled_draw_str(0, 7, "K1:PANEL K2:MUTE");
}

static void draw_panel(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "系统面板");

    oled_draw_str(0, 2, "Ver   2.0");

    snprintf(buf, sizeof(buf), "TEMP  %s", s_temp_ok ? "OK" : "NG");
    oled_draw_str(0, 3, buf);

    snprintf(buf, sizeof(buf), "HX711 %s", s_hx_ok ? "OK" : "--");
    oled_draw_str(0, 4, buf);

    snprintf(buf, sizeof(buf), "IR    %s", s_ir_seen ? "OK" : "--");
    oled_draw_str(0, 5, buf);

    snprintf(buf, sizeof(buf), "NET   %s", s_net_ok ? "OK" : "--");
    oled_draw_str(0, 6, buf);

    oled_draw_str(0, 7, "K1:BACK K3:SET");
}

static void draw_set(void)
{
    alert_cfg_t *c = alert502_cfg();
    char buf[24];

    oled_draw_cn(0, 0, "参数设置");

    snprintf(buf, sizeof(buf), "%cTEMP HI %2d.%dC",
             s_set_idx == 0 ? '>' : ' ', c->temp_alarm / 10, c->temp_alarm % 10);
    oled_draw_str(0, 2, buf);

    snprintf(buf, sizeof(buf), "%cWEIGHT  %3d g",
             s_set_idx == 1 ? '>' : ' ', c->w_alarm);
    oled_draw_str(0, 3, buf);

    snprintf(buf, sizeof(buf), "%cRATE   %3d dpm",
             s_set_idx == 2 ? '>' : ' ', c->rate_high);
    oled_draw_str(0, 4, buf);

    oled_draw_str(0, 6, "K3:NEXT K4:+");
    oled_draw_str(0, 7, "K3 LONG:DEFAULT ");
}

static void set_next(void)
{
    s_set_idx = (uint8_t)((s_set_idx + 1) % 3);
    printf("[SET] next idx=%d\r\n", s_set_idx);
}

static void set_inc(void)
{
    alert_cfg_t *c = alert502_cfg();

    switch (s_set_idx) {
    case 0:
        c->temp_alarm = (int16_t)(c->temp_alarm + 5);
        if (c->temp_alarm > 450) c->temp_alarm = 400;
        c->temp_warn = (int16_t)(c->temp_alarm - 5);
        break;
    case 1:
        c->w_alarm = (int16_t)(c->w_alarm + 10);
        if (c->w_alarm > 200) c->w_alarm = 10;
        c->w_warn = (int16_t)(c->w_alarm * 2);
        break;
    default:
        c->rate_high = (int16_t)(c->rate_high + 10);
        if (c->rate_high > 200) c->rate_high = 60;
        break;
    }
    printf("[SET] temp=%d w=%d rate=%d\r\n", c->temp_alarm, c->w_alarm, c->rate_high);
}

static void handle_keys(void)
{
    uint8_t k  = key_any_just_pressed();
    uint8_t e4 = key4_event();

    if (key3_pressed()) {
        if (!s_k3_held) { s_k3_held = 1; s_k3_down = millis(); }
        else if ((millis() - s_k3_down) >= 3000) {
            s_k3_held = 0;
            alert502_defaults();
            printf("[SET] 已恢复默认阈值\r\n");
            buzzer(1); delay_ms(80); buzzer(0);
        }
    } else {
        s_k3_held = 0;
    }

    if (key4_pressed()) {
        if (!s_k4_held) { s_k4_held = 1; s_k4_down = millis(); }
        else if ((millis() - s_k4_down) >= 3000) {
            s_k4_held = 0;
            if (s_hx_ok) {
                hx711_tare(20);
                printf("[UI] 去皮完成 offset=%ld\r\n", (long)hx711_get_offset());
                buzzer(1); delay_ms(80); buzzer(0);
            } else {
                printf("[UI] 称重未就绪，无法去皮\r\n");
            }
        }
    } else {
        s_k4_held = 0;
    }

    if (k == 1) {
        s_ui = (s_ui == UI_RUN) ? UI_PANEL : UI_RUN;
        printf("[UI] -> %s\r\n", s_ui == UI_RUN ? "运行" : "面板");
        return;
    }

    if (k == 2) {
        if (alert502_is_muted()) {
            alert502_mute(0);
            printf("[UI] 取消静音\r\n");
        } else {
            alert502_mute(600000);
            printf("[UI] 静音 10 分钟\r\n");
        }
        return;
    }

    if (k == 3) {
        if (s_ui == UI_SET) {
            set_next();
        } else {
            s_ui = UI_SET;
            s_set_idx = 0;
            printf("[UI] 进入设置\r\n");
        }
        return;
    }

    if (e4 == 3 && s_ui == UI_SET) {
        set_inc();
    }
}

static void boot_screen(void)
{
    char buf[24];
    int16_t t;

    oled_clear();
    oled_draw_cn(0, 0, "开机自检");
    oled_refresh();

    ds18b20_start();
    delay_ms(800);
    t = ds18b20_read();
    s_temp_ok = (t != DS18B20_ERR && t != 850) ? 1 : 0;
    s_temp10 = s_temp_ok ? t : 0;

    s_raw = hx711_read_raw();
    s_hx_ok = (s_raw != HX711_ERR) ? 1 : 0;

    s_net_ok = report_init() ? 1 : 0;

    oled_clear();
    oled_draw_cn(0, 0, "开机自检");
    snprintf(buf, sizeof(buf), "TEMP  %s", s_temp_ok ? "OK " : "NG ");
    oled_draw_str(0, 2, buf);
    snprintf(buf, sizeof(buf), "HX711 %s", s_hx_ok ? "OK " : "-- ");
    oled_draw_str(0, 3, buf);
    oled_draw_str(0, 4, "IR    -- ");
    snprintf(buf, sizeof(buf), "NET   %s", s_net_ok ? "OK " : "-- ");
    oled_draw_str(0, 5, buf);
    oled_draw_str(0, 7, "v2.0 3s...");
    oled_refresh();

    printf("[BOOT] TEMP=%d HX711=%d NET=%d\r\n", s_temp_ok, s_hx_ok, s_net_ok);
    delay_ms(2500);
}

int main(void)
{
    char frame[72];

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n========================================\r\n");
    printf("  502 整机固件 v2.0\r\n");
    printf("  K1 面板  K2 静音  K3 设置  K4 +1/长按去皮\r\n");
    printf("========================================\r\n");

    ds18b20_init();
    hx711_init();
    drop_init();
    alert502_init();
    alert502_defaults();

    i2c_init();
    oled_init();

    boot_screen();

    while (1) {
        drop_tick();
        report_poll();
        if (drop_total() > 0) s_ir_seen = 1;

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

            s_level  = alert502_update(s_temp10, s_temp_ok, s_grams, s_hx_ok,
                                       (uint16_t)(drop_rate_dpm() + 0.5f), s_ir_seen);
            s_reason = alert502_reason();
        }

        handle_keys();

        if ((millis() - s_t_draw) >= 200) {
            s_t_draw = millis();
            if (s_ui == UI_RUN)        draw_run();
            else if (s_ui == UI_PANEL) draw_panel();
            else                       draw_set();
        }
        if ((millis() - s_t_page) >= 40) {
            s_t_page = millis();
            oled_refresh_page(s_page);
            s_page = (uint8_t)((s_page + 1) & 7);
        }

        if ((millis() - s_t_send) >= 2000) {
            s_t_send = millis();
            snprintf(frame, sizeof(frame), "T=%d.%d,W=%d,D=%d,L=%d,R=%d\r\n",
                     s_temp10 / 10, s_temp10 % 10, s_grams,
                     (int)(drop_rate_dpm() + 0.5f), s_level, s_reason);
            report_send(frame);
        }

        delay_ms(5);
    }
}
