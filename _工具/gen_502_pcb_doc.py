# -*- coding: utf-8 -*-
"""生成《502 PCB设计与布线规范.docx》"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from docx import Document
from docx.shared import Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH

from docx_common import (new_doc, title, h1, h2, para, bullets, add_table, add_box,
                         add_footer, GREY)

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '文档', '502_PCB设计与布线规范.docx')
IMG = os.path.join(BASE, '文档', '_图_502_PCB布局参考.png')

BOARD = [
    ('外形尺寸', '100 × 80 mm（四角 R3 圆角）', '能放下两块排母 + 4 个按键 + 5 个模块接口'),
    ('层数', '双层（顶层信号、底层地）', '本设计最高频率只有 USB 12MHz 与 I2C 25kHz，双层足够'),
    ('板厚', '1.6 mm', '排母与按键的引脚长度按 1.6mm 设计'),
    ('铜厚', '1 oz（35 µm）', '打板默认'),
    ('工艺', '绿油白字、喷锡有铅/无铅均可', '不喷锡金，没必要'),
    ('安装孔', '4 个 M3（钻孔 3.2mm），四角内缩 5mm', '配铜柱'),
]

TRACE = [
    ('3V3 主干（模块供电）', '≥ 20 mil（0.5 mm）', '蜂鸣器与 ESP32 瞬时电流大'),
    ('3V3 分支（上拉电阻等）', '15 mil（0.38 mm）', '小电流也行，但别比主干还粗显得乱'),
    ('信号线（I2C / UART / 按键 / LED）', '10 mil（0.25 mm）', '电流小，够用'),
    ('GND', '整板铺铜，不单独走线', '铺铜 + 过孔缝合，回流路径最短'),
    ('过孔', '钻孔 0.3 mm / 外径 0.6 mm', '普通信号孔；电源孔可用 0.4/0.8'),
    ('线间距', '≥ 6 mil，建议 8 mil', '打板厂最小 6mil，留余量'),
    ('焊盘环宽', '≥ 0.2 mm', '太窄钻孔会破盘'),
]

ORDER = [
    ('1', '先摆位置', '按布局参考图把 6 个模块座、4 个按键、5 个电阻摆好'),
    ('2', '画板框与安装孔', '100×80，四角 M3'),
    ('3', '手动布 3V3 主干', '从 STM32 的 3V3 脚向各模块扇出，20mil'),
    ('4', '手动布 I2C', 'PB6/PB7 到 OLED，两条线尽量并行走'),
    ('5', '手动布 USART2', 'PA2/PA3 到 ESP32，两条并行走'),
    ('6', '手动布其余短信号', '按键、LED、蜂鸣器、HX611、红外'),
    ('7', '剩余交给自动布线', '只有没布的才交给它，别全自动'),
    ('8', '铺铜 + 过孔缝合', '顶层与底层都铺 GND，多打过孔'),
    ('9', 'DRC + 逐条目检', '见第五节清单'),
]

AUTOROUTE = [
    ('能自动布线吗', '能，但**不要一键全自动**。先手动布电源、I2C、串口这类"有讲究"的线，剩下的短连线再交给它'),
    ('在立创EDA 里的位置', 'PCB 界面 → 顶部菜单「布线」→「自动布线」，可设置线宽与是否允许过孔'),
    ('自动布线前必做', '先把网络类（Net Class）设好：信号 10mil、电源 20mil，否则它会全用默认线宽'),
    ('自动布线后必查', '① 有没有绕大圈 ② 有没有 90° 直角 ③ 有没有压到天线禁布区 ④ 电源线是不是被布成 10mil'),
    ('不要指望它', '它不理解"电源要粗、地要整片、天线不能碰"这三件事，这三件事永远要人来做'),
]

CHECK = [
    '两个排母的间距与实物一致（不是照抄文档的数字）',
    'ESP32 天线区域（板边 15mm 内）无铺铜、无走线、无过孔',
    'STM32 的 Micro USB 与 ESP32 的 Type-C 都在板边，且没有元件挡住',
    '3V3 主干 ≥ 20mil，没有细颈',
    'I2C 两条线等长、并行，上拉电阻靠近 MCU',
    'GND 铺铜完整，顶层与底层之间过孔足够',
    '按键四个都在板边，间距 ≥ 15mm',
    'ISP 烧录口（3V3/GND/PA9/PA10）引到了板边',
    '所有排母的 pin1 方向与模块实物一致（★ 最重要，插反会烧模块）',
    'DRC 0 错误；所有飞线（ratsnest）都已连上',
    '丝印：每个接口标了名称与 pin1 标识',
]


def build():
    doc = new_doc()
    title(doc,
          '502 PCB 设计与布线规范',
          '尺寸、线宽、自动布线怎么用、打板前检查',
          'v1.0 · 2026-10-22')

    h1(doc, '一、布局参考图')
    if os.path.exists(IMG):
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.add_run().add_picture(IMG, width=Cm(17.0))
        para(doc, '图 1  100×80mm 布局参考（俯视）。蓝框=排母座，红框=禁布区/烧录口。',
             size=9, color=GREY, align='center')
    else:
        para(doc, '（图片缺失：先运行 _工具/gen_pcb_layout.py）', color=GREY)

    add_box(doc, [
        '这张图只给出"相对位置关系"和"禁布区"，具体坐标要按你量到的实物尺寸来。',
        '★ 两个 USB 口都安排朝上边，是为了插线方便；如果你的外壳是别的方向，可以整体旋转。',
    ], title_text='怎么用这张图')

    h1(doc, '二、板子参数')
    add_table(doc, ['项', '取值', '说明'], BOARD,
              [3.4, 7.0, 6.6], font_pt=8.5, first_col_bold=True)

    h1(doc, '三、线宽与间距（照这个设）')
    add_table(doc, ['网络', '线宽', '为什么'], TRACE,
              [5.0, 4.6, 7.4], font_pt=8.5, first_col_bold=True)

    add_box(doc, [
        '在立创EDA 里先建"网络类"：把 3V3 设成 20mil，其余默认 10mil。',
        '这样不管是手动还是自动布线，都会按类别自动用对应线宽。',
    ], fill='F2F5F9', border='9AA7B4', title_text='先设规则，再布线')

    h1(doc, '四、布线顺序（不要一上来就自动布线）')
    add_table(doc, ['步', '做什么', '要点'], ORDER,
              [1.0, 5.0, 11.0], font_pt=8.5,
              aligns=['center', 'left', 'left'])

    h1(doc, '五、自动布线：能用，但只能用来收尾')
    add_table(doc, ['问题', '说明'], AUTOROUTE,
              [3.6, 13.4], font_pt=8.5, first_col_bold=True)

    add_box(doc, [
        '正确姿势：手动布完电源、I2C、串口 → 剩下的按键/LED 短线交给自动布线 → 布完逐条目检。',
        '错误姿势：一键全自动 → 电源被布成 10mil、天线区被布线、I2C 绕半个板子绕回来。',
    ], fill='FFF6DC', border='BF8F00', title_text='一句话记住')

    h1(doc, '六、打板前检查清单（逐条打勾）')
    add_table(doc, ['#', '检查项', '☐'],
              [(str(i + 1), t, '') for i, t in enumerate(CHECK)],
              [0.9, 14.6, 1.5], font_pt=9,
              aligns=['center', 'left', 'center'])

    h1(doc, '七、打板下单参数（嘉立创为例）')
    add_table(doc, ['项', '选什么'],
              [('层数', '2 层'),
               ('尺寸', '100×80mm（超过 10×10cm 会进"大板"档，价格以当期报价为准）'),
               ('板厚', '1.6mm'),
               ('阻焊颜色', '绿色（最快最便宜）'),
               ('丝印颜色', '白色'),
               ('表面处理', '喷锡（有铅/无铅均可）'),
               ('过孔工艺', '过孔盖油'),
               ('拼板', '不需要'),
               ('测试', '一般选"飞针测试"或不测，按预算）')],
              [4.0, 13.0], font_pt=9, first_col_bold=True)

    add_box(doc, [
        '★ 价格与免费额度以嘉立创当期活动为准，这里不写具体金额（写了也不准）。',
        '★ 下单前务必把 Gerber 用立创EDA 自带的"PCB 预览"或嘉立创的 Gerber 查看器过一遍，',
        '　 确认丝印、板框、孔位都对。',
    ], fill='F2F5F9', border='9AA7B4', title_text='关于价格与免费打板')

    h1(doc, '八、立创EDA 具体操作（简版）')
    bullets(doc, [
        '① 打开工程 → 从原理图「设计 → 更新/转换 PCB」生成板图',
        '② 画板框：100×80，四角 R3；放 4 个 M3 安装孔',
        '③ 按图 1 摆放排母、按键、电阻；**先量实物再定坐标**',
        '④ 设网络类线宽（3V3=20mil，其余 10mil）',
        '⑤ 手动布 3V3、I2C、USART2',
        '⑥ 其余用「布线 → 自动布线」收尾',
        '⑦ 顶层+底层铺 GND 铜，打过孔缝合',
        '⑧ 跑 DRC，逐条过第六节清单',
        '⑨ 导出 Gerber（含钻孔文件），下单',
    ], size=9.5)

    add_footer(doc, '502 PCB 设计与布线规范 v1.0')
    doc.save(OUT)
    print('SAVED', OUT)


if __name__ == '__main__':
    build()
