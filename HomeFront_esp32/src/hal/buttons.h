#pragma once
#include <Arduino.h>

// 按键事件
enum ButtonEvent {
    BTN_NONE = 0,
    BTN_PRESS,       // 短按
    BTN_LONG_PRESS,  // 长按 (>800ms)
};

// 初始化三个按键 (KEY1=GPIO32, KEY2=GPIO33, KEY3=GPIO13)
// 全部 INPUT_PULLUP, 按下 = LOW
void buttons_init();

// 轮询按键, 返回事件类型; key_index = 0/1/2 对应 KEY1/KEY2/KEY3
ButtonEvent button_get_event(int key_index);
