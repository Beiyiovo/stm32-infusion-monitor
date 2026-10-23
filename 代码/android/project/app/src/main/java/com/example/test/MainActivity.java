package com.example.test;

import android.animation.ObjectAnimator;
import android.animation.ValueAnimator;
import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.view.View;
import android.view.animation.LinearInterpolator;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;
import androidx.core.graphics.Insets;
import androidx.core.view.ViewCompat;
import androidx.core.view.WindowInsetsCompat;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * 502 输液状态监测 —— 手机端 v3
 *
 * 主视觉是「剩余药液重量」，其次液温、滴速；带重量趋势图与剩余时间估算。
 *
 * 链路：手机连 ESP32 热点 502-Infusion（12345678）→ TCP 连 192.168.4.1:8080
 * 帧格式：T=34.2,W=480,D=60,L=0,R=0
 *      L：0 正常 / 1 预警 / 2 报警
 *      R：位掩码 1=温度过高 2=温度过低 4=药液将尽 8=滴速过快
 */
public class MainActivity extends AppCompatActivity {

    private static final String DEFAULT_HOST = "192.168.4.1";
    private static final int    DEFAULT_PORT = 8080;

    /** 满袋按 500 g 估算（500 ml ≈ 500 g，密度按 1 算） */
    private static final float  BAG_GRAMS   = 500f;
    /** 输液器滴系数：20 滴 = 1 ml */
    private static final float  DROPS_PER_ML = 20f;

    private TextView tvConn, tvWeight, tvPercent, tvEstimate, tvDrop, tvTotal, tvState;
    private GaugeView gaugeTemp;
    private LevelBarView barWeight;
    private TrendView trend;
    private View dotDrop, dotConn;
    private Button btnConnect;

    private volatile boolean running = false;
    private volatile boolean connected = false;
    private Thread netThread;
    private Socket socket;
    private ObjectAnimator dropAnim;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private String host = DEFAULT_HOST;
    private int port = DEFAULT_PORT;

    private float lastWeight = -1f;
    private int   totalDrops = 0;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        /* Android 15+ 强制边到边，必须自己让出状态栏/导航栏 */
        final View root = findViewById(R.id.root);
        final int padL = root.getPaddingLeft(), padR = root.getPaddingRight();
        ViewCompat.setOnApplyWindowInsetsListener(root, (v, insets) -> {
            Insets bars = insets.getInsets(WindowInsetsCompat.Type.systemBars());
            v.setPadding(padL, bars.top + 8, padR, bars.bottom + 8);
            return insets;
        });

        tvConn     = findViewById(R.id.tvConn);
        tvWeight   = findViewById(R.id.tvWeight);
        tvPercent  = findViewById(R.id.tvPercent);
        tvEstimate = findViewById(R.id.tvEstimate);
        tvDrop     = findViewById(R.id.tvDrop);
        tvTotal    = findViewById(R.id.tvTotal);
        tvState    = findViewById(R.id.tvState);
        gaugeTemp  = findViewById(R.id.gaugeTemp);
        barWeight  = findViewById(R.id.barWeight);
        trend      = findViewById(R.id.trend);
        dotDrop    = findViewById(R.id.dotDrop);
        dotConn    = findViewById(R.id.dotConn);
        btnConnect = findViewById(R.id.btnConnect);

        gaugeTemp.setRange(20f, 45f);
        gaugeTemp.setLabel("温度", "℃");
        barWeight.setColors(0xFF2E7DE0, 0xFF35D0BA);

        dropAnim = ObjectAnimator.ofFloat(dotDrop, "alpha", 1f, 0.2f);
        dropAnim.setDuration(700);
        dropAnim.setRepeatMode(ValueAnimator.REVERSE);
        dropAnim.setRepeatCount(ValueAnimator.INFINITE);
        dropAnim.setInterpolator(new LinearInterpolator());

