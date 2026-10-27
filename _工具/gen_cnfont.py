# -*- coding: utf-8 -*-
"""生成 16×16 中文字库（cnfont_data.h / cnfont.c）并输出一张预览图。

字形来源：Windows 自带 simhei.ttf（黑体），16px 渲染后二值化。
格式与 oled.c 约定一致：
    每个字 32 字节
    前 16 字节 = 上半页（16 列，每字节 bit0 在上）
    后 16 字节 = 下半页（16 列）
"""
import os
from PIL import Image, ImageDraw, ImageFont

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_H = os.path.join(BASE, '代码', '_common', 'cnfont_data.h')
OUT_C = os.path.join(BASE, '代码', '_common', 'cnfont.c')
PREVIEW = os.path.join(BASE, '文档', '_图_字库预览.png')

# 需要的中文字（去重、保持顺序）
CHARS = (
    "输液监测状态温度重量滴速正常预警报"
    "过高低药将尽快静音分钟系统面板"
    "版本传感在线未连网络接口"
    "参数设置上下限默认恢复"
    "开机自检通过失败"
    "按键切换返回称已无有中联成功阈值标定行息是否异"
    "待慢"
)

FONT_PATH = r'C:\Windows\Fonts\simhei.ttf'


def render(ch, font):
    """渲染单个字 → 16×16 的 0/1 矩阵（行优先）"""
    img = Image.new('L', (16, 16), 0)
    d = ImageDraw.Draw(img)
    d.text((8, 8), ch, font=font, fill=255, anchor="mm")
    px = img.load()
    mat = [[1 if px[x, y] > 110 else 0 for x in range(16)] for y in range(16)]
    return mat


def to_bytes(mat):
    """16×16 → 32 字节（列主序，上下两页）"""
    out = []
    for page in (0, 1):
        for x in range(16):
            b = 0
            for bit in range(8):
                y = page * 8 + bit
                if mat[y][x]:
                    b |= (1 << bit)
            out.append(b)
    return out


def c_array(name, data, static=False):
    s = "static " if static else ""
    lines = ["%sconst unsigned char %s[][32] = {" % (s, name)]
    for i, ch in enumerate(CHARS):
        lines.append("    {   /* [%d] '%s' */" % (i, ch))
        vals = data[i]
        for r in range(0, 32, 8):
            lines.append("        " + ",".join("0x%02X" % v for v in vals[r:r + 8]) + ",")
        lines.append("    },")
    lines.append("};")
    return "\n".join(lines)


def main():
    font = ImageFont.truetype(FONT_PATH, 16, index=0)

    mats, blobs = [], []
    for ch in CHARS:
        m = render(ch, font)
        mats.append(m)
        blobs.append(to_bytes(m))

    header = '''/**
 ******************************************************************************
 *  cnfont_data.h —— OLED 中文字库（16×16 点阵）
 ******************************************************************************
 *  ★ 由 _工具/gen_cnfont.py 自动生成，不要手工改
 *  ★ 只在 oled.c 里 #include 一次
 *
 *  格式：每个汉字 32 字节
 *      前 16 字节 = 上半页（16 列）
 *      后 16 字节 = 下半页（16 列）
 *      每字节 bit0 = 该页最上一行，bit7 = 最下一行
 *
 *  本套字库共 %d 字，覆盖：状态/温度/重量/滴速/报警/面板/设置/开机自检
 ******************************************************************************
 */
#ifndef __CNFONT_DATA_H
#define __CNFONT_DATA_H

/* 字表（UTF-8，每个汉字 3 字节，顺序和下面数组一一对应） */
static const char cn_chars[] = "%s";

/* 字库字数（oled.c 的查找循环用它） */
#define CN_COUNT    %d

/* 点阵数据 */
%s

#endif /* __CNFONT_DATA_H */
''' % (len(CHARS), CHARS, len(CHARS), c_array('cn_data', blobs, static=True))

    with open(OUT_H, 'w', encoding='utf-8', newline='\n') as f:
        f.write(header)

    cfile = '''/**
 ******************************************************************************
 *  cnfont.c —— OLED 中文字库（独立编译单元版）
 ******************************************************************************
 *  ★ 由 _工具/gen_cnfont.py 自动生成
 *  ★ 工程里其实不需要它 —— oled.c 已经 #include 了 cnfont_data.h
 *    保留它只是为了老工程的文件列表不改
 ******************************************************************************
 */
#include "cnfont.h"

const char cnfont_chars[] = "%s";

const unsigned char cnfont_data[][32] = {
''' % CHARS

    for i, ch in enumerate(CHARS):
        cfile += "    {   /* [%d] '%s' */\n" % (i, ch)
        vals = blobs[i]
        for r in range(0, 32, 8):
            cfile += "        " + ",".join("0x%02X" % v for v in vals[r:r + 8]) + ",\n"
        cfile += "    },\n"
    cfile += "};\n"

    with open(OUT_C, 'w', encoding='utf-8', newline='\n') as f:
        f.write(cfile)

    # 预览图：每个字放大 4 倍并排
    cols = 12
    rows = (len(CHARS) + cols - 1) // cols
    S = 4
    sheet = Image.new('RGB', (cols * 16 * S + cols * 6, rows * 16 * S + rows * 22), 'white')
    d = ImageDraw.Draw(sheet)
    lbl = ImageFont.truetype(FONT_PATH, 14)
    for i, ch in enumerate(CHARS):
        cx = (i % cols) * (16 * S + 6)
        cy = (i // cols) * (16 * S + 22)
        for y in range(16):
            for x in range(16):
                if mats[i][y][x]:
                    d.rectangle([cx + x * S, cy + y * S, cx + x * S + S - 1, cy + y * S + S - 1],
                                fill=(20, 40, 70))
        d.text((cx, cy + 16 * S + 2), ch, font=lbl, fill=(120, 120, 120))
    sheet.save(PREVIEW)

    print('OK %d 字 -> %s' % (len(CHARS), OUT_H))
    print('预览 ->', PREVIEW)


if __name__ == '__main__':
    main()
