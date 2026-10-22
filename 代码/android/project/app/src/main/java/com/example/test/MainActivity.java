package com.example.test;

import android.graphics.Color;
import android.os.Bundle;
import android.os.Handler;
import android.os.Looper;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.Socket;

/**
 * 502 输液状态监测 —— 手机端
 *
 * 链路：手机连 ESP32 的热点 502-Infusion（密码 12345678）
 *       → 本 App 作为 TCP 客户端连 192.168.4.1:8080
 *       → STM32 每 2 秒推一行：T=34.2,W=480,D=60,L=0
 *
 * L（报警等级）：0=正常 1=黄色预警 2=红色报警
 *
 * ★ TCP 是字节流，必须按换行符切帧，不能"收到一次算一帧"
 */
public class MainActivity extends AppCompatActivity {

    private static final String DEFAULT_HOST = "192.168.4.1";
    private static final int    DEFAULT_PORT = 8080;

    private TextView tvConn, tvTemp, tvWeight, tvDrop, tvState;
    private Button btnConnect;

    private volatile boolean running = false;
    private volatile boolean connected = false;
    private Thread netThread = null;
    private Socket socket = null;

    private final Handler ui = new Handler(Looper.getMainLooper());

    private String host = DEFAULT_HOST;
    private int port = DEFAULT_PORT;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        tvConn    = findViewById(R.id.tvConn);
        tvTemp    = findViewById(R.id.tvTemp);
        tvWeight  = findViewById(R.id.tvWeight);
        tvDrop    = findViewById(R.id.tvDrop);
        tvState   = findViewById(R.id.tvState);
        btnConnect = findViewById(R.id.btnConnect);

        btnConnect.setOnClickListener(v -> {
            if (running) {
                disconnect();
            } else {
                startConnect();
            }
        });

        // 长按按钮可以改 IP（比如以后换成路由器模式）
        btnConnect.setOnLongClickListener(v -> {
            new android.app.AlertDialog.Builder(this)
                    .setTitle("设置服务器地址")
                    .setMessage("当前：" + host + ":" + port)
                    .setPositiveButton("用默认 192.168.4.1:8080", (d, w) -> {
                        host = DEFAULT_HOST; port = DEFAULT_PORT; updateConnText();
                    })
                    .setNegativeButton("关闭", null)
                    .show();
            return true;
        });
    }

    private void updateConnText() {
        ui.post(() -> tvConn.setText(connected
                ? "已连接 " + host + ":" + port
                : "未连接  " + host + ":" + port));
    }

    private void startConnect() {
        running = true;
        btnConnect.setText("断开");
        updateConnText();

        netThread = new Thread(this::netLoop, "tcp-client");
        netThread.start();
    }

    private void disconnect() {
        running = false;
        connected = false;
        try { if (socket != null) socket.close(); } catch (Exception ignored) {}
        socket = null;
        btnConnect.setText("连接 " + host + ":" + port);
        updateConnText();
        ui.post(() -> tvState.setText("待机"));
    }

    /** 后台线程：连接 → 读行 → 断线重连 */
    private void netLoop() {
        while (running) {
            try {
                Socket s = new Socket();
                s.connect(new InetSocketAddress(host, port), 3000);
                s.setSoTimeout(5000);
                socket = s;
                connected = true;
                updateConnText();

                BufferedReader in = new BufferedReader(new InputStreamReader(s.getInputStream(), "UTF-8"));
                OutputStream out = s.getOutputStream();

                // 心跳/请求：每 2 秒发一个字节，兼作 keep-alive
                Thread hb = new Thread(() -> {
                    while (running && connected) {
                        try {
                            out.write("Q\n".getBytes("UTF-8"));
                            out.flush();
                        } catch (Exception ignored) { }
                        try { Thread.sleep(2000); } catch (InterruptedException e) { return; }
                    }
                }, "tcp-hb");
                hb.setDaemon(true);
                hb.start();

                String line;
                while (running && (line = in.readLine()) != null) {
                    handleLine(line.trim());
                }
            } catch (Exception e) {
                // 连不上/断开：1 秒后重试（running 期间）
            } finally {
                connected = false;
                updateConnText();
                try { if (socket != null) socket.close(); } catch (Exception ignored) {}
                socket = null;
            }

            if (running) {
                try { Thread.sleep(1000); } catch (InterruptedException e) { return; }
            }
        }
    }

    /** 解析一行：T=34.2,W=480,D=60,L=0 */
    private void handleLine(String line) {
        if (line.isEmpty()) return;

        String t = null, w = null, d = null, l = null;
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
            }
        }

        final String ft = t, fw = w, fd = d, fl = l;
        ui.post(() -> {
            if (ft != null) tvTemp.setText(ft);
            if (fw != null) tvWeight.setText(fw);
            if (fd != null) tvDrop.setText(fd);
            if (fl != null) applyLevel(fl);
        });
    }

    private void applyLevel(String lv) {
        switch (lv) {
            case "2":
                tvState.setText("报警：立即处理");
                tvState.setTextColor(Color.WHITE);
                tvState.setBackgroundColor(0xFFB02A2A);
                break;
            case "1":
                tvState.setText("预警：请关注");
                tvState.setTextColor(0xFF1A1A1A);
                tvState.setBackgroundColor(0xFFE8B33A);
                break;
            default:
                tvState.setText("正常");
                tvState.setTextColor(0xFF7FD1A0);
                tvState.setBackgroundColor(0xFF16263C);
                break;
        }
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
        running = false;
        try { if (socket != null) socket.close(); } catch (Exception ignored) {}
    }
}
