#pragma once
#include <Arduino.h>

// 编码器事件
enum EncoderEvent {
    ENC_NONE = 0,
    ENC_UP,        // 顺时针 (正向)
    ENC_DOWN,      // 逆时针
    ENC_CLICK,     // 按下
};

// 初始化 EC11 编码器 (A=GPIO34, B=GPIO35, KEY=GPIO39)
// 使用 GPIO 中断, 不轮询
void encoder_init();

// 读取并清除最后一个编码器事件 (调用后状态复位)
EncoderEvent encoder_get_event();

// 直接读取编码器按键状态 (LOW = 按下)
bool encoder_is_pressed();
