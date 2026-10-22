# -*- coding: utf-8 -*-
"""把 13_ui 的 5 个界面画成预览图（模拟 128×64 OLED，放大 4 倍）。"""
import os
from PIL import Image, ImageDraw, ImageFont

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '文档', '_图_OLED界面预览.png')

CN = r'C:\Windows\Fonts\simhei.ttf'
S = 4

screens = [
    ("开机自检", [
        (0, 0, "cn", "开机自检"),
        (0, 16, "cn", "温度"), (40, 22, "cn", "通过"),
        (0, 32, "cn", "称重"), (40, 38, "cn", "未接"),
        (0, 48, "cn", "网络"), (40, 54, "cn", "通过"),
    ]),
    ("主页（正常）", [
        (0, 0, "cn", "输液监测"),
        (0, 16, "cn", "温度"), (36, 22, "en", "34.2 C"),
        (0, 32, "cn", "重量"), (36, 38, "en", "480.0 g"),
        (0, 48, "cn", "滴速"), (36, 54, "en", "  60"),
    ]),
    ("主页（报警）", [
        (0, 0, "cn", "报警"), (40, 0, "cn", "温度过高"),
        (0, 16, "cn", "温度"), (36, 22, "en", "43.1 C"),
        (0, 32, "cn", "重量"), (36, 38, "en", "480.0 g"),
        (0, 48, "cn", "滴速"), (36, 54, "en", "  60"),
    ]),
    ("系统状态页（KEY1）", [
        (0, 0, "cn", "系统状态"),
        (0, 16, "cn", "静音"), (40, 16, "cn", "否"),
        (0, 32, "cn", "网络"), (40, 32, "cn", "正常"),
        (0, 48, "cn", "状态"), (40, 48, "cn", "正常"),
    ]),
    ("参数设置（KEY3）", [
        (0, 0, "cn", "参数设置"),
        (0, 16, "cn", "温度上限"), (70, 22, "en", "42.0C"), (118, 16, "en", ">"),
        (0, 32, "cn", "重量下限"), (70, 38, "en", " 50 g"),
        (0, 48, "cn", "滴速上限"), (70, 54, "en", "120"),
    ]),
]


def main():
    cn_font = ImageFont.truetype(CN, 16)
    en_font = ImageFont.truetype(r'C:\Windows\Fonts\consola.ttf', 9)

    cols = 2
    rows = (len(screens) + 1) // cols
    W, H = 128 * S, 64 * S
    pad = 26
    sheet = Image.new('RGB', (cols * (W + pad) + pad, rows * (H + pad + 22) + pad), (245, 246, 248))
    d = ImageDraw.Draw(sheet)

    for i, (title, items) in enumerate(screens):
        ox = pad + (i % cols) * (W + pad)
        oy = pad + (i // cols) * (H + pad + 22)

        d.text((ox, oy - 18), title, font=ImageFont.truetype(CN, 14), fill=(60, 60, 60))
        d.rectangle([ox, oy, ox + W - 1, oy + H - 1], fill=(0, 0, 0))

        for (x, y, kind, text) in items:
            f = cn_font if kind == "cn" else en_font
            d.text((ox + x * S, oy + y * S), text, font=f, fill=(190, 230, 255))

    sheet.save(OUT)
    print('SAVED', OUT, sheet.size)


if __name__ == '__main__':
    main()
