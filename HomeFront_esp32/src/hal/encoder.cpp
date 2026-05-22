#include "encoder.h"
#include "../pinmap.h"

/*
 * EC11 旋钮编码器
 * - A=GPIO34, B=GPIO35 (输入专用引脚, 支持中断)
 * - KEY=GPIO39 (按下 = LOW, 输入专用)
 * - 旋转逻辑: A 变化时读 B → A==B 则顺时针, 否则逆时针
 */

static volatile int encoder_pos = 0;
static volatile EncoderEvent last_event = ENC_NONE;
static volatile bool event_consumed = true;

// 中断服务: A 相变化
static void IRAM_ATTR encoder_isr()
{
    static uint8_t last_a = HIGH;
    uint8_t a = digitalRead(PIN_EC11_A);
    if (a != last_a)
    {
        last_a = a;
        uint8_t b = digitalRead(PIN_EC11_B);
        if (a == b)
        {
            encoder_pos++;
            last_event = ENC_UP;
        }
        else
        {
            encoder_pos--;
            last_event = ENC_DOWN;
        }
        event_consumed = false;
    }
}

// 中断服务: 按键按下
static void IRAM_ATTR encoder_key_isr()
{
    last_event = ENC_CLICK;
    event_consumed = false;
}

void encoder_init()
{
    pinMode(PIN_EC11_A, INPUT);
    pinMode(PIN_EC11_B, INPUT);
    pinMode(PIN_EC11_KEY, INPUT);

    attachInterrupt(digitalPinToInterrupt(PIN_EC11_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_EC11_KEY), encoder_key_isr, FALLING);
}

EncoderEvent encoder_get_event()
{
    if (event_consumed)
        return ENC_NONE;

    EncoderEvent ev = last_event;
    last_event = ENC_NONE;
    event_consumed = true;
    return ev;
}

bool encoder_is_pressed()
{
    return digitalRead(PIN_EC11_KEY) == LOW;
}
