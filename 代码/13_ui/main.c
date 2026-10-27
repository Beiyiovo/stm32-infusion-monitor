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
 *  蜂鸣器：进入报警响 3 声短促哔后自停；滴速过快时红灯 4Hz 快闪
 *
 *  ★ 2026-10-27 v2.1：
 *    · 开机提示改为「滴两声 + 灯亮 1.5 秒」（原 3 秒长响太吵）
 *    · 报警蜂鸣改为 3 声短促哔（原 3 秒长响太吵）
 *    · 称重改 10Hz 非阻塞采样 + 15 点去极值中均值滤波（原 1.8s 才采一个、5 点平均）
 *    · 红外模块开机自检 + 运行时电平监测（原来要数到第一滴才显示已接）
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
#define UI_SET2     3               /* ★ 2026-10-27：进阶阈值页（运行页 K3 长按进入） */

static uint8_t s_ui = UI_RUN;
static uint8_t s_set_idx = 0;
static uint8_t s_set2_idx = 0;

static int16_t  s_temp10 = 0;
static uint8_t  s_temp_ok = 0;
static int32_t  s_raw = 0;
static int32_t  s_raw_f = 0;            /* 去极值中均值滤波后的原始值 */
static uint32_t s_t_hx = 0;             /* 最近一次成功采样的时刻（掉线看门狗） */

#define RAW_AVG_N   15                  /* 滤波窗口：15 个样本 ≈ 1.5 秒（10Hz 采样） */
#define RAW_TRIM    3                   /* 排序后每端去掉 3 个极值，中间 9 个取平均 */
static int32_t  s_raw_buf[RAW_AVG_N];
static uint8_t  s_raw_i = 0, s_raw_n = 0;

/* ★ 2026-10-27：去极值中均值（trimmed mean）滤波
 *   HX711 单点读数偶发毛刺（磕碰/电磁干扰），纯平均会被拉偏；
 *   排序后去掉两端极值再平均，毛刺被完全剔除，随机噪声降到约 1/3。
 *   均值不变 → 不影响标定系数。 */
static int32_t raw_smooth(int32_t v)
{
    int32_t tmp[RAW_AVG_N];
    int32_t t, sum = 0;
    uint8_t i, j;

    s_raw_buf[s_raw_i] = v;
    s_raw_i = (uint8_t)((s_raw_i + 1) % RAW_AVG_N);
    if (s_raw_n < RAW_AVG_N) s_raw_n++;

    for (i = 0; i < s_raw_n; i++) tmp[i] = s_raw_buf[i];
    for (i = 0; i + 1 < s_raw_n; i++)                   /* 插入排序（n 小，够快） */
        for (j = (uint8_t)(i + 1); j < s_raw_n; j++)
            if (tmp[j] < tmp[i]) { t = tmp[i]; tmp[i] = tmp[j]; tmp[j] = t; }

    {
        uint8_t trim = (uint8_t)(RAW_TRIM < (s_raw_n / 3) ? RAW_TRIM : (s_raw_n / 3));
        uint8_t hi  = (uint8_t)(s_raw_n - trim);
        for (i = trim; i < hi; i++) sum += tmp[i];
        return (int32_t)(sum / (hi - trim));
    }
}
static int16_t  s_grams = 0;
static uint8_t  s_hx_ok = 0;
static uint8_t  s_level = AL_OK;
static uint8_t  s_reason = 0;
static uint8_t  s_net_ok = 0;
static uint8_t  s_ir_seen = 0;

static uint32_t s_t_conv = 0, s_t_send = 0, s_t_draw = 0, s_t_page = 0;
static uint8_t  s_converting = 0, s_page = 0;

static uint32_t s_k3_down = 0;
static uint8_t  s_k3_held = 0;

static const char *reason_text(uint8_t r)
{
    if (r & R_TEMP_HIGH)   return "温度过高";
    if (r & R_WEIGHT_LOW)  return "药液将尽";
    if (r & R_RATE_FAST)   return "滴速过快";
    if (r & R_TEMP_LOW)    return "温度过低";
    if (r & R_WEIGHT_HIGH) return "重量异常";
    if (r & R_RATE_SLOW)   return "滴速过慢";
    return "";
}

