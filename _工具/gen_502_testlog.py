# -*- coding: utf-8 -*-
"""生成《502测试记录.docx》—— 全部数据来自 2026-10-22/24 的实际串口与实测。"""
import os
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from docx_common import (new_doc, title, h1, h2, para, bullets, add_table, add_box,
                         add_footer, add_code, GREY)

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(BASE, '测试记录', '502测试记录.docx')

ENV = [
    ('主控', 'STM32F103C8T6 最小系统板（排母插装）'),
    ('无线', 'ESP32-C3 CORE 开发板（AT 固件 V4.1.1.0，排母插装）'),
    ('显示', '0.96" OLED，SSD1306，I2C，地址 0x3C'),
    ('温度', 'DS18B20（三线，4.7kΩ 上拉）'),
    ('称重', '1 kg 电阻应变式传感器 + HX711 模块'),
    ('数滴', '槽型/对射红外模块（**测试时尚未到货**）'),
    ('烧录', 'CH340 USB-TTL，串口 ISP，波特率 38400'),
    ('软件', 'Keil MDK 5.42 / ARMCLANG V6.23 · Android App（自研）'),
]

STEPS = [
    ('T-01', '10-22', '接线自检 10_selftest', '烧录后复位，观察串口与屏幕',
     'I2C 扫到 0x1E/0x5E；OLED(0x3C) 在线 ✔',
     'I2C 总线与 OLED 接线正确'),
    ('T-02', '10-22', '温度采集', '手捂探头观察读数变化',
     '34.2 → 35.2℃ 连续变化 ✔', 'DS18B20 单总线通信正常'),
    ('T-03', '10-22', 'LED 专项 12_led', '三个相位各 2 秒',
     '红、黄单独亮均正常；同时亮正常', '两路 LED 与限流电阻接线正确'),
    ('T-04', '10-22', 'ESP32 上电握手', '烧 13_ui / 08_tcp 观察串口',
     'AT ✔ / CWMODE ✔ / CWSAP ✔ / CIPMUX ✔ / CIPSERVER ✔',
     'AT 固件与串口接线（PA2→GPIO6、PA3→GPIO7）正确'),
    ('T-05', '10-22', '手机接入', '手机连热点 502-Infusion 后打开 App',
     '串口打印「客户端 #0 已连接」，App 收到数据帧', 'TCP 链路端到端打通'),
    ('T-06', '10-24', '称重标定 11_calib', '空秤 KEY1 → 放 500ml 水 → KEY2×2 → KEY4',
     'ZERO=764832；SCALE=801.990 / 801.920 counts/g（两次偏差 0.009%）',
     '称重链路可用，重复性良好'),
    ('T-07', '10-24', '称重复核', '用 193 g 手机实测',
     '读数 189.1 g，偏小 2.0%', '标定基准含容器重量，需砝码复标'),
    ('T-08', '10-24', '四键功能', 'K1/K2/K3/K4 逐项操作',
     '切页、静音、进设置、加减、长按去皮均正常', '按键扫描与消抖逻辑正确'),
]

BUGS = [
    ('K4 完全无反应', 'app502.c 的 key4_event() 未调用 key4_scan()，扫描从不执行',
     '在 key4_event() 开头补 key4_scan()'),
    ('OLED 显示「半个字」', '每帧未清显存，切换界面后旧内容残留在未重写的页',
     '三个绘制函数开头加 oled_clear()'),
    ('红灯闪不到 2Hz', 'alert502_update() 只在每秒的采集块里调用，LED 翻转被限速',
     '移到主循环每轮调用'),
    ('11_calib 的 KEY2 无效', 'key_any_just_pressed() 读后即清，同一循环调两次',
     '只读一次存入变量再比较'),
    ('KEY4 定系数无响应', '只判 event==1（短按），按久了返回 2（长按）被丢弃',
     '改为事件非 0 即接受，并打印事件码'),
    ('开机重量显示 764.8 g', '上电时零点 offset=0，raw/scale 直接当重量',
     '开机自动去皮（50 次平均）'),
    ('串口助手打开即崩溃', '脚本含 ✓ 字符，GBK 控制台无法编码',
     '启动时把 stdout/stderr 重设为 UTF-8'),
]

