#pragma once
#include <Arduino.h>

// ---- 时间工具 ----
bool get_localtime();
int  time_gap(tm now, tm set);
bool time_plus_check(int wat_begin_hour, int wat_begin_min, tm timeinfo);

// ---- 浇水决策 ----
bool time2go();
bool soil_go();
bool go_watering();

// ---- 阀门/水泵控制 ----
void pump_work();
void Solenoid_OffAll(int a = 0);  // a=0 全关; a=1~7 只开第 a 路
void shut_all();
void shut_all_soft();  // 触发状态机慢关, 不直接断硬件 (防锤)

// ---- 5 状态机执行器 ----
void flag_execute();
