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

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * 502 输液状态监测 —— 手机端（炫酷版）
 *
 * 链路：手机连 ESP32 热点 502-Infusion（密码 12345678）
 *       → App 作为 TCP 客户端连 192.168.4.1:8080
 *       → STM32 每 2 秒推一行：T=34.2,W=480,D=60,L=0
 *
 * L：0=正常 1=黄色预警 2=红色报警
 */
public class MainActivity extends AppCompatActivity {

    private static final String DEFAULT_HOST = "192.168.4.1";
    private static final int    DEFAULT_PORT = 8080;
    private static final float  MAX_WEIGHT  = 1000f;   // g
    private static final float  MAX_DRIP    = 120f;    // 滴/分

    private TextView tvConn, tvWeight, tvDrop, tvState;
    private GaugeView gaugeTemp;
    private LevelBarView barWeight;
    private View barDrop, dotDrop, dotConn;
    private Button btnConnect;

    private volatile boolean running = false;
    private volatile boolean connected = false;
    private Thread netThread;
    private Socket socket;
    private ObjectAnimator dropAnim;

    private final Handler ui = new Handler(Looper.getMainLooper());
    private String host = DEFAULT_HOST;
    private int port = DEFAULT_PORT;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvConn     = findViewById(R.id.tvConn);
        gaugeTemp  = findViewById(R.id.gaugeTemp);
        tvWeight   = findViewById(R.id.tvWeight);
        barWeight  = findViewById(R.id.barWeight);
        tvDrop     = findViewById(R.id.tvDrop);
        barDrop    = findViewById(R.id.barDrop);
        dotDrop    = findViewById(R.id.dotDrop);
        dotConn    = findViewById(R.id.dotConn);
        tvState    = findViewById(R.id.tvState);
        btnConnect = findViewById(R.id.btnConnect);

        gaugeTemp.setRange(20f, 45f);
        gaugeTemp.setLabel("液温", "℃");

        barWeight.setColors(0xFF2E7DE0, 0xFF35D0BA);
        barDrop.setPivotX(0f);
        barDrop.setScaleX(0f);

        // 滴速指示点呼吸动画
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
                tvWeight.setText(fw);
                try {
                    float g = Float.parseFloat(fw);
                    barWeight.setPercent(g / MAX_WEIGHT);
                } catch (Exception ignored) { }
            }
            if (fd != null) {
                tvDrop.setText(fd);
                try {
                    float dpm = Float.parseFloat(fd);
                    float f = Math.min(1f, dpm / MAX_DRIP);
                    barDrop.setScaleX(f);
                    if (dpm > 1f && !dropAnim.isRunning()) dropAnim.start();
                    else if (dpm <= 1f && dropAnim.isRunning()) dropAnim.cancel();
                } catch (Exception ignored) { }
            }
            if (fl != null) applyLevel(fl, fr);
        });
    }

    /** 报警原因位掩码：1=温度过高 2=温度过低 4=药液将尽 8=滴速过快 */
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
