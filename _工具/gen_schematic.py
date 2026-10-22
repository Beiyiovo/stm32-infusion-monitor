# -*- coding: utf-8 -*-
"""画 502 接线原理图（PNG）v2 —— 修正 v1 的模块重叠与标注压字问题。

输出：文档/_图_502接线原理图.png
"""
import os
from PIL import Image, ImageDraw, ImageFont

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '文档', '_图_502接线原理图.png')

W, H = 2400, 1740
FONT_PATH = r'C:\Windows\Fonts\msyh.ttc'
FONT_BD = r'C:\Windows\Fonts\msyhbd.ttc'

RED = (200, 40, 40)
BLACK = (30, 30, 30)
BLUE = (30, 70, 160)
GREEN = (25, 120, 60)
ORANGE = (200, 120, 20)
GREY = (110, 110, 110)
LIGHT = (246, 248, 251)
NAVY = (31, 59, 96)


def font(size, bold=False):
    p = FONT_BD if bold and os.path.exists(FONT_BD) else FONT_PATH
    return ImageFont.truetype(p, size)


def box(d, x, y, w, h, title, lines, color=BLACK, fill=LIGHT, title_size=28):
    d.rounded_rectangle([x, y, x + w, y + h], radius=10, outline=color, width=3, fill=fill)
    d.text((x + 16, y + 10), title, font=font(title_size, True), fill=color)
    yy = y + 54
    for ln in lines:
        d.text((x + 18, yy), ln, font=font(22), fill=BLACK)
        yy += 30


def signal(d, x1, y1, x2, y2, color, label):
    """正交走线：模块边 → 竖线 → MCU 引脚"""
    mx = (x1 + x2) // 2
    d.line([x1, y1, mx, y1], fill=color, width=3)
    d.line([mx, y1, mx, y2], fill=color, width=3)
    d.line([mx, y2, x2, y2], fill=color, width=3)
    # 网络名：居中放在"模块边 ↔ MCU 边"之间的空隙里，避免压到任何一边的文字
    tw = d.textlength(label, font=font(17))
    gap_mid = (min(x1, x2) + max(x1, x2)) / 2
    lx, ly = gap_mid - tw / 2, y1 - 26
    d.rectangle([lx - 5, ly - 2, lx + tw + 5, ly + 22], fill='white')
    d.text((lx, ly), label, font=font(17), fill=color)


