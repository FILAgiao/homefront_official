#include "buttons.h"
#include "../pinmap.h"

/*
 * 三按键输入
 * - KEY1=GPIO32, KEY2=GPIO33, KEY3=GPIO13
 * - 外部上拉 (按下 = LOW)
 * - 软件消抖 + 长按检测
 */

static const uint8_t btn_pins[3] = {PIN_KEY1, PIN_KEY2, PIN_KEY3};
static const unsigned long debounce_ms = 50;
static const unsigned long long_press_ms = 800;

// 每键状态
static uint8_t         btn_last_stable[3] = {HIGH, HIGH, HIGH};
static uint8_t         btn_last_raw[3]     = {HIGH, HIGH, HIGH};
static unsigned long   btn_debounce_time[3] = {0, 0, 0};
static unsigned long   btn_press_start[3]   = {0, 0, 0};
static bool            btn_press_reported[3] = {false, false, false};
static bool            btn_long_reported[3]  = {false, false, false};

void buttons_init()
{
    for (int i = 0; i < 3; i++)
    {
        pinMode(btn_pins[i], INPUT_PULLUP);
    }
}

ButtonEvent button_get_event(int key_index)
{
    if (key_index < 0 || key_index > 2) return BTN_NONE;

    uint8_t raw = digitalRead(btn_pins[key_index]);

    // 消抖: 电平变化时重置计时器
    if (raw != btn_last_raw[key_index])
    {
        btn_debounce_time[key_index] = millis();
        btn_last_raw[key_index] = raw;
    }

    // 仅当稳定超过消抖时间才确认状态
    if (millis() - btn_debounce_time[key_index] < debounce_ms)
        return BTN_NONE;

    uint8_t stable = raw;

    // 检测按下沿
    if (stable == LOW && btn_last_stable[key_index] == HIGH)
    {
        btn_last_stable[key_index] = LOW;
        btn_press_start[key_index] = millis();
        btn_press_reported[key_index] = false;
        btn_long_reported[key_index] = false;
    }

    // 按下期间检测长按
    if (stable == LOW && !btn_long_reported[key_index])
    {
        if (millis() - btn_press_start[key_index] >= long_press_ms)
        {
            btn_long_reported[key_index] = true;
            return BTN_LONG_PRESS;
        }
    }

    // 检测释放沿
    if (stable == HIGH && btn_last_stable[key_index] == LOW)
    {
        btn_last_stable[key_index] = HIGH;
        // 如果长按已触发, 释放时不再报短按
        if (!btn_long_reported[key_index])
        {
            return BTN_PRESS;
        }
    }

    btn_last_stable[key_index] = stable;
    return BTN_NONE;
}
