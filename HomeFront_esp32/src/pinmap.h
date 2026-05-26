#pragma once

// ============================================================
// pinmap.h — 智能大棚核心板 ESP32 引脚映射
// 595 通道以 Netlist_核心板_2026-05-25.tel 为权威来源
// GPIO 映射基于用户实测验证
// ============================================================
// [已实测验证] 标 ✅, [待验证] 标 ⚠️

// ---- 74HC595 移位寄存器控制（3线 → 16路输出）----
// 以网表 Netlist_核心板_2026-05-25.tel 为权威来源
#define PIN_595_DS      13    // 数据 (SER) → U8.14/U9.14 级联
#define PIN_595_STCP    12    // 锁存 (RCLK) — 注意: GPIO12 是启动引脚, PCB 设计已处理
#define PIN_595_SHCP    14    // 时钟 (SRCLK) → U8.11/U9.11

// ---- 595 U8 (bit 0~7) → 继电器物理映射 (以网表为准) ----
// U8 是第一片 595 (hc595_data 低字节), 级联方向: ESP32 → U8 → U9
// RLY1~RLY6 = SRA-9VDC-CL 小继电器 120mA (电磁阀)
// RLY7~RLY8 = SLA-XXVDC-XL-A 大继电器 185mA (水泵)
#define HC595_CH_VALVE1   0    // Q0→SW0→RLY1 小 24V_OUT1(P4)
#define HC595_CH_PUMP2    1    // Q1→SW1→RLY8 大 AC_L_OUT2(U37)
#define HC595_CH_PUMP1    2    // Q2→SW2→RLY7 大 AC_L_OUT1(U36)
#define HC595_CH_VALVE2   3    // Q3→SW3→RLY2 小 24V_OUT2(P3)
#define HC595_CH_VALVE3   4    // Q4→SW4→RLY3 小 24V_OUT3(P5)
#define HC595_CH_VALVE4   5    // Q5→SW5→RLY4 小 12V_OUT1(P6)
#define HC595_CH_VALVE5   6    // Q6→SW6→RLY5 小 12V_OUT2(P8)
#define HC595_CH_VALVE6   7    // Q7→SW7→RLY6 小 12V_OUT3(P7)

// ---- 595 U9 (bit 8~15) → LED / 辅助控制 ----
// U9 是第二片 595 (hc595_data 高字节)
#define HC595_CH_LED1     8    // Q0→LED1 红色 状态指示
#define HC595_CH_LED2     9    // Q1→LED2 红色 状态指示
#define HC595_CH_LED3     10   // Q2→LED3 红色 状态指示
#define HC595_CH_LED4     11   // Q3→LED4 红色 状态指示
#define HC595_CH_M0       12   // Q4→HC595_M0 (经Q18至LORA_M0)
#define HC595_CH_M1       13   // Q5→HC595_M1 (经Q19至LORA_M1)
#define HC595_CH_4G_RST   14   // Q6→4G_RST (经R100至P9.6)
// bit 15 (Q7) 未连接, 预留

// ---- OLED I2C ----
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22

// ---- RS485 (UART0, GPIO1=TX, GPIO3=RX, 经光耦隔离至 U2 收发器) ----
// 以网表为准: U1.6→U1_RX→光耦U4→RS485收发器U2→RS485A/B
//              U1.7→U1_TX→光耦U5→RS485收发器U2→RS485A/B
// UART0 与 USB 串口调试共用, RS485 通信期间暂停调试输出
#define PIN_RS485_TX     1     // ✅ 网表: U1.7=U1_TX=GPIO1
#define PIN_RS485_RX     3     // ✅ 网表: U1.6=U1_RX=GPIO3

// ---- 4G 模块 (P9 排母, UART2) ----
// 以网表为准: U1.15→4G_TX=GPIO5, U1.8→4G_RX=GPIO25
#define PIN_4G_TX        5     // ⚠️ 网表: U1.15=4G_TX=GPIO5 (经R97)
#define PIN_4G_RX        25    // ⚠️ 网表: U1.8=4G_RX=GPIO25 (经R98)

// ---- 调试串口 (H3 排针, UART2) ----
// 以网表为准: U1.23→U2_TX=GPIO17, U1.24→U2_RX=GPIO16
#define PIN_DBG_TX       17    // ⚠️ 网表: U1.23=U2_TX=GPIO17 (经R63)
#define PIN_DBG_RX       16    // ⚠️ 网表: U1.24=U2_RX=GPIO16 (经R64)

// ---- EC11 旋钮编码器 ----
// ✅ EC11_KEY=GPIO32 (实测: 按下编码器触发)
// ✅ EC11_A=GPIO19  (实测: 转动滚轮电平变化)
// ✅ EC11_B=GPIO18  (实测: 滚轮正常工作)
#define PIN_EC11_A       19    // ✅
#define PIN_EC11_B       18    // ✅
#define PIN_EC11_KEY     32    // ✅

// ---- 按键 (按下 = LOW, INPUT_PULLUP) ----
// ✅ KEY1=GPIO4  (实测: 按钮正常响应)
// ✅ KEY2=GPIO27 (实测: 按钮正常响应)
// ✅ KEY3=GPIO33 (实测: 按钮正常响应)
#define PIN_KEY1         4     // ✅
#define PIN_KEY2         27    // ✅
#define PIN_KEY3         33    // ✅

// ---- ADC 模拟输入 (10:1 分压, 输入0~33V → ESP32 0~3.3V) ----
// ✅ Gemini 验证: ADC_IN1=34(ADC1_CH6), ADC_IN2=35(ADC1_CH7)
// 旧值 GPIO36 也是 ADC 但非原理图设计; GPIO4 无 ADC 功能, 确认错误
#define PIN_ADC1         34
#define PIN_ADC2         35

// ---- 光耦隔离开关量输入 (仅无源开关) ----
// ✅ Gemini 验证: SG_IN1=39, SG_IN2=36
// 旧值 GPIO18/19 与 EC11 编码器冲突, 已修正
#define PIN_SG1          39
#define PIN_SG2          36
