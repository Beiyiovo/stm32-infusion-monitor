/**
 ******************************************************************************
 *  502 毕业设计 —— 步骤 2：OLED 显示
 ******************************************************************************
 *  目标：OLED 点亮，显示中文标题 + 实时计数 + 端口信息
 *        → 证明 I2C + OLED 驱动在本项目电路上可用
 *
 *  接线（详见《502引脚分配与逐脚连线表》）：
 *      OLED VCC → 3V3     GND → GND
 *      OLED SCL → PB6     ★ 4.7kΩ 上拉到 3V3
 *      OLED SDA → PB7     ★ 4.7kΩ 上拉到 3V3
 *
 *  ★ 三个常见失败点：
 *      1) 缺 4.7kΩ 上拉 → 屏幕全黑（项目1 为此卡过一整天）
 *      2) SDA/SCL 接反   → 也全黑
 *      3) 没共地         → 全黑或花屏
 *
 *  串口：USART1 115200（PA9/PA10），同时打印状态便于定位
 ******************************************************************************
 */

#include "bsp.h"
#include "i2c.h"
#include "oled.h"

/* ==================== 主程序 ==================== */
int main(void)
{
    uint32_t n = 0;
    char line[32];

    /* --- 板级：解锁 DWT 延时、板载 LED、按键 --- */
    bsp_init();
    uart1_init();

    printf("\r\n[BOOT] 502 步骤2 · OLED 测试\r\n");

    /* --- I2C + OLED --- */
    i2c_init();
    oled_init();
    oled_clear();

    /* --- 静态界面 --- */
    oled_draw_cn(0, 0, "输液监测");       /* 第 1 行（16×16 中文，占 2 页） */
    oled_draw_str(0, 2, "T: --.-C  W: ---g");
    oled_draw_str(0, 3, "D: --.-dpm");
    oled_draw_str(0, 5, "OLED OK");
    oled_draw_str(0, 6, "502 STEP 2");
    oled_refresh();

    printf("[OLED] 初始化完成，屏幕应显示「输液监测」\r\n");

    while (1) {
        /* 每 500ms 更新一次计数行 */
        n++;
        snprintf(line, sizeof(line), "CNT:%lu", (unsigned long)n);
        oled_draw_str(0, 7, "                ");   /* 先擦除这一行 */
        oled_draw_str(0, 7, line);
        oled_refresh();

        led_onboard(n & 1);                         /* 板载 LED 跟着闪，证明主循环活着 */
        printf("[RUN] %lu 次刷新\r\n", (unsigned long)n);
        delay_ms(500);
    }
}