static void draw_run(void)
{
    char buf[24];

    oled_clear();

    /* 标题行兼做报警条：正常显示标题，报警时直接显示原因 */
    if (s_level == AL_ALARM) {
        oled_draw_cn(0, 0, "报警");
        oled_draw_cn(40, 0, reason_text(s_reason));
    } else if (s_level == AL_WARN) {
        oled_draw_cn(0, 0, "预警");
        oled_draw_cn(40, 0, reason_text(s_reason));
    } else {
        oled_draw_cn(0, 0, "输液监测");
    }

    /* 温度 */
    oled_draw_cn(0, 2, "温度");
    if (s_temp_ok)
        snprintf(buf, sizeof(buf), "%2d.%d C ", s_temp10 / 10, (s_temp10 % 10 + 10) % 10);
    else
        snprintf(buf, sizeof(buf), " --.- C ");
    oled_draw_str(36, 3, buf);

    /* 重量 */
    oled_draw_cn(0, 4, "重量");
    if (s_hx_ok) {
        int32_t g10 = hx711_grams_x10(s_raw_f);
        snprintf(buf, sizeof(buf), "%4ld.%ld g", (long)(g10 / 10), (long)((g10 % 10 + 10) % 10));
    } else {
        snprintf(buf, sizeof(buf), "  ---- g");
    }
    oled_draw_str(36, 5, buf);

    /* 滴速 */
    oled_draw_cn(0, 6, "滴速");
    if (s_ir_seen) {
        snprintf(buf, sizeof(buf), "%4d   ", (int)(drop_rate_dpm() + 0.5f));
        oled_draw_str(36, 7, buf);
    } else {
        oled_draw_cn(40, 6, "未接");
    }
}

static void draw_panel(void)
{
    oled_clear();
    oled_draw_cn(0, 0, "系统状态");

    /* 静音 */
    oled_draw_cn(0, 2, "静音");
    oled_draw_cn(40, 2, alert502_is_muted() ? "是" : "否");

    /* 网络 */
    oled_draw_cn(0, 4, "网络");
    if (!s_net_ok)                  oled_draw_cn(40, 4, "异常");
    else if (report_connected())    oled_draw_cn(40, 4, "已连");
    else                            oled_draw_cn(40, 4, "待机");

    /* 当前状态 */
    oled_draw_cn(0, 6, "状态");
    if (s_level == AL_ALARM)      oled_draw_cn(40, 6, "报警");
    else if (s_level == AL_WARN)  oled_draw_cn(40, 6, "预警");
    else                          oled_draw_cn(40, 6, "正常");
}

static void draw_set(void)
{
    alert_cfg_t *c = alert502_cfg();
    char buf[24];
    uint8_t i;
    const int16_t *vals[3] = { &c->temp_alarm, &c->w_alarm, &c->rate_high };

    oled_clear();
    oled_draw_cn(0, 0, "参数设置");
    oled_draw_str(78, 0, "K2- K4+");

    /* 三行：'>' 光标 + 标签 + 数值 */
    for (i = 0; i < 3; i++) {
        uint8_t page = (uint8_t)(2 + i * 2);

        oled_draw_str(0, page, (s_set_idx == i) ? ">" : " ");

        if (i == 0) {
            oled_draw_cn(12, page, "温度上限");
            snprintf(buf, sizeof(buf), "%2d.%dC", (*vals[0]) / 10, (*vals[0]) % 10);
        } else if (i == 1) {
            oled_draw_cn(12, page, "重量下限");
            snprintf(buf, sizeof(buf), "%4d g", (*vals[1]));
        } else {
            oled_draw_cn(12, page, "滴速上限");
            snprintf(buf, sizeof(buf), "%4d  ", (*vals[2]));
        }
        oled_draw_str(84, (uint8_t)(page + 1), buf);
    }
}

/* ★ 2026-10-27：进阶阈值页（运行页 K3 长按 2 秒进入）
 *   温度下限 / 重量上限 / 滴速下限 —— 只影响黄色预警，不触发红色报警/蜂鸣 */
