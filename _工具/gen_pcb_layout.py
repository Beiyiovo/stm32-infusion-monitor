# -*- coding: utf-8 -*-
"""502 PCB 布局参考图 v2 —— 重排坐标，避免重叠；图例放板下方。"""
import os
from PIL import Image, ImageDraw, ImageFont

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '文档', '_图_502_PCB布局参考.png')

S = 12                       # px/mm
W_MM, H_MM = 100, 80
OX, OY = 60, 120             # 板子左上角在画布中的位置

W, H = W_MM * S + OX * 2, H_MM * S + OY + 190

FP = r'C:\Windows\Fonts\msyh.ttc'
FPB = r'C:\Windows\Fonts\msyhbd.ttc'
GREEN = (22, 110, 70)
BLUE = (30, 70, 160)
RED = (200, 40, 40)
GREY = (120, 120, 120)
DARK = (40, 40, 40)
PAD = (200, 160, 40)


def font(sz, bold=False):
    return ImageFont.truetype(FPB if bold else FP, sz)


def mm(v):
    return int(v * S)


def P(x, y):
    return OX + mm(x), OY + mm(y)


def draw():
    img = Image.new('RGB', (W, H), 'white')
    d = ImageDraw.Draw(img)

    d.text((24, 18), '502 输液监测 · PCB 布局参考', font=font(30, True), fill=DARK)
    d.text((24, 56), '100 × 80 mm · 双层板 · 俯视 · 所有模块用排母座（可插拔）',
           font=font(16), fill=GREY)

    # 板框
    d.rounded_rectangle([OX, OY, OX + W_MM * S, OY + H_MM * S], radius=14,
                        outline=GREEN, width=5, fill=(247, 251, 248))

    def block(x1, y1, x2, y2, color=BLUE, w=2):
        d.rectangle([*P(x1, y1), *P(x2, y2)], outline=color, width=w)

    def label(x, y, text, color=DARK, sz=13, bold=False):
        d.text(P(x, y), text, font=font(sz, bold), fill=color)

    def vpins(x, y1, n, label_text=None):
        for i in range(n):
            cy = y1 + i * 2.54
            c = P(x, cy)
            d.ellipse([c[0] - 5, c[1] - 5, c[0] + 5, c[1] + 5], fill=PAD, outline=DARK)
        if label_text:
            label(x - 1.5, y1 - 5, label_text, BLUE, 12, True)

    def hpins(x1, y, n, label_text=None):
        for i in range(n):
            cx = x1 + i * 2.54
            c = P(cx, y)
            d.ellipse([c[0] - 5, c[1] - 5, c[0] + 5, c[1] + 5], fill=PAD, outline=DARK)
        if label_text:
            label(x1 - 1, y - 5.5, label_text, BLUE, 12, True)

    # ---------------- 安装孔 ----------------
    for (hx, hy) in [(5, 5), (95, 5), (5, 75), (95, 75)]:
        cx, cy = P(hx, hy)
        d.ellipse([cx - 15, cy - 15, cx + 15, cy + 15], outline=DARK, width=2)
        d.ellipse([cx - 8, cy - 8, cx + 8, cy + 8], fill='white', outline=DARK)

    # ---------------- STM32 ----------------
    block(20, 6, 40, 62)
    vpins(22, 9, 20, 'Lst32')
    vpins(37, 9, 20, 'Rst32')
    label(21, 63, 'STM32F103C8T6  2×1×20P', BLUE, 13, True)
    label(21, 2.5, '▲ Micro USB 朝这一边', RED, 11)

    # ---------------- OLED ----------------
    block(4, 8, 17, 16)
    hpins(6, 12, 4, 'OLED')
    label(4, 17, 'OLED 4P', BLUE, 12, True)

    # ---------------- 红外 ----------------
    block(4, 32, 12, 44)
    vpins(8, 34, 3, 'IR')
    label(4, 45, '红外 3P', BLUE, 12, True)
    label(13, 34, '← 靠板边，\n   方便夹滴壶', GREY, 10)

    # ---------------- HX711 ----------------
    block(4, 58, 13, 72)
    vpins(8, 60, 4, 'HX711')
    label(4, 54, 'HX711 4P', BLUE, 12, True)

    # ---------------- 蜂鸣器 ----------------
    block(41, 6, 52, 12)
    hpins(43, 9, 3, 'BUZZ')
    label(41, 2.5, '蜂鸣器 3P（VCC=3V3）', BLUE, 11, True)

    # ---------------- LED ----------------
    block(46, 20, 51, 24)
    label(52, 20, '红', RED, 13)
    block(46, 28, 51, 32)
    label(52, 28, '黄', (170, 120, 10), 13)

    # ---------------- 电阻 ----------------
    for i, t in enumerate(['R1', 'R3', 'R4', 'R2', 'R5']):
        y = 38 + i * 5
        block(46, y, 51, y + 2.2, GREY, 2)
        label(52, y - 1.5, t, GREY, 9)

    # ---------------- ESP32 ----------------
    block(56, 12, 81, 58)
    vpins(59, 15, 16, 'Lesp32')
    vpins(76, 15, 16, 'Resp32')
    label(57, 59, 'ESP32-C3  2×1×16P', BLUE, 13, True)
    label(57, 8, '▲ Type-C 朝这一边', RED, 11)

    # 天线禁布区
    d.rectangle([*P(56, 2.5), *P(82, 7.5)], outline=RED, width=2)
    for i in range(0, mm(27), 9):
        x = P(56, 0)[0] + i
        d.line([x, P(0, 2.5)[1], x + 9, P(0, 7.5)[1]], fill=(250, 205, 205), width=2)
    label(60, 3.5, '天线禁铺铜 / 禁走线', RED, 11, True)

    # ---------------- ISP 烧录口 ----------------
    block(86, 16, 95, 30, RED, 2)
    vpins(90, 18, 4, 'ISP')
    label(86, 31, 'ISP：3V3/GND/PA9/PA10', RED, 10, True)

    # ---------------- 按键 ----------------
    for i, name in enumerate(['KEY1', 'KEY2', 'KEY3', 'KEY4']):
        x = 48 + i * 12
        block(x, 66, x + 6, 74, GREY, 2)
        label(x, 75, name, DARK, 11)

    # ---------------- 图例 ----------------
    ly = OY + H_MM * S + 22
    d.text((24, ly), '图例', font=font(16, True), fill=DARK)
    items = [('排母座（蓝框，金点=焊盘）', BLUE), ('烧录口', RED),
             ('安装孔 M3', DARK), ('按键 / 电阻', GREY)]
    for i, (t, c) in enumerate(items):
        x = 90 + i * 240
        d.rectangle([x, ly + 2, x + 22, ly + 14], outline=c, width=3)
        d.text((x + 30, ly), t, font=font(13), fill=DARK)

    tips = ('线宽：3V3 主干 ≥20mil · 信号 10mil · GND 整板铺铜 · 过孔 0.3/0.6mm    '
            '｜  两个 USB 口都朝板子上边，方便同时插线')
    d.text((24, ly + 34), tips, font=font(13), fill=GREY)

    img.save(OUT)
    print('SAVED', OUT, img.size)


if __name__ == '__main__':
    draw()