def main():
    img = Image.new('RGB', (W, H), 'white')
    d = ImageDraw.Draw(img)

    d.text((40, 22), '502 输液状态 WiFi 监测装置 · 接线原理图', font=font(44, True), fill=NAVY)
    d.text((42, 80), 'STM32F103C8T6 + ESP32-C3(AT 固件) + DS18B20 + HX711(1kg) + 15mm 红外计数 + 0.96" OLED',
           font=font(22), fill=GREY)
    d.line([40, 122, W - 40, 122], fill=(200, 200, 200), width=2)

    # ---------------- MCU ----------------
    mcu_x, mcu_y, mcu_w, mcu_h = 880, 170, 640, 1420
    d.rounded_rectangle([mcu_x, mcu_y, mcu_x + mcu_w, mcu_y + mcu_h], radius=14,
                        outline=NAVY, width=5, fill=(240, 245, 252))
    d.text((mcu_x + 22, mcu_y + 14), 'STM32F103C8T6', font=font(36, True), fill=NAVY)
    d.text((mcu_x + 22, mcu_y + 60), '3.3V 系统 · 72MHz · 无板载 CH340', font=font(20), fill=GREY)

    # ---------------- 左侧模块 ----------------
    box(d, 60, 200, 640, 190, '0.96" OLED（I2C）',
        ['VCC → 3V3    GND → GND', 'SCL → PB6（4.7kΩ 上拉）', 'SDA → PB7（4.7kΩ 上拉）'])
    box(d, 60, 470, 640, 150, 'DS18B20 温度探头',
        ['红→3V3  黑→GND  黄(DQ)→PA0', 'DQ 需 4.7kΩ 上拉到 3V3'])
    box(d, 60, 700, 640, 150, '有源蜂鸣器模块',
        ['VCC → 3V3（★ 不可接 5V）', 'GND → GND    I/O → PB0（低电平响）'])
    box(d, 60, 930, 640, 190, 'LED ×2（红 / 黄）',
        ['红：PB1 → 220Ω → LED → GND', '黄：PB5 → 220Ω → LED → GND', '长脚为正极，短脚接 GND'])

    # ---------------- 右侧模块 ----------------
    box(d, 1700, 190, 640, 240, '轻触按键 ×4',
        ['K1 → PA1（设置）   K2 → PA4（加）', 'K3 → PA5（减）   K4 → PA6（确认/去皮）',
         '另一端全部接 GND（内部上拉）'])
    box(d, 1700, 490, 640, 180, 'HX711 + 1kg 悬臂梁',
        ['VCC → 3V3    GND → GND', 'SCK → PB10    DT → PB11', '传感器 4 线接 E+/E-/A+/A-'])
    box(d, 1700, 740, 640, 170, '红外计数（15mm 槽型）',
        ['VCC → 3V3    GND → GND', 'OUT → PB12（外部中断）', '滴壶外径需 ≤14mm，卡进槽内'])
    box(d, 1700, 980, 640, 220, 'ESP32-C3 核心板（AT 固件）',
        ['GPIO6 ← PA2（TX 交叉）', 'GPIO7 → PA3（RX 交叉）', 'GND — GND（必须共地）',
         '★ 电源走自己的 Type-C，不接 3V3'])
    box(d, 1700, 1270, 640, 170, 'USB-TTL（CH340）烧录/调试',
        ['RXD ← PA9（ST 的 TX）', 'TXD → PA10（ST 的 RX）', 'GND → GND      3V3/VCC ★不接'])

    # ---------------- 连线：左侧 ----------------
    left_conn = [
        (300, 350, BLUE, 'I2C_SCL'), (350, 420, BLUE, 'I2C_SDA'),
        (600, 560, GREEN, 'DS_DQ'),
        (790, 690, ORANGE, 'BUZZ'),
        (1000, 830, RED, 'LED_R'), (1050, 900, ORANGE, 'LED_Y'),
    ]
    for (my, mcu_y_, col, net) in left_conn:
        signal(d, 700, my, mcu_x, mcu_y_, col, net)

    mcu_left = [350, 420, 560, 690, 830, 900]
    names_left = ['PB6', 'PB7', 'PA0', 'PB0', 'PB1', 'PB5']
    for yy, nm in zip(mcu_left, names_left):
        d.text((mcu_x + 14, yy - 15), nm, font=font(22, True), fill=NAVY)

    # ---------------- 连线：右侧 ----------------
    right_conn = [
        (240, 300, BLACK, 'KEY1'), (290, 370, BLACK, 'KEY2'),
        (340, 440, BLACK, 'KEY3'), (390, 510, BLACK, 'KEY4'),
        (590, 610, GREEN, 'HX_SCK'), (640, 680, GREEN, 'HX_DT'),
        (840, 760, RED, 'IR_OUT'),
        (1050, 860, BLUE, 'USART2_TX'), (1100, 930, BLUE, 'USART2_RX'),
        (1340, 1010, GREY, 'USART1_TX'), (1390, 1080, GREY, 'USART1_RX'),
    ]
    for (my, mcu_y_, col, net) in right_conn:
        signal(d, 1700, my, mcu_x + mcu_w, mcu_y_, col, net)

    mcu_right = [300, 370, 440, 510, 610, 680, 760, 860, 930, 1010, 1080]
    names_right = ['PA1', 'PA4', 'PA5', 'PA6', 'PB10', 'PB11', 'PB12', 'PA2', 'PA3', 'PA9', 'PA10']
    for yy, nm in zip(mcu_right, names_right):
        tw = d.textlength(nm, font=font(22, True))
        d.text((mcu_x + mcu_w - tw - 14, yy - 15), nm, font=font(22, True), fill=NAVY)

    # ---------------- 底部说明 ----------------
    d.rounded_rectangle([60, 1180, 700, 1400], radius=10, outline=RED, width=3, fill=(255, 247, 247))
    d.text((80, 1192), '电源：一共两套', font=font(27, True), fill=RED)
    d.text((80, 1236), '① 5V 适配器 → STM32 Micro USB', font=font(21), fill=BLACK)
    d.text((80, 1264), '   → 板载 AMS1117 → 3V3 排针 → 面包板正极轨', font=font(21), fill=BLACK)
    d.text((80, 1292), '   （所有模块的 VCC 都从这条 3V3 轨取）', font=font(21), fill=GREY)
    d.text((80, 1330), '② 另一个 5V 适配器 → ESP32-C3 的 Type-C', font=font(21), fill=BLACK)
    d.text((80, 1358), '   ESP32 只与主板共地，★ 不接 3V3（尖峰 350mA）', font=font(21), fill=RED)

    d.rounded_rectangle([60, 1440, 700, 1630], radius=10, outline=BLUE, width=3, fill=(245, 249, 255))
    d.text((80, 1452), '阻容清单（全部为直插/0805 均可）', font=font(25, True), fill=BLUE)
    d.text((80, 1496), '4.7kΩ ×3：I2C SCL、I2C SDA、DS18B20 DQ', font=font(21), fill=BLACK)
    d.text((80, 1524), '220Ω  ×2：红 LED、黄 LED 限流', font=font(21), fill=BLACK)
    d.text((80, 1552), '10kΩ  ×1：红外 OUT 上拉（模块开漏时才需要）', font=font(21), fill=BLACK)
    d.text((80, 1580), '100µF ×1：3V3 去耦（可选，抗干扰）', font=font(21), fill=BLACK)

    d.rounded_rectangle([1700, 1470, 2340, 1630], radius=10, outline=GREEN, width=3, fill=(246, 252, 247))
    d.text((1720, 1482), '画板要点（详见文档）', font=font(25, True), fill=GREEN)
    d.text((1720, 1524), '· 两个 USB 口朝同一边，方便同时插线', font=font(20), fill=BLACK)
    d.text((1720, 1552), '· ESP32 天线正下方禁铺铜', font=font(20), fill=BLACK)
    d.text((1720, 1580), '· 3V3 走线加粗；上拉电阻靠近 MCU 放', font=font(20), fill=BLACK)
    d.text((1720, 1608), '· 称重传感器装在板外挂架，4 线引回', font=font(20), fill=BLACK)

    d.text((40, 1680), '注：本图为接线级原理图，用于接线与画板参考；逐脚细节见《502引脚分配与逐脚连线表》。',
           font=font(19), fill=GREY)

    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    img.save(OUT)
    print('SAVED', OUT, img.size)


if __name__ == '__main__':
    main()
