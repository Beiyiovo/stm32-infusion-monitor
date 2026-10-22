# -*- coding: utf-8 -*-
"""生成 502采购清单(可勾选).xlsx"""
import os
from openpyxl import Workbook
from openpyxl.styles import Font, PatternFill, Alignment, Border, Side
from openpyxl.worksheet.datavalidation import DataValidation

OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   '文档', '502采购清单(可勾选).xlsx')

NAVY = '1F3B60'
LIGHT = 'EAF0F7'
ZEBRA = 'F4F7FB'
YELLOW = 'FFF6DC'
RED = 'FBE4E4'
GREEN = 'E4F2E7'

thin = Side(style='thin', color='B7C2CE')
BORDER = Border(left=thin, right=thin, top=thin, bottom=thin)

F_TITLE = Font(name='微软雅黑', size=15, bold=True, color=NAVY)
F_SUB = Font(name='微软雅黑', size=9, color='606B75')
F_HDR = Font(name='微软雅黑', size=9.5, bold=True, color='FFFFFF')
F_BODY = Font(name='微软雅黑', size=9)
F_BOLD = Font(name='微软雅黑', size=9, bold=True)

WRAP = Alignment(horizontal='left', vertical='center', wrap_text=True)
CENTER = Alignment(horizontal='center', vertical='center', wrap_text=True)

