package com.example.test;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.Path;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;

/**
 * 重量趋势折线图：保存最近 N 个采样点，画出走势 + 渐变填充。
 * 输液过程中重量持续下降，这条线会稳定往下走 —— 演示时很直观。
 */
public class TrendView extends View {

    private static final int CAP = 60;          // 最多保留 60 个点

    private final float[] vals = new float[CAP];
    private int count = 0;
    private int head = 0;                        // 环形缓冲的写入位置

    private final Paint grid  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint line  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fill  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txt   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Path  path  = new Path();
    private final Path  area  = new Path();
    private final float d;

    public TrendView(Context c) { this(c, null); }

    public TrendView(Context c, AttributeSet a) {
        super(c, a);
        d = getResources().getDisplayMetrics().density;

        grid.setColor(0xFF1E3048);
        grid.setStrokeWidth(1 * d);

        line.setColor(0xFF35D0BA);
        line.setStrokeWidth(2.2f * d);
        line.setStyle(Paint.Style.STROKE);
        line.setStrokeCap(Paint.Cap.ROUND);
        line.setStrokeJoin(Paint.Join.ROUND);

        fill.setStyle(Paint.Style.FILL);

        txt.setColor(0xFF607894);
        txt.setTextSize(9 * d);
        txt.setTextAlign(Paint.Align.RIGHT);
    }

    /** 追加一个采样值（克） */
    public void add(float v) {
        vals[head] = v;
        head = (head + 1) % CAP;
        if (count < CAP) count++;
        invalidate();
    }

    public void clearData() {
        count = 0;
        head = 0;
        invalidate();
    }

    private float at(int i) {
        int start = (head - count + CAP) % CAP;
        return vals[(start + i) % CAP];
    }

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        float padL = 4 * d, padR = 34 * d, padT = 6 * d, padB = 6 * d;
        float gw = w - padL - padR, gh = h - padT - padB;

        // 网格
        for (int i = 0; i <= 3; i++) {
            float y = padT + gh * i / 3f;
            c.drawLine(padL, y, padL + gw, y, grid);
        }

        if (count < 2) {
            c.drawText("等待数据…", w / 2f, h / 2f, txt);
            return;
        }

        float min = Float.MAX_VALUE, max = -Float.MAX_VALUE;
        for (int i = 0; i < count; i++) {
            float v = at(i);
            if (v < min) min = v;
            if (v > max) max = v;
        }
        if (max - min < 5f) {          // 数据几乎不变时给一个最小跨度，避免线贴边
            float mid = (max + min) / 2f;
            min = mid - 2.5f;
            max = mid + 2.5f;
        }

        path.reset();
        area.reset();
        for (int i = 0; i < count; i++) {
            float x = padL + gw * i / (float) (CAP - 1);
            float y = padT + gh * (1f - (at(i) - min) / (max - min));
            if (i == 0) { path.moveTo(x, y); area.moveTo(x, y); }
            else        { path.lineTo(x, y); area.lineTo(x, y); }
        }
        area.lineTo(padL + gw * (count - 1) / (float) (CAP - 1), padT + gh);
        area.lineTo(padL, padT + gh);
        area.close();

        fill.setShader(new LinearGradient(0, padT, 0, padT + gh,
                0x5535D0BA, 0x0035D0BA, Shader.TileMode.CLAMP));
        c.drawPath(area, fill);
        c.drawPath(path, line);

        // 右侧刻度
        c.drawText(String.format("%.0f", max), w - 2 * d, padT + 8 * d, txt);
        c.drawText(String.format("%.0f", min), w - 2 * d, padT + gh, txt);
    }
}
