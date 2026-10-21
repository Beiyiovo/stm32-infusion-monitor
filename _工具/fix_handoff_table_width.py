# -*- coding: utf-8 -*-
"""修正《项目交接说明.docx》里那张原就超宽的选型对比表（表格 #3，宽 18.4cm > 可用 17.19cm）。

只改列宽，不动内容。可重复执行。
"""
import os
from docx import Document
from docx.shared import Cm
from docx.oxml.ns import qn

DOC = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   '文档', '项目交接说明.docx')
NEW_W = [2.4, 3.8, 6.4, 4.4]  # = 17.0cm，页可用 17.19cm


def main():
    doc = Document(DOC)
    usable = (doc.sections[0].page_width - doc.sections[0].left_margin - doc.sections[0].right_margin) / 360000
    fixed = []
    for idx, t in enumerate(doc.tables):
        cur = sum((c.width.cm if c.width else 0) for c in t.rows[0].cells)
        if cur <= usable + 0.05:
            continue
        if len(t.columns) != len(NEW_W):
            print('SKIP table', idx, 'cols', len(t.columns), 'width %.2f' % cur)
            continue
        t.autofit = False
        for row in t.rows:
            for ci, w in enumerate(NEW_W):
                row.cells[ci].width = Cm(w)
        grid = t._tbl.find(qn('w:tblGrid'))
        if grid is not None:
            for ci, gc in enumerate(grid.findall(qn('w:gridCol'))):
                gc.set(qn('w:w'), str(int(NEW_W[ci] * 567)))
        fixed.append((idx, round(cur, 2), sum(NEW_W)))
    doc.save(DOC)
    print('usable %.2fcm | fixed:' % usable, fixed)


if __name__ == '__main__':
    main()