# ---------------------------------------------------------------- 数据
# (优先级, 类别, 买什么, 搜索关键词, 数量, 低, 高, 为什么必须买, 关键要求与坑)
BOM = [
    ('★必须', '称重', '称重传感器（悬臂梁 1kg · 全桥 4 线）',
     '称重传感器 悬臂梁 1kg 电子秤 人体秤', '1 个', 8, 15,
     '课题要求 2「称重精度 ≤±2g」的唯一信号来源，没有它整条重量链路做不了。',
     '★要【全桥 4 线】不要半桥；量程 1kg —— 已确定按 500ml 袋设计：袋约 550g + 挂具 ≤0.25kg ≈ 0.8kg，留约 20% 余量；'
     '★不要图省事买 5kg 或 2kg：线性度/迟滞按 0.05%FS 估，1kg≈0.5g、2kg≈1.0g、5kg≈2.5g，5kg 单项就吃掉 ±2g 全部余量'),
    ('★必须', '称重', 'HX711 24 位称重 ADC 模块',
     'HX711 称重传感器 24位AD 模块', '1 个', 5, 8,
     '传感器满量程输出只有约 4.3mV（1mV/V × 4.3V），STM32 的 12 位 ADC 根本分辨不出 ±2g；HX711 是专用 24 位前端。',
     '选带【RATE 跳线（10Hz/80Hz）】的绿板；与传感器打包买通常更便宜；到货先量 VCC-GND 是否短路'),
    ('★必须', '主控无线', 'ESP32-C3 核心板（CORE-ESP32，与项目1 同型号）',
     'ESP32-C3 核心板 Type-C 4MB WIFI 蓝牙', '1 块', 15, 25,
     '项目1 唯一一块已焊在自己 PCB 上（B2：没有备用），拆了等于毁掉项目1，只能买。',
     '★必须【同型号 51×21mm、2×16 针】——只有这样，烧项目1 那份 AT 固件才能百分百复现；'
     '到货需烧固件，bin 已在 T:\\TE\\ESP32-C3-MINI-1-AT-V4.1.1.0\\，不用联网'),
    ('★必须', '显示', '0.96 寸 OLED（I2C SSD1306 · 4 针）',
     '0.96寸 OLED I2C SSD1306 4针 白色', '1 个', 10, 15,
     '课题要求 4「OLED 实时显示温度与重量」；项目1 唯一一块已装在自己板上。',
     '★4 针 I2C，别买 7 针 SPI；建议白色（病房/台灯下比蓝色清楚）；到货先单独点亮再焊'),
    ('★必须', '报警', '有源蜂鸣器模块（低电平触发）',
     '有源蜂鸣器模块 低电平触发 3.3V 5V', '1 套', 4, 8,
     '课题要求 3「蜂鸣器报警」；项目1 唯一一套已装在自己板上。',
     '★要【模块】不是裸蜂鸣器；【有源】（给电就响，不用 PWM）；【低电平触发】；'
     '★VCC 接 3.3V 不接 5V —— 项目1 实测 S8550 PNP 在 5V 下关不彻底'),
    ('★必须', '报警', '红色 LED + 黄色 LED（5mm 直插）',
     '5mm LED 直插 红 黄 发光二极管', '各 5 只', 2, 4,
     '课题要「黄/红两级分级报警」，项目1 只买了红色，还差黄色；多买几只备用。',
     '若电阻电容套装里已含 5mm 红/黄 LED，可先不买；限流 220Ω（套装里有）'),
    ('★必须', '标定', '标准砝码 100g（或 200g/500g 组合），或借实验室电子天平',
     '标准砝码 100g 校准砝码', '1 套', 10, 25,
     '★★ 没有已知质量基准，就无法证明 ±2g 达标。这是项目1 用血氧仪/体温计当基准的同类缺失，原清单漏了。',
     '至少一个 100g；普通商用砝码即可，不需要 M1 级；能借到电子天平/分析天平更好，可省这笔'),
    ('建议', '人机', '轻触按键 6×6×5（补 2 个）',
     '轻触开关 6x6x5 直插', '2 个', 1, 1,
     '项目1 买 6 个用掉 3 个，本项目要 4 个（设置/加/减/确认），差 1 个。',
     '先用手头剩的 3 个；这 2 个当备份（手感不一致时成套换）'),
    ('★必须', '滴速', '红外对射（分体式：发射 + 接收各一个）',
     '红外对射 对管 光电传感器 模块 3.3V 5V', '1 套', 5, 15,
     '滴速是课题要求之一（2026-10-22 导师确认要做）；每落一滴遮断一次光路，靠外部中断计数。',
     '★必须【分体式】发射+接收各一个 —— 槽型光耦不可用：槽口约 5mm，滴壶直径约 20mm 卡不进去；'
     '★规格：3.3~5V 供电、带比较器【数字输出】、检测距离 ≥10cm、每头带 2.54 排针（发射 2 针、接收 3 针）；'
     '固定用泡棉+夹子并加遮光罩防环境光；若卖家有【夹扣式输液滴速传感器】成品也可考虑，但要先确认接口电平'),
    ('可选', '结构', '输液袋挂具/挂钩 + 硬质支架',
     '实验支架 挂钩 输液架 桌面', '1 套', 0, 30,
     '称重必须让袋子垂直受力，不能拿手提着测，否则读数抖到没法看。',
     '可以先用洞洞板 + 铜柱手工搭一个，不一定花钱；先看结构需求再决定'),
    ('可选', 'PCB', '排母座 2.54mm（1×20P×2、1×16P×2、1×4P、1×3P）',
     '排母 2.54 单排 圆孔 直插', '6 条', 8, 15,
     '只有做 PCB 才需要；面包板阶段一根都不需要（B3 已答「够用」）。',
     '真做 PCB 前再核一遍数量：条数 = 模块的排数，不是"够用就行"'),
    ('可选', '结构', '洞洞板 5×7cm + 铜柱螺丝',
     '洞洞板 5x7 双面 铜柱 M3', '1 套', 12, 22,
     '做成品外壳或承重支架时用。',
     '项目1 那套若没拆可复用；先翻自己的元件盒'),
]

