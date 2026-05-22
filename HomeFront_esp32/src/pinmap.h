#pragma once

// ============================================================
// pinmap.h — 智能大棚核心板 ESP32 引脚映射
// 基于 SCH_核心板_2026-03-12 和 引脚等详细说明.txt
// ============================================================

// ---- 74HC595 移位寄存器控制（3线 → 16路输出）----
#define PIN_595_DS      25    // 数据 (SER)
#define PIN_595_STCP    26    // 锁存 (RCLK)
#define PIN_595_SHCP    27    // 时钟 (SRCLK)

// ---- 595 输出位 → 功能映射 ----
// bit 0~5: SW0~SW5 → RLY1~RLY6 (小继电器 120mA)
// bit 6~7: SW6~SW7 → RLY7~RLY8 (大继电器 185mA)
// bit 8~11: LED1~LED4

#define RELAY_CH_VALVE1   0    // SW0 → RLY1 小 → 电磁阀1
#define RELAY_CH_VALVE2   1    // SW1 → RLY2 小 → 电磁阀2
#define RELAY_CH_VALVE3   2    // SW2 → RLY3 小 → 电磁阀3
#define RELAY_CH_SPARE4   3    // SW3 → RLY4 小 → 预留
#define RELAY_CH_SPARE5   4    // SW4 → RLY5 小 → 预留
#define RELAY_CH_SPARE6   5    // SW5 → RLY6 小 → 预留
#define RELAY_CH_PUMP     6    // SW6 → RLY7 大 → 水泵 (185mA)
#define RELAY_CH_PUMP2    7    // SW7 → RLY8 大 → 备用水泵

// ---- OLED I2C ----
#define PIN_I2C_SDA      21
#define PIN_I2C_SCL      22

// ---- RS485 (UART1, 注意: 与 ESP32 下载串口共用，烧录时断开485) ----
#define PIN_RS485_RX     3
#define PIN_RS485_TX     1

// ---- LoRa / LTE 预留 UART2 ----
#define PIN_LORA_RX      16
#define PIN_LORA_TX      17

// ---- EC11 旋钮编码器 (仅输入) ----
#define PIN_EC11_A       34
#define PIN_EC11_B       35
#define PIN_EC11_KEY     39

// ---- 按键 (按下 = LOW, 外部上拉) ----
#define PIN_KEY1         32
#define PIN_KEY2         33
#define PIN_KEY3         13

// ---- ADC 模拟输入 (10:1 分压, 输入0~33V → ESP32 0~3.3V) ----
#define PIN_ADC1         36
#define PIN_ADC2         4

// ---- 光耦隔离开关量输入 (仅无源开关) ----
#define PIN_SG1          18
#define PIN_SG2          19
