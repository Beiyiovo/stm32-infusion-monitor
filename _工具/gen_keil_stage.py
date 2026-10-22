"""按模板批量生成各阶段的 Keil 工程（.uvprojx）。

模板：代码/02_oled/502_02_oled.uvprojx（已验证可编译）
只替换 <OutputName> 与 <Files> 块，其余（器件、编译选项、优化等级）全部沿用。

用法：python gen_keil_stage.py
"""
import os
import re

BASE = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CODE = os.path.join(BASE, '代码')
TPL = os.path.join(CODE, '02_oled', '502_02_oled.uvprojx')

C = r'..\_common'

STAGES = {
    '03_ds18b20': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                   ('ds18b20.c', C + r'\ds18b20.c')],
    '04_key': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
               ('app502.c', C + r'\app502.c')],
    '05_alert': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                 ('app502.c', C + r'\app502.c'), ('alert502.c', C + r'\alert502.c')],
    '06_hx711': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                 ('app502.c', C + r'\app502.c'), ('hx711.c', C + r'\hx711.c')],
    '07_drop': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                ('app502.c', C + r'\app502.c'), ('drop.c', C + r'\drop.c'),
                ('stm32f10x_exti.c', C + r'\lib\stm32f10x_exti.c')],
    '08_tcp': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
               ('app502.c', C + r'\app502.c'), ('ds18b20.c', C + r'\ds18b20.c'),
               ('hx711.c', C + r'\hx711.c'), ('drop.c', C + r'\drop.c'),
               ('alert502.c', C + r'\alert502.c'), ('report_tcp.c', C + r'\report_tcp.c'),
               ('i2c.c', C + r'\i2c.c'), ('oled.c', C + r'\oled.c'),
               ('cnfont.c', C + r'\cnfont.c'),
               ('stm32f10x_exti.c', C + r'\lib\stm32f10x_exti.c')],
    '09_main': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                ('app502.c', C + r'\app502.c'), ('ds18b20.c', C + r'\ds18b20.c'),
                ('hx711.c', C + r'\hx711.c'), ('drop.c', C + r'\drop.c'),
                ('alert502.c', C + r'\alert502.c'), ('report_tcp.c', C + r'\report_tcp.c'),
                ('i2c.c', C + r'\i2c.c'), ('oled.c', C + r'\oled.c'),
                ('cnfont.c', C + r'\cnfont.c'),
                ('stm32f10x_exti.c', C + r'\lib\stm32f10x_exti.c')],
    '10_selftest': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                    ('app502.c', C + r'\app502.c'), ('ds18b20.c', C + r'\ds18b20.c'),
                    ('i2c.c', C + r'\i2c.c'), ('oled.c', C + r'\oled.c'),
                    ('cnfont.c', C + r'\cnfont.c')],
    '11_calib': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
                 ('app502.c', C + r'\app502.c'), ('hx711.c', C + r'\hx711.c'),
                 ('i2c.c', C + r'\i2c.c'), ('oled.c', C + r'\oled.c'),
                 ('cnfont.c', C + r'\cnfont.c')],
    '12_led': [('main.c', r'.\main.c'), ('bsp.c', C + r'\bsp.c'),
               ('app502.c', C + r'\app502.c')],
}

FILE_TPL = """            <File>
              <FileName>{name}</FileName>
              <FileType>1</FileType>
              <FilePath>{path}</FilePath>
            </File>
"""


def main():
    with open(TPL, encoding='utf-8') as f:
        tpl = f.read()

    for stage, files in STAGES.items():
        out_name = '502_' + stage
        block = '<Files>\n' + ''.join(
            FILE_TPL.format(name=n, path=p) for n, p in files) + '            </Files>'
        xml = re.sub(r'<Files>.*?</Files>', lambda m: block, tpl, flags=re.S)
        xml = xml.replace('502_02_oled', out_name)

        stage_dir = os.path.join(CODE, stage)
        os.makedirs(stage_dir, exist_ok=True)
        out = os.path.join(stage_dir, out_name + '.uvprojx')
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write(xml)
        print('OK', stage, '->', out_name + '.uvprojx', '|', len(files), 'files')


if __name__ == '__main__':
    main()
