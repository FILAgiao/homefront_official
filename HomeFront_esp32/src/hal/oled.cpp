#include "oled.h"

/*
 * 0.96" SSD1306 OLED (128x64) — 中文 wqy12 字体 (12x12)
 * 布局: 标题栏 14px + 最多 5 行 × 10px = 64px
 */

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

void oled_init()
{
    u8g2.begin();
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.clearBuffer();
    u8g2.drawStr(0, 12, "HomeFront启动中");
    u8g2.sendBuffer();
}

#define VISIBLE_ROWS 5      // 屏幕上最多同时显示的行数
#define ROW_HEIGHT   10     // 每行高度 (px)
#define TITLE_H      14     // 标题栏高度

void oled_draw_page(const char *title, const char *lines[], int totalCount, int cursor, bool cursorVisible)
{
    u8g2.clearBuffer();

    // 标题栏 — 反白
    u8g2.setDrawColor(1);
    u8g2.drawBox(0, 0, 128, TITLE_H);
    u8g2.setDrawColor(0);
    u8g2.drawStr(2, 12, title);
    u8g2.setDrawColor(1);

    if (totalCount == 0) { u8g2.sendBuffer(); return; }

    // 计算可见窗口 (以 cursor 为中心)
    int winStart = 0;
    if (cursorVisible && cursor >= 0)
    {
        winStart = cursor - VISIBLE_ROWS / 2;
        if (winStart < 0) winStart = 0;
        if (winStart + VISIBLE_ROWS > totalCount)
            winStart = totalCount - VISIBLE_ROWS;
        if (winStart < 0) winStart = 0;
    }

    int winEnd = winStart + VISIBLE_ROWS;
    if (winEnd > totalCount) winEnd = totalCount;

    for (int vi = winStart; vi < winEnd; vi++)
    {
        int row = vi - winStart;
        int y = TITLE_H + 2 + (row + 1) * ROW_HEIGHT;
        if (y > 62) break;

        if (cursorVisible && vi == cursor)
        {
            u8g2.drawBox(0, y - 10, 128, ROW_HEIGHT);
            u8g2.setDrawColor(0);
            u8g2.drawStr(2, y, lines[vi]);
            u8g2.setDrawColor(1);
        }
        else
        {
            u8g2.drawStr(2, y, lines[vi]);
        }
    }

    u8g2.sendBuffer();
}
