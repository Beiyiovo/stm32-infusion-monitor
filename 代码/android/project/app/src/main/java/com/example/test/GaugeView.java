package com.example.test;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.LinearGradient;
import android.graphics.Matrix;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.graphics.SweepGradient;
import android.graphics.Typeface;
import android.util.AttributeSet;
import android.view.View;

/**
 * 环形仪表盘：270° 圆弧 + 渐变色 + 中心大数字
 * 纯自绘，不依赖任何第三方库。
 */
public class GaugeView extends View {

    private final Paint arcBg   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint arcVal  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txtVal  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txtUnit = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint txtLbl  = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF oval    = new RectF();

    private float min = 0f, max = 50f;
    private float value = Float.NaN;
    private String label = "温度";
    private String unit  = "℃";
    private final float d;

    public GaugeView(Context c) { this(c, null); }

    public GaugeView(Context c, AttributeSet a) {
        super(c, a);
        d = getResources().getDisplayMetrics().density;

        arcBg.setStyle(Paint.Style.STROKE);
        arcBg.setStrokeWidth(14 * d);
        arcBg.setStrokeCap(Paint.Cap.ROUND);
        arcBg.setColor(0xFF1B2A40);

        arcVal.setStyle(Paint.Style.STROKE);
        arcVal.setStrokeWidth(14 * d);
        arcVal.setStrokeCap(Paint.Cap.ROUND);

        txtVal.setColor(Color.WHITE);
        txtVal.setTextAlign(Paint.Align.CENTER);
        txtVal.setTypeface(Typeface.create("sans-serif-light", Typeface.NORMAL));

        txtUnit.setColor(0xFF8FA6C4);
        txtUnit.setTextAlign(Paint.Align.CENTER);

        txtLbl.setColor(0xFF8FA6C4);
        txtLbl.setTextAlign(Paint.Align.CENTER);
    }

    public void setRange(float min, float max) {
        this.min = min; this.max = max; invalidate();
    }

    public void setLabel(String label, String unit) {
        this.label = label; this.unit = unit; invalidate();
    }

    public void setValue(float v) {
        this.value = v; invalidate();
    }

    @Override
    protected void onDraw(Canvas c) {
        int w = getWidth(), h = getHeight();
        float pad = 26 * d;
        float size = Math.min(w, h) - pad * 2;
        float cx = w / 2f, cy = h / 2f;
        oval.set(cx - size / 2, cy - size / 2, cx + size / 2, cy + size / 2);

        final float START = 135f, SWEEP = 270f;

        c.drawArc(oval, START, SWEEP, false, arcBg);

        if (!Float.isNaN(value)) {
            float f = (value - min) / (max - min);
            if (f < 0) f = 0; if (f > 1) f = 1;

            int c1 = 0xFF35D0BA, c2 = 0xFFFFC371, c3 = 0xFFFF6B6B;
            SweepGradient sg = new SweepGradient(cx, cy,
                    new int[]{c1, c2, c3}, new float[]{0f, 0.55f, 1f});
            Matrix m = new Matrix();
            m.setRotate(START - 90f, cx, cy);
            sg.setLocalMatrix(m);
            arcVal.setShader(sg);

            c.drawArc(oval, START, SWEEP * f, false, arcVal);
        }

        // 文字
        txtLbl.setTextSize(13 * d);
        c.drawText(label, cx, cy - size * 0.26f, txtLbl);

        txtVal.setTextSize(size * 0.30f);
        String vs = Float.isNaN(value) ? "--.-" : String.format("%.1f", value);
        c.drawText(vs, cx, cy + size * 0.10f, txtVal);

        txtUnit.setTextSize(15 * d);
        c.drawText(unit, cx, cy + size * 0.26f, txtUnit);
    }
}
