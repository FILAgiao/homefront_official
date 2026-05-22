#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <Ticker.h>
#include <ArduinoJson.h>

// ---- 硬件上限 (PCB 有 6 个小继电器+2 个大继电器) ----
#define MAX_VALVES 6
#define MAX_PUMPS  2

// ---- 状态机常量 ----
#define STATE_IDLE         0
#define STATE_OPEN_VALVE   1
#define STATE_PUMP_ON      2
#define STATE_CLOSE_PUMP   3
#define STATE_CLOSE_VALVE  4
#define VALVE_DELAY        5000

// ============================================================
// 所有跨模块共享的全局变量 extern 声明
// 定义在 main.cpp 顶部
// ============================================================

// ---- 网络 / WiFi ----
extern String ssid;
extern String password;
extern String device_id;
extern const char *host;
extern const uint16_t httpPort;
extern WiFiClient client;
extern Ticker tk;

// ---- 时间 ----
extern struct tm timeinfo;
extern struct tm NET_LOSTING_time;
extern struct tm start_work_time;
extern char time_temp[10];
extern char set_begin_time[10];
extern unsigned long BEGIN_TIMESTAMP;
extern bool NET_LOSTING_FLAG;

// ---- 硬件布局 (NVS 可配, 现场设定) ----
extern int valve_count;        // 实际阀门数 (1~MAX_VALVES)
extern int pump_count;         // 实际水泵数 (0~MAX_PUMPS)

// ---- 浇水参数 ----
extern int pin_watering_time[MAX_VALVES];
extern int working_solenoid_valve[MAX_VALVES];
extern int wat_begin_hour;
extern int wat_begin_min;
extern int work_times;
extern int time_gap_min;
extern int solenoid_line;

// ---- 浇水标志 ----
extern int auto_soil_watering_flag;
extern int auto_timing_watering_flag;
extern int carwash_flag;
extern int hand_watering_flag;
extern int pump_working_flag;
extern int vegetable_flag_hand;
extern int vegetable_flag_net;
extern int net_solenoid_flag;
extern int carwash_duration_min;   // 洗车持续分钟数
extern int field_valve_num;        // 菜地阀门号 1-based, 1~valve_count
extern int soil2wat;
extern int reboot_flag;

// ---- 状态机 ----
extern int current_state;
extern unsigned long state_start_time;

// ---- 心跳/断点 ----
extern volatile int breakpoint_flag;
extern int wifi_retry_times;
extern int wifi_to_reboot_times;

// ---- 网络重连参数 ----
extern const unsigned long wifiRetryInterval;
extern const unsigned long clientRetryInterval;

// ---- 传感器 ----
extern unsigned char item[8];
extern String data_soil;
extern float soil_moisture;
extern float soil_moisture_need;
extern int soil_moisture_test_maxsize;
extern int soil_moisture_list_size;

// ---- 调试/上报 ----
extern char data[128];
extern String time_status;
extern int ota_status;
extern String ota_feedback;

// ---- 遥测缓存 ----
extern bool time_to_go_flag;
extern bool soil_to_go_flag;

// ---- 物理按钮 ----
extern int physical_buttons;
extern int trigger_pin_status;

// ---- OTA ----
extern String upUrl;
