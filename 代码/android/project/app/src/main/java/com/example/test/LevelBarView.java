package com.example.test;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.LinearGradient;
import android.graphics.Paint;
import android.graphics.RectF;
import android.graphics.Shader;
import android.util.AttributeSet;
import android.view.View;

/**
 * 横向进度条（用于"剩余重量"的液位显示）
 * 自带渐变填充，纯自绘。
 */
public class LevelBarView extends View {

    private final Paint bg   = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint fill = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final RectF r    = new RectF();
    private float pct = 0f;          // 0~1
    private final float d;
    private int colorFrom = 0xFF2E7DE0;
    private int colorTo   = 0xFF35D0BA;

    public LevelBarView(Context c) { this(c, null); }

    public LevelBarView(Context c, AttributeSet a) {
        super(c, a);
        d = getResources().getDisplayMetrics().density;
        bg.setColor(0xFF1B2A40);
        fill.setColor(colorTo);
    }

    public void setColors(int from, int to) {
        colorFrom = from; colorTo = to; invalidate();
    }

    /** p: 0~1 */
    public void setPercent(float p) {
        if (p < 0) p = 0; if (p > 1) p = 1;
        pct = p; invalidate();
    }

    @Override
    protected void onDraw(Canvas c) {
        float h = getHeight();
        float rad = h / 2f;
        r.set(0, 0, getWidth(), h);

        c.drawRoundRect(r, rad, rad, bg);

        if (pct > 0.001f) {
            float right = getWidth() * pct;
            if (right < h) right = h;               // 至少画成圆形
            RectF fr = new RectF(0, 0, right, h);
            fill.setShader(new LinearGradient(0, 0, getWidth(), 0,
                    colorFrom, colorTo, Shader.TileMode.CLAMP));
            c.drawRoundRect(fr, rad, rad, fill);
        }

        // 右侧 8% 处画一条高光，做出"玻璃"感
        Paint gloss = new Paint(Paint.ANTI_ALIAS_FLAG);
        gloss.setColor(0x18FFFFFF);
        RectF gr = new RectF(h * 0.2f, h * 0.18f, getWidth() - h * 0.2f, h * 0.42f);
        c.drawRoundRect(gr, h * 0.12f, h * 0.12f, gloss);
    }
}
