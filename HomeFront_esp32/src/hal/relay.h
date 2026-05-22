#pragma once

#include <Arduino.h>

// 初始化 74HC595 GPIO 并清零所有继电器
void relay_init();

// 设置继电器通道 ch (0~7), on=true 吸合
void relay_set(uint8_t ch, bool on);

// 关闭所有继电器（紧急停止）
void relay_all_off();