TODO = [
    ('红外数滴', '传感器未到货', '到货后烧 07_drop 验证计数与滴速'),
    ('称重 ±2 g', '当前偏小 2.0%（基准含容器）', '砝码到货后按公式复标'),
    ('滴速过快报警', '依赖红外输入', '红外到位后一并验证'),
    ('PCB 打样', '已布完线，DRC 0 错误', '待打样后复测'),
]


def build():
    doc = new_doc()
    title(doc,
          '502 输液状态监测装置 · 测试记录',
          '本文件记录 2026-10-22 ~ 10-24 期间的真实测试过程与数据',
          'v1.0 · 谢玲 2352500401')

    add_box(doc, [
        '本记录中的数据全部来自实际硬件的串口输出与人工观察，**不是设计预期值**。',
        '未完成的测试项在最后一节如实列出，未做推断。',
    ], title_text='阅读说明')

    h1(doc, '一、测试环境')
    add_table(doc, ['项', '配置'], ENV, [3.4, 13.6], font_pt=8.5, first_col_bold=True)

    h1(doc, '二、测试记录')
    add_table(doc, ['编号', '日期', '项目', '操作', '结果', '结论'], STEPS,
              [1.2, 1.0, 3.0, 4.2, 4.2, 3.4], font_pt=7.5,
              aligns=['center', 'center', 'left', 'left', 'left', 'left'])

    h1(doc, '三、称重标定实测数据')

    h2(doc, '3.1 原始数据（串口实录）')
    add_code(doc, [
        '[BOOT] 502 称重标定固件',
        '[CAL] ★ ZERO = 764832',
        '[CAL] 标定重量 → 200 g',
        '[CAL] 标定重量 → 500 g',
        '[KEY] KEY4 事件=2',
        '[CAL] AVG = 1165827, CAL = 500 g',
        '[CAL] SCALE = 801.990 counts/g',
        '[KEY] KEY4 事件=2',
        '[CAL] AVG = 1165792, CAL = 500 g',
        '[CAL] SCALE = 801.920 counts/g',
    ], font_pt=9)

    h2(doc, '3.2 数据处理')

    add_table(doc, ['项', '数值', '说明'],
              [('零点 offset', '764832', '空秤，20 次平均'),
               ('加载后原始值', '1165827', '500 ml 水（含容器，约 510 g）'),
               ('两次系数差', '0.070 counts/g', '相对偏差 0.009%，重复性好'),
               ('系统采用值', '802.0 counts/g', '取中间值写入 hx711.h'),
               ('复核（193 g 手机）', '189.1 g', '偏小 2.0%，与容器未计入吻合')],
              [4.6, 4.4, 8.0], font_pt=8.5, first_col_bold=True)

    add_box(doc, [
        '误差来源已经定位：标定时把 500 ml 水当作 500 g，但容器本身约 10 g，',
        '真实质量约 510 g，导致系数偏大 2%。',
        '复标公式：新系数 = 旧系数 × (装置读数 ÷ 砝码真实重量)',
    ], fill='FFF6DC', border='BF8F00', title_text='偏差说明')

    h1(doc, '四、测试中发现的问题与修复')
    para(doc, '以下问题均在实测中暴露并已修复，修复后重新编译验证（13 个固件 0 Error 0 Warning）。',
         size=9)
    add_table(doc, ['问题', '原因', '修复'], BUGS,
              [4.2, 7.4, 5.4], font_pt=8, first_col_bold=True)

    h1(doc, '五、尚未完成的测试项')
    add_table(doc, ['项', '当前状态', '后续计划'], TODO,
              [3.4, 6.4, 7.2], font_pt=8.5, first_col_bold=True)

    add_footer(doc, '502 输液状态监测装置 · 测试记录 v1.0')
    doc.save(OUT)
    print('SAVED', OUT)


if __name__ == '__main__':
    build()