REUSE = [
    ('STM32F103C8T6 开发板', '2 块', '1 块在项目1 板上，1 块闲置', '✅ 可用闲置那块',
     'B1 已确认。⚠️ 这块板没有板载 CH340，只能用 USB-TTL 串口烧（9600 才稳）'),
    ('DS18B20 防水探头', '2 个', '1 个在项目1 板上', '✅ 用另 1 个', '精度 ±0.5℃，正好卡在指标线上，没有余量'),
    ('0.96 寸 OLED', '1 个', '已装项目1 板上', '❌ 不能拆', '拆了项目1 就没显示 → 必须买'),
    ('有源蜂鸣器模块', '1 套', '已装项目1 板上', '❌ 不能拆', '必须买'),
    ('红色 LED', '1 个', '已装项目1 板上', '❌ 不能拆', '而且分级报警还要黄色'),
    ('ESP32-C3 核心板', '1 块', '已装项目1 板上', '❌ 不能拆', '必须买（B2 已确认无备用）'),
    ('轻触按键 6×6×5', '6 个', '项目1 用掉 3 个，剩 3 个', '⚠️ 差 1 个', '本项目要 4 个 → 补 2 个做备份'),
    ('电阻电容套装', '1 套', '有', '✅ 可用', '含 4.7kΩ（上拉）与 220Ω（LED 限流）'),
    ('面包板 830 孔', '1 块', '项目1 可能在用', '⚠️ 看情况', '若项目1 已占板，再买 1 块（¥8~15）；或直接用洞洞板'),
    ('杜邦线 公对公 / 公对母', '各 1 扎', '有', '✅ 可用', '公对母用于 ESP32/OLED 外接，别剪断'),
    ('5V 电源适配器', '2 个', '项目1 用了 2 个', '⚠️ 要补 2 个',
     '本项目要 2 路（STM32 + ESP32）。家里手机充电头/充电宝可直接顶，不计入预算'),
    ('USB-TTL（CH340）', '1 个', '项目1 在用', '⚠️ 两项目轮流用', '烧录/看串口时只能一次连一边，注意别插错口'),
    ('洞洞板 5×7cm', '1 套', '可能已用在项目1', '⚠️ 看情况', '做支架/成品时再翻'),
    ('数字万用表', '1 台', '有', '✅ 可用', '静态检查（电压/短路/通断）全靠它'),
    ('电子体温计', '1 支', '有', '✅ 可用', '温度标定的基准（25/37/42℃ 三点）'),
    ('电烙铁/焊锡/助焊剂/吸锡器/镊子', '各 1', '有', '✅ 可用', '做 PCB/焊排母时才用，面包板阶段不用'),
    ('游标卡尺', '0', '没有', '❌ 暂不买', '用户已决定：靠淘宝商详页尺寸 + 钢板尺量，够用'),
]

ORDER = [
    ('1', '称重传感器 + HX711 + 标准砝码',
     '这三件决定项目能不能达标（±2g），也是到货后最需要时间验证的部分 —— 先买先验，别等其他件'),
    ('2', 'ESP32-C3 + OLED + 蜂鸣器 + 红/黄 LED + 按键',
     '都是"项目1 单件不能拆"的补齐件，同一批下单省运费'),
    ('3', '排母 / 洞洞板 / 支架（可选）',
     '等面包板全通、确定要做 PCB 或成品外壳时再买，避免先买错尺寸'),
]

ACCEPT = [
    ('称重传感器', '万用表量 E+/E- 输入阻抗；手指按压，量 A+/A- 电压变化',
     '输入阻抗约 1kΩ（±200Ω）；按压时 A+/A- 有 mV 级变化', '读数完全不动 = 断线/坏件，直接退'),
    ('HX711 模块', '3.3V 供电，先量 DOUT 电平；再给 SCK 10 个脉冲',
     '上电 DOUT 为高；脉冲后 DOUT 拉低一次（数据就绪）', '常低或常高 = 模块坏，换'),
    ('ESP32-C3 核心板', 'USB-TTL 接 GPIO6/7，115200 发 AT',
     '回 OK（若 Type-C 口回 AT 说明固件在 UART0，需改接法或重烧）', '无回应：先查 BOOT/波特率，再确认固件'),
    ('OLED', 'I2C 地址扫描（0x3C）或直接点亮',
     '地址 0x3C 有应答、屏能点亮', '无应答先查 4.7kΩ 上拉与 SDA/SCL 是否接反'),
    ('蜂鸣器模块', '3.3V 供电，I/O 脚接 GND',
     '响（有源蜂鸣器给电就响）', '不响 = 买成无源的了，换有源'),
    ('红色/黄色 LED', '3.3V 串 220Ω 点亮', '亮，颜色正确', '不亮换一只（LED 便宜，别将就）'),
    ('轻触按键', '万用表通断档，按下/松开', '按下导通、松开断开', '常通 = 卡死，换'),
    ('标准砝码', '在能借到的电子秤上核对', '例如 100g 砝码读数 99.5~100.5g', '偏差大就换，别拿它当基准'),
]


def style_header(ws, row, ncols, fill=NAVY):
    for c in range(1, ncols + 1):
        cell = ws.cell(row=row, column=c)
        cell.font = F_HDR
        cell.fill = PatternFill('solid', fgColor=fill)
        cell.alignment = CENTER
        cell.border = BORDER
    ws.row_dimensions[row].height = 26


