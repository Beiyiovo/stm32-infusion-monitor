/**
 ******************************************************************************
 *  502 标定固件（11_calib）—— 称重零点与换算系数，一次按键搞定
 ******************************************************************************
 *  硬件：HX711 VCC→3V3  GND→GND  SCK→PB10  DT→PB11
 *        传感器 4 线接 HX711 的 E+/E−/A+/A−
 *
 *  流程（屏幕和串口同步显示）：
 *      1) 上电后不放东西，等读数稳定
 *      2) KEY1 短按 → 采 20 次平均作为【零点 offset】
 *      3) 放上 100g 砝码（KEY2 可在 100/200/500g 之间切换）
 *      4) KEY4 短按 → 采 20 次平均，自动算出【换算系数 scale】
 *      5) 把串口打印出来的两行数字抄进代码即可
 *
 *  屏幕显示：
 *      RAW:   -123456
 *      ZERO:  -120000
 *      CAL:   100 g
 *      K:     1012.34 c/g
 *
 *  ★ 为什么不用买"称重显示模块"：一个传感器只能接一个 HX711，
 *    带显示的成品是独立一套，跟本电路不能共用；本固件已经能显示标定结果。
 ******************************************************************************
 */
#include "bsp.h"
#include "i2c.h"
#include "oled.h"
#include "app502.h"
#include "hx711.h"

static int32_t s_offset = 0;
static float   s_scale  = 0.0f;
static int32_t s_raw    = 0;
static int16_t s_cal_g  = 100;      /* 当前标定用的砝码重量 */
static int32_t s_zero_prev = 0;     /* 上一次的零点，用于异常检测 */

static int32_t avg_raw(uint16_t n)
{
    int32_t  sum = 0;
    uint16_t ok  = 0;
    uint16_t i;
    int32_t  r;

    for (i = 0; i < n; i++) {
        r = hx711_read_raw();
        if (r != HX711_ERR) { sum += r; ok++; }
        delay_ms(40);
    }
    if (!ok) return 0;
    return sum / (int32_t)ok;
}

static void draw(void)
{
    char buf[24];

    oled_draw_cn(0, 0, "称重标定");

    snprintf(buf, sizeof(buf), "RAW:%8ld", (long)s_raw);
    oled_draw_str(0, 2, buf);

    snprintf(buf, sizeof(buf), "ZERO:%7ld", (long)s_offset);
    oled_draw_str(0, 3, buf);

    snprintf(buf, sizeof(buf), "CAL:%3d g ", s_cal_g);
    oled_draw_str(0, 4, buf);

    if (s_scale > 0.0f) {
        snprintf(buf, sizeof(buf), "K:%8.2f", (double)s_scale);
        oled_draw_str(0, 5, buf);
        snprintf(buf, sizeof(buf), "W:%6.1f g", (double)hx711_grams(s_raw));
        oled_draw_str(0, 6, buf);
    } else {
        oled_draw_str(0, 5, "K: -- not cal");
        oled_draw_str(0, 6, "                 ");
    }

    oled_draw_str(0, 7, "K1:ZERO K4:CAL");
    oled_refresh();
}

int main(void)
{
    uint32_t t_draw = 0;

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n[BOOT] 502 称重标定固件\r\n");
    printf("[CAL] 步骤：空载 → KEY1 定零点 → 放砝码 → KEY4 定系数\r\n");

    hx711_init();
    delay_ms(300);

    i2c_init();
    oled_init();
    oled_clear();

    while (1) {
        uint8_t k;

        s_raw = hx711_read_raw();

        /* ★ 只读一次：key_any_just_pressed() 读后即清，调两次会丢掉第二个键 */
        k = key_any_just_pressed();

        /* --- KEY1：定零点 --- */
        if (k == 1) {
            printf("[CAL] 采零点中（空载，别碰）...\r\n");
            s_offset = avg_raw(20);
            printf("[CAL] ★ ZERO = %ld\r\n", (long)s_offset);
            if (s_zero_prev != 0 && (s_offset - s_zero_prev > 100000 || s_zero_prev - s_offset > 100000)) {
                printf("[CAL] !! 警告：零点比上次差了 %ld counts\r\n",
                       (long)(s_offset - s_zero_prev));
                printf("[CAL] !! 秤上是不是还放着东西？这组零点不可用，取下来重新按 KEY1\r\n");
            }
            s_zero_prev = s_offset;
            buzzer(1); delay_ms(60); buzzer(0);
        }

        /* --- KEY2：切换砝码重量 --- */
        if (k == 2) {
            if      (s_cal_g == 100) s_cal_g = 200;
            else if (s_cal_g == 200) s_cal_g = 500;
            else                     s_cal_g = 100;
            printf("[CAL] 标定重量 → %d g\r\n", s_cal_g);
        }

        /* --- KEY4：定换算系数（短按 1 / 长按 2 都接受） --- */
        {
            uint8_t e4 = key4_event();
            if (e4 != 0) {
                int32_t a;
                printf("[KEY] KEY4 事件=%u\r\n", e4);
                a = avg_raw(20);
            if (s_scale == 0.0f && s_offset == 0) {
                printf("[CAL] 提示：还没定零点，先按 KEY1\r\n");
            }
            if (a != 0) {
                s_scale = (float)(a - s_offset) / (float)s_cal_g;
                hx711_set_scale(s_scale);
                printf("[CAL] AVG = %ld, CAL = %d g\r\n", (long)a, s_cal_g);
                {
                    int32_t milli = (int32_t)(s_scale * 1000.0f + 0.5f);
                    printf("[CAL] SCALE = %ld.%03ld counts/g\r\n",
                           (long)(milli / 1000), (long)(milli % 1000));
                    printf("[CAL] 抄进代码：hx711_set_scale(%ld.%03ldf);\r\n",
                           (long)(milli / 1000), (long)(milli % 1000));
                }
                buzzer(1); delay_ms(120); buzzer(0);
                }
            }
        }

        /* --- 屏幕 200ms 刷一次 --- */
        if ((millis() - t_draw) >= 200) {
            t_draw = millis();
            draw();
        }

        delay_ms(20);
    }
}
