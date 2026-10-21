/**
 ******************************************************************************
 *  502 毕业设计 —— 步骤 1：串口打印 + LED 心跳（环境验证）
 ******************************************************************************
 *  目标：烧进去后串口能看到打印 = STM32 开发环境在 502 工程里也完全打通
 *
 *  硬件：STM32F103C8T6 开发板（项目1 闲置的那块，★没有板载 CH340）
 *        USB-TTL（CH340）接 PA9 / PA10 / GND
 *
 *  接线：
 *      USB-TTL TXD  ->  STM32 PA10  (USART1_RX)
 *      USB-TTL RXD  ->  STM32 PA9   (USART1_TX)
 *      USB-TTL GND  ->  STM32 GND
 *      USB-TTL VCC  ->  不接（板子自己 Micro USB 供电）
 *
 *  串口设置：115200 8 1 None 无流控
 *  ⚠️ 这块板实测只在 9600 能稳定握手烧录；串口打印 115200 若乱码，先降到 9600 试
 ******************************************************************************
 */

#include "stm32f10x.h"
#include <stdio.h>

#define UART_BAUD   115200

/* ==================== 延时（粗略，够用） ==================== */
static void delay_ms(uint32_t ms)
{
    volatile uint32_t i;
    while (ms--) {
        for (i = 0; i < 7200; i++) { __NOP(); }
    }
}

/* ==================== LED（PC13 板载，低电平点亮） ==================== */
static void LED_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_13;
    gpio.GPIO_Mode  = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOC, &gpio);

    GPIO_SetBits(GPIOC, GPIO_Pin_13);   /* 先灭掉 */
}

/* ==================== USART1（PA9=TX, PA10=RX） ==================== */
static void USART1_Init(void)
{
    GPIO_InitTypeDef  gpio;
    USART_InitTypeDef usart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);

    gpio.GPIO_Pin   = GPIO_Pin_9;
    gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &gpio);

    gpio.GPIO_Pin  = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    usart.USART_BaudRate            = UART_BAUD;
    usart.USART_WordLength          = USART_WordLength_8b;
    usart.USART_StopBits            = USART_StopBits_1;
    usart.USART_Parity              = USART_Parity_No;
    usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART1, &usart);

    USART_Cmd(USART1, ENABLE);
}

/* ==================== printf 重定向到 USART1 ==================== */
int fputc(int ch, FILE *f)
{
    (void)f;
    USART_SendData(USART1, (uint8_t)ch);
    while (USART_GetFlagStatus(USART1, USART_FLAG_TC) == RESET) { }
    return ch;
}

/* ==================== 主程序 ==================== */
int main(void)
{
    uint32_t n = 0;

    LED_Init();
    USART1_Init();

    printf("\r\n");
    printf("================================================\r\n");
    printf("  502 输液状态 WiFi 监测装置\r\n");
    printf("  步骤 1：串口 + 心跳 环境验证\r\n");
    printf("================================================\r\n");
    printf("  芯片型号 : STM32F103C8T6\r\n");
    printf("  系统主频 : %u Hz\r\n", (unsigned)SystemCoreClock);
    printf("  串口     : USART1 (PA9/PA10)  %u 8N1\r\n", (unsigned)UART_BAUD);
    printf("  编译时间 : %s %s\r\n", __DATE__, __TIME__);
    printf("------------------------------------------------\r\n");
    printf("  本项目引脚（详见 502引脚分配与逐脚连线表）\r\n");
    printf("    PA0  DS18B20 数据      PB0  蜂鸣器\r\n");
    printf("    PA1  KEY1 设置         PB1  红 LED\r\n");
    printf("    PA2/PA3 USART2 -> ESP32  PB5  黄 LED\r\n");
    printf("    PA4  KEY2 加           PB6/PB7  I2C(OLED)\r\n");
    printf("    PA5  KEY3 减           PB10/PB11 HX711\r\n");
    printf("    PA6  KEY4 确认/去皮     PB12 预留(滴速)\r\n");
    printf("    PA9/PA10 USART1 调试    PC13 板载 LED\r\n");
    printf("================================================\r\n");
    printf("  >>> 看到这段文字 = 502 工程环境已打通 <<<\r\n");
    printf("================================================\r\n\r\n");

    while (1) {
        n++;
        GPIO_ResetBits(GPIOC, GPIO_Pin_13);
        printf("[RUN] 第 %u 次心跳  LED=亮\r\n", (unsigned)n);
        delay_ms(500);

        GPIO_SetBits(GPIOC, GPIO_Pin_13);
        printf("[RUN] 第 %u 次心跳  LED=灭\r\n", (unsigned)n);
        delay_ms(500);
    }
}
