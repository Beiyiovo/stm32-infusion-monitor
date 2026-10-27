/**
 ******************************************************************************
 *  hx711.c —— HX711 24 位称重 ADC（软件时序，通道 A 增益 128）
 ******************************************************************************
 *  ★ 关键点：
 *    1) DOUT 必须等它变低（就绪）再移数据，读的时候不能超时死等
 *    2) 一定要发第 25 个脉冲，否则下一次掉到别的通道/增益
 *    3) 零点（offset）与换算系数（scale）必须实测标定，默认值只是占位
 *    4) 2026-10-27：去皮改「去极值平均」（排序去两端极值再平均），抗单点毛刺；
 *       去掉样本间的额外延时（read_raw 本身就等新样本），20 个样本 ≈ 2 秒
 ******************************************************************************
 */
#include "hx711.h"

#define HX_SCK_H()      GPIO_SetBits(HX_SCK_GPIO, HX_SCK_PIN)
#define HX_SCK_L()      GPIO_ResetBits(HX_SCK_GPIO, HX_SCK_PIN)
#define HX_DT_READ()    (GPIO_ReadInputDataBit(HX_DT_GPIO, HX_DT_PIN) ? 1 : 0)

#define HX_TIMEOUT_MS   200

static int32_t s_offset = 0;
static float   s_scale  = HX711_DEFAULT_SCALE;

/* ==================== 初始化 ==================== */
void hx711_init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* SCK = 推挽输出，上电先拉低（HX711 要求） */
    gpio.GPIO_Pin   = HX_SCK_PIN;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(HX_SCK_GPIO, &gpio);
    HX_SCK_L();

    /* DOUT = 上拉输入；★ 传感器没接时它一直是高，所以读函数必须带超时 */
    gpio.GPIO_Pin  = HX_DT_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(HX_DT_GPIO, &gpio);

    delay_ms(100);          /* 等 HX711 内部上电稳定 */
}

uint8_t hx711_ready(void)
{
    return (HX_DT_READ() == 0) ? 1 : 0;
}

/* ==================== 读一次原始值 ==================== */
int32_t hx711_read_raw(void)
{
    int32_t  v = 0;
    uint8_t  i;
    uint32_t t0 = millis();

    /* 1) 等数据就绪 */
    while (!hx711_ready()) {
        if ((millis() - t0) > HX_TIMEOUT_MS) return HX711_ERR;
    }

    /* 2) 移出 24 位，MSB 先出 */
    for (i = 0; i < 24; i++) {
        HX_SCK_H();
        delay_us(1);
        v = (v << 1) | HX_DT_READ();
        HX_SCK_L();
        delay_us(1);
    }

    /* 3) 第 25 个脉冲：通道 A / 增益 128 */
    HX_SCK_H();
    delay_us(1);
    HX_SCK_L();
    delay_us(1);

    /* 4) 24 位补码 → 32 位有符号 */
    if (v & 0x00800000L) {
        v |= 0xFF000000L;
    }
    return v;
}

/* ==================== 去极值平均（2026-10-27 新增） ====================
 * 排序后去掉两端各 trim 个，剩下的取平均。
 * 手碰秤盘、电磁毛刺这类单点离谱值会被直接扔掉，零点不再被拉偏。 */
static int32_t trimmed_avg(int32_t *buf, uint16_t n, uint16_t trim)
{
    int32_t  t;
    uint16_t i, j;
    int32_t  sum = 0;

    for (i = 0; i + 1 < n; i++)                 /* 插入排序（n≤24，够快） */
        for (j = (uint16_t)(i + 1); j < n; j++)
            if (buf[j] < buf[i]) { t = buf[i]; buf[i] = buf[j]; buf[j] = t; }

    for (i = trim; i < (uint16_t)(n - trim); i++) sum += buf[i];
    return (int32_t)(sum / (int32_t)(n - 2 * trim));
}

/* ==================== 去皮 ==================== */
int32_t hx711_tare(uint16_t samples)
{
    int32_t  buf[24];
    uint16_t n = 0;
    uint16_t i;
    int32_t  r;

    if (samples == 0) samples = 10;
    if (samples > 24) samples = 24;

    for (i = 0; i < samples; i++) {
        r = hx711_read_raw();       /* 读函数自带等待，出来就是新样本 */
        if (r != HX711_ERR) buf[n++] = r;
    }

    if (n >= 10)      s_offset = trimmed_avg(buf, n, 3);
    else if (n >= 5)  s_offset = trimmed_avg(buf, n, 1);
    else if (n > 0)   s_offset = trimmed_avg(buf, n, 0);

    return s_offset;
}

void hx711_set_scale(float g_per_count)
{
    if (g_per_count > 0.000001f && g_per_count < 1000000.0f) {
        s_scale = g_per_count;
    }
}

float hx711_get_scale(void)  { return s_scale; }
int32_t hx711_get_offset(void) { return s_offset; }

float hx711_grams(int32_t raw)
{
    if (raw == HX711_ERR) return 0.0f;
    return ((float)(raw - s_offset)) / s_scale;
}

/* 克 ×10（四舍五入），给 printf 用整数打印，避免依赖浮点格式化 */
int32_t hx711_grams_x10(int32_t raw)
{
    float g = hx711_grams(raw);

    if (g >= 0.0f) return (int32_t)(g * 10.0f + 0.5f);
    return (int32_t)(g * 10.0f - 0.5f);
}
