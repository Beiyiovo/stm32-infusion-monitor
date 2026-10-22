/**
 ******************************************************************************
 *  report_tcp.c —— 局域网 TCP 上报（ESP32-C3 AT 固件）
 ******************************************************************************
 */
#include "report_tcp.h"
#include <stdio.h>
#include <string.h>

static int  s_link    = -1;             /* 当前连接的 link id，-1 = 没有 */
static char s_acc[128];                 /* 接收累积缓冲（找 +IPD / CONNECT） */
static int  s_acc_len = 0;

/* ==================== AT 辅助 ==================== */
static uint8_t wait_ok(uint32_t timeout_ms)
{
    char line[80];
    uint32_t t0 = millis();

    while ((millis() - t0) < timeout_ms) {
        if (uart2_recv_line(line, sizeof(line), 50) > 0) {
            if (strstr(line, "OK"))    return 1;
            if (strstr(line, "ERROR")) return 0;
        }
    }
    return 0;
}

static uint8_t at_cmd(const char *cmd, uint32_t timeout_ms)
{
    char buf[192];

    snprintf(buf, sizeof(buf), "%s\r\n", cmd);
    uart2_clear();
    uart2_send(buf);
    return wait_ok(timeout_ms);
}

/* ==================== 初始化 ==================== */
uint8_t report_init(void)
{
    uart2_init();
    delay_ms(300);
    uart2_clear();

    printf("[NET] AT 测试...\r\n");
    if (!at_cmd("AT", 1000))                       { printf("[NET] AT 无响应\r\n"); return 0; }

    printf("[NET] 设为 AP 模式...\r\n");
    if (!at_cmd("AT+CWMODE=2", 2000))              { printf("[NET] CWMODE 失败\r\n"); return 0; }

    printf("[NET] 建热点 %s ...\r\n", REPORT_SSID);
    {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "AT+CWSAP=\"%s\",\"%s\",5,3", REPORT_SSID, REPORT_PASS);
        if (!at_cmd(cmd, 5000))                    { printf("[NET] CWSAP 失败\r\n"); return 0; }
    }

    printf("[NET] 多连接模式...\r\n");
    if (!at_cmd("AT+CIPMUX=1", 2000))              { printf("[NET] CIPMUX 失败\r\n"); return 0; }

    printf("[NET] 开 TCP Server :%d ...\r\n", REPORT_PORT);
    {
        char cmd[48];
        snprintf(cmd, sizeof(cmd), "AT+CIPSERVER=1,%d", REPORT_PORT);
        if (!at_cmd(cmd, 3000))                    { printf("[NET] CIPSERVER 失败\r\n"); return 0; }
    }

    printf("[NET] 就绪：手机连热点 %s，访问 192.168.4.1:%d\r\n", REPORT_SSID, REPORT_PORT);
    return 1;
}

/* ==================== 解析上行 ==================== */
void report_poll(void)
{
    char c;

    while (uart2_getc_nonblock(&c)) {
        if (s_acc_len < (int)sizeof(s_acc) - 1) {
            s_acc[s_acc_len++] = c;
            s_acc[s_acc_len]   = '\0';
        } else {
            /* 缓冲满了还没匹配到，丢掉最前面的，避免死锁 */
            memmove(s_acc, s_acc + 64, s_acc_len - 64);
            s_acc_len -= 64;
            s_acc[s_acc_len] = '\0';
        }

        /* 1) 客户端发来数据：+IPD,<id>,<len>:<data> */
        {
            char *p = strstr(s_acc, "+IPD,");
            if (p) {
                int id = -1, len = 0;
                if (sscanf(p, "+IPD,%d,%d", &id, &len) >= 1 && id >= 0) {
                    s_link = id;
                    printf("[NET] 收到客户端 #%d 的请求\r\n", id);
                }
                s_acc_len = 0;
                s_acc[0]  = '\0';
                continue;
            }
        }

        /* 2) 客户端连上来：<id>,CONNECT */
        {
            char *p = strstr(s_acc, ",CONNECT");
            if (p) {
                int id = p[-1] - '0';
                if (id >= 0 && id <= 4) {
                    s_link = id;
                    printf("[NET] 客户端 #%d 已连接\r\n", id);
                }
                s_acc_len = 0;
                s_acc[0]  = '\0';
                continue;
            }
        }

        /* 3) 客户端断开：<id>,CLOSED */
        {
            char *p = strstr(s_acc, ",CLOSED");
            if (p) {
                printf("[NET] 客户端断开\r\n");
                s_link    = -1;
                s_acc_len = 0;
                s_acc[0]  = '\0';
                continue;
            }
        }
    }
}

uint8_t report_connected(void)
{
    return (s_link >= 0) ? 1 : 0;
}

/* ==================== 发送 ==================== */
uint8_t report_send(const char *payload)
{
    char cmd[64];
    char c;
    uint32_t t0;
    int len;

    if (s_link < 0 || payload == NULL) return 0;

    len = (int)strlen(payload);
    if (len <= 0) return 0;

    snprintf(cmd, sizeof(cmd), "AT+CIPSEND=%d,%d\r\n", s_link, len);
    uart2_clear();
    uart2_send(cmd);

    /* 等 ESP32 回 '>'（最多 1 秒） */
    t0 = millis();
    while ((millis() - t0) < 1000) {
        if (uart2_getc_nonblock(&c)) {
            if (c == '>') break;
        }
    }
    if ((millis() - t0) >= 1000) {
        printf("[NET] 等 '>' 超时，发送放弃\r\n");
        s_link = -1;
        return 0;
    }

    uart2_send(payload);
    return 1;
}
