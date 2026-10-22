/**
 ******************************************************************************
 *  502 步骤 4：4 个按键
 ******************************************************************************
 *  接线：每个按键一端接 GPIO，另一端接 GND（内部上拉，按下=低）
 *      PA1 KEY1 设置    PA4 KEY2 加    PA5 KEY3 减    PA6 KEY4 确认/长按去皮
 *
 *  验收：按哪个键，串口打印对应编号；长按 KEY4 打印"长按"
 ******************************************************************************
 */
#include "bsp.h"
#include "app502.h"

int main(void)
{
    uint8_t k, e;

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n[BOOT] 502 步骤4 · 4 键测试\r\n");
    printf("[KEY] 按任意键看编号；按住 KEY4 超过 0.8 秒 = 长按\r\n");

    while (1) {
        /* 前三个键：bsp 提供的事件接口 */
        k = key_any_just_pressed();
        if (k) {
            printf("[KEY] KEY%u 短按\r\n", k);
            if (k == 1) { led_red(1); delay_ms(80); led_red(0); }
            if (k == 2) { led_yellow(1); delay_ms(80); led_yellow(0); }
            if (k == 3) { led_onboard(1); delay_ms(80); led_onboard(0); }
        }

        /* KEY4：本项目新增 */
        e = key4_event();
        if (e == 1) {
            printf("[KEY] KEY4 短按（确认）\r\n");
            led_red(1); delay_ms(80); led_red(0);
        } else if (e == 2) {
            printf("[KEY] KEY4 长按（去皮）\r\n");
            buzzer(1); delay_ms(80); buzzer(0);
        }

        delay_ms(10);
    }
}