        btnConnect.setOnClickListener(v -> { if (running) disconnect(); else startConnect(); });
        btnConnect.setOnLongClickListener(v -> {
            host = DEFAULT_HOST; port = DEFAULT_PORT;
            updateConnText();
            return true;
        });
    }

    private void updateConnText() {
        ui.post(() -> {
            tvConn.setText(connected ? "已连接" : "未连接");
            dotConn.setBackgroundResource(connected ? R.drawable.dot_on : R.drawable.dot_off);
        });
    }

    private void startConnect() {
        running = true;
        btnConnect.setText("断开连接");
        updateConnText();
        netThread = new Thread(this::netLoop, "tcp-client");
        netThread.start();
    }

    private void disconnect() {
        running = false;
        connected = false;
        try { if (socket != null) socket.close(); } catch (Exception ignored) { }
        socket = null;
        btnConnect.setText("连接设备");
        updateConnText();
        ui.post(() -> {
            tvState.setText("待机");
            tvState.setTextColor(0xFF8FA6C4);
            tvState.setBackgroundResource(R.drawable.card_bg);
        });
    }

    private void netLoop() {
        while (running) {
            try {
                Socket s = new Socket();
                s.connect(new InetSocketAddress(host, port), 3000);
                s.setSoTimeout(6000);
                socket = s;
                connected = true;
                updateConnText();

                BufferedReader in = new BufferedReader(new InputStreamReader(s.getInputStream(), "UTF-8"));
                final OutputStream out = s.getOutputStream();

                Thread hb = new Thread(() -> {
                    while (running && connected) {
                        try { out.write("Q\n".getBytes("UTF-8")); out.flush(); } catch (Exception ignored) { }
                        try { Thread.sleep(2000); } catch (InterruptedException e) { return; }
                    }
                }, "tcp-hb");
                hb.setDaemon(true);
                hb.start();

                String line;
                while (running && (line = in.readLine()) != null) {
                    handleLine(line.trim());
                }
            } catch (Exception ignored) {
            } finally {
                connected = false;
                updateConnText();
                try { if (socket != null) socket.close(); } catch (Exception ignored) { }
                socket = null;
            }
            if (running) {
                try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
            }
        }
    }

    private void handleLine(String line) {
        if (line.isEmpty()) return;

        String t = null, w = null, d = null, l = null, r = null;
        for (String part : line.split(",")) {
            int eq = part.indexOf('=');
            if (eq <= 0) continue;
            String k = part.substring(0, eq).trim();
            String v = part.substring(eq + 1).trim();
            switch (k) {
                case "T": t = v; break;
                case "W": w = v; break;
                case "D": d = v; break;
                case "L": l = v; break;
                case "R": r = v; break;
            }
        }

        final String ft = t, fw = w, fd = d, fl = l, fr = r;
        ui.post(() -> {
            if (ft != null) {
                try { gaugeTemp.setValue(Float.parseFloat(ft)); } catch (Exception ignored) { }
            }
            if (fw != null) {
                try {
                    float g = Float.parseFloat(fw);
                    applyWeight(g);
                } catch (Exception ignored) { }
            }
            if (fd != null) {
                try {
                    float dpm = Float.parseFloat(fd);
                    tvDrop.setText(String.format("%.0f", dpm));
                    /* 帧间隔 2 秒，累加滴数 */
                    totalDrops += Math.round(dpm * 2f / 60f);
                    tvTotal.setText("累计 " + totalDrops + " 滴");
                    if (dpm > 1f && !dropAnim.isRunning()) dropAnim.start();
                    else if (dpm <= 1f && dropAnim.isRunning()) dropAnim.cancel();
                } catch (Exception ignored) { }
            }
            if (fl != null) applyLevel(fl, fr);
        });
    }

    /** 克重是主视觉：数字 + 液位条 + 百分比 + 预计剩余时间 */
    private void applyWeight(float g) {
        tvWeight.setText(String.format("%.1f", g));

        float pct = g / BAG_GRAMS;
        if (pct < 0) pct = 0;
        if (pct > 1) pct = 1;
        barWeight.setPercent(pct);
        tvPercent.setText(String.format("液位 %.0f%%", pct * 100));

        lastWeight = g;
        trend.add(g);
        updateEstimate();
    }

    /** 剩余时间 ≈ 剩余重量(ml) ÷ 当前流速(ml/min)，滴系数按 20 滴/ml */
    private void updateEstimate() {
        String dps = tvDrop.getText().toString().replaceAll("[^0-9.]", "");
        float dpm;
        try { dpm = Float.parseFloat(dps); } catch (Exception e) { dpm = 0; }

        if (lastWeight < 0 || dpm < 1f) {
            tvEstimate.setText("预计剩余 --");
            return;
        }
        float mlPerMin = dpm / DROPS_PER_ML;
        int minutes = (int) (lastWeight / mlPerMin);

        if (minutes >= 60) tvEstimate.setText(String.format("预计剩余 %d 时 %d 分", minutes / 60, minutes % 60));
        else               tvEstimate.setText(String.format("预计剩余 %d 分", minutes));
    }

    private static String reasonText(String r) {
        if (r == null) return "";
        int v;
        try { v = Integer.parseInt(r); } catch (Exception e) { return ""; }
        if ((v & 1) != 0) return "温度过高";
        if ((v & 4) != 0) return "药液将尽";
        if ((v & 8) != 0) return "滴速过快";
        if ((v & 2) != 0) return "温度过低";
        return "";
    }

    private void applyLevel(String lv, String rs) {
        String why = reasonText(rs);
        String tail = why.isEmpty() ? "" : " · " + why;
        switch (lv) {
            case "2":
                tvState.setText("报警" + tail);
                tvState.setTextColor(Color.WHITE);
                tvState.setBackgroundColor(0xFFB02A2A);
                break;
            case "1":
                tvState.setText("预警" + tail);
                tvState.setTextColor(0xFF231A00);
                tvState.setBackgroundColor(0xFFE8B33A);
                break;
            default:
                tvState.setText("正常");
                tvState.setTextColor(0xFF7FD1A0);
                tvState.setBackgroundResource(R.drawable.card_bg);
                break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        running = false;
        try { if (socket != null) socket.close(); } catch (Exception ignored) { }
        if (dropAnim != null) dropAnim.cancel();
    }
}
