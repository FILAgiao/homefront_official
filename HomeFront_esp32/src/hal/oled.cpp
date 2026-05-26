#include "oled.h"
#include <Wire.h>

/*
 * 0.96" SSD1306 OLED (128x64) — 中文 wqy12 字体 (12x12)
 * 布局: 标题栏 14px + 内容行 + 可选底部状态栏
 */

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void oled_init()
{
    // 第1阶段: I2C 总线扫描 (独立 Wire 会话, 用完释放)
    Wire.begin();
    delay(10);

    Serial.print("[OLED] I2C scan... ");
    bool found = false;
    for (uint8_t addr = 1; addr < 127; addr++)
    {
        Wire.beginTransmission(addr);
        if (Wire.endTransmission() == 0)
        {
            Serial.print("found at 0x");
            Serial.print(addr, HEX);
            Serial.print(" ");
            found = true;
        }
    }
    if (!found)
    {
        Serial.println("NO DEVICE FOUND!");
    }
    else
    {
        Serial.println();
    }

    // 第2阶段: U8g2 初始化 (先释放 Wire 让 U8g2 干净启动)
    Wire.end();
    delay(100);

    uint8_t ok = u8g2.begin();
    Serial.print("[OLED] u8g2.begin()=");
    Serial.println(ok);

    // 显式确保显示开启 + 对比度最大
    // SSD1306 有时在 U8g2 默认初始化后仍处于暗屏状态
    u8g2.setContrast(255);
    u8g2.setPowerSave(0);
    delay(10);

    if (ok)
    {
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);
        u8g2.clearBuffer();
        u8g2.drawUTF8(0, 12, "HomeFront启动中");
        u8g2.sendBuffer();
        Serial.println("[OLED] buffer sent OK");
    }
    else
    {
        Serial.println("[OLED] U8G2 INIT FAILED!");
    }
}

#define TITLE_H      14     // 标题栏高度
#define ROW_HEIGHT   10     // 每行高度 (px)
#define FOOTER_H     10     // 底部状态栏高度

void oled_draw_page(const char *title, const char *lines[], int totalCount,
                    int cursor, bool cursorVisible, const char *footer,
                    const char *title_right, bool wifi_ok)
{
    u8g2.clearBuffer();

    // 标题栏 — 反白
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, TITLE_H);
    u8g2.setDrawColor(0);
    u8g2.drawUTF8(2, 12, title);

    // 标题栏右侧: 时间 + 📶 信号条
    if (title_right)
    {
        int rw = u8g2.getUTF8Width(title_right);
        int rx = 128 - rw - 4 - (4 * 2 + 3 * 2);  // 时间 + 4根条(宽2+间距2)
        if (rx < 56) rx = 56;
        u8g2.drawUTF8(rx, 12, title_right);

        // 4根信号柱: 宽2px, 间距2px, 同一基线, 从左到右渐高
        int bar_x = rx + rw + 4;
        int bar_bottom = TITLE_H - 1;               // 基线 y=13
        int heights[4] = {4, 7, 10, 13};           // ▁▄▆█ 从矮到高
        for (int i = 0; i < 4; i++)
        {
            int bar_top = bar_bottom - heights[i];
            if (wifi_ok)
                u8g2.drawBox(bar_x + i * 4, bar_top, 2, heights[i]);
            else
                u8g2.drawFrame(bar_x + i * 4, bar_top, 2, heights[i]);
        }
    }
    u8g2.setDrawColor(1);

    // 底部状态栏 (反白)
    if (footer)
    {
        u8g2.drawBox(0, 64 - FOOTER_H, 128, FOOTER_H);
        u8g2.setDrawColor(0);
        u8g2.drawUTF8(2, 64 - 2, footer);
        u8g2.setDrawColor(1);
    }

    if (totalCount == 0) { u8g2.sendBuffer(); return; }

    int visibleRows = footer ? 4 : 5;  // footer 占一行

    // 计算可见窗口 (以 cursor 为中心)
    int winStart = 0;
    if (cursorVisible && cursor >= 0)
    {
        winStart = cursor - visibleRows / 2;
        if (winStart < 0) winStart = 0;
        if (winStart + visibleRows > totalCount)
            winStart = totalCount - visibleRows;
        if (winStart < 0) winStart = 0;
    }

    int winEnd = winStart + visibleRows;
    if (winEnd > totalCount) winEnd = totalCount;

    for (int vi = winStart; vi < winEnd; vi++)
    {
        int row = vi - winStart;
        int y = TITLE_H + 2 + (row + 1) * ROW_HEIGHT;
        int maxY = footer ? 64 - FOOTER_H - 2 : 62;
        if (y > maxY) break;

        if (cursorVisible && vi == cursor)
        {
            u8g2.drawBox(0, y - 10, 128, ROW_HEIGHT);
            u8g2.setDrawColor(0);
            u8g2.drawUTF8(2, y, lines[vi]);
            u8g2.setDrawColor(1);
        }
        else
        {
            u8g2.drawUTF8(2, y, lines[vi]);
        }
    }

    u8g2.sendBuffer();
}
