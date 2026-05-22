#include "relay.h"
#include "../pinmap.h"

// 内部状态：16位，低8位=继电器SW0~SW7，高8位=LED等
static uint16_t hc595_data = 0x0000;

// 写入 2 个级联的 74HC595 (16bit)
// 发送顺序: 高字节先发 → 进入远端595(U2, 控制LED)
//           低字节后发 → 留在近端595(U1, 控制继电器SW0~SW7)
// MSBFIRST: bit15 最先移出 → 最终到 U2.Q7, bit0 最后 → U1.Q0
static void hc595_write(uint16_t data)
{
    digitalWrite(PIN_595_STCP, LOW);
    shiftOut(PIN_595_DS, PIN_595_SHCP, MSBFIRST, (data >> 8) & 0xFF);  // 高字节 → U2
    shiftOut(PIN_595_DS, PIN_595_SHCP, MSBFIRST, data & 0xFF);         // 低字节 → U1
    digitalWrite(PIN_595_STCP, HIGH);  // 锁存脉冲，输出到 Q0~Q7
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
    if (ch > 7) return;  // 范围保护

    if (on)
        hc595_data |= (1 << ch);
    else
        hc595_data &= ~(1 << ch);

    hc595_write(hc595_data);
}

void relay_all_off()
{
    hc595_data = 0x0000;
    hc595_write(hc595_data);
}
