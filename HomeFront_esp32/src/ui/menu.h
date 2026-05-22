#pragma once

// 初始化菜单系统
void menu_init();

// 每帧调用一次 (放在 loop() 中), 处理编码器/按键输入并刷新 OLED
void menu_tick();