static void draw_set2(void)
{
    alert_cfg_t *c = alert502_cfg();
    char buf[24];
    uint8_t i;
    const int16_t *vals[3] = { &c->temp_low, &c->w_high, &c->rate_low };

    oled_clear();
    oled_draw_cn(0, 0, "设置");
    oled_draw_str(32, 0, "2");
    oled_draw_str(78, 0, "K2- K4+");

    for (i = 0; i < 3; i++) {
        uint8_t page = (uint8_t)(2 + i * 2);

        oled_draw_str(0, page, (s_set2_idx == i) ? ">" : " ");

        if (i == 0) {
            oled_draw_cn(12, page, "温度下限");
            snprintf(buf, sizeof(buf), "%2d.%dC", (*vals[0]) / 10, (*vals[0]) % 10);
        } else if (i == 1) {
            oled_draw_cn(12, page, "重量上限");
            if (*vals[1] > 0) snprintf(buf, sizeof(buf), "%4d g", (*vals[1]));
            else              snprintf(buf, sizeof(buf), " ---- ");   /* 0 = 关闭 */
        } else {
            oled_draw_cn(12, page, "滴速下限");
            if (*vals[2] > 0) snprintf(buf, sizeof(buf), "%4d  ", (*vals[2]));
            else              snprintf(buf, sizeof(buf), " ---- ");   /* 0 = 关闭 */
        }
        oled_draw_str(84, (uint8_t)(page + 1), buf);
    }
}

static void set_next(void)
{
    s_set_idx = (uint8_t)((s_set_idx + 1) % 3);
    printf("[SET] next idx=%d\r\n", s_set_idx);
}

