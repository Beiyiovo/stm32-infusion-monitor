# -*- coding: utf-8 -*-
"""把交接说明里的量程从 2kg 改成 1kg（用户 2026-10-21 确定按 500ml 袋设计）。可重复执行。"""
import os
from docx import Document

DOC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   '文档', '项目交接说明.docx')

REPL = [
    ('按 ≤1000ml 定 2kg 传感器', '按 500ml 袋定 1kg 传感器'),
    ('只用 500ml → 换 1kg 更准；3000ml 营养袋 → 必须 5kg',
     '若实际要用 1000ml → 换 2kg（余量变薄）；3000ml 营养袋 → 必须 5kg'),
    ('2kg 全桥', '1kg 全桥'),
    ('5kg 的 0.05%FS≈2.5g，单项就吃掉 ±2g 余量',
     '1kg 按 0.05%FS≈0.5g；5kg≈2.5g，会吃掉 ±2g 全部余量'),
]


def main():
    doc = Document(DOC)
    hits = 0
    targets = list(doc.paragraphs)
    for t in doc.tables:
        for row in t.rows:
            for cell in row.cells:
                targets.extend(cell.paragraphs)
    for p in targets:
        for old, new in REPL:
            if old in p.text:
                # 整段替换：把首个含该串的 run 换掉，其余 run 清空
                done = False
                for r in p.runs:
                    if old in r.text:
                        r.text = r.text.replace(old, new)
                        done = True
                        hits += 1
                if not done:
                    # 跨 run：退化为整段重写（会丢该段局部格式，但这里都是纯文本单元格）
                    full = p.text.replace(old, new)
                    for r in p.runs:
                        r.text = ''
                    if p.runs:
                        p.runs[0].text = full
                    hits += 1
    doc.save(DOC)
    print('替换处数:', hits)


if __name__ == '__main__':
    main()
