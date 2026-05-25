#include "relay.h"
#include "../pinmap.h"

// 内部状态: 16 位
// 低 8 位 (bit 0~7)  = U8 (第一片 595) → 继电器 SW0~SW7
// 高 8 位 (bit 8~15) = U9 (第二片 595) → LED / 辅助控制
// 级联方向: ESP32(DS) → U8 → U9
static uint16_t hc595_data = 0x0000;

// 写入 2 个级联的 74HC595 (16bit)
// shiftOut 顺序: 先发高字节 → 推入 U8 再级联到 U9
//                后发低字节 → 留在 U8
// MSBFIRST: bit15 最先移出 → 最终到 U9.Q7, bit0 最后 → U8.Q0
static void hc595_write(uint16_t data)
{
    digitalWrite(PIN_595_STCP, LOW);
    shiftOut(PIN_595_DS, PIN_595_SHCP, MSBFIRST, (data >> 8) & 0xFF);  // 高字节 → U9
    shiftOut(PIN_595_DS, PIN_595_SHCP, MSBFIRST, data & 0xFF);         // 低字节 → U8
    digitalWrite(PIN_595_STCP, HIGH);  // 锁存脉冲
}

void relay_init()
{
    pinMode(PIN_595_DS,   OUTPUT);
    pinMode(PIN_595_STCP, OUTPUT);
    pinMode(PIN_595_SHCP, OUTPUT);

    // 上电默认全关
    hc595_data = 0x0000;
    hc595_write(hc595_data);
}

void relay_set(uint8_t ch, bool on)
{
    if (ch > 15) return;

    if (on)
        hc595_data |= (1 << ch);
    else
        hc595_data &= ~(1 << ch);

    hc595_write(hc595_data);
}

// 仅关继电器 (bit 0~7), LED/辅助状态保留
void relay_all_off()
{
    hc595_data &= 0xFF00;  // 清低 8 位, 保留高 8 位
    hc595_write(hc595_data);
}

void led_set(uint8_t led, bool on)
{
    uint8_t ch = 0;
    switch (led)
    {
        case 1: ch = HC595_CH_LED1; break;
        case 2: ch = HC595_CH_LED2; break;
        case 3: ch = HC595_CH_LED3; break;
        case 4: ch = HC595_CH_LED4; break;
        default: return;
    }
    relay_set(ch, on);
}

void aux_set(uint8_t ch, bool on)
{
    // 仅允许 M0/M1/4G_RST 通道
    if (ch == HC595_CH_M0 || ch == HC595_CH_M1 || ch == HC595_CH_4G_RST)
        relay_set(ch, on);
}
