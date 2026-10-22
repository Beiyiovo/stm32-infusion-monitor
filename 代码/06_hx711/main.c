/**
 ******************************************************************************
 *  502 步骤 6：HX711 称重
 ******************************************************************************
 *  接线：HX711 VCC→3V3  GND→GND  SCK→PB10  DT(DOUT)→PB11
 *        传感器 4 线接 HX711 的 E+/E−/A+/A−（颜色以实物说明为准）
 *
 *  验收：
 *      1) 串口每 500ms 打印原始值，手指轻压传感器时数值变化
 *      2) 长按 KEY4 去皮，之后显示 0.0g
 *      3) 放 100g 砝码，显示约 100g（换算系数默认值不准，属正常，见阶段 3 标定）
 *
 *  ★ 若一直打印"读失败"：查 HX711 是否供电、是否共地、DT/SCK 是否接反
 ******************************************************************************
 */
#include "bsp.h"
#include "app502.h"
#include "hx711.h"

int main(void)
{
    int32_t raw;
    uint32_t n = 0;

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n[BOOT] 502 步骤6 · HX711 称重\r\n");

    hx711_init();
    printf("[HX] 初始化完成，零点=%ld，换算系数=%.3f counts/g\r\n",
           (long)hx711_get_offset(), hx711_get_scale());
    printf("[HX] 长按 KEY4 去皮\r\n");

    while (1) {
        raw = hx711_read_raw();
        n++;

        if (raw == HX711_ERR) {
            printf("[%lu] 读失败：查供电/共地/DT 是否接 PB11\r\n", (unsigned long)n);
        } else {
            printf("[%lu] raw=%ld  %.1f g\r\n",
                   (unsigned long)n, (long)raw, hx711_grams(raw));
        }

        /* 长按 KEY4 → 去皮 */
        if (key4_event() == 2) {
            printf("[HX] 去皮中...\r\n");
            hx711_tare(20);
            printf("[HX] 去皮完成，零点=%ld\r\n", (long)hx711_get_offset());
        }

        led_onboard(n & 1);
        delay_ms(500);
    }
}
