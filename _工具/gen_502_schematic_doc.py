# -*- coding: utf-8 -*-
"""生成《502原理图与PCB设计要点.docx》"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from docx import Document
from docx.shared import Cm
from docx.enum.text import WD_ALIGN_PARAGRAPH

from docx_common import (new_doc, title, h1, h2, para, bullets, add_table, add_box,
                         add_footer, GREY)

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '文档', '502原理图与PCB设计要点.docx')
IMG = os.path.join(BASE, '文档', '_图_502接线原理图.png')

NETS = [
    ('3V3', 'STM32 的 3V3 排针 → OLED VCC / DS18B20 VDD / 蜂鸣器 VCC / HX711 VCC / 红外 VCC / 3 个上拉电阻的一端', '3.3V 主轨'),
    ('GND', '所有模块 GND + ESP32 GND + USB-TTL GND + 按键公共端', '★ 必须全部连通'),
    ('I2C_SCL', 'PB6 — OLED SCL — 4.7kΩ 到 3V3', '软件 I2C，速率约 25kHz'),
    ('I2C_SDA', 'PB7 — OLED SDA — 4.7kΩ 到 3V3', '同上'),
    ('DS_DQ', 'PA0 — DS18B20 黄线 — 4.7kΩ 到 3V3', '单总线'),
    ('BUZZ', 'PB0 — 蜂鸣器模块 I/O', '低电平响'),
    ('LED_R', 'PB1 — 220Ω — 红 LED 长脚（短脚接 GND）', '高电平亮'),
    ('LED_Y', 'PB5 — 220Ω — 黄 LED 长脚（短脚接 GND）', '高电平亮'),
    ('KEY1~KEY4', 'PA1 / PA4 / PA5 / PA6 — 各接一个按键 — 另一端全部接 GND', '内部上拉，按下为低'),
    ('HX_SCK', 'PB10 — HX711 SCK', '时钟'),
    ('HX_DT', 'PB11 — HX711 DT(DOUT)', '数据，代码带超时'),
    ('IR_OUT', 'PB12 — 红外模块 OUT（若模块开漏，另加 10kΩ 上拉到 3V3）', '外部中断数滴'),
    ('USART2_TX', 'PA2 — ESP32-C3 GPIO6（模块 RX）', '★ 交叉'),
    ('USART2_RX', 'PA3 — ESP32-C3 GPIO7（模块 TX）', '★ 交叉'),
    ('USART1_TX', 'PA9 — USB-TTL 的 RXD', '★ 交叉'),
    ('USART1_RX', 'PA10 — USB-TTL 的 TXD', '★ 交叉'),
    ('PC13', '板载 LED（模块自带，不引出）', '心跳指示'),
]

RES = [
    ('4.7kΩ', '2 个', 'OLED 的 SCL、SDA 上拉到 3V3', '缺了屏幕一定全黑（项目1 为此卡过一整天）'),
    ('4.7kΩ', '1 个', 'DS18B20 的 DQ 上拉到 3V3', '缺了常读 85℃ 或读不到'),
    ('220Ω', '2 个', '红 / 黄 LED 限流', '不串电阻会烧 IO 或 LED'),
    ('10kΩ', '1 个（备用）', '红外模块 OUT 上拉', '只有模块是开漏输出且板上没上拉时才需要'),
    ('100µF', '1 个（可选）', '3V3 与 GND 之间去耦', '蜂鸣器响、ESP32 发射时抗电压跌落'),
    ('—', '不需要', '按键外部上拉', 'STM32 内部上拉 40kΩ 已够用'),
    ('—', '不需要', 'HX711 外围', '模块上已经带好了（含稳压与滤波）'),
]

PCB = [
    ('板子尺寸', '建议 100×80mm 起，四角留 M3 安装孔', '要放下 2 个排母 + 4 个按键 + 5 个模块接口'),
    ('STM32 排母', '两条 1×20P 单排母，两排间距按实物量（约 15mm）', '★ 不能用 2×20P 双排母，针距 2.54mm 插不上'),
    ('ESP32 排母', '两条 1×16P 单排母，间距按实物量（约 17.5mm）', '★ 以实物为准，画板前必须量'),
    ('天线净空', 'ESP32 天线正下方与前方 15mm 内不铺铜、不走线', '否则 WiFi 距离骤降'),
    ('电源走线', '3V3 主干 ≥ 20mil，GND 整板铺铜', '蜂鸣器与 ESP32 是大电流节点'),
    ('上拉电阻', '3 个 4.7kΩ 尽量靠近 MCU 与 OLED 座', '走线越短，I2C 越稳'),
    ('按键', '4 个 6×6 按键放板边，间距 ≥ 15mm', '手指按得下'),
    ('红外接口', '3P 排母放板边，方便夹滴壶；或改飞线引出', '模块要贴着滴壶，位置受结构限制'),
    ('称重接口', 'HX711 用 4P 排母上板；传感器 4 线走端子出板', '传感器装在板外挂架上，称重不能压在板上'),
    ('USB 口', 'STM32 Micro USB 与 ESP32 Type-C 朝同一板边', '两个都要能插'),
    ('丝印', '每个引脚标网络名（PB6/PB7/…），接口标模块名', '调试时不用翻文档'),
]

CHECK = [
    '两张排母间距是**量出来的**，不是照抄文档',
    'ESP32 天线下方没有铺铜、没有走线',
    '3V3 与 GND 在原理图里没有短路（DRC 通过）',
    '所有上拉电阻都在位（3 个 4.7kΩ）',
    '两个 LED 都串了 220Ω',
    '按键另一端接的是 GND，不是 3V3',
    '红外接口的 3 根线顺序与模块丝印一致（VCC/GND/OUT）',
    'HX711 的 VCC 接 3V3（不是 5V）',
    'ESP32 的电源与主板 3V3 完全隔离，只共地',
    '每个接口旁都有丝印标注',
]


def build():
    doc = new_doc()
    title(doc,
          '502 原理图与 PCB 设计要点',
          '接线级原理图 + 网表 + 阻容清单 + 布局规则 + 打板前检查',
          'v1.0 · 2026-10-22')

    h1(doc, '一、接线原理图')

    if os.path.exists(IMG):
        p = doc.add_paragraph()
        p.alignment = WD_ALIGN_PARAGRAPH.CENTER
        p.add_run().add_picture(IMG, width=Cm(17.0))
        para(doc, '图 1  502 接线原理图（模块级）', size=9, color=GREY, align='center')
    else:
        para(doc, '（图片缺失：请先运行 _工具/gen_schematic.py）', color=GREY)

    add_box(doc, [
        '本图是"模块级"原理图：每个模块（OLED、DS18B20、HX711、ESP32…）在图上是一个方框，',
        '画 PCB 时它们在原理图里对应的是**排母符号**，不是芯片本身。',
        '这种画法的好处：模块可以插拔更换，坏了直接换件，不用拆焊。',
    ], fill='F2F5F9', border='9AA7B4', title_text='这张图怎么用')

    h1(doc, '二、网表（画 PCB 时按这个连）')
    add_table(doc, ['网络名', '连接点', '说明'], NETS,
              [3.0, 10.4, 3.6], font_pt=8, first_col_bold=True)

    h1(doc, '三、阻容清单：每个电阻是干什么的')
    add_table(doc, ['阻值', '数量', '位置', '不装会怎样'], RES,
              [2.6, 2.6, 6.4, 5.4], font_pt=8.5, first_col_bold=True)

    add_box(doc, [
        '★ 一共只有 **6 个元件**要焊：3 个 4.7kΩ + 2 个 220Ω（+1 个可选的 10kΩ）。',
        '其余全是模块与排母，插上去就行 —— 这也是为什么这块板子焊接量很小。',
    ], title_text='一句话：这块板子上真正需要"焊"的只有 5~6 个电阻')

    h1(doc, '四、PCB 布局规则（照这个摆，别自己发挥）')
    add_table(doc, ['项', '规则', '为什么'], PCB,
              [3.0, 8.4, 5.6], font_pt=8, first_col_bold=True)

    h1(doc, '五、画板步骤（立创EDA）')
    bullets(doc, [
        '① 打开立创EDA（本机已装：T:\\TOOL\\download\\lceda-pro），新建工程 → 原理图',
        '② 原理图里每个模块放一个**排母符号**：STM32 用 2 个 HDR-1X20，ESP32 用 2 个 HDR-1X16，'
        'OLED 1×4，蜂鸣器 1×3，HX711 1×4（MCU 侧）+1×4（传感器侧），红外 1×3',
        '③ 按本文第二节的网表连线；上拉与限流电阻放在对应网络旁',
        '④ 转 PCB：先摆模块（两个排母 + 4 个按键 + 5 个接口），再走线，最后铺铜',
        '⑤ 跑 DRC（设计规则检查），确认无短路/无未连接',
        '⑥ 导出 Gerber → 嘉立创下单（5×5cm 内常有免费额度，具体规则以当期活动为准）',
    ], size=9.5)

    add_box(doc, [
        '★ 画板前**必须**做的一件事：拿卡尺量两个模块的实际尺寸与排针间距。',
        '文档里写的 15mm / 17.5mm 是项目1 的实测值，**换一批货可能就不一样**。',
        '没有卡尺的话：把模块放在纸上描出针脚位置，再用钢板尺量——误差 1mm 内可用。',
    ], fill='FFF6DC', border='BF8F00', title_text='最容易被忽略、也最容易翻车的一步')

    h1(doc, '六、打板前检查清单（逐条打勾）')
    add_table(doc, ['#', '检查项', '☐'],
              [(str(i + 1), t, '') for i, t in enumerate(CHECK)],
              [0.9, 14.6, 1.5], font_pt=9, aligns=['center', 'left', 'center'])

    h1(doc, '七、诚实说明')
    para(doc, '本文的排母间距、模块尺寸来自项目1 的实测记录，**不是本次实物测量**；'
              '网表与引脚分配已与代码逐条对应（11 个固件均按此编写并通过编译），'
              '但**整块板尚未制作、未上电验证**。打板前请按第五节末尾的方法实测尺寸。',
         size=9.5)

    add_footer(doc, '502 原理图与 PCB 设计要点 v1.0')
    doc.save(OUT)
    print('SAVED', OUT)


if __name__ == '__main__':
    build()
