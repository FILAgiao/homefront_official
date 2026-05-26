#include "encoder.h"
#include "../pinmap.h"

/*
 * EC11 旋钮编码器 — 双相全状态机驱动
 *
 * 原理:
 *   编码器 A/B 两相输出 2-bit Gray 码, 旋转时状态按固定序列变化:
 *     CW:  00→01→11→10→00
 *     CCW: 00→10→11→01→00
 *   其他跳变 (如 00→11) 均为触点抖动, 状态机会自动过滤。
 *
 * 每个 detent (卡位) 经过 4 次状态转换, 累积 4 步后在 encoder_get_event()
 * 中折算为 1 个方向事件 (ENC_UP / ENC_DOWN), 保证光标移动 1 格/卡位。
 *
 * 双相均挂 CHANGE 中断, 任何一相变化都触发状态机。
 * 按键使用轮询 (50ms 消抖), 避免浮空引脚 ISR 风暴。
 */

static volatile int encoder_pos = 0;  // 累加步数 (正=CW, 负=CCW)

// ---- Gray 码状态机 (在 ISR 中执行) ----
// 查表: old_state*4 + new_state → delta
//   old_state, new_state ∈ {0:00, 1:01, 2:10, 3:11}
static const int8_t enc_delta_table[16] = {
     0,  1, -1,  0,   // 00→{00,01,10,11}
    -1,  0,  0,  1,   // 01→{00,01,10,11}
     1,  0,  0, -1,   // 10→{00,01,10,11}
     0, -1,  1,  0,   // 11→{00,01,10,11}
};

static void IRAM_ATTR encoder_isr()
{
    static uint8_t last_state = 0;
    uint8_t a = digitalRead(PIN_EC11_A);
    uint8_t b = digitalRead(PIN_EC11_B);
    uint8_t state = (a << 1) | b;

    if (state == last_state) return;

    int8_t delta = enc_delta_table[(last_state << 2) | state];
    encoder_pos += delta;
    last_state = state;
}

void encoder_init()
{
    pinMode(PIN_EC11_A, INPUT_PULLUP);
    pinMode(PIN_EC11_B, INPUT_PULLUP);
    pinMode(PIN_EC11_KEY, INPUT);

    // 双相均挂中断: 任一相变化都会触发状态机
    attachInterrupt(digitalPinToInterrupt(PIN_EC11_A), encoder_isr, CHANGE);
    attachInterrupt(digitalPinToInterrupt(PIN_EC11_B), encoder_isr, CHANGE);
}

EncoderEvent encoder_get_event()
{
    // ---- 轮询编码器按键 (消抖 50ms, 检测释放沿) ----
    {
        static uint8_t  ek_last_raw    = HIGH;
        static uint8_t  ek_last_stable = HIGH;
        static unsigned long ek_debounce_time = 0;

        uint8_t ek_raw = digitalRead(PIN_EC11_KEY);
        if (ek_raw != ek_last_raw)
        {
            ek_debounce_time = millis();
            ek_last_raw = ek_raw;
        }

        if (millis() - ek_debounce_time >= 50)
        {
            uint8_t ek_stable = ek_raw;
            if (ek_stable == LOW && ek_last_stable == HIGH)
                ek_last_stable = LOW;
            if (ek_stable == HIGH && ek_last_stable == LOW)
            {
                ek_last_stable = HIGH;
                Serial.println("[EK]");
                return ENC_CLICK;
            }
            ek_last_stable = ek_stable;
        }
    }

    // ---- 旋转事件: 从累加步数折算为 detent 事件 ----
    int pos;
    noInterrupts();
    pos = encoder_pos;
    interrupts();

    // 2 次状态转换 = 1 个 detent (卡位)
    if (pos >= 2)
    {
        noInterrupts();
        encoder_pos -= 2;
        interrupts();
        Serial.println("[E>]");
        return ENC_UP;
    }
    if (pos <= -2)
    {
        noInterrupts();
        encoder_pos += 2;
        interrupts();
        Serial.println("[E<]");
        return ENC_DOWN;
    }

    return ENC_NONE;
}

bool encoder_is_pressed()
{
    return digitalRead(PIN_EC11_KEY) == LOW;
}