static void set_dec(void)
{
    alert_cfg_t *c = alert502_cfg();

    switch (s_set_idx) {
    case 0:
        c->temp_alarm = (int16_t)(c->temp_alarm - 5);
        if (c->temp_alarm < 400) c->temp_alarm = 450;
        c->temp_warn = (int16_t)(c->temp_alarm - 5);
        break;
    case 1:
        c->w_alarm = (int16_t)(c->w_alarm - 10);
        if (c->w_alarm < 10) c->w_alarm = 200;
        c->w_warn = (int16_t)(c->w_alarm * 2);
        break;
    default:
        c->rate_high = (int16_t)(c->rate_high - 10);
        if (c->rate_high < 60) c->rate_high = 200;
        break;
    }
    printf("[SET] temp=%d w=%d rate=%d\r\n", c->temp_alarm, c->w_alarm, c->rate_high);
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

/* ============ 进阶阈值（只黄色预警，与常规设置不冲突） ============ */
static void set2_next(void)
{
    s_set2_idx = (uint8_t)((s_set2_idx + 1) % 3);
    printf("[SET2] next idx=%d\r\n", s_set2_idx);
}

static void set2_inc(void)
{
    alert_cfg_t *c = alert502_cfg();

    switch (s_set2_idx) {
    case 0:     /* 温度下限 25.0~38.0℃，步进 0.5℃ */
        c->temp_low = (int16_t)(c->temp_low + 5);
        if (c->temp_low > 380) c->temp_low = 250;
        break;
    case 1:     /* 重量上限 0=关，50~1000g，步进 50g */
        c->w_high = (int16_t)(c->w_high + 50);
        if (c->w_high > 1000) c->w_high = 0;
        break;
    default:    /* 滴速下限 0=关，10~100 滴/分，步进 10 */
        c->rate_low = (int16_t)(c->rate_low + 10);
        if (c->rate_low > 100) c->rate_low = 0;
        break;
    }
    printf("[SET2] temp_low=%d w_high=%d rate_low=%d\r\n",
           c->temp_low, c->w_high, c->rate_low);
}

static void set2_dec(void)
{
    alert_cfg_t *c = alert502_cfg();

    switch (s_set2_idx) {
    case 0:
        c->temp_low = (int16_t)(c->temp_low - 5);
        if (c->temp_low < 250) c->temp_low = 380;
        break;
    case 1:
        c->w_high = (int16_t)(c->w_high - 50);
        if (c->w_high < 0) c->w_high = 1000;
        break;
    default:
        c->rate_low = (int16_t)(c->rate_low - 10);
        if (c->rate_low < 0) c->rate_low = 100;
        break;
    }
    printf("[SET2] temp_low=%d w_high=%d rate_low=%d\r\n",
           c->temp_low, c->w_high, c->rate_low);
}

static void handle_keys(void)
{
    uint8_t k  = key_any_just_pressed();
    uint8_t e4 = key4_event();

    if (key3_pressed() && s_ui == UI_SET) {
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

    /* --- ★ 2026-10-27：运行页 K3 改「松手判定」——短按进常规设置，长按 2 秒进进阶设置 --- */
    if (s_ui == UI_RUN) {
        uint8_t e3 = key_get_event(3);
        if (e3 == 1) {
            s_ui = UI_SET;
            s_set_idx = 0;
            printf("[UI] 进入设置\r\n");
            return;
        }
        if (e3 == 2) {
            s_ui = UI_SET2;
            s_set2_idx = 0;
            printf("[UI] 进入进阶设置（温度下限/重量上限/滴速下限）\r\n");
            return;
        }
    }

    /* KEY4：短按=设置里 +1；长按=去皮（事件码由 app502 给出：1 短 / 2 长） */
    if (e4 == 2) {
        if (s_hx_ok) {
            hx711_tare(20);
            printf("[UI] 去皮完成 offset=%ld\r\n", (long)hx711_get_offset());
            buzzer(1); delay_ms(80); buzzer(0);
        } else {
            printf("[UI] 称重未就绪，无法去皮\r\n");
        }
    }

    if (k == 1) {
        s_ui = (s_ui == UI_RUN) ? UI_PANEL : UI_RUN;
        printf("[UI] -> %s\r\n", s_ui == UI_RUN ? "运行" : "面板");
        return;
    }

    if (k == 2) {
        if (s_ui == UI_SET) {          /* 设置界面里 K2 = 减 */
            set_dec();
            return;
        }
        if (s_ui == UI_SET2) {         /* 进阶设置里 K2 = 减 */
            set2_dec();
            return;
        }
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
            return;
        }
        if (s_ui == UI_SET2) {
            set2_next();
            return;
        }
        /* 运行页 K3 改为「松手判定」（见上面 key_get_event），按下时不做事 */
        return;
    }

    if (e4 == 1) {
        if (s_ui == UI_SET) {
            set_inc();
        } else if (s_ui == UI_SET2) {
            set2_inc();
        }
    }
}

static void boot_screen(void)
{
    int16_t t;

    oled_clear();
    oled_draw_cn(0, 0, "开机自检");
    oled_refresh();

    /* ★ 2026-10-27：上电提示改为「滴滴两声 + 红黄灯亮 1.5 秒」（原来响 3 秒太吵）。
     *   这 1.5 秒顺便做红外模块自检：上拉输入下读不到低电平 ≈ 模块没接好 */
    led_red(1);
    led_yellow(1);
    buzzer(1);  delay_ms(80);   buzzer(0);  delay_ms(120);
    buzzer(1);  delay_ms(80);   buzzer(0);
    {
        uint8_t i;
        for (i = 0; i < 20; i++) {
            if (drop_level() == 0) s_ir_seen = 1;   /* 读到低 = 模块在线 */
            delay_ms(60);
        }
    }
    led_red(0);
    led_yellow(0);

    ds18b20_start();
    delay_ms(800);
    t = ds18b20_read();
    s_temp_ok = (t != DS18B20_ERR && t != 850) ? 1 : 0;
    s_temp10 = s_temp_ok ? t : 0;

    s_raw = hx711_read_raw();
    s_hx_ok = (s_raw != HX711_ERR) ? 1 : 0;

    /* ★ 上电自动去皮：否则 offset=0 时重量会显示 raw/scale（比如 764.8 g） */
    if (s_hx_ok) {
        uint8_t k;
        hx711_tare(20);
        s_raw = hx711_read_raw();
        for (k = 0; k < RAW_AVG_N; k++) {
            int32_t r = hx711_read_raw();
            if (r != HX711_ERR) s_raw_f = raw_smooth(r);   /* 坏值不进滤波器 */
        }
        s_t_hx = millis();
        printf("[HX] 上电自动去皮，offset=%ld\r\n", (long)hx711_get_offset());
    }

    s_net_ok = report_init() ? 1 : 0;

    oled_clear();
    oled_draw_cn(0, 0, "开机自检");
    oled_draw_cn(0, 2, "温度");
    oled_draw_cn(40, 2, s_temp_ok ? "通过" : "失败");
    oled_draw_cn(0, 4, "称重");
    oled_draw_cn(40, 4, s_hx_ok ? "通过" : "未接");
    oled_draw_cn(0, 6, "网络");
    oled_draw_cn(40, 6, s_net_ok ? "通过" : "未连");
    oled_refresh();

    printf("[BOOT] TEMP=%d HX711=%d IR=%d NET=%d\r\n", s_temp_ok, s_hx_ok, s_ir_seen, s_net_ok);
    delay_ms(1500);
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
        /* ★ 2026-10-27：运行时也盯着 OUT 电平 —— 模块预热后拉低、线被碰一下都算在线，
         *   不再非要等到第一滴才把「未接」翻过来 */
        if (!s_ir_seen && drop_level() == 0) {
            s_ir_seen = 1;
            printf("[IR] 检测到红外模块（OUT=低）\r\n");
        }

        if (!s_converting && (millis() - s_t_conv) >= 1000) {
            ds18b20_start();
            s_converting = 1;
        }
        if (s_converting && (millis() - s_t_conv) >= 800) {
            s_converting = 0;
            s_t_conv = millis();

            s_temp10 = ds18b20_read();
            s_temp_ok = (s_temp10 != DS18B20_ERR && s_temp10 != 850) ? 1 : 0;
        }

        /* ★ 2026-10-27：称重改 10Hz 非阻塞采样 —— HX711 每 100ms 出一个新样本，
         *   就绪就读一个进滤波器（原来 1.8 秒才采一个、5 点平均拖 7 秒，又迟钝又毛糙） */
        if (hx711_ready()) {
            int32_t r = hx711_read_raw();
            if (r != HX711_ERR) {
                int32_t g10;
                s_raw = r;
                s_t_hx = millis();
                s_hx_ok = 1;
                s_raw_f = raw_smooth(r);
                g10 = hx711_grams_x10(s_raw_f);
                /* 死区：±2 g 以内当作 0，避免静止时数字乱跳 */
                if (g10 > -20 && g10 < 20) g10 = 0;
                s_grams = (int16_t)(g10 / 10);
            }
        }
        /* HX711 掉线看门狗：3 秒没有新样本 → 标记未接 */
        if (s_hx_ok && (millis() - s_t_hx) > 3000) {
            s_hx_ok = 0;
            printf("[HX] 3 秒无新数据，标记称重未接\r\n");
        }

        /* 告警执行必须每轮跑：LED 闪烁/蜂鸣器计时都在里面 */
        s_level  = alert502_update(s_temp10, s_temp_ok, s_grams, s_hx_ok,
                                   (uint16_t)(drop_rate_dpm() + 0.5f), s_ir_seen);
        s_reason = alert502_reason();

        handle_keys();

        if ((millis() - s_t_draw) >= 200) {
            s_t_draw = millis();
            if (s_ui == UI_RUN)        draw_run();
            else if (s_ui == UI_PANEL) draw_panel();
            else if (s_ui == UI_SET)   draw_set();
            else                       draw_set2();
        }
        if ((millis() - s_t_page) >= 40) {
            s_t_page = millis();
            oled_refresh_page(s_page);
            s_page = (uint8_t)((s_page + 1) & 7);
        }

        if ((millis() - s_t_send) >= 2000) {
            s_t_send = millis();
            {
                int32_t g10 = s_hx_ok ? hx711_grams_x10(s_raw_f) : 0;
                snprintf(frame, sizeof(frame), "T=%d.%d,W=%ld.%ld,D=%d,L=%d,R=%d\r\n",
                         s_temp10 / 10, s_temp10 % 10,
                         (long)(g10 / 10), (long)((g10 % 10 + 10) % 10),
                         (int)(drop_rate_dpm() + 0.5f), s_level, s_reason);
            }
            report_send(frame);
        }

        delay_ms(5);
    }
}