def put_title(ws, text, sub, ncols):
    ws.merge_cells(start_row=1, start_column=1, end_row=1, end_column=ncols)
    c = ws.cell(row=1, column=1, value=text)
    c.font = F_TITLE
    c.alignment = Alignment(horizontal='left', vertical='center')
    ws.row_dimensions[1].height = 30
    ws.merge_cells(start_row=2, start_column=1, end_row=2, end_column=ncols)
    c2 = ws.cell(row=2, column=1, value=sub)
    c2.font = F_SUB
    c2.alignment = Alignment(horizontal='left', vertical='center', wrap_text=True)
    ws.row_dimensions[2].height = 30


def build_bom(wb):
    ws = wb.active
    ws.title = '采购清单'
    put_title(ws, '502 输液状态 WiFi 监测装置 —— 采购清单（2026-10-21 定稿）',
              '用法：【已买】列点单元格 → 下拉选 √ → 整行变绿。★ 价格为淘宝同期的估算区间（项目1 清单口径），'
              '未逐项核实当日报价；下单前请按关键词搜一遍实价。', 11)

    headers = ['已买', '#', '优先级', '类别', '买什么', '搜索关键词（淘宝）', '数量',
               '预算低', '预算高', '为什么必须买 / 为什么不能复用项目1', '关键要求与坑']
    hrow = 4
    for i, h in enumerate(headers, start=1):
        ws.cell(row=hrow, column=i, value=h)
    style_header(ws, hrow, len(headers))

    widths = [6, 4, 7, 9, 30, 30, 8, 7, 7, 44, 48]
    for i, w in enumerate(widths, start=1):
        ws.column_dimensions[chr(64 + i)].width = w

    r = hrow + 1
    for idx, (pri, cat, item, kw, qty, low, high, why, trap) in enumerate(BOM, start=1):
        vals = ['', idx, pri, cat, item, kw, qty, low, high, why, trap]
        for ci, v in enumerate(vals, start=1):
            cell = ws.cell(row=r, column=ci, value=v)
            cell.font = F_BODY
            cell.border = BORDER
            cell.alignment = CENTER if ci <= 9 else WRAP
        fill = {'★必须': 'FDF2F2', '建议': YELLOW, '可选': 'F0F0F0'}[pri]
        for ci in range(1, 12):
            ws.cell(row=r, column=ci).fill = PatternFill('solid', fgColor=fill)
        ws.row_dimensions[r].height = 52
        r += 1

    # 合计行
    ws.cell(row=r, column=1, value='')
    ws.merge_cells(start_row=r, start_column=2, end_row=r, end_column=7)
    c = ws.cell(row=r, column=2, value='必买 %d 项合计（元）' % sum(1 for x in BOM if x[0] == '★必须'))
    c.font = F_BOLD
    c.alignment = Alignment(horizontal='right', vertical='center')
    low_sum = sum(x[5] for x in BOM if x[0] == '★必须')
    high_sum = sum(x[6] for x in BOM if x[0] == '★必须')
    for ci, v in ((8, low_sum), (9, high_sum)):
        cell = ws.cell(row=r, column=ci, value=v)
        cell.font = Font(name='微软雅黑', size=10, bold=True, color='B02A2A')
        cell.alignment = CENTER
        cell.fill = PatternFill('solid', fgColor='FBE4E4')
        cell.border = BORDER
    ws.cell(row=r, column=10, value='不含"建议/可选"项；可选件全买再加 ¥25~82').font = F_SUB
    r += 1

    ws.merge_cells(start_row=r, start_column=2, end_row=r, end_column=7)
    c = ws.cell(row=r, column=2, value='全 %d 项合计（元）' % len(BOM))
    c.font = F_BOLD
    c.alignment = Alignment(horizontal='right', vertical='center')
    for ci, v in ((8, sum(x[5] for x in BOM)), (9, sum(x[6] for x in BOM))):
        cell = ws.cell(row=r, column=ci, value=v)
        cell.font = Font(name='微软雅黑', size=10, bold=True)
        cell.alignment = CENTER
        cell.border = BORDER
    r += 2

    ws.merge_cells(start_row=r, start_column=1, end_row=r, end_column=11)
    note = ('★ 预算修订原因：原估「只买传感器+HX711 ≈¥15」是错的 —— 项目1 的 ESP32-C3 / OLED / 蜂鸣器各只买了 1 件，'
            '且都已焊在赖振雄自己的毕设板上，不能拆；项目2 必须独立买齐。另补一项原清单漏掉的【标准砝码】：'
            '没有已知质量基准，「±2g」这条指标无法证明。')
    c = ws.cell(row=r, column=1, value=note)
    c.font = Font(name='微软雅黑', size=9, color='7F5F00')
    c.fill = PatternFill('solid', fgColor=YELLOW)
    c.alignment = Alignment(horizontal='left', vertical='center', wrap_text=True)
    ws.row_dimensions[r].height = 46

    # 已买列下拉
    dv = DataValidation(type='list', formula1='"√"', allow_blank=True)
    ws.add_data_validation(dv)
    dv.add('A5:A%d' % (hrow + len(BOM)))
    ws.freeze_panes = 'A5'
    return ws


