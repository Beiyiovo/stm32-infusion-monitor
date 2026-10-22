/**
 ******************************************************************************
 *  502 自检固件（10_selftest）—— 一次上电，把接线全部验一遍
 ******************************************************************************
 *  不依赖称重传感器，今天就能跑。串口 115200 输出。
 *
 *  它依次检查：
 *      ① 3V3 电源与主循环          心跳行一直滚 = MCU 活着
 *      ② I2C 总线                  扫描 0x3C（OLED）
 *      ③ DS18B20                   读温度（剔除 85℃ 假值）
 *      ④ 4 个按键                  PA1/PA4/PA5/PA6，按下打印
 *      ⑤ 蜂鸣器 + 红/黄 LED        开机自检：响一声、红黄各闪一次；之后 KEY 触发
 *      ⑥ 红外对射（PB12）          电平变化就打印，用来判"有没有跳变"
 *
 *  判据（全部满足 = 接线过关）：
 *      · 心跳每秒一行
 *      · I2C 扫到 0x3C
 *      · 温度在 15~45℃，不是 85℃
 *      · 四个键都能打印出编号
 *      · 蜂鸣器响、红黄灯都能亮
 *      · 手挡红外时 PB12 电平打印 "1→0" 或 "0→1"
 ******************************************************************************
 */
#include "bsp.h"
#include "i2c.h"
#include "oled.h"
#include "app502.h"
#include "ds18b20.h"

static uint8_t s_ir_last = 0xFF;

static void beep(uint16_t ms)
{
    buzzer(1);
    delay_ms(ms);
    buzzer(0);
}

int main(void)
{
    uint8_t  i, found;
    int16_t  t;
    uint32_t last_hb = 0;
    uint32_t last_t  = 0;
    uint8_t  ir;

    bsp_init();
    app502_init();
    uart1_init();

    printf("\r\n");
    printf("==============================================\r\n");
    printf("  502 接线自检 v1.0\r\n");
    printf("  串口 115200 8N1\r\n");
    printf("==============================================\r\n");

    /* ---------- ① 电源提示 ---------- */
    led_onboard(1);
    printf("[1] 板载 LED 应已点亮（PC13，低电平点亮）\r\n");

    /* ---------- ② I2C 扫描 ---------- */
    i2c_init();
    delay_ms(50);
    found = 0;
    for (i = 1; i < 128; i++) {
        if (i2c_probe((uint8_t)(i << 1)) == I2C_OK) {
            printf("[2] I2C 发现设备: 0x%02X\r\n", i);
            found++;
        }
    }
    if (!found) {
        printf("[2] I2C 一个设备都没扫到  ← 查 4.7kΩ 上拉 / SCL=PB6 SDA=PB7 / 共地\r\n");
    } else if (i2c_probe(OLED_ADDR) == I2C_OK) {
        printf("[2] OLED(0x3C) 在线 ✔\r\n");
    }

    /* ---------- ③ DS18B20 ---------- */
    ds18b20_init();
    (void)ds18b20_get();
    t = ds18b20_get();
    if (t == DS18B20_ERR)   printf("[3] DS18B20 读失败  ← 查 DQ=PA0 / 4.7kΩ 上拉 / 共地\r\n");
    else if (t == 850)      printf("[3] DS18B20 读到 85.0℃  ← 多半没接上\r\n");
    else                    printf("[3] DS18B20 = %d.%d℃ ✔\r\n", t / 10, t % 10);

    /* ---------- ⑤ 声光自检 ---------- */
    printf("[5] 声光自检：蜂鸣器响一声，红/黄灯各闪一次\r\n");
    led_red(1);   beep(120); led_red(0);
    led_yellow(1); beep(120); led_yellow(0);

    /* ---------- OLED 显示自检结果 ---------- */
    oled_init();
    oled_clear();
    oled_draw_cn(0, 0, "自检");
    oled_draw_str(0, 3, "I2C:");
    oled_draw_str(40, 3, found ? "OK" : "NG");
    oled_draw_str(0, 4, "TEMP:");
    oled_draw_str(40, 4, (t > 0 && t < 450) ? "OK" : "NG");
    oled_draw_str(0, 5, "KEY: press");
    oled_draw_str(0, 7, "IR: watch log");
    oled_refresh();

    printf("[6] 红外对射：用手挡一下光路，看下面是否打印电平变化\r\n");
    printf("------------------------------------------------\r\n");

    /* ---------- 主循环：持续监视 ---------- */
    s_ir_last = GPIO_ReadInputDataBit(IR_GPIO, IR_PIN) ? 1 : 0;
    printf("[IR] 初始电平 = %u（通光时）\r\n", s_ir_last);

    while (1) {
        /* 心跳 */
        if ((millis() - last_hb) >= 1000) {
            last_hb = millis();
            printf("[HB] 运行中 %lu s\r\n", (unsigned long)(millis() / 1000));
            led_onboard((millis() / 1000) & 1);
        }

        /* 按键 */
        {
            uint8_t k = key_any_just_pressed();
            if (k) {
                printf("[KEY] KEY%u 按下\r\n", k);
                beep(40);
            }
            if (key4_pressed() && (millis() % 500) < 20) {
                printf("[KEY] KEY4 按下\r\n");
            }
        }

        /* 红外电平变化 */
        ir = GPIO_ReadInputDataBit(IR_GPIO, IR_PIN) ? 1 : 0;
        if (ir != s_ir_last) {
            printf("[IR] 电平 %u → %u\r\n", s_ir_last, ir);
            s_ir_last = ir;
        }

        /* 每 3 秒报一次温度，看是否稳定 */
        if ((millis() - last_t) >= 3000) {
            last_t = millis();
            t = ds18b20_get();
            if (t != DS18B20_ERR && t != 850) {
                printf("[TEMP] %d.%d℃\r\n", t / 10, t % 10);
            }
        }

        delay_ms(10);
    }
}
