#pragma once

#include <Arduino.h>

// 初始化 74HC595 GPIO 并清零所有输出
void relay_init();

// 设置任意 595 通道 ch (0~15), on=true 输出高
// ch 0~7  = U8 (低字节) → 继电器
// ch 8~15 = U9 (高字节) → LED/辅助控制
void relay_set(uint8_t ch, bool on);

// 关闭所有继电器 (仅低字节 ch 0~7), LED/辅助状态保留
void relay_all_off();

// 控制 LED1~4 指示灯 (led 取值 1~4)
void led_set(uint8_t led, bool on);

// 控制 U9 辅助输出: HC595_CH_M0 / HC595_CH_M1 / HC595_CH_4G_RST
void aux_set(uint8_t ch, bool on);