def build_reuse(wb):
    ws = wb.create_sheet('可复用核对')
    put_title(ws, '现有元器件核对（按项目1 实际采购 + 已装情况）',
              '★ 关键结论：真正能复用的只有 4 类；OLED / 蜂鸣器 / ESP32 都是单件且已装在项目1 板上，必须买。', 5)
    headers = ['器件', '项目1 买了几件', '项目1 现状', '本项目能否用', '说明']
    for i, h in enumerate(headers, start=1):
        ws.cell(row=4, column=i, value=h)
    style_header(ws, 4, 5)
    for i, w in enumerate([26, 14, 24, 18, 52], start=1):
        ws.column_dimensions[chr(64 + i)].width = w
    r = 5
    for row in REUSE:
        for ci, v in enumerate(row, start=1):
            cell = ws.cell(row=r, column=ci, value=v)
            cell.font = F_BODY
            cell.border = BORDER
            cell.alignment = CENTER if ci in (2, 4) else WRAP
        ok = row[3].startswith('✅')
        fill = GREEN if ok else (RED if row[3].startswith('❌') else YELLOW)
        for ci in range(1, 6):
            ws.cell(row=r, column=ci).fill = PatternFill('solid', fgColor=fill)
        ws.row_dimensions[r].height = 40
        r += 1
    ws.freeze_panes = 'A5'


def build_order(wb):
    ws = wb.create_sheet('采购顺序')
    put_title(ws, '下单顺序（为什么不能一次全买）', '先买决定指标成败的件，再做补齐件，结构件最后买。', 3)
    for i, h in enumerate(['顺序', '买什么', '为什么这个顺序'], start=1):
        ws.cell(row=4, column=i, value=h)
    style_header(ws, 4, 3)
    for i, w in enumerate([8, 44, 70], start=1):
        ws.column_dimensions[chr(64 + i)].width = w
    r = 5
    for row in ORDER:
        for ci, v in enumerate(row, start=1):
            cell = ws.cell(row=r, column=ci, value=v)
            cell.font = F_BODY
            cell.border = BORDER
            cell.alignment = CENTER if ci == 1 else WRAP
        ws.row_dimensions[r].height = 48
        r += 1


def build_accept(wb):
    ws = wb.create_sheet('到货验收')
    put_title(ws, '到货当天验收（不合格当场退，别等焊上去才发现）',
              '★ 原则：每件单独验、单独记录。项目1 的教训是"整机不工作"时无法定位是哪一件坏。', 5)
    for i, h in enumerate(['器件', '怎么验（5 分钟）', '判据（什么算过）', '不合格怎么办', '实测结果（手填）'], start=1):
        ws.cell(row=4, column=i, value=h)
    style_header(ws, 4, 5)
    for i, w in enumerate([20, 40, 40, 32, 20], start=1):
        ws.column_dimensions[chr(64 + i)].width = w
    r = 5
    for row in ACCEPT:
        for ci, v in enumerate(row + ('',), start=1):
            cell = ws.cell(row=r, column=ci, value=v)
            cell.font = F_BODY
            cell.border = BORDER
            cell.alignment = WRAP
        ws.row_dimensions[r].height = 42
        r += 1


def main():
    wb = Workbook()
    build_bom(wb)
    build_reuse(wb)
    build_order(wb)
    build_accept(wb)
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    wb.save(OUT)
    print('SAVED', OUT)


if __name__ == '__main__':
    main()
