#pragma once
#include <Arduino.h>
#include <U8g2lib.h>

// 0.96" SSD1306 OLED, I2C, 中文 wqy12 字体
extern U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2;

void oled_init();

// 绘制页面 (支持滚动)
//   title        — 顶行标题
//   lines[]      — 全部内容行
//   totalCount   — lines 的总数
//   cursor       — 当前选中行 (0-based, -1=不显示光标)
//   cursorVisible — 是否绘制光标反白
// 最多同时显示 4 行, 超出时自动滚动
void oled_draw_page(const char *title, const char *lines[], int totalCount, int cursor, bool cursorVisible);
