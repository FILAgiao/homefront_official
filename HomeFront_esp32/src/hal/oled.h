#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

// 0.96" SSD1306 OLED, I2C, 中文 wqy12 字体
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

void oled_init();

// 绘制页面 (支持滚动 + 可选底部状态栏)
//   title        — 顶行标题
//   lines[]      — 全部内容行
//   totalCount   — lines 的总数
//   cursor       — 当前选中行 (0-based, -1=不显示光标)
//   cursorVisible — 是否绘制光标反白
//   footer       — 底部状态栏文字 (NULL=不显示)
//   title_right  — 标题栏右侧时间文字 (NULL=不显示)
//   wifi_ok      — 是否绘制信号条 (实心=已连接, 空心=未连接)
void oled_draw_page(const char *title, const char *lines[], int totalCount,
                    int cursor, bool cursorVisible, const char *footer = nullptr,
                    const char *title_right = nullptr, bool wifi_ok = false);
