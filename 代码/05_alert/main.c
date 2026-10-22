/**
 ******************************************************************************
 *  502 步骤 5：两级告警（黄=预警 / 红=超限）
 ******************************************************************************
 *  这一步不接传感器，用按键切换"模拟值"来验证告警逻辑：
 *      KEY1 短按：循环切换 5 种工况
 *          0 正常      T=36.5  W=500
 *          1 温度预警  T=41.6  W=500   → 黄灯闪，蜂鸣器不响
 *          2 温度报警  T=42.3  W=500   → 红灯闪 + 蜂鸣器
 *          3 重量预警  T=36.5  W=80    → 黄灯闪
 *          4 重量报警  T=36.5  W=40    → 红灯闪 + 蜂鸣器
 *      KEY2 短按：消音 10 秒（红灯继续闪）
 *
 *  验收：5 种工况的灯/声音与上表一致，切换后 1 秒内生效
 ******************************************************************************
 */
#include "bsp.h"
#include "app502.h"
#include "alert502.h"

typedef struct { int16_t temp10; int16_t grams; uint16_t dpm; const char *name; } scene_t;

static const scene_t SCENES[] = {
    { 365, 500,  60, "正常（滴速 60）" },
    { 416, 500,  60, "温度偏高 41.6℃ → 黄灯慢闪" },
    { 423, 500,  60, "温度过高 42.3℃ → 红灯 2Hz + 响 3 秒" },
    { 365,  80,  60, "重量 80g → 黄灯慢闪" },
    { 365,  40,  60, "重量 40g → 红灯 2Hz + 响 3 秒" },
    { 365, 500, 140, "滴速 140 过快 → 红灯 4Hz 快闪" },
    { 300, 500,  60, "温度过低 30.0℃ → 黄灯慢闪" },
};
#define SCENE_N (sizeof(SCENES)/sizeof(SCENES[0]))

int main(void)
{
    uint8_t idx = 0, k, lv;
    const scene_t *s;

    bsp_init();
    app502_init();
    uart1_init();
    alert502_init();
    alert502_defaults();

    printf("\r\n[BOOT] 502 步骤5 · 两级告警测试\r\n");
    printf("[AL] KEY1 切换工况，KEY2 消音 10 秒\r\n");

    while (1) {
        /* --- 按键 --- */
        k = key_any_just_pressed();
        if (k == 1) {
            idx = (idx + 1) % SCENE_N;
            printf("[AL] 工况 → %u：%s\r\n", idx, SCENES[idx].name);
        } else if (k == 2) {
            alert502_mute(10000);
            printf("[AL] 消音 10 秒（红灯继续闪）\r\n");
        }

        s  = &SCENES[idx];
        lv = alert502_update(s->temp10, 1, s->grams, 1, (uint16_t)s->dpm, 1);

        /* 每 500ms 打印一次当前等级，便于对照 */
        {
            static uint32_t last = 0;
            if ((millis() - last) >= 500) {
                last = millis();
                printf("[AL] T=%d.%d W=%d  等级=%s\r\n",
                       s->temp10 / 10, s->temp10 % 10, s->grams,
                       lv == AL_ALARM ? "红(报警)" : (lv == AL_WARN ? "黄(预警)" : "正常"));
            }
        }

        delay_ms(20);
    }
}
